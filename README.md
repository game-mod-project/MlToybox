# MLToybox

Manor Lords(Steam)용 UE4SS Lua 치트 모드와 외부 제어 패널.

## 구성
- `mod/MLToybox` — UE4SS Lua 모드 (게임 폴더 `ue4ss/Mods/MLToybox`로 배포)
- `mod/MLToyboxDump` — 분석용 덤프 모드 (필요할 때만 배포)
- `panel/` — .NET 8 WinForms 제어 패널 (`dist/panel/MLToybox.Panel.exe`)
- `tools/` — 배포·백업·덤프 스크립트
- `analysis/findings.md` — 게임 내부 대상 매핑

전제: 게임 폴더에 UE4SS v3.0.1(`dwmapi.dll` 프록시)이 설치돼 있어야 한다.

## 기능

| 기능 | 담당 | 패널 탭 |
|---|---|---|
| ① 자원 목표값 유지 (자원 55종 + 지역 재화, 부족분만 보충, 내 지역마다). [자원] 탭 "영지"에서 영지를 고르면 그 영지의 재고를 보고 영지별 목표를 따로 줄 수 있다(빈칸 = 공통 목표 따름, 0 = 그 영지는 보충 안 함). | Lua | 자원 |
| ① 영주 자원: 국고·영향력·왕의 총애를 영지와 무관한 전체 값으로 따로 목표 유지(항목별 체크, 체크 해제 = 관리 안 함). 예전 설정의 자원 목표 Treasury/Influence는 처음 불러올 때 이쪽으로 옮겨진다 | Lua (`ChangeTreasury`, `changeKingsFavour`) | 영주 |
| ② 자재 불필요, 즉시 수리 | Lua | 건설 |
| ② 배치 제한 무시(영지 경계 안), 즉시 완공 | 네이티브 DLL | 건설 |
| ③ 업그레이드 조건·비용·해금 무시 | Lua | 업그레이드 |
| ④ 부대 수 상한 99, 모집비·용병 비용 0, 장비·훈련·집 레벨 요구 해제 | Lua | 군사 |
| 인구: 자연 이민 배율(1~10배), 목표 가족 수 유지, 즉시 가족 추가(빈 집 우선, 집당 최대 2가족) | Lua (`spawnManorServantsInside`) | 인구 |
| ④ 병력 생성: 병종(민병대 6·친위대 2·용병 5종)을 골라 1~5개 분대를 새로 생성 (주민 수와 무관) | Lua (`spawnArmy`) | 군사 |
| ④ 생성 분대 재구성: 생성 분대는 집이 없어 게임의 해제→집결이 되지 않는다(해제하면 0/N 빈 카드). [군사] 탭 '재구성'이 빈 카드와 같은 병종으로 다시 생성하고, 빈 카드는 한 틱에 하나씩 정리한다 | Lua (`spawnArmy`, `removeSquad`) | 군사 |
| 게임 버그 방어: AI 민병대원 가족이 집을 잠깐 비운 사이 AI 틱이 돌면 튕기는 크래시를 막음 (항상 켜짐) | 네이티브 DLL (militia_guard) | 상태 |

데이터 표(업그레이드·건물·유닛·용병) 변경은 메모리에만 적용되며 게임을 다시 켜면 원복된다. 기능을 꺼도 이미 적용된 값은 되돌리지 않는다.

## 설치
    pwsh tools/backup-saves.ps1
    pwsh tools/deploy.ps1 -Mod MLToybox
    pwsh tools/deploy.ps1 -Mod MLToybox -Panel
게임을 실행한 뒤 `dist/panel/MLToybox.Panel.exe`를 켜고, 탭에서 기능을 설정하고 "적용"을 누른다.
모드는 게임 시작 시에 로드되므로, 배포는 게임을 끈 상태에서 하거나 배포 후 게임을 재시작한다.

## 제거
    pwsh tools/deploy.ps1 -Mod MLToybox -Remove

## 테스트
    dotnet test panel/MLToybox.sln
    pwsh tools/tests/Tools.Tests.ps1

## 개발 도구
- 인게임 Lua 실행기: `pwsh tools/deploy.ps1 -Mod MLToyboxLab` 후 게임 재시작, 그다음 `pwsh tools/lab.ps1 -File probe.lua`. 게임 스레드에서 실행하고 `print` 출력을 돌려준다. 개발이 끝나면 `-Remove`로 제거한다.
- Lua 파일은 백슬래시가 깨지지 않도록 셸 heredoc이 아닌 편집기로 작성한다.

## 네이티브 계층 (spec §11)
- 빌드: `pwsh tools/build-native.ps1 -Test` → `native/build/mltoybox_native.dll`
- 배포: `pwsh tools/deploy.ps1 -Mod MLToybox` (DLL이 있으면 `Mods/MLToybox/native/`에 복사. 게임 실행 중에는 DLL이 잠겨 경고만 남음)
- 상태: 패널 [상태] 탭 "[네이티브]" (미로드/응답 없음/동작 중, 기능별 installed/active)
- 분석 도구: `dotnet run --project tools/re/MLToybox.Re -- <exec|disasm|sig|count|strings> ...`
  - `exec <UFunction>`은 후보를 둘 이상 낼 수 있다. `mov rax,[rdx+20h]`로 시작해 구현 함수를 `call`하는 쪽이 exec 썽크이고, `lea rdx,...; call` 형태는 UFunction 생성 함수다.
- 게임 업데이트로 기능이 `pattern not found`/`pattern ambiguous`가 되면:
  1. `analysis/findings.md`의 해당 기능 절에 적힌 UFunction 이름 또는 문자열로 `exec`/`strings`를 다시 실행해 대상 함수를 찾는다
  2. `sig <VA>`로 새 패턴을 만들고 `count`로 1인지 확인한다
  3. `native/src/features/<기능>.h`의 패턴 상수(`kPattern`)와 오프셋을 바꾸고, `.cpp`의 레이아웃 검사(`bodyChecks`) 명령 바이트도 새 exe에 맞춰 갱신한 뒤 빌드·배포한다
  - 레이아웃 검사가 실패하면 기능은 `layout check failed: ...`로 설치되지 않는다(잘못된 함수에 쓰는 것을 방지).
  - 배치 제한 무시는 UFunction이 없어 exec 추적이 불가능하다. 복구하려면 개발용 메모리 프로브(`bridge/probe_request.txt`에 `seq 주소 크기`)로 배치 가능·불가 상태의 플레이어 폰 메모리를 비교해 불가 플래그를 찾고, 그 플래그를 쓰는 함수를 다시 찾는다(Plan 3 부록 A.3 절차).

## 게임 업데이트 후 복구 절차
1. `pwsh tools/deploy.ps1 -Mod MLToyboxDump`, 게임 실행 후 세이브 로드
2. `pwsh tools/dump.ps1` → `analysis/dumps/<시각>/` 생성
3. 패널 [상태] 탭에서 "비활성(대상 없음)"인 기능을 확인하고, `findings.md`의 해당 항목을 새 덤프로 다시 확인해 식별자를 수정
4. `pwsh tools/deploy.ps1 -Mod MLToyboxDump -Remove` 후 `pwsh tools/deploy.ps1 -Mod MLToybox`

## 주의
모드가 기록한 값은 세이브에 저장된다. 테스트 전에 항상 `backup-saves.ps1`을 실행한다.
