# MLToybox — Manor Lords 치트 모드 설계

- 작성일: 2026-09-28
- 대상 게임: Manor Lords (Steam appid 1363080, buildid 24905706, UE5, 싱글플레이)
- 상태: 설계 승인 대기

## 1. 목적과 범위

개인 싱글플레이용 편의(치트) 모드와, 게임 업데이트 후에도 재분석·재배포가 쉬운 모드 제작 환경을 `E:\MLToybox`에 구성한다.

### 1.1 기능 요구사항

| # | 기능 | 확정 동작 |
|---|---|---|
| ① | 자원 수치 수정 | 설정한 자원별 목표값을 주기적으로 유지한다 (모든 자원: 건축 자재·식량·상품·지역 재화·영주 금고) |
| ② | 건물 무조건 건설/수리 | 배치 제한(지형·영역·겹침)을 무시하고, 배치 즉시 완공한다. 파손 건물은 즉시 수리한다 |
| ③ | 건물 업그레이드 조건 무시 | 충족 조건(생활 수준), 비용(재화·자재), 선행 해금(개발 포인트)을 모두 무시한다 |
| ④ | 민병대 무한징병·유지비 제거 | 민병대 장비 요구와 인구(징집 가능 인원) 제한을 무시하고, 친위대·용병 유지비를 0으로 만들고, 부대 수 상한을 해제한다 |

### 1.2 성공 기준

1. 게임 실행 시 모드가 로드되고, 패널에 "연결됨"이 표시된다.
2. 네 기능 각각을 패널에서 켜고 끌 수 있으며, 인게임 체크리스트(§6.3)를 통과한다.
3. 게임 업데이트 후 `tools\dump.ps1` → `analysis\findings.md` 갱신 → `tools\deploy.ps1` 절차로 복구할 수 있다.

### 1.3 범위 밖

인게임 UI, 게임 내 단축키, 멀티플레이, 원격 git 레포, 기능 해제 시 값 되돌리기(롤백).

## 2. 현황 (2026-09-28 조사)

- UE4SS가 이미 설치되어 있다 (`Binaries\Win64\dwmapi.dll` 프록시, `ue4ss\UE4SS.dll`, 2026-09-16).
  - 활성 모드: CheatManagerEnablerMod, ConsoleCommandsMod, ConsoleEnablerMod, BPML_GenericFunctions, BPModLoaderMod, Keybinds
  - `UE4SS.log`가 0바이트라 정상 기동 기록이 없다. 분석 1단계에서 확인한다.
  - `GuiConsoleEnabled = 0`, `GraphicsAPI = opengl`
- `Content\Paks\LogicMods`는 비어 있다. SDK 덤프는 아직 없다.
- 세이브 위치: `%LOCALAPPDATA%\ManorLords\Saved\SaveGames`
- 도구: git, python, .NET SDK 8.0.425

## 3. 접근 방식

**UE4SS Lua 모드로 출발하고, 필요한 기능만 C++로 승격한다.**

- 1차 구현은 UE4SS Lua로 한다. 프로퍼티는 주기적으로 기록하고, UFunction은 `RegisterHook`으로 후킹한다.
- Lua로 도달할 수 없는 로직(예: 네이티브 배치 판정)은 분석 단계에서 증거와 함께 판정한다. 그런 기능은 UE4SS C++ 모드 승격 후보로 사용자에게 보고하고 결정을 받는다. 승격은 이 설계의 범위 밖이며 별도로 설계한다.
- Pak/DataTable 재패킹은 쓰지 않는다. DataTable 값 변경이 필요하면 런타임에 Lua로 덮어쓴다.

검토했다가 기각한 방식:
- Pak 데이터 모드 단독: 로직성 요구사항(자원 유지·즉시 완공·배치·인구 제한)을 구현할 수 없다.
- 인게임 UMG 위젯: UE5 에디터 쿠킹이 필요하고 업데이트에 취약하다.
- UE4SS ImGui 탭: C++ 빌드 체인이 필수다. C++ 승격이 확정되면 그때 재검토한다.
- 핫키 제어: 인게임 단축키와 충돌할 위험이 있다.

## 4. 아키텍처

```
E:\MLToybox\                        (git, 로컬 전용, develop 흐름)
├─ CLAUDE.md                        레포 규칙 (브랜치 전략 포함)
├─ README.md                        설치·사용·업데이트 대응 절차
├─ mod\MLToybox\
│   ├─ enabled.txt
│   └─ Scripts\
│       ├─ main.lua                 진입점: 설정 로드 → 브리지 시작 → 기능 등록 → 맵 로드 감지
│       ├─ config.lua               기본값 (초기 상태, 자원 목표값, 주기, 실패 임계치)
│       ├─ lib\json.lua             순수 Lua JSON (rxi/json.lua, MIT, 번들)
│       ├─ core\
│       │   ├─ log.lua              "[MLToybox]" 접두 로그 (UE4SS.log)
│       │   ├─ safe.lua             pcall 래퍼, IsValid 검사, 연속 실패 카운트
│       │   ├─ finder.lua           클래스·인스턴스 조회와 캐시 (맵 전환 시 무효화)
│       │   ├─ ticker.lua           LoopAsync 기반 기능별 주기 실행
│       │   └─ bridge.lua           control.json 폴링 / status.json 기록
│       └─ features\
│           ├─ resources.lua        ①
│           ├─ build.lua            ②
│           ├─ upgrade.lua          ③
│           └─ military.lua         ④
├─ mod\MLToyboxDump\Scripts\main.lua  분석용 임시 모드: 맵 로드 후 덤프 1회 실행
├─ panel\
│   ├─ MLToybox.Panel\              .NET 8 WinForms 앱
│   └─ MLToybox.Panel.Tests\        xUnit
├─ tools\
│   ├─ deploy.ps1                   모드를 게임 ue4ss\Mods\에 복사하고 mods.txt에 등록, 패널을 publish해 dist\에 둠
│   ├─ dump.ps1                     덤프 모드 배포·해제, 결과를 analysis\dumps\로 수집
│   └─ backup-saves.ps1             SaveGames를 backups\<타임스탬프>\로 복사
├─ analysis\
│   ├─ dumps\                       (gitignore) 오브젝트 덤프·CXX 헤더·Lua 타입
│   └─ findings.md                  기능별 대상 매핑과 Lua 가능 여부 판정 (증거 포함)
└─ docs\superpowers\specs\
```

### 4.1 기능 모듈 규약

각 `features\*.lua`는 다음 테이블을 반환한다.

```lua
return {
  name = "resources",
  init = function(state) end,     -- 맵 로드 후 1회: 후킹 등록, 대상 조회
  tick = function(state) end,     -- ticker가 주기 호출 (주기형 기능만)
  enable = function(state) end,   -- 패널에서 켤 때
  disable = function(state) end,  -- 패널에서 끌 때: UnregisterHook, 주기 작업 중지
}
```

- `main.lua`는 모든 모듈을 로드하고, 활성 여부는 `control.json`(없으면 `config.lua`) 기준으로 결정한다.
- 한 모듈이 실패해도 다른 모듈에 영향을 주지 않는다.

### 4.2 런타임 흐름

1. UE4SS가 `main.lua`를 로드한다. 브리지를 시작하고 heartbeat 기록을 시작한다.
2. 맵 로드를 감지한다 (`LoadMap` 또는 GameState `BeginPlay` 후킹. 구체 대상은 분석에서 확정). 그 뒤 활성 모듈의 `init`/`enable`을 호출한다.
3. 브리지는 1초 간격으로 `control.json`의 mtime을 비교한다. 변경되면 파싱하고 차이를 모듈에 적용한 뒤 `appliedSeq`를 갱신한다.
4. 메인 메뉴로 돌아가면 `finder` 캐시를 무효화하고 `inGame=false`를 기록한다.

## 5. 제어 패널과 브리지

### 5.1 패널 (.NET 8 WinForms)

- 게임 경로 탐지: Steam 설치 경로 → `steamapps\libraryfolders.vdf` → appid 1363080이 있는 라이브러리 → `common\Manor Lords`. 실패하면 수동으로 지정하고 `%APPDATA%\MLToybox\panel.json`에 저장한다.
- 탭 구성:
  - [자원] 활성 토글, 자원별 목표값 표. 자원 목록은 분석에서 확정하고, 모드가 `status.json`으로 알려준 목록을 쓴다.
  - [건설] 배치 제한 무시·즉시 완공·즉시 수리 개별 토글
  - [업그레이드] 활성 토글
  - [군사] 장비 무시·인구 제한 무시·유지비 0·부대 수 상한 해제 개별 토글
  - [상태] 연결 상태, 기능별 활성·오류, 현재 자원값, 적용 대기 여부
- 적용: `control.json.tmp`에 쓴 뒤 `File.Replace`/`Move`로 원자적으로 교체한다. `seq`를 1씩 증가시킨다.
- 상태 폴링: 1초 간격으로 `status.json`을 읽는다. heartbeat가 5초 넘게 갱신되지 않으면 "게임 미실행 또는 모드 미로드"로 표시한다.

### 5.2 브리지 파일

위치: `<게임>\ManorLords\Binaries\Win64\ue4ss\Mods\MLToybox\bridge\`

`control.json` (패널 → 모드):
```json
{
  "version": 1,
  "seq": 12,
  "features": {
    "resources": { "enabled": true, "intervalSec": 2, "targets": { "<ResourceId>": 500 } },
    "build":     { "enabled": true, "ignorePlacement": true, "instantBuild": true, "instantRepair": true },
    "upgrade":   { "enabled": true },
    "military":  { "enabled": true, "ignoreEquipment": true, "ignorePopulation": true, "zeroUpkeep": true, "unlimitedSquads": true }
  }
}
```

`status.json` (모드 → 패널):
```json
{
  "version": 1,
  "heartbeat": 1790569176,
  "inGame": true,
  "appliedSeq": 12,
  "features": { "resources": { "active": true, "lastError": null } },
  "resourceIds": ["<ResourceId>"],
  "resources": { "<ResourceId>": 500 }
}
```

- `version`이 맞지 않으면 모드가 무시하고 오류를 보고한다.
- Lua의 `status.json` 기록도 임시 파일 기록 후 `os.rename`으로 교체한다.

## 6. 기능별 구현 전략

정확한 식별자는 분석 단계(§7)에서 `findings.md`로 확정한다. 아래는 탐색 방향과 대체 수단이다.

### 6.1 전략 표

| 기능 | 1차 수단 | 탐색 대상 | 대체 수단 |
|---|---|---|---|
| ① 자원 유지 | 주기(기본 2초)마다 저장소 수량 프로퍼티를 목표값으로 기록 | 자원 타입 enum/DataTable, 저장소(창고·곡창·시장) 수량 필드, 지역 재화·영주 금고 필드 | 저장소가 분산돼 수량을 한 곳에 쓰기 어려우면 지정 저장소 1곳에 몰아서 채우기 |
| ② 배치 무시 | 배치 유효성 UFunction 후킹 → 결과를 "유효"로 강제 | 건설 미리보기 액터의 유효성 함수·플래그 | Lua로 불가능하면 C++ 승격 후보로 보고 |
| ② 즉시 완공·수리 | 건설 현장 생성 또는 틱 시점에 진행도를 최대로 두고 완공 함수 호출, 내구도를 최대로 기록 | 공사 진행도·완공 UFunction·건물 내구도 필드 | 진행도만 최대로 두고 자연 완공에 맡김 |
| ③ 업그레이드 | 업그레이드 가능 판정 후킹 → true, 비용 0, 해금 체크 통과 | 업그레이드 요구 조건 함수, 비용 DataTable, 개발 포인트 해금 체크 | 런타임에 DataTable 행(비용·요구 조건)을 0 또는 무조건으로 덮어쓰기 |
| ④ 장비·인구 | 부대 정원·징집 가능 판정 후킹, 장비 요구 우회 | 민병대 부대 클래스, 정원·장비 슬롯, 징집 인원 계산 | 정원 프로퍼티를 직접 최대로 기록 |
| ④ 유지비 0 | 친위대·용병 upkeep 필드를 주기적으로 0으로 기록 | 부대·계약 데이터의 upkeep | 월 정산 함수 후킹 후 차감 취소 |
| ④ 부대 수 상한 | 최대 부대 수 필드를 큰 값으로 기록 | 플레이어 군사 관리자의 max squad | 부대 생성 가능 판정 후킹 |

### 6.2 공통 원칙

- 모든 쓰기·후킹은 `safe` 안에서 실행한다. 대상을 찾지 못하면 해당 기능을 비활성화하고 `lastError`를 기록한다. 크래시로 번지지 않게 한다.
- 기능을 끄면 후킹을 해제하고 주기 작업을 중지한다. 이미 반영된 값은 되돌리지 않는다.
- 모드가 기록한 값은 세이브에 그대로 저장된다. 그래서 테스트 전 세이브 백업을 필수로 한다.

### 6.3 인게임 검증 체크리스트

| 기능 | 통과 조건 |
|---|---|
| ① | 목표값을 500으로 적용하면 3초 안에 해당 자원 표시가 500이 된다. 소비해도 다음 주기에 500으로 복귀한다 |
| ② 배치 | 평소 배치 불가(빨간 미리보기)인 위치에 배치할 수 있다 |
| ② 완공 | 배치 직후 자재 운반 없이 건물이 완성 상태가 된다 |
| ② 수리 | 파손 건물이 다음 주기 안에 내구도 최대가 된다 |
| ③ | 조건·비용·해금을 충족하지 못한 주거지의 업그레이드 버튼이 활성화되고, 실행하면 레벨이 오른다 |
| ④ 장비·인구 | 장비 재고가 0이고 징집 인원이 부족해도 민병대 부대가 정원을 채워 생성된다 |
| ④ 유지비 | 월 정산 후 영주 금고와 지역 재화가 친위대·용병 유지비만큼 줄지 않는다 |
| ④ 상한 | 기본 최대치를 넘는 부대를 생성할 수 있다 |

## 7. 분석 절차

1. **UE4SS 기동 확인.** 게임을 실행하고 `UE4SS.log`에서 UE4SS 버전, 엔진 버전 감지, Lua 모드 로드를 확인한다. 실패하면 원인을 진단하고 수정한다.
2. **자동 덤프.** `MLToyboxDump`를 배포한다. 맵 로드 후 `DumpAllObjects`·`GenerateSDK`·`GenerateLuaTypes`를 1회 호출한다. 호출 가능 여부는 UE4SS 버전의 API로 확인한다. 결과는 `dump.ps1`로 `analysis\dumps\`에 수집하고, 끝나면 덤프 모드를 해제한다.
3. **데이터 확인(선택).** 덤프만으로 부족하면 FModel로 DataTable을 확인한다. 외부 다운로드이므로 사용자 승인을 받는다.
4. **매핑 문서화.** `findings.md`에 기능별 대상, 근거(덤프 파일과 줄), Lua 가능/불가 판정을 기록한다. Lua로 불가능한 항목은 C++ 승격 여부를 사용자에게 묻는다.

## 8. 테스트

| 대상 | 방법 |
|---|---|
| 패널 | xUnit: 경로 탐지(vdf 파싱), control.json 원자적 기록, status.json 파싱, heartbeat 타임아웃 |
| Lua 순수 로직 | 게임과 무관한 부분(브리지 병합, 설정 머지, seq 처리)을 독립 Lua 인터프리터로 테스트한다. 인터프리터 설치 여부는 계획 단계에서 확인한다 |
| 인게임 | §6.3 체크리스트를 `status.json`과 UE4SS 로그로 교차 검증한다 |

역할 분담: 게임 실행, 배포, 로그와 status 판독은 AI가 한다. 세이브 로드와 건물 배치 같은 인게임 조작은 AI가 computer-use로 먼저 시도하고, 막히면 정확한 조작 순서를 사용자에게 요청한다.

## 9. 에러 처리

- Lua: 같은 기능에서 연속 5회(`config.lua`에서 조정 가능) 실패하면 해당 기능만 자동 비활성화한다. 오브젝트를 참조할 때마다 `IsValid`를 검사한다.
- 브리지: 파싱에 실패하거나 버전이 맞지 않는 `control.json`은 무시하고 직전 상태를 유지하며 오류를 보고한다.
- 게임 업데이트: 대상을 찾지 못한 기능은 "비활성(대상 없음)"으로 표시한다. 복구 절차(재덤프 → findings 갱신 → 식별자 수정 → 배포)를 README에 명시한다.

## 10. 저장소 규칙

- 로컬 git, `main` 보호, `develop` 통합 브랜치, 작업은 `feat/*`·`fix/*`·`chore/*`·`docs/*`에서 한다.
- 원격 레포가 없으므로 PR 대신 로컬 `--no-ff` 머지로 같은 2단계 흐름(작업 브랜치 → develop → 릴리스 시 main)을 따른다.
- `.gitignore`: `analysis/dumps/`, `panel/**/bin/`, `panel/**/obj/`, `dist/`, `backups/`
