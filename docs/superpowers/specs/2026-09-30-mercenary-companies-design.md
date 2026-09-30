# 용병 고용 창 관리와 커스텀 용병단 — 설계

- 작성일: 2026-09-30
- 대상: MLToybox Lua 모드(`mod/MLToybox`)와 패널(`panel/`). 네이티브 DLL은 바꾸지 않는다.
- 근거: `analysis/findings.md` "용병 고용 — 목록 보충과 커스텀 용병단 (2026-09-30, 스파이크)"
- 상태: 설계 승인, 구현 계획 작성 전

## 1. 목적

게임의 용병 고용 창이 비지 않게 하고, 플레이어가 패널에서 정의한 용병단을 고용 창에 등록해 몇 번이든 고용할 수 있게 한다. AI 영주가 이 기능의 덕을 보지 않게 한다.

### 1.1 배경 (실측)

- 고용 창 목록(`engine.availableMercs`)은 새 게임 시작 때 한 번만 채워진다. 주기적으로 다시 채워지지 않는다.
- 고용을 막는 조건은 "고용비 > 국고" 하나뿐이다. 부대 수나 계약 수 제한은 없다.
- AI 영주도 같은 목록에서 고용한다. 조건은 "국고 ≥ 고용비"다. 지금 모드의 `military.zeroUpkeep`이 용병 표의 고용비를 0으로 만들어서 AI가 공짜로 고용한다.
- `engine:rerollMercenaries()`는 목록을 비우고 용병 표에서 최대 3개를 다시 뽑는다. 고용 중인 용병단과 이름이 같은 행은 뽑지 않는다. `questOnly` 특성의 2행은 100회 뽑기에 한 번도 나오지 않았다(이하 "quest 행").
- 용병 표는 11행이고, quest 행을 뺀 9행이 순정 후보다.
- 목록의 칸은 Lua에서 제자리 쓰기가 된다(이름, 고용비, 분대 목록, 도착 영지, 도착 일수). 칸 수는 Lua로 늘리거나 줄일 수 없고 다시 뽑기로만 바뀐다.
- `engine.availableMercs = { ... }`처럼 배열 전체를 대입하면 게임이 튕긴다.

### 1.2 요구사항

| # | 요구 | 확정 동작 |
|---|---|---|
| R1 | 고용 창 자동 보충 | 고용 창에 빈 칸이 생기면 고용 중이 아닌 순정 용병단으로 채운다 |
| R2 | 커스텀 용병단 등록 | 패널에서 이름, 분대 구성, 고용비, 도착 영지를 정해 등록하면 고용 창에 카드로 뜬다 |
| R3 | 상시 유지 | 등록한 용병단은 삭제하거나 끌 때까지 고용 창에 남는다. 고용되면 같은 구성이 다시 채워진다 |
| R4 | 전부 고용한 뒤에도 고용 | 순정 용병단이 모두 고용 중이어도 커스텀 용병단은 뜬다 |
| R5 | 플레이어만 환급 | 고용비는 원래 값을 유지한다. 플레이어가 고용하면 고용비를 돌려주고 그 용병단의 유지비를 0으로 만든다 |
| R6 | AI 잠금 | 커스텀 용병단은 고용 창이 닫혀 있는 동안 AI가 살 수 없는 가격으로 둔다 |

### 1.3 범위 밖

- 순정 용병단의 중복 고용. 순정 9개가 모두 고용 중이면 순정 칸은 빈다.
- 고용 창 칸 수 늘리기(3칸 고정).
- 커스텀 용병단의 깃발·문장·색 지정. 덮어쓴 칸의 것을 물려받는다.
- 순정 용병단에 대한 AI 제한. AI는 순정 규칙대로 자기 국고만큼 고용한다.
- 기능을 껐을 때 되돌리기. 이미 바뀐 목록과 환급은 그대로 둔다(기존 원칙과 같다).
- 패널 기능을 게임 안 창으로 옮기는 일(별도 작업).

## 2. 동작

### 2.1 고용 창 목록

- 칸은 최대 3개(`MAX_SLOTS = 3`)다.
- 사용 중인 커스텀 용병단이 앞 칸을 차지한다(최대 3개).
- 남는 칸은 고용 중이 아닌 순정 용병단으로 채운다. 이미 떠 있는 순정 카드는 다른 칸이 바뀌어도 유지한다.
- 순정 칸의 고용비는 용병 표의 값과 같게 맞춘다. 예전 옵션 때문에 0으로 저장된 칸도 고친다.
- 목표 칸 수: `n = min(3, 사용 중인 커스텀 수 + 고용 중이 아닌 순정 후보 수)`. quest 행은 순정 후보가 아니다.

### 2.2 커스텀 용병단

| 필드 | 규칙 |
|---|---|
| 이름 | 앞뒤 공백을 뗀 뒤 1~40자. 다른 커스텀 용병단이나 용병 표의 `Name`과 겹치지 않는다. 고용 창에 그대로 표시된다 |
| 분대 구성 | 병종 이름의 목록. 길이 1~10(`MAX_SQUADS = 10`). 각 병종은 유닛 표(`DT_UnitTemplates`)에 있어야 한다 |
| 고용비 | 0 이상의 정수 |
| 도착 영지 | 내 영지의 키(`regionUniqueTag`). 비었거나 내 영지가 아니면 내 첫 영지 |
| 사용 | 고용 창에 띄울지 여부. 사용 중인 것은 최대 3개. 설정에 4개 이상이면 모드는 앞의 3개만 쓰고 나머지를 `skipped`로 보고한다 |

- 목록에 쓰는 값: `Name`, `units`, `cost`, `arrivalRegion`, `arrivesIn = 1`. 특성(`traits`)은 비운다. 깃발과 색은 그 칸에 있던 값을 그대로 둔다.
- 목록에서 커스텀 칸은 이름으로 알아본다. 이름이 용병 표와 겹치지 않게 검증하므로 순정 칸과 혼동되지 않는다.

### 2.3 환급과 유지비 (`refund`)

- 대상: 고용 중 용병단(`engine.hiredMercs`) 가운데, 그 용병단 분대(`squad.companyID == id`)의 소유자가 플레이어이고 `cost > 0`인 것.
- 기능이 켜진 뒤 처음 성공한 점검에서는 환급 없이 `cost`만 0으로 만든다(켜기 전에 고용한 용병단).
- 그 뒤에 찾은 대상은 새 고용이다. `MLCheatManager_C:ChangeTreasury(cost)`로 돌려준 뒤 `cost`를 0으로 만든다.
- 분대가 하나도 없는 용병단과 AI 소유 용병단은 건드리지 않는다.

### 2.4 AI 잠금 (`lockFromAi`)

- 고용 창(`mercenaryScreen_C`)이 닫혀 있으면 커스텀 칸의 `cost`를 `LOCK_COST = 10,000,000`으로 둔다.
- 고용 창이 열려 있으면 설정한 고용비로 바꾸고 `updateCompanies()`로 화면을 갱신한다. 점검 주기가 1초라 창을 연 직후 최대 1초 동안 잠금 가격이 보일 수 있다.
- 정수 제자리 쓰기만 하므로 다시 뽑지 않고 알림도 없다.
- 순정 칸에는 적용하지 않는다.

### 2.5 기존 옵션 변경

- `military.zeroUpkeep`은 민병대 모집비(`pawn.recruitCost`)만 0으로 만든다. 용병 표와 고용 중 용병단의 `cost`는 더 건드리지 않는다.
- 패널 라벨을 "민병대 모집비 0"으로 바꾼다. 설정 키는 그대로 둔다.
- 용병 비용을 0으로 쓰던 사용자는 새 탭의 "환급"을 켜야 같은 효과를 얻는다. CHANGELOG에 적는다.

### 2.6 알려진 부작용

- 목록을 재구성하면 게임의 "새 용병단" 알림이 한 번 뜬다. 고용 한 번에 한 번꼴이다.
- 재구성 사이에는 최소 3초를 둔다(`REBUILD_MIN_INTERVAL = 3`).
- 목록은 세이브에 저장된다. AI 잠금이 켜진 채 저장한 세이브에는 커스텀 칸이 잠금 가격으로 남는다. 모드를 제거하면 그 카드는 10,000,000짜리로 남는다.

## 3. 브리지

프로토콜 `version`은 1을 유지하고 필드만 추가한다.

`control.json`의 `features.mercenaries`:
```json
{
  "enabled": false,
  "refund": true,
  "lockFromAi": true,
  "companies": [
    {
      "name": "토이박스 용병단",
      "units": ["mercenary_infantry", "mercenary_infantry", "mercenary_crossbowmen"],
      "cost": 3000,
      "region": "gold",
      "enabled": true
    }
  ]
}
```

`status.json`의 `mercenaries`(기능이 켜져 있고 인게임일 때만):
```json
{
  "slots": [
    { "name": "토이박스 용병단", "cost": 10000000, "custom": true },
    { "name": "wayward_sons", "cost": 90, "custom": false }
  ],
  "hiredMine": 2,
  "hiredAi": 3,
  "refunded": 6000,
  "skipped": [ { "name": "궁수대", "reason": "unknown unit: foo" } ],
  "note": null
}
```
- `refunded`는 맵을 불러온 뒤의 환급 합계다(맵을 떠나면 기능 상태가 지워진다).
- `skipped`는 검증을 통과하지 못했거나 칸이 모자라 띄우지 못한 커스텀이다.
- `note`는 재구성이 기대한 칸 수를 만들지 못했을 때의 설명이다.

## 4. Lua 모듈

| 파일 | 역할 | 의존 |
|---|---|---|
| `features/merc_plan.lua` | 순수 계산. 게임 객체를 만지지 않는다 | 없음 |
| `features/merc_list.lua` | 게임 쪽 읽기와 쓰기 | `core.game`, `core.datatable` |
| `features/mercenaries.lua` | 기능 등록, 환급, 상태 보고 | 위 둘, `core.game` |

기존 파일 변경:
- `config.lua`: `featureModules`에 `"mercenaries"` 추가.
- `core/registry.lua`: `status()`에 `mercenaries = st.mercenaries` 추가.
- `features/military.lua`: 용병 표와 `hiredMercs`의 `cost`를 0으로 만드는 코드 삭제.
- `core/game.lua`: 고용 창 위젯 조회(`mercScreen()`), 영지 키로 영지 객체 찾기(`regionByKey(key)`).

### 4.1 `merc_plan.lua`

입력과 출력은 모두 평범한 Lua 표다.

```lua
-- 정의 검증. 통과한 것과 건너뛴 것({ name, reason })을 돌려준다
plan.validate(companies, vanillaNames, unitExists) -> valid, skipped

-- 있어야 할 목록과 조치
plan.build({
  rows = { { rowName, name, cost, quest } ... },   -- 용병 표
  hiredNames = { [name] = true },
  current = { { name, cost } ... },                -- 지금 목록
  customs = valid,                                 -- 사용 중인 것만, 최대 3개
  screenOpen = bool, lockFromAi = bool,
  pick = function(candidates, count) ... end,      -- 새 순정 고르기(테스트에서는 고정)
}) -> {
  desired = { { kind = "custom", company = c, cost = n } | { kind = "vanilla", rowName = r } ... },
  action = "none" | "inplace" | "rebuild",
  renames = k,          -- 재구성 때 임시로 이름을 바꿀 표 행 수
}
```

규칙:
- 순정 후보 = `quest`가 아니고 `hiredNames`에 없는 행.
- `pick`의 기본 구현은 `math.random`으로 고른다.
- `desired`는 커스텀(정의 순서) 다음에 순정을 둔다. 순정은 지금 목록에 있는 후보를 먼저 유지하고, 남는 칸을 `pick`으로 채운다.
- 커스텀 칸의 `cost`는 `lockFromAi and not screenOpen`이면 `LOCK_COST`, 아니면 정의의 고용비다.
- `#current == #desired`이면 `action`은 `"none"`(모든 칸이 맞음) 또는 `"inplace"`다. 다르면 `"rebuild"`다.
- `renames = max(0, #desired - 순정 후보 수)`.

### 4.2 `merc_list.lua`

```lua
list.read(engine)                      -- { { name, cost } ... }
list.rows()                            -- 용병 표 행 요약. quest = traits 에 "questOnly" 가 있음
list.hiredNames(engine)                -- { [name] = true }
list.screenState()                     -- { open = bool, confirming = bool }
list.applyInPlace(engine, desired)     -- 다른 칸만 덮어쓴다
list.rebuild(engine, desired, renames) -- 4.3 절차. 실제 칸 수를 돌려준다
list.refreshScreen()                   -- 고용 창이 열려 있으면 updateCompanies()
```

칸 쓰기:
- 커스텀: `Name`, `units`(Lua 표 대입), `traits`(빈 표), `cost`, `arrivesIn = 1`, `arrivalRegion`.
- 순정: 표 행에서 `Name`, `units`, `traits`, `banner`, `cost`, `colorA`, `colorB`, `emblemA`, `emblemB`를 복사한다. 유지하는 칸은 `arrivalRegion`과 `arrivesIn`을 재구성 전 값으로 되돌리고, 새 칸은 다시 뽑기가 준 값을 둔다.
- 배열 전체 대입은 어디서도 하지 않는다.

### 4.3 재구성 절차

한 번의 게임 스레드 호출 안에서 끝낸다.

1. 유지할 순정 칸의 `arrivalRegion`과 `arrivesIn`을 Lua 값으로 기억한다.
2. `renames > 0`이면 `quest`가 아니고 이름이 `hiredNames`에 있는 표 행을 앞에서부터 `renames`개 골라 `Name`을 `원래 이름 .. "#mlt"`로 바꾼다.
3. `engine:rerollMercenaries()`를 `pcall`로 부른다.
4. 2에서 바꾼 행의 `Name`을 되돌린다. 3이 실패해도 반드시 실행한다.
5. 실제 칸 수 `m`을 읽는다. `min(m, #desired)`칸을 `desired` 순서대로 덮어쓴다.
6. `m ~= #desired`이면 상태 `note`에 적고, 다음 재구성은 10초 뒤에 시도한다.

### 4.4 `mercenaries.lua`

```lua
M = { name = "mercenaries", intervalSec = 1 }
M.enable(state, settings)     -- state.merc = { baselineDone = false, refunded = 0, lastRebuild = nil }
M.tick(state, settings)       -- 설정 변경은 다음 tick 이 반영한다(configure 는 두지 않는다)
```

`tick` 순서:
1. 폰, 엔진이 없으면 끝낸다.
2. `refund`가 켜져 있으면 2.3을 수행한다.
3. `list.screenState()`의 `confirming`이 참이면 목록을 건드리지 않는다.
4. `plan.validate` → `plan.build` → `action`에 따라 `applyInPlace` 또는 `rebuild`(최소 간격을 지킴) → 바뀐 것이 있으면 `refreshScreen`.
5. `state.mercenaries`에 3절의 상태를 쓴다.

## 5. 패널

### 5.1 "용병" 탭

`MLToybox.Panel/MercenaryTab.cs`(UserControl)로 분리한다. `MainForm.cs`에는 탭 추가와 연결만 넣는다.

- 옵션: "용병 기능 사용 (고용 창 자동 보충)", "내 용병단 고용비 환급, 유지비 0", "커스텀 용병단 AI 잠금 (고용 창을 열 때만 설정한 고용비)".
- 등록한 용병단 목록: 사용(체크), 이름, 구성 요약("보병 2, 석궁병 1"), 고용비, 도착 영지. 버튼 "새 용병단", "삭제".
- 편집 영역: 이름, 분대 추가(병종 선택 + 분대 수 + "추가"), 구성(병종별 수와 제거, "합계 n / 10"), 고용비, 도착 영지(모드가 보고하는 내 영지), "등록".
- 상태 줄: 고용 창 목록, 내 용병단 수와 AI 용병단 수, 이번 세션 환급 합계, 띄우지 못한 커스텀과 이유.

동작:
- "등록"은 편집 내용을 목록에 저장하고 바로 적용한다(`control.json` 기록). 목록의 행을 고르면 편집 영역에 불러온다. 같은 행을 고친 뒤 "등록"하면 그 행을 갱신한다.
- 사용 체크가 3개인 상태에서 네 번째를 체크하면 막고 안내한다.
- "삭제"는 목록에서 지우고 바로 적용한다. 이미 고용한 용병단은 게임에 남는다.
- 검증에 걸리면 저장하지 않고 이유를 보여 준다.
- 옵션 체크박스 세 개는 다른 탭과 같이 하단 "적용" 버튼으로 반영한다.
- 상태 줄의 환급 합계는 "맵을 불러온 뒤" 기준이다.

`MercenaryTab`의 공개 면:
```csharp
void LoadFrom(MercenariesControl control);
MercenariesControl Read();
void ShowStatus(StatusDocument? status);
event EventHandler ApplyRequested;   // "등록", "삭제", 사용 체크 변경
```

### 5.2 `MLToybox.Panel.Core`

- `ControlDocument.cs`: `FeaturesControl.Mercenaries`, `MercenariesControl { Enabled, Refund = true, LockFromAi = true, Companies }`, `MercCompany { Name, Units, Cost, Region, Enabled }`.
- `StatusDocument.cs`: `MercenaryStatus { Slots, HiredMine, HiredAi, Refunded, Skipped, Note }`.
- `MercCompanyRules.cs`: 2.2의 검증(패널 쪽), 구성 요약 문자열, 사용 3개 제한. 용병 표의 `Name` 11개는 상수로 둔다(모드는 실제 표로 다시 검증한다).
- 병종 목록은 기존 `UnitCatalog`를 쓴다.

### 5.3 군사 탭

- "용병 비용·모집비 0 (...)" 라벨을 "민병대 모집비 0"으로 바꾼다.

## 6. 에러 처리

- 모든 게임 접근은 기존 `registry`의 `safe.call` 안에서 돈다. 연속 5회 실패하면 이 기능만 꺼지고 `lastError`가 보고된다.
- 표 행 이름 되돌리기(4.3의 4)는 다시 뽑기의 성공 여부와 무관하게 실행한다.
- 검증을 통과하지 못한 커스텀은 건너뛰고 `skipped`로 보고한다. 나머지는 계속 처리한다.
- 고용 확인 창이 떠 있는 동안에는 목록을 바꾸지 않는다.
- 내 영지가 하나도 없으면 커스텀을 띄우지 않고 `skipped`에 이유를 적는다.
- 치트 매니저를 찾지 못하면 환급을 건너뛰고 `cost`도 그대로 둔다(다음 점검에서 다시 시도).

## 7. 구현 첫 단계의 실측

결과에 따라 설계가 갈리는 항목이다. 구현 계획의 첫 태스크로 Lab에서 확인하고 `findings.md`에 기록한다.

| # | 확인할 것 | 안 될 때 |
|---|---|---|
| M1 | 표 행의 `banner`(객체)와 `traits`(이름 배열)를 목록 칸에 쓰는 동작 | 순정 칸은 다시 뽑기 결과를 그대로 쓴다. 기존 순정 카드 유지는 하지 않고, 칸 수가 같은데 순정 칸을 바꿔야 하면 재구성한다. 커스텀은 임시 이름(`#mlt`) 칸부터 덮어쓴다 |
| M2 | 민병대·친위대 병종이 용병 고용 경로(`hireMercs`)로 생성되는지 | 안 되는 병종을 커스텀 용병단의 병종 목록에서 뺀다 |
| M3 | 고용 뒤 부대 패널의 용병단 배너와 툴팁에 커스텀 이름이 보이는지 | 보이는 대로 README에 적는다(동작은 바꾸지 않는다) |
| M4 | 커스텀 용병단(목록과 고용 중)이 저장과 로드를 거쳐 유지되는지 | 목록은 로드 뒤 모드가 다시 맞춘다. 고용 중 용병단이 깨지면 이 기능을 보류하고 사용자에게 보고한다 |
| M5 | 고용 창이 열렸을 때 `mercenaryScreen_C:IsVisible()`이 참인지 | BP 함수 `Open`/`Close` 후킹으로 열림 상태를 추적한다. 그것도 안 되면 AI 잠금을 빼고 패널에서 옵션을 숨긴다 |

## 8. 테스트

| 대상 | 내용 |
|---|---|
| `tests/merc_plan_spec.lua` | 목표 칸 수, 커스텀 우선 배치, 기존 순정 칸 유지, 조치 판정, `renames` 계산, AI 잠금 가격, 정의 검증(이름·분대 수·고용비·중복·모르는 병종) |
| `tests/merc_list_spec.lua` | 가짜 `rerollMercenaries`(고용 중 이름과 quest 행 제외, 최대 3개)로 재구성 뒤 칸 수와 내용 확인. 다시 뽑기가 실패해도 표 행 이름이 되돌아옴. 배열 전체 대입을 하지 않음. 제자리 수정은 다른 칸만 씀 |
| `tests/mercenaries_spec.lua` | 첫 점검은 환급 없이 0, 이후 새 고용은 환급 후 0, AI 소유와 분대 없는 용병단은 그대로, 확인 창이 떠 있으면 목록을 건드리지 않음, 재구성 최소 간격, 상태 보고 |
| `tests/military_spec.lua` | 용병 표와 고용 중 `cost`를 건드리지 않음 |
| `MLToybox.Tests` (xUnit) | `MercCompanyRules`, `features.mercenaries` 직렬화 왕복과 기본값, `status.mercenaries` 읽기(빈 배열 포함) |

실행: `dotnet test panel/MLToybox.sln`과 `dotnet build panel/MLToybox.sln`.

### 8.1 인게임 검증

검증은 Lab과 `control.json`으로 한다. 세이브는 Lab으로 불러온다(findings "개발 절차 메모"). 시작 전에 `tools/backup-saves.ps1`을 실행한다. 저장·로드 검증은 새 슬롯에 저장하고 기존 세이브를 덮어쓰지 않는다.

| 항목 | 통과 조건 |
|---|---|
| 자동 보충 | 고용 창이 빈 세이브에서 2초 안에 순정 카드가 채워지고, 고용비가 표의 원래 값이다 |
| 커스텀 등록 | 패널에서 등록하면 고용 창에 그 이름, 구성, 고용비의 카드가 뜬다 |
| 상시 유지 | 커스텀을 고용하면 분대가 생기고 같은 카드가 다시 뜬다. 3회 연속 고용해도 된다 |
| 전부 고용 | 순정 9개가 모두 고용 중인 상태에서도 커스텀 카드가 뜬다 |
| 환급 | 고용 직후 국고가 고용 전 값으로 돌아오고, 내 용병단의 `cost`가 0이다. AI 용병단의 `cost`는 그대로다 |
| AI 잠금 | 고용 창을 닫으면 커스텀 `cost`가 10,000,000, 열면 설정값이다. 닫은 채 5분 동안 AI가 커스텀을 고용하지 않는다 |
| 안정성 | 재구성 10회와 맵 재로드 1회 뒤에도 모드 오류와 크래시가 없고, 표 행 이름이 원래대로다 |
| 끄기 | 기능을 끄면 목록을 더 건드리지 않는다 |

## 9. 문서와 브랜치

- README: "용병" 탭 절 추가, 군사 탭 설명 수정, 주의 절에 "커스텀 용병단은 세이브에 남는다" 추가.
- `docs/CHANGELOG.md`: 기능 추가와 `zeroUpkeep` 동작 변경.
- `analysis/findings.md`: 7절 실측 결과.
- 브랜치: `docs/mercenary-spike`를 `develop`에 병합한 뒤 `feat/mercenary-companies`에서 작업한다.
