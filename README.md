# MLToybox

Manor Lords(Steam, UE5)용 치트 모드와 외부 제어 패널입니다. 구성 요소는 네 가지입니다.

- **UE4SS Lua 모드**: 게임 안에서 실행됩니다.
- **네이티브 DLL**: C++ 후킹을 맡습니다.
- **오버레이 DLL**: 게임 화면 안에 설정 창을 띄웁니다(Insert). 지금은 영주·건설·업그레이드·상태 탭이 있습니다.
- **.NET 8 WinForms 패널**: 게임 밖에서 기능을 켜고 끕니다. 게임 단축키와 겹치지 않도록 별도 창으로 만들었습니다.

- 대상 빌드: Steam buildid 24905706. 게임이 업데이트되면 아래 "게임 업데이트 후 복구 절차"를 따릅니다.
- 전제: 게임 폴더에 UE4SS v3.0.1(`dwmapi.dll` 프록시)이 설치돼 있어야 합니다.
- 변경 이력: [`docs/CHANGELOG.md`](docs/CHANGELOG.md)
- 설계와 계획: [`docs/superpowers/specs/`](docs/superpowers/specs/), [`docs/superpowers/plans/`](docs/superpowers/plans/)
- 게임 내부 분석 기록(오프셋·함수·실측값): [`analysis/findings.md`](analysis/findings.md)

## 구성

| 경로 | 내용 |
|---|---|
| `mod/MLToybox` | UE4SS Lua 모드. 게임 폴더 `ue4ss/Mods/MLToybox`로 배포 |
| `mod/MLToyboxLab` | 개발용 인게임 Lua 실행기(필요할 때만 배포) |
| `mod/MLToyboxDump` | 분석용 덤프 모드(필요할 때만 배포) |
| `native/` | C++20 네이티브 DLL(MinHook)과 오버레이 DLL(Dear ImGui). Lua가 `package.loadlib`로 불러옴 |
| `panel/` | .NET 8 WinForms 제어 패널, 코어 라이브러리, 테스트(Lua 스펙 포함) |
| `tools/` | 배포·백업·덤프·빌드 스크립트, `tools/re` 실행 파일 분석 도구 |
| `analysis/` | 분석 기록(`findings.md`). 덤프 결과는 git에서 제외 |

### 동작 구조
```
패널 ──control.json──▶ Lua 모드 ──(게임 스레드)──▶ 게임 객체·데이터 표·UFunction
패널 ◀──status.json── Lua 모드
                       └─ 네이티브 DLL ◀─control.json / ─▶ native_status.json (후킹)
```
- 패널이 `bridge/control.json`에 설정과 일회성 명령을 씁니다. 모드는 1초마다 이 파일을 읽어 적용하고, `status.json`에 현재 상태를 씁니다.
- 명령은 id로 중복을 걸러 한 번만 실행합니다. 60초가 지난 명령은 무시합니다.
- 모드의 JSON 인코더는 빈 표를 `[]`로 씁니다. 패널은 이것을 빈 사전으로 읽습니다.
- 오버레이 DLL도 패널과 같은 방식으로 `control.json`을 쓰고 `status.json`을 읽습니다. 자기 설정은 `bridge/overlay.json`, 상태는 `bridge/overlay_status.json`에 둡니다.

## 게임 안 창 (오버레이)
게임 화면 위에 MLToybox 창을 띄웁니다. 패널을 켜지 않고 게임 안에서 설정을 바꿉니다.

- **여닫기**: Insert. 키는 [상태] 탭에서 Insert, Home, End, F7, F8, F9 가운데 고릅니다. 게임을 켜는 동안 화면 왼쪽 위에 8초 동안 안내가 뜨지만, 메인 메뉴가 나오기 전의 검은 시작 화면에 떠서 놓치기 쉽습니다.
- **즉시 반영**: "적용" 버튼이 없습니다. 체크는 바꾸는 즉시, 숫자 칸은 Enter를 누르거나 다른 곳을 누를 때 저장됩니다. 창 맨 위 줄이 "적용 대기 중"에서 "적용됨"으로 바뀌면 모드가 받은 것입니다.
- **지금 있는 탭**: 영주, 건설, 업그레이드, 상태. 자원·군사·용병·인구는 아직 패널에서 설정합니다(다음 단계에서 옮깁니다).
- **패널과 함께**: 둘 다 같은 `control.json`을 씁니다. 나중에 저장한 쪽이 반영되고, 오버레이는 패널이 바꾼 값을 1~2초 안에 다시 읽습니다.
  - **주의**: 패널은 켤 때와 "되돌리기"를 누를 때만 파일을 읽습니다. 패널을 켜 둔 채 오버레이에서 값을 바꾼 뒤 패널의 "적용"이나 명령 버튼(분대 생성, 가족 추가 등)을 누르면, 패널 화면에 남아 있던 옛 값(영주·건설·업그레이드 포함)이 다시 저장돼 오버레이에서 바꾼 것이 되돌아갑니다. 패널을 쓰기 전에 "되돌리기"를 눌러 다시 읽거나 패널을 껐다 켜세요.
- **입력**: 마우스가 창 위에 있거나 입력 칸에 커서가 있으면 그 입력은 게임에 가지 않습니다(2026-10-01 사용자 확인).
- **글자 크기**: [상태] 탭의 "글자 배율"(80~150%).
- **끄기**: 게임 폴더 `ue4ss/Mods/MLToybox/Scripts/config.lua`의 `overlay = true`를 `false`로 바꾸거나 `native/mltoybox_overlay.dll`을 지웁니다. 다른 기능은 그대로 동작합니다.
- **창이 안 뜰 때**: `bridge/overlay_status.json`의 `state`와 `reason`을 봅니다. `status.json`의 `overlay`에도 같은 내용이 있습니다. 오버레이는 그리기나 입력 처리에 실패하면 게임을 튕기게 하지 않고 스스로 꺼지도록 만들었습니다(`state`가 `disabled`).
- **확인하지 못한 조건**: 프레임 생성(FSR·DLSS FG), HDR, 전체 화면 전용 모드, 모니터 사이 이동. 문제가 있으면 위 방법으로 끄고 패널을 쓰세요.

## 기능 (패널 탭별)

### 자원
- 자원 55종과 지역 재화를 **영지마다** 목표값까지 채웁니다. 모자란 만큼만 채우고, 목표보다 많으면 그대로 둡니다.
- "영지" 목록에서 영지를 고르면 그 영지의 재고가 보이고, 영지별 목표를 따로 줄 수 있습니다.
  - 빈칸이면 공통 목표를 따릅니다.
  - 0이면 그 영지는 채우지 않습니다.

### 영주
- 국고, 영향력, 왕의 총애를 영지와 무관한 **영주 전체 값**으로 따로 관리합니다.
- 체크한 항목만 목표 아래로 떨어질 때 채웁니다. 체크를 해제하면 관리하지 않습니다.
- **지금 설정**: 체크 여부와 상관없이 입력한 값으로 한 번 정확히 맞춥니다. 내리는 것도 됩니다.
- 영향력과 왕의 총애는 값을 바꾼 뒤 HUD 갱신 함수(`updatePlayerStats`)를 부릅니다. 이 함수를 부르지 않으면 게임 화면 숫자가 바뀌지 않습니다.
- 기능이 꺼져 있어도 "현재" 값은 계속 보고합니다.

### 건설
- **배치 제한 무시**(영지 경계 안, 건물만): 네이티브 DLL이 담당합니다. 도로·성벽 배치에는 적용되지 않습니다. 성벽도 같은 플래그를 쓰는데, 지우면 크래시가 납니다.
- **즉시 완공**: 네이티브 DLL이 담당합니다.
- **자재 불필요, 즉시 수리**: Lua가 담당합니다.
- **지역당 개수 제한 해제**: `buildingStats.maxInRegion`을 0으로 만듭니다.
  - 대상 9종: 영주 저택 모듈(수비용 탑 등), 세금 징수소, 장작 수레, 식량 수레.

### 업그레이드
- 업그레이드 조건, 비용, 해금 요구를 무시합니다.

### 군사
- 부대 수 상한을 99로 올리고, 민병대 모집비를 0으로 만들고, 장비·훈련·집 레벨 요구를 풉니다. 용병 비용은 [용병] 탭이 맡습니다.
- **병력 생성**: 병종(민병대 6종, 친위대 2종, 용병 5종)을 골라 분대를 1~5개 새로 만듭니다(`spawnArmy`). 주민 수와 무관합니다.
  - "위치"에서 고른 내 영지의 영주 저택 옆에 생깁니다.
- **재구성**: 생성한 분대의 병사에게는 집이 없습니다. 그래서 게임에서 해제하면 병사가 사라지고 0/N 빈 카드만 남습니다.
  - "재구성"을 누르면 빈 카드와 같은 병종으로 다시 생성합니다.
  - 빈 카드는 `removeSquad`로 한 틱에 하나씩, 높은 ID부터 정리합니다.
- **수행원 꾸미기**: 병력 생성이나 커스텀 용병 고용으로 만든 친위대 분대는 게임이 수행원으로 보지 않아 꾸미기 화면이 열리지 않습니다. 분대를 고르고 "꾸미기 열기"를 누르면 모드가 그 분대를 잠깐 "위치" 영지의 수행원 분대로 두고 게임의 꾸미기 화면을 엽니다. 화면을 닫으면 원래 수행원 분대로 되돌립니다.
  - 화면이 그 분대 병사를 대상으로 열리고 선택·갑옷 업그레이드 함수가 그 병사에게 적용되는 것까지 확인했습니다. 화면의 무늬·색·이름 조작은 직접 눌러서 확인해야 합니다.
  - 이렇게 연 화면은 게임을 일시정지하지 않습니다(실측). 화면을 닫기 전까지 그 영지의 수행원 분대 번호가 모드 분대를 가리키고 있습니다.

### 용병
- **고용 창 자동 보충**: 게임은 용병 고용 창을 새 게임을 시작할 때 한 번만 채웁니다. 모드는 빈 칸을 고용 중이 아닌 순정 용병단으로 채웁니다(최대 3칸).
- **커스텀 용병단**: 이름, 분대 구성(1~10개), 고용비, 도착 영지를 정해 등록하면 고용 창에 카드로 뜹니다.
  - 고용해도 같은 카드가 다시 채워지므로 몇 번이든 고용할 수 있습니다.
  - 사용 중인 것은 최대 3개이고 앞 칸을 차지합니다. 순정 용병단이 모두 고용 중이어도 뜹니다.
  - 깃발은 순정 용병단 11개의 것 가운데 하나를 고를 수 있습니다(색·문장도 같이 따라옵니다). 고르지 않으면 덮어쓴 칸의 것을 물려받습니다.
- **환급**: 고용비는 원래 값을 유지합니다. AI 영주도 같은 목록에서 고용하고, 국고가 고용비 이상이면 고용하기 때문입니다. 내가 고용한 용병단만 고용비를 국고에 돌려주고 유지비를 0으로 만듭니다.
- **AI 잠금**: 커스텀 카드의 고용비를 고용 창이 닫혀 있는 동안 10,000,000으로 두고, 창을 열면 설정한 고용비로 바꿉니다.
  - 고용 창이 열려 있는 동안 게임은 일시정지 상태입니다(실측). 그 사이에 AI가 고용하지 못하는지는 따로 확인하지 않았습니다.
  - 점검 주기가 1초라, 창을 연 직후 최대 1초 동안 잠금 가격이 보이고 닫은 직후 최대 1초 동안 설정한 고용비가 남아 있을 수 있습니다.
- 목록을 다시 만들 때(고용 직후 등) 게임의 새 용병단 알림이 뜰 수 있습니다.

### 인구 (영지별)
- **자연 이민 배율(1~10배)**: 영지마다 계산합니다. 그 영지에 자연 이민으로 늘어난 만큼, 그 영지의 집에 추가로 들입니다. 모드가 들인 가족은 배율 계산에서 뺍니다.
- **목표 가족 수**: 공통 값은 "영지마다 최소 N가족"이라는 뜻입니다. 영지별로 따로 지정할 수 있습니다.
- **가족 추가**: 선택한 영지에 들입니다. "공통"이면 빈 자리가 많은 영지부터 채웁니다.
- 추가한 가족은 **미배치** 상태로 들어옵니다. 게임 함수(`spawnManorServantsInside`)는 새 가족을 그 집의 일꾼으로 배치하므로, 들인 직후 `unassignFamily`로 배치를 풉니다.

### 상태
- 각 기능의 활성 상태, 명령 결과, 네이티브 기능별 installed/active를 보여 줍니다.
- **게임 버그 방어(항상 켜짐)**: AI 민병대 점검 함수 두 곳이 분대원의 `Home`을 null 검사 없이 읽습니다. 민병대원 가족이 집을 잠깐 비운 사이에 이 함수가 돌면 게임이 튕깁니다. 이 상황에서는 호출을 건너뜁니다(`militia_guard`, `militia_guard_2`). 모드 없이도 같은 크래시가 재현됐습니다.

데이터 표(업그레이드·건물·유닛) 변경은 메모리에만 적용되고, 게임을 다시 켜면 원래대로 돌아갑니다. 기능을 꺼도 이미 적용된 값은 되돌리지 않습니다.

## 설치
    pwsh tools/backup-saves.ps1
    pwsh tools/build-native.ps1
    pwsh tools/deploy.ps1 -Mod MLToybox
    pwsh tools/deploy.ps1 -Mod MLToybox -Panel

1. 게임을 실행합니다.
2. 게임 안에서 Insert를 눌러 MLToybox 창을 엽니다(영주·건설·업그레이드·상태).
3. 나머지 탭(자원·군사·용병·인구)은 `dist/panel/MLToybox.Panel.exe`를 켜서 설정하고 "적용"을 누릅니다.

모드는 게임 시작 시 로드됩니다. 배포는 게임을 끈 상태에서 하거나, 배포한 뒤 게임을 재시작하세요. 패널은 실행 중이면 파일이 잠겨 배포할 수 없습니다.

## 제거
    pwsh tools/deploy.ps1 -Mod MLToybox -Remove

## 테스트
    dotnet test panel/MLToybox.sln          # 패널 코어 + Lua 스펙(NLua)
    dotnet build panel/MLToybox.sln         # WinForms 패널 컴파일 확인(테스트와 별도로 확인)
    pwsh tools/build-native.ps1 -Test       # 네이티브 DLL + 네이티브 테스트
    pwsh tools/tests/Tools.Tests.ps1        # 배포·백업 스크립트

## 개발 도구
- **인게임 Lua 실행기**
  1. `pwsh tools/deploy.ps1 -Mod MLToyboxLab`로 배포하고 게임을 재시작합니다.
  2. `pwsh tools/lab.ps1 -File probe.lua`를 실행합니다. 스크립트를 게임 스레드에서 실행하고 `print` 출력을 돌려줍니다.
  3. 개발이 끝나면 `-Remove`로 제거합니다.
  4. `& tools/lab-load.ps1 -Slot saveGame_8 -Start`는 게임을 켜고 그 세이브를 불러옵니다(화면 조작 불필요). `tools/lab/`에는 용병 상태 읽기(`merc_state.lua`), 고용 창 열기·닫기(`merc_screen.lua`), 카드 고용(`merc_hire.lua`) 스크립트가 있습니다. `lab.ps1 -Vars @{ NAME = '...' }`는 스크립트의 `__NAME__`을 바꿔 넣습니다.
- **Lua 파일 작성**: 백슬래시가 깨지지 않도록 셸 heredoc이 아닌 편집기로 작성합니다.
- **크래시 분석**: `%LOCALAPPDATA%\ManorLords\Saved\Crashes\*\UEMinidump.dmp`의 예외 주소를 `tools/re`의 `disasm`으로 역추적합니다(`analysis/findings.md` 크래시 절 참고).
- **게임 창 캡처**: `pwsh tools/capture-game.ps1 -Out shot.png`는 게임 창만 찍습니다(가려져 있어도 됩니다). `-Key Insert`는 키 메시지를 보냅니다. `bridge/overlay.json`의 `startOpen`(열린 채 시작)과 `devTab`(열 탭 이름)을 쓰면 마우스 없이 오버레이 화면을 확인할 수 있습니다. `tools/lab/resize.lua`는 해상도를 바꿉니다.

## 네이티브 계층
- **빌드**: `pwsh tools/build-native.ps1 -Test`를 실행하면 `native/build/mltoybox_native.dll`과 `mltoybox_overlay.dll`이 만들어집니다.
- **배포**: `pwsh tools/deploy.ps1 -Mod MLToybox`가 두 DLL을 `Mods/MLToybox/native/`에 복사합니다. 게임 실행 중에는 DLL이 잠겨 경고만 남깁니다.
- **기능**: `instant_build`, `placement`, `militia_guard`, `militia_guard_2`. 모두 패턴이 정확히 1곳에서 찾아지고 레이아웃 검사(`bodyChecks`)를 통과할 때만 설치됩니다.
- **분석 도구**: `dotnet run --project tools/re/MLToybox.Re -- <exec|disasm|sig|count|strings|xrefs> ...`
  - `exec <UFunction>`은 후보를 둘 이상 낼 수 있습니다.
  - `mov rax,[rdx+20h]`로 시작해 구현 함수를 `call`하는 쪽이 exec 썽크입니다.
  - `lea rdx,...; call` 형태는 UFunction 생성 함수입니다.
- **오버레이**(`native/overlay/`): `core/`는 설정·상태 문서와 저장 규칙(단위 테스트), `render/`는 DXGI `Present`·`ResizeBuffers`와 D3D12 `ExecuteCommandLists` 후킹과 입력, `ui/`는 Dear ImGui 화면입니다. 게임 코드의 주소가 아니라 DXGI·D3D12의 가상 함수 표에 후킹하므로 게임이 업데이트돼도 패턴을 다시 찾지 않습니다.
- **외부 라이브러리**: MinHook, Dear ImGui v1.92.9b, nlohmann/json v3.12.0(`native/third_party/`, 폴더마다 라이선스 파일).

## 게임 업데이트 후 복구 절차
1. `pwsh tools/deploy.ps1 -Mod MLToyboxDump`로 배포하고, 게임을 실행해 세이브를 불러옵니다.
2. `pwsh tools/dump.ps1`을 실행하면 `analysis/dumps/<시각>/`이 생깁니다.
3. 패널 [상태] 탭에서 비활성인 Lua 기능을 찾습니다. `findings.md`의 해당 항목을 새 덤프와 비교해 식별자를 고칩니다.
4. 네이티브 기능이 `pattern not found`, `pattern ambiguous`, `layout check failed`이면 다음 순서로 고칩니다.
   1. `findings.md`에 적힌 UFunction 이름이나 문자열로 `exec`/`strings`를 실행해 대상 함수를 다시 찾습니다.
   2. `sig <VA>`로 새 패턴을 만들고, `count`로 1곳에서만 찾아지는지 확인합니다.
   3. `native/src/features/<기능>.h`의 `kPattern`과 오프셋, `.cpp`의 `bodyChecks`를 새 실행 파일에 맞춰 고친 뒤 빌드하고 배포합니다.
   - 배치 제한 무시는 UFunction이 없습니다. 개발용 메모리 프로브(`bridge/probe_request.txt`에 `seq 주소 크기`)로 배치 가능·불가 상태의 플레이어 폰 메모리를 비교해 다시 찾습니다(Plan 3 부록 A.3).
5. `pwsh tools/deploy.ps1 -Mod MLToyboxDump -Remove`로 덤프 모드를 제거하고, `pwsh tools/deploy.ps1 -Mod MLToybox`로 다시 배포합니다.

## 주의
- 모드가 바꾼 값은 세이브에 저장됩니다. 테스트 전에는 항상 `backup-saves.ps1`을 실행하세요.
- 생성 분대, 추가 가족, 지역당 개수 제한 해제로 지은 건물은 모드를 제거해도 세이브에 남습니다.
- 용병 고용 창 목록과 고용한 커스텀 용병단은 세이브에 남습니다. AI 잠금을 켠 채 저장한 세이브에는 커스텀 카드가 10,000,000 고용비로 남습니다(용병 기능을 끄거나 모드를 제거한 뒤에도 그대로입니다).
