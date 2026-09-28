# MLToybox Plan 2 — Lua 기능 구현 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Lua로 가능한 치트 기능(① 자원 목표값 유지, ② 자재 불필요·즉시 수리, ③ 업그레이드 조건·비용·해금 무시, ④ 부대 수 상한·유지비·장비·훈련 요구 해제)을 구현하고, 인게임 실행기 `MLToyboxLab`을 정식 개발 도구로 만든다.

**Architecture:** Plan 1의 레지스트리 계약(`enable/disable/configure/tick(state, settings)`)을 따르는 기능 모듈 4개를 `mod/MLToybox/Scripts/features/`에 추가한다. 게임 오브젝트 접근은 `core/game.lua`, DataTable 접근은 `core/datatable.lua` 한 곳으로 모은다. 두 모듈의 조회 함수를 테스트에서 가짜로 바꿔 끼워 게임 없이 검증한다.

**Tech Stack:** UE4SS 3.0.1 Lua, xUnit + NLua(Lua spec 러너), .NET 8 WinForms, PowerShell 7

**Spec:** `docs/superpowers/specs/2026-09-28-mltoybox-mod-design.md` (§6, §11.7). 확정 API는 `analysis/findings.md` "스파이크 결과".

## Global Constraints

- 게임 오브젝트 클래스(스파이크 확정): 플레이어 `MyPawnCPP_BP3_C`, 지역 `BP_Region_C`(소유 판별 `r.ownerPawn:GetAddress() == pawn:GetAddress()`), 관리자 `MyRTSMultiEngineCPP_BP_C`, 치트 `MLCheatManager_C`, 금고 HUD `W_HUD_LordPanel_V2_C`의 `TreasuryNumeric.CurrentNumericValue`
- DataTable 경로:
  - `/Game/NotStronghold/Data/DT_Upgrades.DT_Upgrades`
  - `/Game/NotStronghold/Data/buildingStats.buildingStats`
  - `/Game/NotStronghold/Data/DT_UnitTemplates.DT_UnitTemplates`
  - `/Game/NotStronghold/Data/DT_MercenaryCompanies.DT_MercenaryCompanies`
  - 설정 CDO: `/Script/ManorLords.Default__ResidentialRequirementSettings`
- DataTable 행은 `dt:GetRowNames()` + `dt:FindRow(name)`(참조 반환)으로 접근하고, 배열 필드는 `:Empty()`로 비운다. 설정 배열은 통째로 비우지 않고 값만 0으로 만든다.
- `MLCheatManager_C:ChangeTreasury(n)`은 **증감**이다. 금고 보충 후 5초 동안은 다시 보충하지 않는다(HUD 반영 지연 대비).
- 자원 계열 보충은 **부족분만** 채운다. 목표보다 많은 값은 줄이지 않는다. 목표값은 **내 지역마다** 적용한다(금고·영향력은 전역 1개).
- 기능을 꺼도 이미 반영된 값(DataTable 패치, 지급된 자원)은 되돌리지 않는다(spec §6.2). DataTable 패치는 게임을 다시 켜면 원복된다.
- UE4SS 전역(`FindFirstOf` 등)은 `core/game.lua`·`core/datatable.lua` 안에서만 부른다. 기능 모듈은 이 두 모듈만 통해 게임에 접근한다.
- 파일 작성: Lua 파일에 백슬래시가 들어가므로 **Write 도구로만 작성**한다(Bash heredoc 금지, Plan 1 Task 11과 스파이크에서 백슬래시가 깨짐).
- git: 작업 브랜치 `feat/plan2-lua-features`(`develop`에서 분기), 태스크마다 커밋, 끝나면 `develop`으로 `--no-ff` 머지. 커밋 메시지 끝에 `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- 인게임 테스트 전 `tools\backup-saves.ps1` 필수, 테스트는 테스트 세이브에서.
- spec §11.7과 다른 점: 패널 상태의 `native` 항목 표시는 데이터를 만드는 네이티브 DLL이 생기는 Plan 3으로 옮긴다(지금 만들면 검증할 수 없는 빈 UI가 된다).

## Review Focus

1. **게임 오브젝트가 아직 없거나 사라진 시점의 tick**(로드 직후, 메뉴 복귀 직전): 기능이 예외를 내지 않고 조용히 건너뛰어야 한다. 반복 예외로 trip 되면 안 된다. → Task 2 `player_regions_empty_without_pawn`, Task 3 `tick_without_regions_is_noop`
2. **금고 HUD 값이 늦게 갱신됨**: 같은 부족분을 두 번 더하지 않아야 한다. → Task 3 `treasury_topup_has_cooldown`
3. **목표값보다 이미 많은 자원**: 줄이지 않아야 한다(사용자 재고 보호). → Task 3 `no_grant_when_stock_above_target`
4. **패널이 설정만 바꾸고 enabled 유지**(예: 군사에서 장비 무시만 끔): `configure`가 새 설정으로 다시 적용해야 한다. → Task 6 `configure_reapplies_with_new_settings`
5. **DataTable을 찾지 못함**(게임 업데이트로 경로 변경): 해당 기능만 오류로 비활성화되고 다른 기능은 계속 동작해야 한다. → Task 2 `forEachRow_missing_table_errors`(레지스트리가 enable 실패를 격리하는 것은 Plan 1에서 검증됨)

---

## 파일 구조

```
mod/MLToybox/Scripts/
├─ config.lua                          featureModules 등록 (Task 7)
├─ core/game.lua                       [신규] 게임 오브젝트 조회
├─ core/datatable.lua                  [신규] DataTable·CDO 조회와 행 순회
└─ features/
    ├─ resources_catalog.lua           [신규] 자원 ID ↔ EItemType
    ├─ resources.lua                   [신규] ①
    ├─ build.lua                       [신규] ② (Lua 부분)
    ├─ upgrade.lua                     [신규] ③
    └─ military.lua                    [신규] ④ (Lua 부분)
mod/MLToybox/tests/
├─ fakes.lua                           [신규] 가짜 UObject/TArray/TMap/DataTable
├─ game_spec.lua, datatable_spec.lua, resources_spec.lua, build_spec.lua, upgrade_spec.lua, military_spec.lua
└─ entrypoints_spec.lua                기능 모듈 require·Lab 컴파일 검사 추가
mod/MLToyboxLab/Scripts/main.lua       [신규] 인게임 Lua 실행기 (스파이크 버전 정식화)
tools/lab.ps1                          [신규] Lab 실행 래퍼
tools/deploy.ps1                       ValidateSet에 MLToyboxLab, lab 폴더 생성
panel/MLToybox.Panel.Core/ControlDocument.cs   BuildControl.NoMaterials
panel/MLToybox.Panel/MainForm.cs       "자재 불필요" 체크박스
```

---

### Task 1: MLToyboxLab 정식화

**Files:**
- Create: `mod/MLToyboxLab/Scripts/main.lua`(스크래치 `MLToyboxLab/Scripts/main.lua` 내용과 동일, 첫 줄 주석만 변경)
- Create: `tools/lab.ps1`
- Modify: `tools/deploy.ps1`(ValidateSet, lab 폴더 생성)
- Modify: `tools/tests/Tools.Tests.ps1`(Lab 배포 케이스)
- Modify: `mod/MLToybox/tests/entrypoints_spec.lua`(Lab 컴파일 검사)

**Interfaces:**
- Produces: `deploy.ps1 -Mod MLToyboxLab`는 `<Mods>\MLToyboxLab\Scripts`와 `<Mods>\MLToyboxLab\lab\`를 만든다. `lab.ps1 -File <lua> [-TimeoutSec 60]`은 `lab\run.lua`에 쓰고 `lab\out.txt` 내용을 출력한다(`<<END>>`로 끝남).

- [ ] **Step 1: 브랜치 생성**

```powershell
git -C E:\MLToybox switch -c feat/plan2-lua-features develop
```

- [ ] **Step 2: 실패하는 테스트 추가**

`Tools.Tests.ps1`의 `if ($script:failed -gt 0)` 줄 바로 위에 추가:

```powershell
Test-Case 'deploy MLToyboxLab creates lab folder and registers' {
    $g = New-FakeGame
    $mods = Join-Path $g 'ManorLords\Binaries\Win64\ue4ss\Mods'
    & "$PSScriptRoot\..\deploy.ps1" -Mod MLToyboxLab -GameDir $g | Out-Null
    Assert-True (Test-Path "$mods\MLToyboxLab\Scripts\main.lua") 'lab scripts'
    Assert-True (Test-Path "$mods\MLToyboxLab\lab") 'lab dir'
    Assert-Equal (@(Get-Content "$mods\mods.txt" | Where-Object { $_ -match '^\s*MLToyboxLab\s*:\s*1' }).Count) 1 'registered'
}
```

`entrypoints_spec.lua`의 `T.run({` 안에 추가:

```lua
  lab_main_compiles = function()
    local fn, err = loadfile(SCRIPTS_DIR .. "/../../MLToyboxLab/Scripts/main.lua")
    T.truthy(fn, "lab main.lua: " .. tostring(err))
  end,
```

- [ ] **Step 3: 실행해서 실패 확인**

Run: `pwsh -NoProfile -File E:\MLToybox\tools\tests\Tools.Tests.ps1` → Expected: 새 케이스 FAIL(ValidateSet 오류)
Run: `dotnet test E:\MLToybox\panel\MLToybox.sln` → Expected: `entrypoints_spec.lua` FAIL(`lab main.lua: cannot open`)

- [ ] **Step 4: 구현**

`mod/MLToyboxLab/Scripts/main.lua`: 스크래치 파일(`C:\Users\deepe\AppData\Local\Temp\claude\E--SteamLibrary-steamapps-common-Manor-Lords\52988742-7c3c-4d4b-bcc8-beb086dfdc3b\scratchpad\MLToyboxLab\Scripts\main.lua`)을 Read로 읽어 Write로 그대로 옮긴다. 첫 줄은 `-- MLToyboxLab: dev-only in-game Lua runner. Executes lab\run.lua on the game thread and writes lab\out.txt. Deploy only while developing.`로 바꾼다.

`tools/deploy.ps1` 변경:
- `ValidateSet('MLToybox', 'MLToyboxDump')` → `ValidateSet('MLToybox', 'MLToyboxDump', 'MLToyboxLab')`
- `if ($Mod -eq 'MLToybox') { New-Item ... 'bridge' ... }` 바로 아래에 추가:

```powershell
if ($Mod -eq 'MLToyboxLab') { New-Item -ItemType Directory -Force (Join-Path $target 'lab') | Out-Null }
```

`tools/lab.ps1`:

```powershell
param([Parameter(Mandatory)][string]$File, [string]$GameDir, [int]$TimeoutSec = 60)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\common.ps1"
$lab = Join-Path (Get-MLModsDir $GameDir) 'MLToyboxLab\lab'
if (-not (Test-Path $lab)) { throw 'MLToyboxLab not deployed. Run: deploy.ps1 -Mod MLToyboxLab (then restart the game)' }
$out = Join-Path $lab 'out.txt'
Remove-Item $out -ErrorAction SilentlyContinue
Copy-Item $File (Join-Path $lab 'run.lua')
$deadline = (Get-Date).AddSeconds($TimeoutSec)
while (-not (Test-Path $out)) {
    if ((Get-Date) -gt $deadline) { throw "Timed out; is the game running with MLToyboxLab loaded?" }
    Start-Sleep -Milliseconds 300
}
Get-Content $out
```

- [ ] **Step 5: 테스트 통과 확인** — 두 명령 모두 PASS

- [ ] **Step 6: 커밋**

```powershell
git -C E:\MLToybox add mod/MLToyboxLab tools mod/MLToybox/tests/entrypoints_spec.lua
git -C E:\MLToybox commit -m "feat(tools): promote MLToyboxLab in-game Lua runner to a dev tool`n`nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: 게임·DataTable 접근 모듈과 테스트용 가짜

**Files:**
- Create: `mod/MLToybox/Scripts/core/game.lua`, `mod/MLToybox/Scripts/core/datatable.lua`
- Create: `mod/MLToybox/tests/fakes.lua`, `mod/MLToybox/tests/game_spec.lua`, `mod/MLToybox/tests/datatable_spec.lua`

**Interfaces:**
- Produces:
  - `game.find = { first = fn(className) -> obj|nil, all = fn(className) -> {obj} }`: 테스트에서 교체
  - `game.pawn() -> obj|nil`, `game.cheat() -> obj|nil`, `game.engine() -> obj|nil`, `game.playerRegions() -> {region}`, `game.treasury() -> number|nil`, `game.unwrap(x) -> value`(`x:get()`이 있으면 그 결과)
  - `datatable.find = fn(path) -> obj|nil`: 테스트에서 교체
  - `datatable.PATHS = { upgrades, buildingStats, unitTemplates, mercenaries, residentialSettings }`
  - `datatable.forEachRow(key, fn(name, row)) -> count`: 표가 없으면 `error("datatable not found: <key>")`
  - `datatable.object(key) -> obj`: 없으면 error
  - `fakes.object(fields) -> obj`(IsValid·GetAddress 포함), `fakes.array(items) -> arr`(`:Empty()` 포함), `fakes.map(pairs) -> map`(`:ForEach(fn(kWrap, vWrap))`, 요소는 `:get()`), `fakes.datatable(rowsByName) -> dt`(`GetRowNames`, `FindRow`, `IsValid`)

- [ ] **Step 1: 테스트용 가짜 작성** — `mod/MLToybox/tests/fakes.lua`

```lua
local F = {}
local nextAddr = 1000

function F.object(fields)
  nextAddr = nextAddr + 1
  local o = fields or {}
  local addr = nextAddr
  o.IsValid = o.IsValid or function() return true end
  o.GetAddress = function() return addr end
  return o
end

function F.invalid()
  return { IsValid = function() return false end, GetAddress = function() return 0 end }
end

function F.array(items)
  local a = {}
  for i, v in ipairs(items or {}) do a[i] = v end
  a.Empty = function(self) for i = #self, 1, -1 do self[i] = nil end end
  return a
end

function F.wrap(v) return { get = function() return v end } end

function F.map(entries)
  return { ForEach = function(self, fn) for _, e in ipairs(entries) do fn(F.wrap(e[1]), F.wrap(e[2])) end end }
end

function F.datatable(rows)
  local names = {}
  for name in pairs(rows) do names[#names + 1] = name end
  table.sort(names)
  return {
    IsValid = function() return true end,
    GetRowNames = function() return names end,
    FindRow = function(_, name) return rows[name] end,
  }
end

return F
```

- [ ] **Step 2: 실패하는 테스트 작성**

`mod/MLToybox/tests/game_spec.lua`

```lua
local T = require("t")
local F = require("fakes")
local game = require("core.game")

local function install(byClass)
  game.find.first = function(c) local l = byClass[c]; return l and l[1] end
  game.find.all = function(c) return byClass[c] or {} end
end

T.run({
  player_regions_filters_by_owner = function()
    local pawn, other = F.object(), F.object()
    local mine, theirs, ownerless = F.object({ ownerPawn = pawn }), F.object({ ownerPawn = other }), F.object({ ownerPawn = F.invalid() })
    install({ MyPawnCPP_BP3_C = { pawn }, BP_Region_C = { theirs, mine, ownerless } })
    local rs = game.playerRegions()
    T.eq(#rs, 1, "one region"); T.eq(rs[1], mine, "mine")
  end,
  player_regions_empty_without_pawn = function()
    install({ BP_Region_C = { F.object({ ownerPawn = F.object() }) } })
    T.eq(#game.playerRegions(), 0, "no pawn -> none")
  end,
  invalid_objects_are_nil = function()
    install({ MyPawnCPP_BP3_C = { F.invalid() }, MLCheatManager_C = {}, MyRTSMultiEngineCPP_BP_C = { F.object() } })
    T.eq(game.pawn(), nil, "invalid pawn"); T.eq(game.cheat(), nil, "no cheat"); T.truthy(game.engine(), "engine")
  end,
  treasury_reads_first_valid_hud = function()
    local dead = F.object({ TreasuryNumeric = F.invalid() })
    local live = F.object({ TreasuryNumeric = F.object({ CurrentNumericValue = 66116.0 }) })
    install({ W_HUD_LordPanel_V2_C = { dead, live } })
    T.eq(game.treasury(), 66116.0, "treasury")
    install({})
    T.eq(game.treasury(), nil, "no hud")
  end,
  unwrap_handles_wrapped_and_plain = function()
    T.eq(game.unwrap(F.wrap(5)), 5, "wrapped"); T.eq(game.unwrap(7), 7, "plain")
  end,
})
```

`mod/MLToybox/tests/datatable_spec.lua`

```lua
local T = require("t")
local F = require("fakes")
local datatable = require("core.datatable")

T.run({
  forEachRow_visits_rows_by_reference = function()
    local rows = { ["2"] = { v = 1 }, ["4"] = { v = 2 } }
    datatable.find = function(p) if p == datatable.PATHS.upgrades then return F.datatable(rows) end end
    local n = datatable.forEachRow("upgrades", function(name, row) row.v = row.v * 10 end)
    T.eq(n, 2, "count"); T.eq(rows["2"].v, 10, "row 2 mutated"); T.eq(rows["4"].v, 20, "row 4 mutated")
  end,
  forEachRow_missing_table_errors = function()
    datatable.find = function() return nil end
    local ok, err = pcall(datatable.forEachRow, "upgrades", function() end)
    T.eq(ok, false, "errors"); T.truthy(tostring(err):find("datatable not found: upgrades", 1, true), "message")
  end,
  row_names_accept_fname_userdata = function()
    local rows = { a = { v = 1 } }
    local dt = F.datatable(rows)
    dt.GetRowNames = function() return { { ToString = function() return "a" end } } end
    datatable.find = function() return dt end
    local seen
    datatable.forEachRow("upgrades", function(name) seen = name end)
    T.eq(seen, "a", "ToString used")
  end,
  object_errors_when_missing = function()
    datatable.find = function() return nil end
    T.eq((pcall(datatable.object, "residentialSettings")), false, "errors")
  end,
})
```

- [ ] **Step 3: 실행해서 실패 확인**

Run: `dotnet test E:\MLToybox\panel\MLToybox.sln`
Expected: `game_spec.lua`, `datatable_spec.lua` FAIL(`module 'core.game' not found` / `'core.datatable' not found`)

- [ ] **Step 4: 구현**

`mod/MLToybox/Scripts/core/game.lua`

```lua
local safe = require("core.safe")

local M = {}

M.find = {
  first = function(className) return FindFirstOf(className) end,
  all = function(className) return FindAllOf(className) or {} end,
}

local function firstValid(className)
  local o = M.find.first(className)
  if safe.valid(o) then return o end
  return nil
end

function M.pawn() return firstValid("MyPawnCPP_BP3_C") end
function M.cheat() return firstValid("MLCheatManager_C") end
function M.engine() return firstValid("MyRTSMultiEngineCPP_BP_C") end

function M.playerRegions()
  local pawn = M.pawn()
  if not pawn then return {} end
  local addr = pawn:GetAddress()
  local out = {}
  for _, r in ipairs(M.find.all("BP_Region_C")) do
    if safe.valid(r) then
      local owner = r.ownerPawn
      if safe.valid(owner) and owner:GetAddress() == addr then out[#out + 1] = r end
    end
  end
  return out
end

-- 금고 필드는 리플렉션에 없어 HUD 숫자 위젯에서 읽는다 (findings: 스파이크 결과 ①)
function M.treasury()
  for _, w in ipairs(M.find.all("W_HUD_LordPanel_V2_C")) do
    if safe.valid(w) then
      local n = w.TreasuryNumeric
      if safe.valid(n) then return n.CurrentNumericValue end
    end
  end
  return nil
end

function M.unwrap(x)
  local ok, v = pcall(function() return x:get() end)
  if ok then return v end
  return x
end

return M
```

`mod/MLToybox/Scripts/core/datatable.lua`

```lua
local safe = require("core.safe")

local M = {}

M.PATHS = {
  upgrades = "/Game/NotStronghold/Data/DT_Upgrades.DT_Upgrades",
  buildingStats = "/Game/NotStronghold/Data/buildingStats.buildingStats",
  unitTemplates = "/Game/NotStronghold/Data/DT_UnitTemplates.DT_UnitTemplates",
  mercenaries = "/Game/NotStronghold/Data/DT_MercenaryCompanies.DT_MercenaryCompanies",
  residentialSettings = "/Script/ManorLords.Default__ResidentialRequirementSettings",
}

M.find = function(path) return StaticFindObject(path) end

function M.object(key)
  local o = M.find(M.PATHS[key])
  if not safe.valid(o) then error("datatable not found: " .. key, 2) end
  return o
end

local function nameOf(n)
  if type(n) == "string" then return n end
  local ok, s = pcall(function() return n:ToString() end)
  if ok then return s end
  return tostring(n)
end

-- FindRow 는 참조를 돌려주므로 fn 안에서의 필드 쓰기가 표에 남는다 (findings: 스파이크 결과)
function M.forEachRow(key, fn)
  local dt = M.object(key)
  local n = 0
  for _, raw in ipairs(dt:GetRowNames()) do
    local name = nameOf(raw)
    local row = dt:FindRow(name)
    if row then
      n = n + 1
      fn(name, row)
    end
  end
  return n
end

return M
```

- [ ] **Step 5: 테스트 통과 확인** — `dotnet test E:\MLToybox\panel\MLToybox.sln` 전부 PASS

- [ ] **Step 6: 커밋**

```powershell
git -C E:\MLToybox add mod/MLToybox
git -C E:\MLToybox commit -m "feat(lua): add game object and datatable access modules with test fakes`n`nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: ① 자원 목표값 유지

**Files:**
- Create: `mod/MLToybox/Scripts/features/resources_catalog.lua`, `mod/MLToybox/Scripts/features/resources.lua`
- Create: `mod/MLToybox/tests/resources_spec.lua`

**Interfaces:**
- Consumes: `core.game`(pawn, cheat, playerRegions, treasury)
- Produces:
  - `catalog.items = { { id = "Timber", type = 16 }, ... }`, `catalog.special = { "RegionalWealth", "Treasury", "Influence" }`, `catalog.ids() -> {string}`(special 먼저, 그 뒤 items 순서)
  - 기능 `resources`: `intervalSec = 2`, `enable(state)`는 `state.resourceIds`를 채운다. `tick(state, settings)`은 `settings.targets[id]`의 부족분을 보충하고 `state.resources[id]`(내 지역 합계)를 갱신한다. `resources.clock`은 테스트 교체용(기본 `os.time`). `resources.TREASURY_COOLDOWN = 5`.

- [ ] **Step 1: 실패하는 테스트 작성** — `mod/MLToybox/tests/resources_spec.lua`

```lua
local T = require("t")
local F = require("fakes")
local game = require("core.game")
local resources = require("features.resources")
local catalog = require("features.resources_catalog")

local function region(stock, wealth)
  local r = F.object({ stock = stock, regionalWealth = wealth or 0, grants = {} })
  r.getStockOfGood = function(self, t) return self.stock[t] or 0 end
  r.grantResources = function(self, goods, _)
    for _, g in ipairs(goods) do
      self.grants[#self.grants + 1] = { g.Type, g.amt }
      self.stock[g.Type] = (self.stock[g.Type] or 0) + g.amt
    end
  end
  return r
end

local function world(opts)
  local cheat = { changes = {} }
  cheat.ChangeTreasury = function(_, d) cheat.changes[#cheat.changes + 1] = d end
  game.playerRegions = function() return opts.regions or {} end
  game.pawn = function() return opts.pawn end
  game.cheat = function() return cheat end
  game.treasury = function() return opts.treasury end
  return cheat
end

T.run({
  enable_publishes_ids_special_first = function()
    local st = {}
    resources.enable(st, {})
    T.eq(st.resourceIds[1], "RegionalWealth", "special first")
    T.eq(#st.resourceIds, #catalog.special + #catalog.items, "all ids")
  end,
  tops_up_shortfall_per_region = function()
    local r1, r2 = region({ [16] = 100 }), region({ [16] = 450 })
    world({ regions = { r1, r2 } })
    local st = {}
    resources.tick(st, { targets = { Timber = 500 } })
    T.eq(r1.stock[16], 500, "r1 topped"); T.eq(r2.stock[16], 500, "r2 topped")
    T.eq(st.resources.Timber, 1000, "sum reported")
  end,
  no_grant_when_stock_above_target = function()
    local r = region({ [16] = 900 })
    world({ regions = { r } })
    resources.tick({}, { targets = { Timber = 500 } })
    T.eq(#r.grants, 0, "no grant"); T.eq(r.stock[16], 900, "unchanged")
  end,
  untargeted_items_only_reported = function()
    local r = region({ [17] = 12 })
    world({ regions = { r } })
    local st = {}
    resources.tick(st, { targets = {} })
    T.eq(#r.grants, 0, "no grants"); T.eq(st.resources.planks, 12, "reported")
  end,
  wealth_and_influence_raised_to_target = function()
    local r = region({}, 10)
    local pawn = F.object({ influence = 3 })
    world({ regions = { r }, pawn = pawn })
    local st = {}
    resources.tick(st, { targets = { RegionalWealth = 100, Influence = 50 } })
    T.eq(r.regionalWealth, 100, "wealth"); T.eq(pawn.influence, 50, "influence")
    T.eq(st.resources.RegionalWealth, 100, "wealth reported"); T.eq(st.resources.Influence, 50, "influence reported")
  end,
  treasury_topup_uses_delta = function()
    resources.clock = function() return 1000 end
    local cheat = world({ regions = {}, treasury = 300.0 })
    local st = {}
    resources.tick(st, { targets = { Treasury = 1000 } })
    T.eq(cheat.changes[1], 700, "delta"); T.eq(st.resources.Treasury, 1000, "reported as topped")
  end,
  treasury_topup_has_cooldown = function()
    local now = 2000
    resources.clock = function() return now end
    local cheat = world({ regions = {}, treasury = 300.0 })
    local st = {}
    resources.tick(st, { targets = { Treasury = 1000 } })
    now = 2002
    resources.tick(st, { targets = { Treasury = 1000 } })   -- HUD 가 아직 300 을 보여줘도 다시 더하지 않는다
    T.eq(#cheat.changes, 1, "once within cooldown")
    now = 2006
    resources.tick(st, { targets = { Treasury = 1000 } })
    T.eq(#cheat.changes, 2, "again after cooldown")
  end,
  tick_without_regions_is_noop = function()
    world({ regions = {} })
    local st = {}
    resources.tick(st, { targets = { Timber = 500, RegionalWealth = 10, Influence = 1, Treasury = 5 } })
    T.eq(st.resources.Timber, 0, "zero")
    T.eq(st.resources.Treasury, nil, "no hud -> not reported")
  end,
})
```

- [ ] **Step 2: 실행해서 실패 확인** — Expected: `resources_spec.lua` FAIL(`module 'features.resources' not found`)

- [ ] **Step 3: 구현**

`mod/MLToybox/Scripts/features/resources_catalog.lua`

```lua
-- 자원 ID(EItemType 이름) ↔ 값. 출처: CXXHeaderDump/ManorLords_enums.hpp enum class EItemType
local M = {}

M.special = { "RegionalWealth", "Treasury", "Influence" }

M.items = {
  { id = "Timber", type = 16 }, { id = "planks", type = 17 }, { id = "Firewood", type = 216 }, { id = "Charcoal", type = 13 },
  { id = "RoughStone", type = 27 }, { id = "DressedStone", type = 283 }, { id = "Clay", type = 146 }, { id = "clayTILES", type = 269 },
  { id = "IronOre", type = 14 }, { id = "IronSlabs", type = 35 }, { id = "Salt", type = 145 },
  { id = "WheatGrain", type = 1 }, { id = "WheatFlour", type = 5 }, { id = "WheatBread", type = 172 },
  { id = "RyeGrain", type = 299 }, { id = "RyeFlour", type = 33 }, { id = "RyeBread", type = 170 },
  { id = "Berries", type = 171 }, { id = "mushrooms", type = 279 }, { id = "meat", type = 147 }, { id = "fish", type = 30 },
  { id = "Eggs", type = 220 }, { id = "vegetables", type = 230 }, { id = "apples", type = 296 }, { id = "Honey", type = 141 },
  { id = "Pastries", type = 173 }, { id = "Barley", type = 165 }, { id = "Malt", type = 29 }, { id = "Ale", type = 28 },
  { id = "Hops", type = 166 }, { id = "Beer", type = 167 },
  { id = "Hides", type = 3 }, { id = "Leather", type = 4 }, { id = "Pelts", type = 9 }, { id = "Shoes", type = 10 },
  { id = "Flax", type = 11 }, { id = "Cloth_Linen", type = 12 }, { id = "Wool", type = 23 }, { id = "Yarn", type = 148 },
  { id = "Clothes", type = 149 }, { id = "dyes", type = 302 }, { id = "Wax", type = 142 }, { id = "Candle", type = 153 },
  { id = "Irontools", type = 6 },
  { id = "spears", type = 133 }, { id = "weapons_sidearms", type = 177 }, { id = "weapons_polearms", type = 178 },
  { id = "warbows", type = 205 }, { id = "crossbows", type = 206 }, { id = "shields_small", type = 270 }, { id = "shields_large", type = 271 },
  { id = "militia_helmets_resource", type = 273 }, { id = "gambesons", type = 163 }, { id = "mail_armor", type = 164 }, { id = "PlateArmor", type = 293 },
}

function M.ids()
  local out = {}
  for _, s in ipairs(M.special) do out[#out + 1] = s end
  for _, it in ipairs(M.items) do out[#out + 1] = it.id end
  return out
end

return M
```

`mod/MLToybox/Scripts/features/resources.lua`

```lua
local game = require("core.game")
local catalog = require("features.resources_catalog")

local M = { name = "resources", intervalSec = 2, TREASURY_COOLDOWN = 5 }
M.clock = os.time

local function shortfall(current, target)
  if type(target) ~= "number" then return 0 end
  if current < target then return target - current end
  return 0
end

function M.enable(state)
  state.resourceIds = catalog.ids()
  state.resources = {}
  state.treasuryCooldownUntil = nil
end

function M.tick(state, settings)
  local targets = (settings and settings.targets) or {}
  local regions = game.playerRegions()
  local cur = {}

  for _, it in ipairs(catalog.items) do
    local total = 0
    for _, r in ipairs(regions) do
      local stock = r:getStockOfGood(it.type, false, false)
      local need = shortfall(stock, targets[it.id])
      if need > 0 then
        r:grantResources({ { Type = it.type, amt = need } }, false)
        stock = stock + need
      end
      total = total + stock
    end
    cur[it.id] = total
  end

  local wealth = 0
  for _, r in ipairs(regions) do
    if shortfall(r.regionalWealth, targets.RegionalWealth) > 0 then r.regionalWealth = targets.RegionalWealth end
    wealth = wealth + r.regionalWealth
  end
  cur.RegionalWealth = wealth

  local pawn = game.pawn()
  if pawn then
    if shortfall(pawn.influence, targets.Influence) > 0 then pawn.influence = targets.Influence end
    cur.Influence = pawn.influence
  end

  -- ChangeTreasury 는 증감이고 HUD 반영이 늦으므로 보충 후 쿨다운 동안 다시 더하지 않는다
  local treasury = game.treasury()
  if treasury then
    local now = M.clock()
    local need = shortfall(treasury, targets.Treasury)
    local cooling = state.treasuryCooldownUntil and now < state.treasuryCooldownUntil
    if need > 0 and not cooling then
      local cheat = game.cheat()
      if cheat then
        cheat:ChangeTreasury(math.floor(need))
        state.treasuryCooldownUntil = now + M.TREASURY_COOLDOWN
        treasury = treasury + need
      end
    end
    cur.Treasury = treasury
  end

  state.resources = cur
end

return M
```

- [ ] **Step 4: 테스트 통과 확인** — 전부 PASS

- [ ] **Step 5: 커밋**

```powershell
git -C E:\MLToybox add mod/MLToybox
git -C E:\MLToybox commit -m "feat(lua): add resource target keeping feature`n`nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: ③ 업그레이드 조건·비용·해금 무시

**Files:**
- Create: `mod/MLToybox/Scripts/features/upgrade.lua`, `mod/MLToybox/tests/upgrade_spec.lua`

**Interfaces:**
- Consumes: `core.datatable`(forEachRow "upgrades", object "residentialSettings")
- Produces: 기능 `upgrade`: `enable(state)`는 모든 업그레이드 행과 주거 요구 조건을 패치하고 `state.upgradePatch = { rows, requirements }`를 기록한다. `configure`는 `enable`과 같다. `upgrade.patchUpgradeRow(row)`, `upgrade.patchResidential(settings) -> 바꾼 개수`

- [ ] **Step 1: 실패하는 테스트 작성** — `mod/MLToybox/tests/upgrade_spec.lua`

```lua
local T = require("t")
local F = require("fakes")
local datatable = require("core.datatable")
local upgrade = require("features.upgrade")

local function upRow()
  return {
    cost = F.array({ { Type = 16, amt = 2 } }), requiresBuilding = F.array({ 5 }), requiredPerks = F.array({ "x" }),
    regionalWealth = 25, treasury = 10, minimumSettlementLevel = 2, minimumProsperity = 1, minimumHouseLv = 2, lockedInOutposts = true,
  }
end

local function residential()
  local function req(v) return { VarietyRequired = v } end
  return { UpgradeRequirementsPerLevel = F.array({
    { Requirements = F.array({}) },
    { Requirements = F.array({ req(1), req(0), req(2) }) },
  }) }
end

T.run({
  patch_upgrade_row_clears_everything = function()
    local r = upRow()
    upgrade.patchUpgradeRow(r)
    T.eq(#r.cost, 0, "cost"); T.eq(#r.requiresBuilding, 0, "requiresBuilding"); T.eq(#r.requiredPerks, 0, "perks")
    T.eq(r.regionalWealth, 0, "wealth"); T.eq(r.treasury, 0, "treasury"); T.eq(r.minimumSettlementLevel, 0, "settle")
    T.eq(r.minimumProsperity, 0, "prosperity"); T.eq(r.minimumHouseLv, 0, "house"); T.eq(r.lockedInOutposts, false, "outposts")
  end,
  patch_residential_zeroes_values_keeps_arrays = function()
    local s = residential()
    T.eq(upgrade.patchResidential(s), 2, "two changed")
    local reqs = s.UpgradeRequirementsPerLevel[2].Requirements
    T.eq(#reqs, 3, "array kept"); T.eq(reqs[1].VarietyRequired, 0, "zeroed"); T.eq(reqs[3].VarietyRequired, 0, "zeroed")
  end,
  enable_patches_all_rows_and_settings = function()
    local rows = { ["2"] = upRow(), ["4"] = upRow() }
    local s = residential()
    datatable.find = function(p)
      if p == datatable.PATHS.upgrades then return F.datatable(rows) end
      if p == datatable.PATHS.residentialSettings then return F.object(s) end
    end
    local st = {}
    upgrade.enable(st, { enabled = true })
    T.eq(rows["4"].regionalWealth, 0, "row patched"); T.eq(st.upgradePatch.rows, 2, "rows"); T.eq(st.upgradePatch.requirements, 2, "reqs")
  end,
  enable_fails_when_table_missing = function()
    datatable.find = function() return nil end
    T.eq((pcall(upgrade.enable, {}, { enabled = true })), false, "error propagates to registry")
  end,
})
```

- [ ] **Step 2: 실행해서 실패 확인** — Expected: `module 'features.upgrade' not found`

- [ ] **Step 3: 구현** — `mod/MLToybox/Scripts/features/upgrade.lua`

```lua
local datatable = require("core.datatable")

local M = { name = "upgrade" }

local function clear(arr) if arr and arr.Empty then arr:Empty() end end

function M.patchUpgradeRow(row)
  clear(row.cost)
  clear(row.requiresBuilding)
  clear(row.requiredPerks)
  row.regionalWealth = 0
  row.treasury = 0
  row.minimumSettlementLevel = 0
  row.minimumProsperity = 0
  row.minimumHouseLv = 0
  row.lockedInOutposts = false
end

-- 배열을 비우면 네이티브가 레벨 인덱스로 접근할 때 위험하므로 값만 0으로 만든다
function M.patchResidential(settings)
  local levels = settings.UpgradeRequirementsPerLevel
  local changed = 0
  for li = 1, #levels do
    local reqs = levels[li].Requirements
    for ri = 1, #reqs do
      if reqs[ri].VarietyRequired ~= 0 then
        reqs[ri].VarietyRequired = 0
        changed = changed + 1
      end
    end
  end
  return changed
end

function M.enable(state)
  local rows = datatable.forEachRow("upgrades", function(_, row) M.patchUpgradeRow(row) end)
  local requirements = M.patchResidential(datatable.object("residentialSettings"))
  state.upgradePatch = { rows = rows, requirements = requirements }
end

M.configure = M.enable

return M
```

- [ ] **Step 4: 테스트 통과 확인** — 전부 PASS

- [ ] **Step 5: 커밋** — `git add mod/MLToybox`, 메시지 `feat(lua): add upgrade requirement/cost/unlock bypass feature`

---

### Task 5: ② 자재 불필요·즉시 수리 (Lua 부분)

**Files:**
- Create: `mod/MLToybox/Scripts/features/build.lua`, `mod/MLToybox/tests/build_spec.lua`

**Interfaces:**
- Consumes: `core.datatable`(forEachRow "buildingStats"), `core.game`(cheat, playerRegions, unwrap)
- Produces: 기능 `build`: `intervalSec = 10`.
  - `enable`/`configure`: `settings.noMaterials`이면 모든 `buildingStats` 행의 `constructionGoods`를 비우고, 내 지역의 미완공 건물 `constructionGoods`도 비운다.
  - `tick`: `settings.instantRepair`이면 `cheat:MaintainAllBuildings()`를 호출한다.
  - `ignorePlacement`, `instantBuild`는 Plan 3 네이티브가 해석하므로 Lua는 무시한다.

- [ ] **Step 1: 실패하는 테스트 작성** — `mod/MLToybox/tests/build_spec.lua`

```lua
local T = require("t")
local F = require("fakes")
local datatable = require("core.datatable")
local game = require("core.game")
local build = require("features.build")

local function setup()
  local rows = { ["3"] = { constructionGoods = F.array({ { Type = 16, amt = 4 } }) }, ["72"] = { constructionGoods = F.array({ { Type = 17, amt = 2 } }) } }
  datatable.find = function(p) if p == datatable.PATHS.buildingStats then return F.datatable(rows) end end
  local unbuilt = F.object({ constructionGoods = F.array({ { Type = 16, amt = 1 } }) })
  unbuilt.IsConstructed = function() return false end
  local built = F.object({ constructionGoods = F.array({ { Type = 16, amt = 1 } }) })
  built.IsConstructed = function() return true end
  local region = F.object({})
  region.GetBuildings = function() return { F.wrap(unbuilt), F.wrap(built) } end
  local cheat = { maintained = 0 }
  cheat.MaintainAllBuildings = function(self) self.maintained = self.maintained + 1 end
  game.playerRegions = function() return { region } end
  game.cheat = function() return cheat end
  return rows, unbuilt, built, cheat
end

T.run({
  no_materials_clears_stats_and_unbuilt_only = function()
    local rows, unbuilt, built = setup()
    build.enable({}, { enabled = true, noMaterials = true })
    T.eq(#rows["3"].constructionGoods, 0, "stats row"); T.eq(#rows["72"].constructionGoods, 0, "stats row 2")
    T.eq(#unbuilt.constructionGoods, 0, "unbuilt cleared"); T.eq(#built.constructionGoods, 1, "built untouched")
  end,
  no_materials_off_leaves_data = function()
    local rows = setup()
    build.enable({}, { enabled = true, noMaterials = false })
    T.eq(#rows["3"].constructionGoods, 1, "untouched")
  end,
  instant_repair_maintains_on_tick = function()
    local _, _, _, cheat = setup()
    build.tick({}, { enabled = true, instantRepair = true })
    T.eq(cheat.maintained, 1, "maintained")
    build.tick({}, { enabled = true, instantRepair = false })
    T.eq(cheat.maintained, 1, "not when off")
  end,
  tick_without_cheat_is_noop = function()
    setup()
    game.cheat = function() return nil end
    build.tick({}, { enabled = true, instantRepair = true })
  end,
})
```

- [ ] **Step 2: 실행해서 실패 확인** — Expected: `module 'features.build' not found`

- [ ] **Step 3: 구현** — `mod/MLToybox/Scripts/features/build.lua`

```lua
local datatable = require("core.datatable")
local game = require("core.game")

-- ignorePlacement / instantBuild 는 네이티브 계층(spec §11)이 해석한다
local M = { name = "build", intervalSec = 10 }

local function clear(arr) if arr and arr.Empty then arr:Empty() end end

function M.apply(_, settings)
  if not settings.noMaterials then return end
  datatable.forEachRow("buildingStats", function(_, row) clear(row.constructionGoods) end)
  for _, region in ipairs(game.playerRegions()) do
    for _, w in ipairs(region:GetBuildings()) do
      local b = game.unwrap(w)
      if b:IsValid() and not b:IsConstructed() then clear(b.constructionGoods) end
    end
  end
end

M.enable = M.apply
M.configure = M.apply

function M.tick(_, settings)
  if not settings.instantRepair then return end
  local cheat = game.cheat()
  if cheat then cheat:MaintainAllBuildings() end
end

return M
```

- [ ] **Step 4: 테스트 통과 확인** — 전부 PASS

- [ ] **Step 5: 커밋** — 메시지 `feat(lua): add no-materials and instant repair build feature`

---

### Task 6: ④ 부대 수 상한·유지비·장비·훈련 요구 (Lua 부분)

**Files:**
- Create: `mod/MLToybox/Scripts/features/military.lua`, `mod/MLToybox/tests/military_spec.lua`

**Interfaces:**
- Consumes: `core.datatable`(unitTemplates, mercenaries), `core.game`(pawn, engine, unwrap)
- Produces: 기능 `military`: `intervalSec = 5`, `MAX_SQUADS = 99`.
  - `enable`/`configure`: 설정에 따라 데이터를 패치한다.
    - `ignoreEquipment` → 유닛 템플릿 `requiredEquipment`를 비운다.
    - `ignorePopulation` → `minHouseLv`/`minMeleeTraining`/`minArcheryTraining` = 0
    - `zeroUpkeep` → 용병 표 `cost` = 0
  - `tick`: 게임 인스턴스 값을 쓴다.
    - `unlimitedSquads` → `pawn.maxNumOfMilitiaToSpawn` ≥ 99
    - `zeroUpkeep` → `pawn.recruitCost` = 0, 고용 용병 `cost` = 0
  - 주민 수를 넘는 징집은 Plan 3 네이티브가 담당한다.

- [ ] **Step 1: 실패하는 테스트 작성** — `mod/MLToybox/tests/military_spec.lua`

```lua
local T = require("t")
local F = require("fakes")
local datatable = require("core.datatable")
local game = require("core.game")
local military = require("features.military")

local function setup()
  local units = {
    militiaFoot = { requiredEquipment = F.array({ { Type = 133, amt = 1 } }), minHouseLv = 1, minMeleeTraining = 0.5, minArcheryTraining = 0.2 },
    retinue_tier1 = { requiredEquipment = F.array({}), minHouseLv = 1, minMeleeTraining = 2.0, minArcheryTraining = 0 },
  }
  local mercs = { crazy_goose = { cost = 90 } }
  datatable.find = function(p)
    if p == datatable.PATHS.unitTemplates then return F.datatable(units) end
    if p == datatable.PATHS.mercenaries then return F.datatable(mercs) end
  end
  local hired = { { 0, { cost = 50 } }, { 1, { cost = 60 } } }
  local pawn = F.object({ maxNumOfMilitiaToSpawn = 6, recruitCost = 25.0 })
  local engine = F.object({ hiredMercs = F.map(hired) })
  game.pawn = function() return pawn end
  game.engine = function() return engine end
  return units, mercs, hired, pawn
end

local ALL = { enabled = true, ignoreEquipment = true, ignorePopulation = true, zeroUpkeep = true, unlimitedSquads = true }

T.run({
  enable_patches_templates_and_merc_table = function()
    local units, mercs = setup()
    military.enable({}, ALL)
    T.eq(#units.militiaFoot.requiredEquipment, 0, "equipment"); T.eq(units.militiaFoot.minHouseLv, 0, "house")
    T.eq(units.retinue_tier1.minMeleeTraining, 0, "melee"); T.eq(units.militiaFoot.minArcheryTraining, 0, "archery")
    T.eq(mercs.crazy_goose.cost, 0, "merc table cost")
  end,
  configure_reapplies_with_new_settings = function()
    local units, mercs = setup()
    military.configure({}, { enabled = true, ignoreEquipment = true, ignorePopulation = false, zeroUpkeep = false })
    T.eq(#units.militiaFoot.requiredEquipment, 0, "equipment cleared"); T.eq(units.militiaFoot.minHouseLv, 1, "house untouched")
    T.eq(mercs.crazy_goose.cost, 90, "merc untouched")
  end,
  tick_sets_squad_cap_and_zero_upkeep = function()
    local _, _, hired, pawn = setup()
    military.tick({}, ALL)
    T.eq(pawn.maxNumOfMilitiaToSpawn, 99, "cap"); T.eq(pawn.recruitCost, 0, "recruit cost")
    T.eq(hired[1][2].cost, 0, "hired 0"); T.eq(hired[2][2].cost, 0, "hired 1")
  end,
  tick_keeps_higher_cap = function()
    local _, _, _, pawn = setup()
    pawn.maxNumOfMilitiaToSpawn = 150
    military.tick({}, ALL)
    T.eq(pawn.maxNumOfMilitiaToSpawn, 150, "not lowered")
  end,
  tick_respects_flags_off = function()
    local _, _, hired, pawn = setup()
    military.tick({}, { enabled = true })
    T.eq(pawn.maxNumOfMilitiaToSpawn, 6, "cap untouched"); T.eq(hired[1][2].cost, 50, "upkeep untouched")
  end,
  tick_without_objects_is_noop = function()
    setup()
    game.pawn = function() return nil end
    game.engine = function() return nil end
    military.tick({}, ALL)
  end,
})
```

- [ ] **Step 2: 실행해서 실패 확인** — Expected: `module 'features.military' not found`

- [ ] **Step 3: 구현** — `mod/MLToybox/Scripts/features/military.lua`

```lua
local datatable = require("core.datatable")
local game = require("core.game")

-- 주민 수를 넘는 징집은 네이티브 계층(spec §11)이 담당한다
local M = { name = "military", intervalSec = 5, MAX_SQUADS = 99 }

local function clear(arr) if arr and arr.Empty then arr:Empty() end end

function M.apply(_, settings)
  datatable.forEachRow("unitTemplates", function(_, row)
    if settings.ignoreEquipment then clear(row.requiredEquipment) end
    if settings.ignorePopulation then
      row.minHouseLv = 0
      row.minMeleeTraining = 0
      row.minArcheryTraining = 0
    end
  end)
  if settings.zeroUpkeep then
    datatable.forEachRow("mercenaries", function(_, row) row.cost = 0 end)
  end
end

M.enable = M.apply
M.configure = M.apply

function M.tick(_, settings)
  local pawn = game.pawn()
  if pawn then
    if settings.unlimitedSquads and pawn.maxNumOfMilitiaToSpawn < M.MAX_SQUADS then pawn.maxNumOfMilitiaToSpawn = M.MAX_SQUADS end
    if settings.zeroUpkeep and pawn.recruitCost ~= 0 then pawn.recruitCost = 0 end
  end
  if settings.zeroUpkeep then
    local engine = game.engine()
    if engine then
      engine.hiredMercs:ForEach(function(_, v)
        local company = game.unwrap(v)
        if company.cost ~= 0 then company.cost = 0 end
      end)
    end
  end
end

return M
```

- [ ] **Step 4: 테스트 통과 확인** — 전부 PASS

- [ ] **Step 5: 커밋** — 메시지 `feat(lua): add military squad cap, upkeep and requirement bypass feature`

---

### Task 7: 기능 등록 + 패널 "자재 불필요"

**Files:**
- Modify: `mod/MLToybox/Scripts/config.lua`(`featureModules`)
- Modify: `mod/MLToybox/tests/entrypoints_spec.lua`(기능 모듈 require 검사)
- Modify: `panel/MLToybox.Panel.Core/ControlDocument.cs`(`BuildControl.NoMaterials`)
- Modify: `panel/MLToybox.Tests/BridgeClientTests.cs`(noMaterials 직렬화)
- Modify: `panel/MLToybox.Panel/MainForm.cs`(체크박스)

**Interfaces:**
- Produces: `config.featureModules = { "resources", "build", "upgrade", "military" }`, JSON `features.build.noMaterials`(기본 true)

- [ ] **Step 1: 실패하는 테스트 추가**

`entrypoints_spec.lua` `T.run` 안에 추가:

```lua
  feature_modules_load_and_follow_contract = function()
    local cfg = dofile(SCRIPTS_DIR .. "/config.lua")
    T.eq(#cfg.featureModules, 4, "four features")
    for _, name in ipairs(cfg.featureModules) do
      local mod = require("features." .. name)
      T.eq(mod.name, name, "name matches module " .. name)
    end
  end,
```

`BridgeClientTests.cs`의 `SaveControl_WritesCamelCaseProtocol` 마지막 줄 앞에 추가:

```csharp
        Assert.True(root.GetProperty("features").GetProperty("build").GetProperty("noMaterials").GetBoolean());
```

- [ ] **Step 2: 실행해서 실패 확인** — Expected: `feature_modules_load_and_follow_contract` FAIL(`four features: expected <4> got <0>`), `SaveControl_WritesCamelCaseProtocol` FAIL(KeyNotFoundException)

- [ ] **Step 3: 구현**
- `config.lua`: `featureModules = {},` → `featureModules = { "resources", "build", "upgrade", "military" },`. 그 위 주석은 `-- 로드할 기능 모듈 이름 (Scripts/features/<name>.lua)`로 고친다.
- `ControlDocument.cs`의 `BuildControl`에 `public bool NoMaterials { get; set; } = true;`를 `InstantRepair` 아래에 추가한다.
- `MainForm.cs`:
  - 필드 추가: `private readonly CheckBox _noMaterials = new() { Text = "자재 불필요 (건설 자재 없이 공사)", AutoSize = true };`
  - 건설 탭 목록: `Page("건설", _buildEnabled, _ignorePlacement, _instantBuild, _instantRepair, _noMaterials)`
  - `LoadControlIntoUi`: `_noMaterials.Checked = f.Build.NoMaterials;`
  - `Apply`: `f.Build.NoMaterials = _noMaterials.Checked;`
  - 체크박스 텍스트 수정: `_ignorePlacement` → `"배치 제한 무시 (네이티브, Plan 3)"`, `_instantBuild` → `"즉시 완공 (네이티브, Plan 3)"`

- [ ] **Step 4: 테스트·빌드 통과 확인**

Run: `dotnet build E:\MLToybox\panel\MLToybox.sln -warnaserror:nullable` → 오류 0
Run: `dotnet test E:\MLToybox\panel\MLToybox.sln` → 전부 PASS

- [ ] **Step 5: 커밋** — `git add mod panel`, 메시지 `feat: register Lua features and add no-materials option to panel`

---

### Task 8: 인게임 검증

**Files:**
- Modify: `analysis/findings.md`("Plan 2 인게임 검증" 섹션 추가)
- Modify: `README.md`(기능 표, Lab 사용법)

**Interfaces:**
- Consumes: Task 1~7 전부

- [ ] **Step 1: 배포와 재시작 준비**

```powershell
pwsh -NoProfile -File E:\MLToybox\tools\backup-saves.ps1
pwsh -NoProfile -File E:\MLToybox\tools\deploy.ps1 -Mod MLToybox
pwsh -NoProfile -File E:\MLToybox\tools\deploy.ps1 -Mod MLToyboxLab
pwsh -NoProfile -File E:\MLToybox\tools\deploy.ps1 -Mod MLToybox -Panel
```

게임이 실행 중이면 사용자에게 저장·종료를 요청한다(메인 메뉴 상태이고 세이브를 로드한 기록이 없으면 AI가 종료해도 된다. UE4SS.log의 `GameState: RTSGame_C` 줄로 판단). `Start-Process 'steam://rungameid/1363080'`로 실행하고, 사용자에게 **테스트 세이브** 로드를 요청한다.

- [ ] **Step 2: 기능별 검증** — 패널(UI Automation)로 설정을 적용하고, Lab(`tools\lab.ps1`)으로 값을 읽는다. 각 항목의 Expected와 실제 값을 findings에 기록한다.

| # | 조작 | Expected |
|---|---|---|
| ① | 자원 활성, Timber·planks 목표 = 현재+100, Treasury = HUD값+500, 적용 | 5초 안에 `status.json` resources의 Timber·planks·Treasury가 목표 이상이다. Lab으로 `getStockOfGood(16)`이 목표 이상이다. 10초 뒤 금고가 목표+500 이상으로 **두 번** 더해지지 않았다 |
| ③ | 업그레이드 활성, 적용 | Lab으로 스파이크 때 막혔던 레벨 2 주거지의 `canUpgrade(4)`가 true다 |
| ② | 건설 활성 + 자재 불필요 + 즉시 수리, 적용 | Lab으로 미완공 건물의 `constructionGoods`가 0개다. 사용자에게 새 건물 1개 배치를 요청하고, 자재 운반 없이 공사가 진행되는지 확인한다(`getConstructionStatus`가 자재 부족 상태가 아님) |
| ④ | 군사 전부 활성, 적용 | Lab으로 `maxNumOfMilitiaToSpawn` = 99, `recruitCost` = 0, `hiredMercs` cost = 0이다. 다음 인게임 월 정산 뒤 금고가 용병 급여만큼 줄지 않는지 확인한다. 줄면 findings에 기록하고, ①의 금고 유지가 보정한다 |
| 공통 | 메인 메뉴로 나갔다가 같은 세이브 다시 로드 | 패널이 "메인 메뉴" 상태를 거쳐 "적용됨"으로 돌아온다. `status.json` features의 모든 기능이 `active=true`, `lastError` 없음 |

`hiredMercs` 값 쓰기(`game.unwrap(v).cost = 0`)가 맵에 남지 않으면 ruling으로 기록한다. 대체로 `engine.hiredMercs:Add(k, 수정한 구조체)`를 시도하고, 그래도 안 되면 금고 보정에 맡긴다.

- [ ] **Step 3: 결과 기록**

`analysis/findings.md` 끝에 "## Plan 2 인게임 검증 (날짜)" 섹션을 추가한다. 각 행에 기능, 결과(통과/부분/실패), 근거(Lab 출력 요약), 후속 조치를 적는다.

`README.md`의 "## 설치" 위에 기능 표를 추가한다(Lua 담당 / Plan 3 네이티브 담당 구분). "## 개발 도구" 섹션에 `deploy.ps1 -Mod MLToyboxLab`과 `tools\lab.ps1 -File x.lua` 사용법을 추가한다.

- [ ] **Step 4: 전체 테스트 후 커밋**

Run: `dotnet test E:\MLToybox\panel\MLToybox.sln`, `pwsh -NoProfile -File E:\MLToybox\tools\tests\Tools.Tests.ps1` → 모두 PASS

```powershell
git -C E:\MLToybox add analysis/findings.md README.md
git -C E:\MLToybox commit -m "docs: record Plan 2 in-game verification and document features`n`nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

- [ ] **Step 5: 개발 도구 정리** — `deploy.ps1 -Mod MLToyboxLab -Remove`(게임이 실행 중이어도 안전). Plan 3에서 필요하면 다시 배포한다.
