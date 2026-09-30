# 용병 고용 창 관리와 커스텀 용병단 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 게임의 용병 고용 창을 모드가 관리해서, 빈 칸을 순정 용병단으로 채우고, 패널에서 등록한 커스텀 용병단을 상시 띄우고, 플레이어가 고용한 용병단만 고용비를 환급한다.

**Architecture:** Lua 기능 모듈 `mercenaries`를 레지스트리에 추가한다. 순수 계산(`merc_plan.lua`)이 "있어야 할 목록"과 조치를 정하고, 게임 쪽 모듈(`merc_list.lua`)이 `rerollMercenaries()`로 칸 수를 맞춘 뒤 칸을 제자리에서 덮어쓴다. 패널은 새 "용병" 탭(`MercenaryTab`)에서 설정과 커스텀 정의를 `control.json`의 `features.mercenaries`로 보낸다.

**Tech Stack:** UE4SS 3.0.1 Lua 5.4, xUnit + NLua(Lua spec 러너), .NET 8 WinForms, PowerShell 7

**Spec:** `docs/superpowers/specs/2026-09-30-mercenary-companies-design.md`. 실측 근거는 `analysis/findings.md` "용병 고용 — 목록 보충과 커스텀 용병단 (2026-09-30, 스파이크)".

## Global Constraints

- 상수: `MAX_SLOTS = 3`, `MAX_SQUADS = 10`, `NAME_MAX = 40`, `LOCK_COST = 10000000`, `REBUILD_MIN_INTERVAL = 3`(초), 재구성 불일치 뒤 재시도 `MISMATCH_RETRY = 10`(초), 커스텀 `arrivesIn = 1`, 임시 이름 접미사 `"#mlt"`, quest 특성 이름 `"questOnly"`.
- **배열 전체 대입 금지**: `engine.availableMercs = { ... }`는 게임을 튕긴다. 목록 칸은 항상 제자리에서 고친다(`avail[i].cost = ...`, `avail[i].units = { FName(...) }`).
- 브리지 프로토콜 `version`은 1을 유지하고 필드만 추가한다. 키는 camelCase다(`features.mercenaries.lockFromAi`).
- 게임 오브젝트: 엔진 `MyRTSMultiEngineCPP_BP_C`(`availableMercs`, `hiredMercs`, `squads`, `rerollMercenaries()`), 플레이어 `MyPawnCPP_BP3_C`, 치트 `MLCheatManager_C`(`ChangeTreasury(n)`은 증감), 고용 창 `mercenaryScreen_C`(전체 이름에 `/Engine/Transient`가 있는 인스턴스가 실제 위젯), 용병 표 `/Game/NotStronghold/Data/DT_MercenaryCompanies.DT_MercenaryCompanies`.
- UE4SS 전역(`FindFirstOf`, `FindAllOf`, `StaticFindObject`, `FName`)은 `core/game.lua`와 `core/datatable.lua` 안에서만 부른다. 기능 모듈은 이 두 모듈을 통해서만 게임에 접근한다.
- 기능을 꺼도 이미 바뀐 목록과 환급은 되돌리지 않는다.
- Lua 파일은 **Write 도구로만** 작성한다(Bash heredoc 금지, 백슬래시가 깨진다).
- 패널을 고치면 `dotnet test panel/MLToybox.sln`과 별도로 `dotnet build panel/MLToybox.sln`을 실행한다.
- Lua 모드 변경은 게임 재시작 뒤 적용된다. 패널은 실행 중이면 배포할 수 없다.
- 인게임 작업 전에 `pwsh tools/backup-saves.ps1`을 실행한다. 기존 세이브를 덮어쓰지 않는다. 테스트용 저장은 슬롯 `saveGame_900`에만 한다. 테스트가 끝나면 게임을 저장 없이 종료한다(`Stop-Process -Force`).
- git: 작업 브랜치 `feat/mercenary-companies`(`develop`에서 분기). 태스크마다 커밋. 끝나면 `develop`에 `--no-ff`로 병합하고 `develop`을 푸시한다. 커밋 메시지 끝에 `Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>`.
- 모든 git·파일 명령은 절대경로 또는 `git -C E:/MLToybox`를 쓴다(`cd` 뒤 상대경로 금지).
- 스펙과 다른 점: 스펙 7절 M5의 1차 대체안(BP `Open`/`Close` 후킹)은 계획에 넣지 않는다. M5가 실패하면 멈추고 사용자 결정을 받는다(Task 1 Step 9).

## Review Focus

1. **게임 객체가 아직 없거나 사라진 시점의 tick**(로드 직후, 메뉴 복귀 직전): 예외 없이 건너뛰어야 한다. → Task 5 `tick_without_game_objects_is_a_noop`
2. **다시 뽑기가 예외를 낸 경우**: 임시로 바꾼 용병 표 행 이름이 반드시 되돌아와야 한다(안 그러면 그 게임 세션 내내 표가 망가진다). → Task 4 `rebuild_restores_row_names_when_the_reroll_fails`
3. **손으로 고쳤거나 깨진 설정**(`companies`가 표가 아님, 병종이 문자열이 아님, 고용비가 소수·문자열): 그 정의만 건너뛰고 나머지는 계속 처리해야 한다. → Task 2 `validate_tolerates_garbage_settings`
4. **플레이어가 고용 확인 창을 띄운 순간**: 목록을 바꾸면 확인 중인 카드가 다른 용병단으로 바뀐다. 건드리지 않아야 한다. → Task 5 `nothing_changes_while_the_hire_confirmation_is_up`
5. **환급 이중 지급과 치트 매니저 부재**: 같은 고용에 두 번 환급하면 안 되고, 환급할 수 없을 때 비용만 0으로 만들면 환급이 영영 사라진다. → Task 5 `a_later_hire_is_refunded_once_then_zeroed`, `without_the_cheat_manager_the_refund_waits`

## File Structure

| 파일 | 작업 | 책임 |
|---|---|---|
| `tools/lab.ps1` | 수정 | `-Vars`로 스크립트의 `__KEY__` 치환 |
| `tools/lab-load.ps1` | 생성 | 게임 시작(선택)과 메인 메뉴에서 세이브 로드 |
| `tools/lab/merc_state.lua` | 생성 | 용병 상태 읽기(개발용, 읽기 전용) |
| `tools/lab/merc_screen.lua` | 생성 | 고용 창 열기·닫기(개발용) |
| `tools/lab/merc_hire.lua` | 생성 | 이름으로 카드 고용(개발용) |
| `mod/MLToybox/Scripts/features/merc_plan.lua` | 생성 | 순수 계산: 정의 검증, 있어야 할 목록, 조치 |
| `mod/MLToybox/Scripts/features/merc_list.lua` | 생성 | 게임 쪽: 목록 읽기, 재구성, 제자리 쓰기, 고용 창 상태 |
| `mod/MLToybox/Scripts/features/mercenaries.lua` | 생성 | 기능 등록, 환급, 상태 보고 |
| `mod/MLToybox/Scripts/features/military.lua` | 수정 | 용병 비용 코드 삭제 |
| `mod/MLToybox/Scripts/core/game.lua` | 수정 | `mercScreen()`, `regionByKey(key)` |
| `mod/MLToybox/Scripts/core/registry.lua` | 수정 | 상태에 `mercenaries` |
| `mod/MLToybox/Scripts/config.lua` | 수정 | 기능 목록에 `mercenaries` |
| `mod/MLToybox/tests/*_spec.lua` | 생성·수정 | 위 모듈의 스펙 |
| `panel/MLToybox.Panel.Core/ControlDocument.cs` | 수정 | `MercenariesControl`, `MercCompany` |
| `panel/MLToybox.Panel.Core/StatusDocument.cs` | 수정 | `MercenaryStatus` |
| `panel/MLToybox.Panel.Core/MercCompanyRules.cs` | 생성 | 패널 쪽 검증, 구성 요약 |
| `panel/MLToybox.Panel/MercenaryTab.cs` | 생성 | "용병" 탭 UI |
| `panel/MLToybox.Panel/MainForm.cs` | 수정 | 탭 연결, 군사 탭 라벨 |
| `panel/MLToybox.Tests/MercenaryTests.cs` | 생성 | 직렬화, 상태 읽기, 검증 규칙 |
| `README.md`, `docs/CHANGELOG.md`, `analysis/findings.md` | 수정 | 문서 |

---

### Task 1: 브랜치 준비, 개발 도구, 인게임 실측 M1~M5

스펙 7절의 실측 5건을 확인하고 결과를 `findings.md`에 기록한다. 결과가 Task 4(M1), Task 6(M2), README(M3)의 값을 정한다. M4나 M5가 실패하면 멈추고 사용자에게 보고한다.

**Files:**
- Modify: `tools/lab.ps1`
- Modify: `tools/tests/Tools.Tests.ps1`
- Create: `tools/lab-load.ps1`
- Create: `tools/lab/merc_state.lua`, `tools/lab/merc_screen.lua`, `tools/lab/merc_hire.lua`
- Create(커밋하지 않음, gitignore 대상): `analysis/dumps/probes/m1.lua`, `m2_prepare.lua`, `m3.lua`, `m4_save.lua`, `m5.lua`
- Modify: `analysis/findings.md`

**Interfaces:**
- Consumes: 기존 `tools/lab.ps1`(Lab 모드에 Lua 파일을 보내 실행), `tools/common.ps1`의 `Get-MLModsDir`.
- Produces:
  - `& E:\MLToybox\tools\lab.ps1 -File <lua> [-Vars @{ KEY = 'value' }]`: 스크립트 안의 `__KEY__`를 값으로 바꿔 실행하고 출력을 돌려준다.
  - `& E:\MLToybox\tools\lab-load.ps1 -Slot <슬롯> [-Start]`: `-Start`면 게임이 꺼져 있을 때 Steam으로 켠다. 메인 메뉴에서 그 슬롯을 불러오고 인게임이 될 때까지 기다린다.
  - `tools/lab/merc_state.lua`: `slot`, `hired`, `treasury`, `screen`, `rows` 줄을 출력한다(Task 8이 읽는다).
  - `tools/lab/merc_screen.lua`(`__ACTION__` = `open` 또는 `close`), `tools/lab/merc_hire.lua`(`__NAME__` = 카드의 용병단 이름. 마지막 줄이 `HIRED` 또는 `NOT_HIRED`).
  - `findings.md`의 "용병 실측 M1~M5" 절: M1~M5 각각 `PASS` 또는 `FAIL`과 근거 출력.

- [ ] **Step 1: 브랜치 준비**

```bash
git -C E:/MLToybox checkout develop
git -C E:/MLToybox merge --no-ff docs/mercenary-spike -m "Merge branch 'docs/mercenary-spike' into develop"
git -C E:/MLToybox checkout -b feat/mercenary-companies
git -C E:/MLToybox log --oneline -3
```
Expected: 맨 위 커밋이 `Merge branch 'docs/mercenary-spike' into develop`이고 현재 브랜치가 `feat/mercenary-companies`.

- [ ] **Step 2: `lab.ps1 -Vars`의 실패하는 테스트 작성**

`tools/tests/Tools.Tests.ps1`의 마지막 두 줄(`if ($script:failed -gt 0) ...`) **앞**에 추가한다.

```powershell
Test-Case 'lab.ps1 replaces __KEY__ tokens from -Vars' {
    $g = New-FakeGame
    $lab = New-Item -ItemType Directory -Force (Join-Path $g 'ManorLords\Binaries\Win64\ue4ss\Mods\MLToyboxLab\lab')
    $src = Join-Path $env:TEMP "mltb-lab-$(Get-Random).lua"
    Set-Content $src 'print("__NAME__", __COUNT__)' -Encoding utf8NoBOM -NoNewline
    try { & "$PSScriptRoot\..\lab.ps1" -File $src -GameDir $g -TimeoutSec 1 -Vars @{ NAME = '토이박스'; COUNT = 3 } } catch { }
    Assert-Equal (Get-Content -Raw (Join-Path $lab.FullName 'run.lua')) 'print("토이박스", 3)' 'substituted'
}
```

- [ ] **Step 3: 테스트가 실패하는지 확인**

Run: `pwsh E:\MLToybox\tools\tests\Tools.Tests.ps1`
Expected: `FAIL lab.ps1 replaces __KEY__ tokens from -Vars`(`-Vars` 매개변수가 없음)와 마지막에 `1 test(s) failed`.

- [ ] **Step 4: `tools/lab.ps1` 구현**

파일 전체를 다음으로 바꾼다.

```powershell
param([Parameter(Mandatory)][string]$File, [string]$GameDir, [int]$TimeoutSec = 60, [hashtable]$Vars)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\common.ps1"
$lab = Join-Path (Get-MLModsDir $GameDir) 'MLToyboxLab\lab'
if (-not (Test-Path $lab)) { throw 'MLToyboxLab not deployed. Run: deploy.ps1 -Mod MLToyboxLab (then restart the game)' }
$out = Join-Path $lab 'out.txt'
Remove-Item $out -ErrorAction SilentlyContinue
# -Vars @{ NAME = 'x' } 는 스크립트 안의 __NAME__ 을 x 로 바꿔 넣는다
$code = Get-Content -Raw -Encoding utf8 $File
if ($Vars) { foreach ($k in $Vars.Keys) { $code = $code.Replace("__${k}__", [string]$Vars[$k]) } }
# Lab 모드가 쓰다 만 파일을 읽지 않도록 임시 파일에 쓴 뒤 이름을 바꾼다
$tmp = Join-Path $lab 'run.lua.tmp'
Set-Content -Path $tmp -Value $code -Encoding utf8NoBOM -NoNewline
Move-Item -Force $tmp (Join-Path $lab 'run.lua')
$deadline = (Get-Date).AddSeconds($TimeoutSec)
while (-not (Test-Path $out)) {
    if ((Get-Date) -gt $deadline) { throw "Timed out; is the game running with MLToyboxLab loaded?" }
    Start-Sleep -Milliseconds 300
}
Get-Content $out
```

- [ ] **Step 5: 테스트 통과 확인**

Run: `pwsh E:\MLToybox\tools\tests\Tools.Tests.ps1`
Expected: `PASS lab.ps1 replaces __KEY__ tokens from -Vars`와 마지막 줄 `ALL PASS`.

- [ ] **Step 6: `tools/lab-load.ps1` 작성**

```powershell
# 개발용: 메인 메뉴에서 세이브 슬롯을 불러온다(화면 조작 불필요). MLToyboxLab 이 배포돼 있어야 한다.
param([Parameter(Mandatory)][string]$Slot, [string]$GameDir, [int]$TimeoutSec = 180, [switch]$Start)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\common.ps1"
if ($Slot -notmatch '^[A-Za-z0-9_]+$') { throw "bad slot name: $Slot" }
$lab = Join-Path $PSScriptRoot 'lab.ps1'
$status = Join-Path (Get-MLModsDir $GameDir) 'MLToybox\bridge\status.json'

function Read-Status {
    try { return Get-Content -Raw $status | ConvertFrom-Json } catch { return $null }
}
function Test-Alive($s) {
    return $s -and (([DateTimeOffset]::UtcNow.ToUnixTimeSeconds() - $s.heartbeat) -le 3)
}

if ($Start -and -not (Get-Process -Name 'ManorLords-Win64-Shipping' -ErrorAction SilentlyContinue)) {
    & (Get-ItemProperty 'HKCU:\Software\Valve\Steam').SteamExe -applaunch $script:MLAppId
    $deadline = (Get-Date).AddSeconds(240)
    while (-not (Test-Alive (Read-Status))) {
        if ((Get-Date) -gt $deadline) { throw 'game did not start (no mod heartbeat)' }
        Start-Sleep -Seconds 3
    }
    Start-Sleep -Seconds 15   # 스플래시가 끝나고 메인 메뉴가 뜰 때까지
}
if (-not (Test-Alive (Read-Status))) { throw 'game is not running with MLToybox loaded (use -Start)' }

$tmp = Join-Path ([IO.Path]::GetTempPath()) "mltb-lab-load-$PID"
New-Item -ItemType Directory -Force $tmp | Out-Null
$openLua = Join-Path $tmp 'open.lua'
$loadLua = Join-Path $tmp 'load.lua'
Set-Content $openLua -Encoding utf8NoBOM -Value @'
local w
for _, x in ipairs(FindAllOf("mainMenu_widget_C") or {}) do
  if x:IsValid() and x:IsInViewport() then w = x end
end
if not w then print("NO_MENU") return end
local ready = false
pcall(function() ready = w.LoadMenuWidget:IsValid() end)
if ready then print("READY") else w:SwitchToLoadScreen(false) print("OPENED") end
'@
Set-Content $loadLua -Encoding utf8NoBOM -Value @'
local w
for _, x in ipairs(FindAllOf("mainMenu_widget_C") or {}) do
  if x:IsValid() and x:IsInViewport() then w = x end
end
if not w then print("NO_MENU") return end
local slots = w.LoadMenuWidget.SortedSlotsByDate
for i = 1, #slots do
  local slot = slots[i]
  local name = ""
  pcall(function() name = slot.Descriptor.saveSlot:ToString() end)
  if name == "__SLOT__" then
    w:LoadGame({ SlotIndex = slot.SlotIndex, Descriptor = slot.Descriptor })
    print("LOADING")
    return
  end
end
print("NO_SLOT")
'@

$opened = & $lab -File $openLua -GameDir $GameDir
if ($opened -contains 'NO_MENU') { throw 'main menu is not showing (already in a game?)' }
Start-Sleep -Seconds 6   # 슬롯 목록을 만드는 시간
$loaded = & $lab -File $loadLua -GameDir $GameDir -Vars @{ SLOT = $Slot }
if ($loaded -notcontains 'LOADING') { throw "slot not loaded: $($loaded -join ' ')" }
$deadline = (Get-Date).AddSeconds($TimeoutSec)
while (-not (Read-Status).inGame) {
    if ((Get-Date) -gt $deadline) { throw 'timed out waiting for the save to load' }
    Start-Sleep -Seconds 3
}
Write-Host "Loaded $Slot"
```

- [ ] **Step 7: `tools/lab/` 개발용 스크립트 3개 작성**

`tools/lab/merc_state.lua`:
```lua
-- 개발용(읽기 전용): 용병 고용 창 목록, 고용 중 용병단과 분대 소유, 국고, 고용 창 상태, 용병 표 행 이름
local function s(x) local ok, v = pcall(function() return x:ToString() end); return ok and v or tostring(x) end
local function names(arr)
  local out = {}
  pcall(function() for i = 1, #arr do out[#out + 1] = s(arr[i]) end end)
  return table.concat(out, ",")
end
local engine = FindFirstOf("MyRTSMultiEngineCPP_BP_C")
local pawn = FindFirstOf("MyPawnCPP_BP3_C")
if not engine:IsValid() or not pawn:IsValid() then print("NOT_IN_GAME") return end

local avail = engine.availableMercs
print("available", #avail)
for i = 1, #avail do
  local c = avail[i]
  local region = "nil"
  pcall(function() if c.arrivalRegion:IsValid() then region = s(c.arrivalRegion.regionName) end end)
  print("slot", i, "name=" .. s(c.Name), "cost=" .. tostring(c.cost), "arrivesIn=" .. tostring(c.arrivesIn), "region=" .. region,
    "units=[" .. names(c.units) .. "]", "traits=[" .. names(c.traits) .. "]")
end

local owners = {}
local squads = engine.squads
for i = 1, #squads do
  local sq = squads[i]
  if sq.companyID >= 0 then
    local mine = false
    pcall(function() mine = sq.ownerPawn:GetAddress() == pawn:GetAddress() end)
    local o = owners[sq.companyID] or { squads = 0, mine = 0, units = {} }
    o.squads = o.squads + 1
    if mine then o.mine = o.mine + 1 end
    o.units[#o.units + 1] = s(sq.unitType) .. ":" .. #sq.unitArr
    owners[sq.companyID] = o
  end
end
engine.hiredMercs:ForEach(function(k, v)
  local id, c = k:get(), v:get()
  local o = owners[id] or { squads = 0, mine = 0, units = {} }
  print("hired", id, "name=" .. s(c.Name), "cost=" .. tostring(c.cost), "squads=" .. o.squads, "mine=" .. o.mine, "units=[" .. table.concat(o.units, ",") .. "]")
end)

local treasury = "?"
for _, w in ipairs(FindAllOf("W_HUD_LordPanel_V2_C") or {}) do
  if w:IsValid() then
    pcall(function() if w.TreasuryNumeric:IsValid() then treasury = tostring(w.TreasuryNumeric.CurrentNumericValue) end end)
  end
end
print("treasury", treasury)

for _, x in ipairs(FindAllOf("mercenaryScreen_C") or {}) do
  if x:IsValid() and x:GetFullName():find("/Engine/Transient", 1, true) then
    local confirm = "?"
    pcall(function() confirm = tostring(x.HireConfirmation:IsVisible()) end)
    print("screen", "visible=" .. tostring(x:IsVisible()), "confirmVisible=" .. confirm)
  end
end

local dt = StaticFindObject("/Game/NotStronghold/Data/DT_MercenaryCompanies.DT_MercenaryCompanies")
local rowNames = {}
dt:ForEachRow(function(name, row) rowNames[#rowNames + 1] = tostring(name) .. "=" .. s(row.Name) .. ":" .. tostring(row.cost) end)
print("rows", table.concat(rowNames, " "))
```

`tools/lab/merc_screen.lua`:
```lua
-- 개발용: 용병 고용 창을 열거나 닫는다. __ACTION__ = open | close
local ACTION = "__ACTION__"
local screen
for _, x in ipairs(FindAllOf("mercenaryScreen_C") or {}) do
  if x:IsValid() and x:GetFullName():find("/Engine/Transient", 1, true) then screen = x end
end
if not screen then print("NO_SCREEN") return end
if ACTION == "open" then screen:Open() elseif ACTION == "close" then screen:Close() else print("BAD_ACTION", ACTION) return end
print("screen", ACTION, "visible=" .. tostring(screen:IsVisible()))
```

`tools/lab/merc_hire.lua`:
```lua
-- 개발용: 고용 창에서 용병단 이름이 __NAME__ 인 카드를 게임의 고용 경로(카드 버튼 -> 확인 창의 확인)로 고용한다.
-- 고용 창은 먼저 merc_screen.lua 로 열어 둔다(AI 잠금이 풀린 가격으로 고용하려면 연 뒤 2초 기다린다).
local NAME = "__NAME__"
local function s(x) local ok, v = pcall(function() return x:ToString() end); return ok and v or tostring(x) end
local engine = FindFirstOf("MyRTSMultiEngineCPP_BP_C")
local screen
for _, x in ipairs(FindAllOf("mercenaryScreen_C") or {}) do
  if x:IsValid() and x:GetFullName():find("/Engine/Transient", 1, true) then screen = x end
end
if not screen then print("NO_SCREEN") return end
local function hired()
  local out = {}
  engine.hiredMercs:ForEach(function(k, v) out[#out + 1] = k:get() .. "=" .. s(v:get().Name) end)
  table.sort(out)
  return table.concat(out, ", ")
end
screen:updateCompanies()
local hb = screen.mercenary_companies_HB
local card
for i = 0, hb:GetChildrenCount() - 1 do
  local k = hb:GetChildAt(i)
  if s(k.MercenaryCompany.Name) == NAME then card = k end
end
if not card then print("NO_CARD", NAME) return end
print("card", NAME, "cost=" .. tostring(card.MercenaryCompany.cost), "buttonEnabled=" .. tostring(card.Button_76:GetIsEnabled()))
local before = hired()
card["BndEvt__Button_76_K2Node_ComponentBoundEvent_0_OnButtonReleasedEvent__DelegateSignature"](card)
local conf = screen.HireConfirmation
conf["BndEvt__HireConfirmation_menuButton_K2Node_ComponentBoundEvent_2_onReleased__DelegateSignature"](conf)
print("before", before)
print("after", hired())
print(hired() ~= before and "HIRED" or "NOT_HIRED")
```

- [ ] **Step 8: 실측 프로브 5개 작성 (`analysis/dumps/probes/`, 커밋하지 않음)**

`analysis/dumps/probes/m5.lua` — 고용 창과 확인 창의 `IsVisible()`:
```lua
local function s(x) local ok, v = pcall(function() return x:ToString() end); return ok and v or tostring(x) end
local engine = FindFirstOf("MyRTSMultiEngineCPP_BP_C")
local screen
for _, x in ipairs(FindAllOf("mercenaryScreen_C") or {}) do
  if x:IsValid() and x:GetFullName():find("/Engine/Transient", 1, true) then screen = x end
end
if not screen then print("M5 BLOCKED: no screen") return end
local conf = screen.HireConfirmation
local function show(tag) print(tag, "screen=" .. tostring(screen:IsVisible()), "confirm=" .. tostring(conf:IsVisible())) end
local function hiredCount() local n = 0; engine.hiredMercs:ForEach(function() n = n + 1 end); return n end
if #engine.availableMercs == 0 then engine:rerollMercenaries() end
for i = 1, #engine.availableMercs do engine.availableMercs[i].cost = 900000 + i end
show("closed")
screen:Open()
screen:updateCompanies()
show("open")
local hb = screen.mercenary_companies_HB
if hb:GetChildrenCount() == 0 then print("M5 BLOCKED: no cards") return end
local before = hiredCount()
local card = hb:GetChildAt(0)
card["BndEvt__Button_76_K2Node_ComponentBoundEvent_0_OnButtonReleasedEvent__DelegateSignature"](card)
show("confirming")
conf["BndEvt__HireConfirmation_menuButton_1_K2Node_ComponentBoundEvent_3_onReleased__DelegateSignature"](conf)
show("after cancel")
print("hired changed by cancel", tostring(hiredCount() ~= before))
```

`analysis/dumps/probes/m1.lua` — 표 행의 깃발과 특성을 칸에 쓰기:
```lua
local function s(x) local ok, v = pcall(function() return x:ToString() end); return ok and v or tostring(x) end
local function names(arr)
  local out = {}
  pcall(function() for i = 1, #arr do out[#out + 1] = s(arr[i]) end end)
  return table.concat(out, ",")
end
local function full(o) local ok, v = pcall(function() return o:IsValid() and o:GetFullName() or "invalid" end); return ok and v or "error" end
local function try(label, fn) local ok, err = pcall(fn); print(label, ok and "ok" or ("FAILED: " .. tostring(err):match("^[^\n]*"))) end
local engine = FindFirstOf("MyRTSMultiEngineCPP_BP_C")
local dt = StaticFindObject("/Game/NotStronghold/Data/DT_MercenaryCompanies.DT_MercenaryCompanies")
if #engine.availableMercs < 2 then engine:rerollMercenaries() end
local avail = engine.availableMercs
if #avail < 2 then print("M1 BLOCKED: fewer than two slots", #avail) return end
for i = 1, #avail do avail[i].cost = 900000 + i end
local c = avail[1]
local rowName = "brotherhood_of_the_forest"
if s(c.Name) == s(dt:FindRow(rowName).Name) then rowName = "wayward_sons" end
local row = dt:FindRow(rowName)
print("slot before", s(c.Name), "traits=[" .. names(c.traits) .. "]", "banner=" .. full(c.banner), c.colorA, c.colorB, c.emblemA, c.emblemB)
print("row", rowName, "traits=[" .. names(row.traits) .. "]", "banner=" .. full(row.banner), row.colorA, row.colorB, row.emblemA, row.emblemB)
local traits = {}
for i = 1, #row.traits do traits[i] = FName(s(row.traits[i])) end
try("write banner", function() c.banner = row.banner end)
try("write traits", function() c.traits = traits end)
try("write colors", function() c.colorA = row.colorA; c.colorB = row.colorB; c.emblemA = row.emblemA; c.emblemB = row.emblemB end)
try("empty traits of slot 2", function() avail[2].traits:Empty() end)
local after = engine.availableMercs[1]
print("slot after", "traits=[" .. names(after.traits) .. "]", "banner=" .. full(after.banner), after.colorA, after.colorB, after.emblemA, after.emblemB)
print("slot 2 traits after Empty", "[" .. names(engine.availableMercs[2].traits) .. "]")
local ok = names(after.traits) == names(row.traits) and full(after.banner) == full(row.banner) and after.colorA == row.colorA and after.emblemB == row.emblemB
  and #engine.availableMercs[2].traits == 0
print("M1", ok and "PASS" or "FAIL")
```

`analysis/dumps/probes/m2_prepare.lua` — 민병대·친위대 병종 8종으로 커스텀 칸 만들기:
```lua
local engine = FindFirstOf("MyRTSMultiEngineCPP_BP_C")
local pawn = FindFirstOf("MyPawnCPP_BP3_C")
local region
for _, r in ipairs(FindAllOf("BP_Region_C") or {}) do
  if r:IsValid() and r.ownerPawn:IsValid() and r.ownerPawn:GetAddress() == pawn:GetAddress() then region = region or r end
end
if #engine.availableMercs == 0 then engine:rerollMercenaries() end
local avail = engine.availableMercs
if #avail == 0 or not region then print("M2 BLOCKED", #avail, tostring(region)) return end
for i = 1, #avail do avail[i].cost = 900000 + i end
local c = avail[1]
c.Name = "MLT M2"
c.cost = 0
c.arrivesIn = 1
c.units = { FName("militia"), FName("spearMilitia"), FName("militiaPole"), FName("militiaFoot"), FName("bowMilitia"), FName("crossbowMilitia"), FName("retinue_tier1"), FName("retinue_tier3") }
c.arrivalRegion = region
print("M2 prepared", #engine.availableMercs[1].units, "units")
```

`analysis/dumps/probes/m3.lua` — 부대 패널 위젯이 들고 있는 용병단 이름:
```lua
local function s(x) local ok, v = pcall(function() return x:ToString() end); return ok and v or tostring(x) end
local found = false
for _, cls in ipairs({ "W_HUD_MercenaryCompanyV2_C", "hiredMercenaryCompanyBanner_C" }) do
  for _, w in ipairs(FindAllOf(cls) or {}) do
    if w:IsValid() and w:GetFullName():find("/Engine/Transient", 1, true) then
      local name = "?"
      pcall(function() name = s(w.MercenaryCompany.Name) end)
      if name == "?" then pcall(function() name = s(w["Mercenary Company"].Name) end) end
      if name == "MLT M2" then found = true end
      print(cls, "name=" .. name, "visible=" .. tostring(w:IsVisible()))
    end
  end
end
print("M3", found and "PASS" or "FAIL")
```

`analysis/dumps/probes/m4_save.lua` — 목록에 커스텀 칸을 하나 만들고 새 슬롯에 저장:
```lua
local engine = FindFirstOf("MyRTSMultiEngineCPP_BP_C")
local pawn = FindFirstOf("MyPawnCPP_BP3_C")
if #engine.availableMercs == 0 then engine:rerollMercenaries() end
local avail = engine.availableMercs
if #avail == 0 then print("M4 BLOCKED: list empty") return end
local c = avail[1]
c.Name = "MLT M4 list"
c.cost = 1234
c.arrivesIn = 1
c.units = { FName("mercenary_infantry"), FName("mercenary_crossbowmen") }
pawn:SaveToDiskWithThumbnail("saveGame_900", "MLToybox test")
print("M4 save requested")
```

- [ ] **Step 9: 실측 실행 (M5 → M1 → M2 → M3 → M4)**

게임이 꺼져 있고 패널(`MLToybox.Panel.exe`)도 꺼져 있는지 확인한 뒤 PowerShell에서 실행한다. 셸 상태는 도구 호출 사이에 유지되지 않으므로, 아래 코드 블록을 따로 실행할 때는 맨 앞에 `$lab = 'E:\MLToybox\tools\lab.ps1'; $p = 'E:\MLToybox\analysis\dumps\probes'` 줄을 다시 붙인다. `lab-load.ps1 -Start`가 `game did not start`로 실패하면(직전에 강제 종료한 경우 Steam이 아직 실행 중으로 볼 수 있다) 10초 뒤 한 번 더 실행한다.

```powershell
$lab = 'E:\MLToybox\tools\lab.ps1'; $p = 'E:\MLToybox\analysis\dumps\probes'
pwsh E:\MLToybox\tools\backup-saves.ps1
pwsh E:\MLToybox\tools\deploy.ps1 -Mod MLToyboxLab
& E:\MLToybox\tools\lab-load.ps1 -Slot saveGame_8 -Start
```
Expected: `Loaded saveGame_8`.

M5:
```powershell
& $lab -File "$p\m5.lua"
& $lab -File E:\MLToybox\tools\lab\merc_screen.lua -Vars @{ ACTION = 'close' }
Start-Sleep -Seconds 3
& $lab -File E:\MLToybox\tools\lab\merc_state.lua | Select-String '^screen'
```
M5 PASS 조건: `closed screen=false`, `open screen=true confirm=false`, `confirming screen=true confirm=true`, `after cancel ... confirm=false`, `hired changed by cancel false`, 그리고 닫고 3초 뒤 `screen visible=false`. 하나라도 다르면 M5 FAIL이다. **M5 FAIL이면 여기서 멈추고 `NEEDS_CONTEXT`로 보고한다**(AI 잠금을 뺄지, 열림 상태를 후킹으로 추적할지 사용자가 정한다).

M1:
```powershell
& $lab -File "$p\m1.lua"
```
M1 PASS 조건: 마지막 줄 `M1 PASS`이고 게임이 살아 있다(`Get-Process ManorLords-Win64-Shipping`). `M1 FAIL`이거나 게임이 튕기면 M1 FAIL이다(튕겼으면 `lab-load.ps1 -Slot saveGame_8 -Start`로 다시 불러온 뒤 계속한다).

M2와 M3:
```powershell
& $lab -File "$p\m2_prepare.lua"
& $lab -File E:\MLToybox\tools\lab\merc_screen.lua -Vars @{ ACTION = 'open' }
& $lab -File E:\MLToybox\tools\lab\merc_hire.lua -Vars @{ NAME = 'MLT M2' }
Start-Sleep -Seconds 10
& $lab -File E:\MLToybox\tools\lab\merc_state.lua | Select-String '^hired'
& $lab -File "$p\m3.lua"
& $lab -File E:\MLToybox\tools\lab\merc_screen.lua -Vars @{ ACTION = 'close' }
```
M2 판정: `merc_hire.lua`의 마지막 줄이 `HIRED`이고, `hired ... name=MLT M2` 줄의 `units=[...]`에 병종 8개가 각각 `이름:병사수`로 나온다. 병사 수가 0보다 큰 병종은 PASS, 목록에 없거나 0인 병종은 FAIL이다. FAIL인 병종 id를 기록한다(Task 6에서 뺀다).
M3 판정: 마지막 줄 `M3 PASS`면 위젯이 커스텀 이름을 들고 있다.

M4:
```powershell
& $lab -File "$p\m4_save.lua"
$sav = Join-Path $env:LOCALAPPDATA 'ManorLords\Saved\SaveGames\saveGame_900.sav'
$deadline = (Get-Date).AddSeconds(60); while (-not (Test-Path $sav) -and (Get-Date) -lt $deadline) { Start-Sleep 2 }
Test-Path $sav
Start-Sleep -Seconds 5
Get-Process -Name 'ManorLords-Win64-Shipping' | Stop-Process -Force
Start-Sleep -Seconds 5
& E:\MLToybox\tools\lab-load.ps1 -Slot saveGame_900 -Start
& $lab -File E:\MLToybox\tools\lab\merc_state.lua
Get-Process -Name 'ManorLords-Win64-Shipping' | Stop-Process -Force
```
M4 PASS 조건: `Test-Path`가 `True`, 다시 불러온 뒤 `slot ... name=MLT M4 list cost=1234 ... units=[mercenary_infantry,mercenary_crossbowmen]` 줄과 `hired ... name=MLT M2 ... mine=<M2에서 생긴 분대 수>` 줄이 있다. **`saveGame_900.sav`가 생기지 않거나(`False`), 슬롯을 불러올 수 없거나, 고용 중 용병단이 사라졌으면 M4 FAIL이다. 여기서 멈추고 `NEEDS_CONTEXT`로 보고한다**(스펙 7절: 이 기능을 보류할지 사용자가 정한다).

- [ ] **Step 10: 결과를 `analysis/findings.md`에 기록**

파일 끝에 다음 절을 추가하고, 각 항목의 `PASS/FAIL`과 근거 출력(한두 줄)을 실제 결과로 채운다.

```markdown
## 용병 실측 M1~M5 (2026-09-30, saveGame_8)
구현 계획 `docs/superpowers/plans/2026-09-30-mercenary-companies.md` Task 1의 결과다.

| # | 항목 | 결과 | 근거 |
|---|---|---|---|
| M1 | 표 행의 깃발·특성을 목록 칸에 쓰기 | (PASS 또는 FAIL) | (m1.lua 의 slot after 줄과 마지막 줄) |
| M2 | 민병대·친위대 병종의 용병 고용 경로 생성 | (병종별 PASS/FAIL) | (hired ... name=MLT M2 줄의 units) |
| M3 | 부대 패널 위젯의 커스텀 이름 | (PASS 또는 FAIL) | (m3.lua 출력) |
| M4 | 저장·로드 뒤 커스텀 목록 칸과 고용 중 용병단 유지 | (PASS 또는 FAIL) | (다시 불러온 뒤 merc_state.lua 의 slot, hired 줄) |
| M5 | 고용 창·확인 창 `IsVisible()` | (PASS 또는 FAIL) | (m5.lua 의 closed/open/confirming/after cancel 줄) |

- 테스트 세이브 `saveGame_900`("MLToybox test")이 세이브 폴더에 남아 있다.
- 세이브 로드 자동화: `tools/lab-load.ps1 -Slot <슬롯> -Start`.
```

- [ ] **Step 11: 커밋**

```bash
git -C E:/MLToybox add tools/lab.ps1 tools/lab-load.ps1 tools/lab tools/tests/Tools.Tests.ps1 analysis/findings.md
git -C E:/MLToybox commit -m "chore(tools): lab variables, save loading helper, and mercenary measurements M1-M5

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 2: `merc_plan.lua` — 정의 검증과 목록 계획 (순수 계산)

**Files:**
- Create: `mod/MLToybox/Scripts/features/merc_plan.lua`
- Test: `mod/MLToybox/tests/merc_plan_spec.lua`

**Interfaces:**
- Consumes: 없음(게임 객체와 다른 모듈을 쓰지 않는다).
- Produces:
  - 상수 `plan.MAX_SLOTS = 3`, `plan.MAX_SQUADS = 10`, `plan.NAME_MAX = 40`, `plan.LOCK_COST = 10000000`.
  - `plan.validate(companies, ctx) -> valid, skipped`
    - `companies`: 설정의 `companies`(아무 값이나 올 수 있다).
    - `ctx = { vanillaNames = { [소문자 이름] = true }, regionKeys = { "gold", ... }, unitExists = function(unit) -> bool }`.
    - `valid`: `{ { name, units = { "unit", ... }, cost, region } ... }` 최대 3개. `region`은 항상 `ctx.regionKeys` 중 하나다.
    - `skipped`: `{ { name, reason } ... }`.
  - `plan.build(input) -> { desired, action, renames, slots }`
    - `input = { rows = { { rowName, name, cost, quest } ... }, hiredNames = { [name] = true }, current = { { name, cost, units, region } ... }, customs = valid, screenOpen, lockFromAi, canCopyRows, pick = function(candidates, count) -> list }`.
    - `desired`: `{ kind = "custom", name, company, cost }` 또는 `{ kind = "vanilla", name, rowName, cost, keep }`의 목록(커스텀, 유지하는 순정, 새 순정 순).
    - `action`: `"none"`, `"inplace"`, `"rebuild"`.
    - `renames`: 재구성 때 임시로 이름을 바꿀 표 행 수.
    - `slots`: `action`이 `"none"`이나 `"inplace"`일 때만 있다. `slots[i]`는 지금 목록의 i번 칸에 있어야 할 `desired` 항목이다.

- [ ] **Step 1: 실패하는 스펙 작성**

`mod/MLToybox/tests/merc_plan_spec.lua`:
```lua
local T = require("t")
local plan = require("features.merc_plan")

local function rows()
  return {
    { rowName = "a", name = "a", cost = 10, quest = false },
    { rowName = "b", name = "b", cost = 20, quest = false },
    { rowName = "c", name = "c", cost = 30, quest = false },
    { rowName = "d", name = "d", cost = 40, quest = false },
    { rowName = "q", name = "q", cost = 50, quest = true },
  }
end

local function first(list, n)
  local out = {}
  for i = 1, n do out[i] = list[i] end
  return out
end

-- plan.validate 를 통과한 모양의 커스텀 정의
local function company(over)
  local c = { name = "토이박스", units = { "inf", "bow" }, cost = 3000, region = "nus" }
  for k, v in pairs(over or {}) do c[k] = v end
  return c
end

local function input(over)
  local i = { rows = rows(), hiredNames = {}, current = {}, customs = {}, screenOpen = false, lockFromAi = true, canCopyRows = true, pick = first }
  for k, v in pairs(over or {}) do i[k] = v end
  return i
end

-- 목록에 이미 올라 있는 커스텀 칸
local function listed(c, cost)
  return { name = c.name, cost = cost, units = { table.unpack(c.units) }, region = c.region }
end

local function names(desired)
  local out = {}
  for i, d in ipairs(desired) do out[i] = d.name end
  return table.concat(out, ",")
end

local function ctx(over)
  local c = { vanillaNames = { a = true, greencaps = true }, regionKeys = { "gold", "nus" }, unitExists = function(u) return u == "inf" or u == "bow" end }
  for k, v in pairs(over or {}) do c[k] = v end
  return c
end

-- 설정 파일에 들어 있는 모양의 정의
local function def(over)
  local c = { name = "토이박스", units = { "inf", "bow" }, cost = 3000, region = "nus", enabled = true }
  for k, v in pairs(over or {}) do c[k] = v end
  return c
end

T.run({
  empty_list_is_rebuilt_with_three_vanilla = function()
    local r = plan.build(input())
    T.eq(r.action, "rebuild", "action"); T.eq(names(r.desired), "a,b,c", "first three candidates"); T.eq(r.renames, 0, "no renames")
    T.eq(r.desired[1].kind, "vanilla", "kind"); T.eq(r.desired[1].rowName, "a", "row"); T.eq(r.desired[1].cost, 10, "table cost")
    T.eq(r.desired[1].keep, false, "new pick")
  end,
  quest_and_hired_rows_are_never_candidates = function()
    local r = plan.build(input({ hiredNames = { a = true, b = true } }))
    T.eq(names(r.desired), "c,d", "only free non-quest rows"); T.eq(r.renames, 0, "enough candidates")
  end,
  customs_come_first_and_lock_while_the_screen_is_closed = function()
    local c = company()
    local r = plan.build(input({ customs = { c } }))
    T.eq(names(r.desired), "토이박스,a,b", "custom first"); T.eq(r.desired[1].kind, "custom", "kind"); T.eq(r.desired[1].company, c, "definition")
    T.eq(r.desired[1].cost, plan.LOCK_COST, "locked")
    T.eq(plan.build(input({ customs = { c }, screenOpen = true })).desired[1].cost, 3000, "open -> configured cost")
    T.eq(plan.build(input({ customs = { c }, lockFromAi = false })).desired[1].cost, 3000, "lock off")
  end,
  listed_vanilla_is_kept_before_new_picks = function()
    local r = plan.build(input({ current = { { name = "d", cost = 40 }, { name = "b", cost = 20 } } }))
    T.eq(names(r.desired), "d,b,a", "kept in list order, then a new pick")
    T.eq(r.desired[1].keep, true, "kept"); T.eq(r.desired[3].keep, false, "new"); T.eq(r.action, "rebuild", "count differs")
  end,
  customs_are_listed_even_when_every_vanilla_is_hired = function()
    local r = plan.build(input({ hiredNames = { a = true, b = true, c = true, d = true }, customs = { company(), company({ name = "궁수대" }) } }))
    T.eq(names(r.desired), "토이박스,궁수대", "customs only"); T.eq(r.renames, 2, "two temp renames"); T.eq(r.action, "rebuild", "rebuild")
  end,
  renames_cover_only_the_shortfall = function()
    local r = plan.build(input({ hiredNames = { a = true, b = true, c = true }, customs = { company() } }))
    T.eq(names(r.desired), "토이박스,d", "custom + last free vanilla"); T.eq(r.renames, 1, "one candidate short")
  end,
  matching_list_needs_nothing = function()
    local c = company()
    local r = plan.build(input({ customs = { c }, current = { listed(c, plan.LOCK_COST), { name = "b", cost = 20 }, { name = "c", cost = 30 } } }))
    T.eq(r.action, "none", "action"); T.eq(names(r.desired), "토이박스,b,c", "desired")
  end,
  opening_the_screen_unlocks_in_place = function()
    local c = company()
    local r = plan.build(input({ customs = { c }, screenOpen = true, current = { listed(c, plan.LOCK_COST), { name = "b", cost = 20 }, { name = "c", cost = 30 } } }))
    T.eq(r.action, "inplace", "action"); T.eq(r.slots[1].cost, 3000, "slot 1 gets the configured cost"); T.eq(r.slots[2].name, "b", "others stay")
  end,
  matching_entries_stay_in_their_slots = function()
    local c = company()
    local r = plan.build(input({ customs = { c }, current = { { name = "b", cost = 20 }, listed(c, plan.LOCK_COST), { name = "c", cost = 30 } } }))
    T.eq(r.action, "none", "order in the list does not matter")
    T.eq(r.slots[1].name, "b", "slot 1"); T.eq(r.slots[2].name, "토이박스", "slot 2"); T.eq(r.slots[3].name, "c", "slot 3")
  end,
  stale_entry_is_replaced_in_place = function()
    local c = company()
    local r = plan.build(input({ customs = { c }, current = { { name = "a", cost = 10 }, { name = "old custom", cost = 5 }, { name = "c", cost = 30 } } }))
    T.eq(r.action, "inplace", "action")
    T.eq(r.slots[1].name, "a", "kept"); T.eq(r.slots[2].name, "토이박스", "stale slot reused"); T.eq(r.slots[3].name, "c", "kept 2")
  end,
  wrong_vanilla_cost_is_fixed_in_place_even_without_row_copy = function()
    local r = plan.build(input({ canCopyRows = false, current = { { name = "a", cost = 0 }, { name = "b", cost = 20 }, { name = "c", cost = 30 } } }))
    T.eq(r.action, "inplace", "cost-only fix"); T.eq(r.slots[1].cost, 10, "table cost")
  end,
  new_vanilla_in_a_stale_slot_needs_row_copy = function()
    local current = { { name = "a", cost = 10 }, { name = "b", cost = 20 }, { name = "gone", cost = 1 } }
    T.eq(plan.build(input({ current = current })).action, "inplace", "row copy available")
    T.eq(plan.build(input({ current = current, canCopyRows = false })).action, "rebuild", "falls back to a reroll")
  end,
  changed_custom_definition_is_dirty = function()
    local c = company()
    local rest = { { name = "a", cost = 10 }, { name = "b", cost = 20 } }
    local other = listed(c, plan.LOCK_COST); other.units = { "inf" }
    T.eq(plan.build(input({ customs = { c }, current = { other, rest[1], rest[2] } })).action, "inplace", "units differ")
    local moved = listed(c, plan.LOCK_COST); moved.region = "gold"
    T.eq(plan.build(input({ customs = { c }, current = { moved, rest[1], rest[2] } })).action, "inplace", "region differs")
    local cased = listed(c, plan.LOCK_COST); cased.units = { "INF", "Bow" }
    T.eq(plan.build(input({ customs = { c }, current = { cased, rest[1], rest[2] } })).action, "none", "unit names compare case-insensitively")
  end,
  empty_table_and_no_customs_is_none = function()
    local r = plan.build(input({ rows = {} }))
    T.eq(r.action, "none", "nothing to list"); T.eq(#r.desired, 0, "empty")
  end,
  validate_accepts_a_good_definition_and_resolves_the_region = function()
    local noRegion = def({ name = "C" }); noRegion.region = nil
    local valid, skipped = plan.validate({ def({ name = "  토이박스  " }), def({ name = "B", region = "zzz" }), noRegion }, ctx())
    T.eq(#valid, 3, "all valid"); T.eq(#skipped, 0, "none skipped")
    T.eq(valid[1].name, "토이박스", "trimmed"); T.eq(valid[1].region, "nus", "own region kept"); T.eq(valid[1].cost, 3000, "cost")
    T.eq(valid[2].region, "gold", "unknown region -> first"); T.eq(valid[3].region, "gold", "missing region -> first")
    T.eq(#valid[1].units, 2, "units copied")
  end,
  validate_reports_each_bad_definition = function()
    local eleven = {}
    for i = 1, 11 do eleven[i] = "inf" end
    local cases = {
      { def({ name = "   " }), "name is empty" },
      { def({ name = string.rep("가", 41) }), "longer than 40" },
      { def({ name = "Greencaps" }), "used by a game company" },
      { def({ units = {} }), "1..10 squads" },
      { def({ units = eleven }), "1..10 squads" },
      { def({ units = { "inf", "dragon" } }), "unknown unit: dragon" },
      { def({ cost = -1 }), "non-negative integer" },
      { def({ cost = 1.5 }), "non-negative integer" },
    }
    for _, case in ipairs(cases) do
      local valid, skipped = plan.validate({ case[1] }, ctx())
      T.eq(#valid, 0, case[2] .. ": rejected")
      T.truthy(skipped[1].reason:find(case[2], 1, true), case[2] .. ": reason was " .. skipped[1].reason)
    end
  end,
  validate_counts_characters_not_bytes = function()
    local valid = plan.validate({ def({ name = string.rep("가", 40) }) }, ctx())
    T.eq(#valid, 1, "40 Korean characters fit")
  end,
  validate_rejects_duplicates_ignoring_case = function()
    local valid, skipped = plan.validate({ def({ name = "Alpha" }), def({ name = "alpha" }) }, ctx())
    T.eq(#valid, 1, "first wins"); T.eq(skipped[1].reason, "duplicate name", "second skipped")
  end,
  validate_uses_at_most_three_enabled_definitions = function()
    local off = def({ name = "off" }); off.enabled = false
    local valid, skipped = plan.validate({ off, def({ name = "1" }), def({ name = "2" }), def({ name = "3" }), def({ name = "4" }) }, ctx())
    T.eq(#valid, 3, "three"); T.eq(#skipped, 1, "only the fourth is reported"); T.eq(skipped[1].name, "4", "which one")
    T.truthy(skipped[1].reason:find("more than 3", 1, true), "reason")
  end,
  validate_without_a_player_region_skips_everything = function()
    local valid, skipped = plan.validate({ def() }, ctx({ regionKeys = {} }))
    T.eq(#valid, 0, "none"); T.eq(skipped[1].reason, "no player region", "reason")
  end,
  validate_tolerates_garbage_settings = function()
    local valid, skipped = plan.validate(nil, ctx())
    T.eq(#valid, 0, "nil companies"); T.eq(#skipped, 0, "nothing to report")
    valid, skipped = plan.validate({ 5, "x", def({ name = 7 }), def({ units = "inf" }), def({ units = { 3 } }), def({ cost = "free" }) }, ctx())
    T.eq(#valid, 0, "all rejected"); T.eq(#skipped, 4, "table entries reported, scalars ignored")
  end,
})
```

- [ ] **Step 2: 실패 확인**

Run: `dotnet test E:\MLToybox\panel\MLToybox.sln --filter "DisplayName~merc_plan_spec"`
Expected: FAIL, 메시지에 `module 'features.merc_plan' not found`.

- [ ] **Step 3: `merc_plan.lua` 구현**

`mod/MLToybox/Scripts/features/merc_plan.lua`:
```lua
-- 용병 고용 창 목록 계획. 게임 객체를 만지지 않는 순수 계산이다 (spec 2026-09-30 §4.1)
local M = { MAX_SLOTS = 3, MAX_SQUADS = 10, NAME_MAX = 40, LOCK_COST = 10000000 }

local function trim(s)
  s = s:gsub("^%s+", "")
  s = s:gsub("%s+$", "")
  return s
end

local function reasonFor(name, c, ctx, seen)
  if name == "" then return "name is empty" end
  local len = utf8.len(name)
  if not len or len > M.NAME_MAX then return "name is longer than " .. M.NAME_MAX .. " characters" end
  local key = name:lower()
  if ctx.vanillaNames[key] then return "name is used by a game company" end
  if seen[key] then return "duplicate name" end
  if type(c.units) ~= "table" or #c.units < 1 or #c.units > M.MAX_SQUADS then
    return "units must list 1.." .. M.MAX_SQUADS .. " squads"
  end
  for _, u in ipairs(c.units) do
    if type(u) ~= "string" or not ctx.unitExists(u) then return "unknown unit: " .. tostring(u) end
  end
  local cost = c.cost
  if type(cost) ~= "number" or cost < 0 or cost ~= math.floor(cost) then return "cost must be a non-negative integer" end
  if #ctx.regionKeys == 0 then return "no player region" end
  return nil
end

-- 사용 중(enabled)인 정의만 본다. 통과한 것은 최대 MAX_SLOTS 개, 나머지는 이유와 함께 skipped 로 돌려준다
function M.validate(companies, ctx)
  local valid, skipped, seen = {}, {}, {}
  if type(companies) ~= "table" then return valid, skipped end
  for _, c in ipairs(companies) do
    if type(c) == "table" and c.enabled == true then
      local name = type(c.name) == "string" and trim(c.name) or ""
      local reason = reasonFor(name, c, ctx, seen)
      if not reason and #valid >= M.MAX_SLOTS then reason = "more than " .. M.MAX_SLOTS .. " enabled companies" end
      if reason then
        skipped[#skipped + 1] = { name = name, reason = reason }
      else
        seen[name:lower()] = true
        local region = ctx.regionKeys[1]
        for _, key in ipairs(ctx.regionKeys) do
          if key == c.region then region = key end
        end
        local units = {}
        for i, u in ipairs(c.units) do units[i] = u end
        valid[#valid + 1] = { name = name, units = units, cost = math.floor(c.cost), region = region }
      end
    end
  end
  return valid, skipped
end

local function sameUnits(a, b)
  if #a ~= #b then return false end
  for i = 1, #a do
    if tostring(a[i]):lower() ~= tostring(b[i]):lower() then return false end
  end
  return true
end

-- 목록의 칸 e 가 있어야 할 항목 d 와 같은가
local function same(e, d)
  if e.name ~= d.name or e.cost ~= d.cost then return false end
  if d.kind == "custom" then
    return sameUnits(e.units or {}, d.company.units) and e.region == d.company.region
  end
  return true
end

function M.build(input)
  local candidates, byName = {}, {}
  for _, r in ipairs(input.rows) do
    if not r.quest and not input.hiredNames[r.name] then
      candidates[#candidates + 1] = r
      byName[r.name] = r
    end
  end

  local desired, used = {}, {}
  local locked = input.lockFromAi and not input.screenOpen
  for _, c in ipairs(input.customs) do
    desired[#desired + 1] = { kind = "custom", name = c.name, company = c, cost = locked and M.LOCK_COST or c.cost }
    used[c.name] = true
  end
  -- 이미 떠 있는 순정 카드는 유지한다
  for _, e in ipairs(input.current) do
    local r = byName[e.name]
    if r and not used[e.name] and #desired < M.MAX_SLOTS then
      used[e.name] = true
      desired[#desired + 1] = { kind = "vanilla", name = r.name, rowName = r.rowName, cost = r.cost, keep = true }
    end
  end
  local free = {}
  for _, r in ipairs(candidates) do
    if not used[r.name] then free[#free + 1] = r end
  end
  local need = math.min(M.MAX_SLOTS - #desired, #free)
  if need > 0 then
    for _, r in ipairs(input.pick(free, need)) do
      desired[#desired + 1] = { kind = "vanilla", name = r.name, rowName = r.rowName, cost = r.cost, keep = false }
    end
  end

  local result = { desired = desired, action = "rebuild", renames = math.max(0, #desired - #candidates) }
  if #input.current ~= #desired then return result end

  -- 칸 수가 같다. 이름이 같은 칸은 그 자리에 두고, 남은 칸에 남은 항목을 순서대로 넣는다
  local slots, taken = {}, {}
  for i, e in ipairs(input.current) do
    for j, d in ipairs(desired) do
      if not taken[j] and d.name == e.name then
        slots[i] = d
        taken[j] = true
        break
      end
    end
  end
  local j = 1
  for i = 1, #input.current do
    if not slots[i] then
      while taken[j] do j = j + 1 end
      slots[i] = desired[j]
      taken[j] = true
    end
  end

  local dirty = false
  for i, e in ipairs(input.current) do
    local d = slots[i]
    if not same(e, d) then
      dirty = true
      -- 다른 순정 용병단으로 바꾸려면 표 행 내용을 칸에 복사해야 한다. 그게 안 되는 환경이면 다시 뽑는다
      if d.kind == "vanilla" and e.name ~= d.name and not input.canCopyRows then return result end
    end
  end
  result.slots = slots
  result.action = dirty and "inplace" or "none"
  return result
end

return M
```

- [ ] **Step 4: 통과 확인**

Run: `dotnet test E:\MLToybox\panel\MLToybox.sln --filter "DisplayName~merc_plan_spec"`
Expected: PASS (1 test).

- [ ] **Step 5: 커밋**

```bash
git -C E:/MLToybox add mod/MLToybox/Scripts/features/merc_plan.lua mod/MLToybox/tests/merc_plan_spec.lua
git -C E:/MLToybox commit -m "feat(mercenaries): plan the hire list and validate custom companies

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 3: `core/game.lua` — 고용 창 위젯과 영지 조회

**Files:**
- Modify: `mod/MLToybox/Scripts/core/game.lua`
- Test: `mod/MLToybox/tests/game_spec.lua`

**Interfaces:**
- Consumes: 기존 `game.find.all(className)`, `game.playerRegions()`, `game.regionKey(region)`, `safe.valid(obj)`.
- Produces:
  - `game.mercScreen() -> widget | nil`: 전체 이름에 `/Engine/Transient`가 있는 유효한 `mercenaryScreen_C`.
  - `game.regionByKey(key) -> region | nil`: 내 영지 가운데 `regionUniqueTag`가 `key`인 것.

- [ ] **Step 1: 실패하는 테스트 추가**

`mod/MLToybox/tests/game_spec.lua`의 `T.run({` 안, `unwrap_handles_wrapped_and_plain` 케이스 **앞**에 추가한다.

```lua
  merc_screen_is_the_live_widget_not_the_template = function()
    local function widget(full) return F.object({ GetFullName = function() return full end }) end
    local template = widget("mercenaryScreen_C /Game/UI/HUD/MainUICPP.MainUICPP_C:WidgetTree.mercenaryScreen")
    local live = widget("mercenaryScreen_C /Engine/Transient.GameEngine_1:BP_MLGameInstance_C_1.MainUICPP_C_1.WidgetTree_1.mercenaryScreen")
    install({ mercenaryScreen_C = { template, live } })
    T.eq(game.mercScreen(), live, "live widget")
    install({ mercenaryScreen_C = { template, F.invalid() } })
    T.eq(game.mercScreen(), nil, "template and invalid objects are ignored")
    install({})
    T.eq(game.mercScreen(), nil, "none")
  end,
  region_by_key_finds_only_player_regions = function()
    local function named(tag, owner)
      local r = F.object({ ownerPawn = owner })
      r.regionUniqueTag = { ToString = function() return tag end }
      return r
    end
    local pawn, other = F.object(), F.object()
    local gold, nus, theirs = named("gold", pawn), named("nus", pawn), named("zzz", other)
    install({ MyPawnCPP_BP3_C = { pawn }, BP_Region_C = { gold, theirs, nus } })
    T.eq(game.regionByKey("nus"), nus, "my region"); T.eq(game.regionByKey("zzz"), nil, "someone else's region"); T.eq(game.regionByKey(nil), nil, "no key")
  end,
```

- [ ] **Step 2: 실패 확인**

Run: `dotnet test E:\MLToybox\panel\MLToybox.sln --filter "DisplayName~game_spec"`
Expected: FAIL, `attempt to call a nil value (field 'mercScreen')`.

- [ ] **Step 3: 구현**

`mod/MLToybox/Scripts/core/game.lua`의 `function M.unwrap(x)` **앞**에 추가한다.

```lua
-- 용병 고용 창. 같은 클래스의 객체가 둘 있다: 위젯 트리의 원본(/Game/UI/HUD/...)과 화면에 붙은 실제 위젯(/Engine/Transient...)
function M.mercScreen()
  for _, w in ipairs(M.find.all("mercenaryScreen_C")) do
    if safe.valid(w) then
      local ok, full = pcall(function() return w:GetFullName() end)
      if ok and type(full) == "string" and full:find("/Engine/Transient", 1, true) then return w end
    end
  end
  return nil
end

-- 내 영지 가운데 regionUniqueTag 가 key 인 것
function M.regionByKey(key)
  if key == nil then return nil end
  for _, r in ipairs(M.playerRegions()) do
    if M.regionKey(r) == key then return r end
  end
  return nil
end
```

- [ ] **Step 4: 통과 확인**

Run: `dotnet test E:\MLToybox\panel\MLToybox.sln --filter "DisplayName~game_spec"`
Expected: PASS.

- [ ] **Step 5: 커밋**

```bash
git -C E:/MLToybox add mod/MLToybox/Scripts/core/game.lua mod/MLToybox/tests/game_spec.lua
git -C E:/MLToybox commit -m "feat(game): look up the mercenary hire screen and player regions by key

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 4: `merc_list.lua` — 목록 읽기, 재구성, 제자리 쓰기

**Files:**
- Create: `mod/MLToybox/Scripts/features/merc_list.lua`
- Test: `mod/MLToybox/tests/merc_list_spec.lua`

**Interfaces:**
- Consumes:
  - Task 2의 `desired` 항목 모양: `{ kind = "custom", name, company = { name, units, cost, region }, cost }`, `{ kind = "vanilla", name, rowName, cost, keep }`.
  - Task 3의 `game.mercScreen()`, `game.regionByKey(key)`. 기존 `game.fname(s)`, `game.regionKey(region)`, `game.unwrap(x)`, `datatable.forEachRow("mercenaries", fn)`, `datatable.object("mercenaries")`, `safe.valid(obj)`.
  - Task 1의 M1 결과(`CAN_COPY_ROWS` 값).
- Produces:
  - `list.CAN_COPY_ROWS`(bool), `list.CUSTOM_ARRIVES_IN = 1`, `list.TEMP_SUFFIX = "#mlt"`, `list.QUEST_TRAIT = "questOnly"`.
  - `list.rows() -> { { rowName, name, cost, quest } ... }`
  - `list.hiredNames(engine) -> { [name] = true }`
  - `list.read(engine) -> { { name, cost, units = { "unit", ... }, region = key | nil } ... }`
  - `list.screenState() -> { open = bool, confirming = bool }`
  - `list.applyInPlace(engine, slots)`: `slots[i]`가 있는 칸만, 다른 필드만 쓴다.
  - `list.rebuild(engine, desired, renames) -> 실제 칸 수`
  - `list.refreshScreen()`: 고용 창이 열려 있으면 `updateCompanies()`.

- [ ] **Step 1: 실패하는 스펙 작성**

`mod/MLToybox/tests/merc_list_spec.lua`:
```lua
local T = require("t")
local F = require("fakes")
local game = require("core.game")
local datatable = require("core.datatable")
local plan = require("features.merc_plan")
local list = require("features.merc_list")

local function row(name, cost, units, traits)
  return { Name = name, cost = cost, units = F.array(units), traits = F.array(traits or {}), banner = F.object({}), colorA = 1, colorB = 2, emblemA = 3, emblemB = 4 }
end

-- 가짜 게임: 용병 표 5행(q 는 quest 행), rerollMercenaries 는 실측한 규칙(고용 중 이름과 quest 행 제외, 최대 3개)을 따른다
local function setup(opts)
  opts = opts or {}
  -- 출하 기본값(M1 결과)과 무관하게 케이스마다 명시한다. 기본은 표 행 복사 가능
  list.CAN_COPY_ROWS = opts.canCopyRows ~= false
  local regions = { gold = F.object({ key = "gold" }), nus = F.object({ key = "nus" }) }
  local rows = {
    a = row("a", 10, { "inf" }),
    b = row("b", 20, { "inf", "bow" }, { "Looters" }),
    c = row("c", 30, { "spear" }),
    d = row("d", 40, { "bow" }),
    q = row("q", 50, { "inf" }, { "questOnly" }),
  }
  datatable.find = function(p)
    if p == datatable.PATHS.mercenaries then return F.datatable(rows) end
  end
  game.fname = function(s) return s end
  game.regionKey = function(r) return r.key end
  game.regionByKey = function(key) return regions[key] end

  local hired = {}
  for i, name in ipairs(opts.hired or {}) do hired[i] = { i - 1, { Name = name, cost = 0 } } end
  local real = F.object({ availableMercs = F.array(opts.list or {}), hiredMercs = F.map(hired), rerolls = 0 })
  real.rerollMercenaries = function(self)
    self.rerolls = self.rerolls + 1
    if opts.rerollFails then error("reroll exploded") end
    local taken = {}
    for _, e in ipairs(hired) do taken[e[2].Name] = true end
    self.availableMercs:Empty()
    for _, rowName in ipairs({ "a", "b", "c", "d", "q" }) do
      local r = rows[rowName]
      if #self.availableMercs < 3 and r.traits[1] ~= "questOnly" and not taken[r.Name] then
        self.availableMercs[#self.availableMercs + 1] = {
          Name = r.Name, cost = r.cost, units = F.array({ table.unpack(r.units) }), traits = F.array({ table.unpack(r.traits) }),
          banner = r.banner, colorA = r.colorA, colorB = r.colorB, emblemA = r.emblemA, emblemB = r.emblemB,
          arrivesIn = 20, arrivalRegion = regions.gold,
        }
      end
    end
  end
  -- 배열 전체 대입(engine.availableMercs = ...)은 게임을 튕긴다. 코드가 그렇게 하면 테스트가 실패한다
  local engine = setmetatable({}, {
    __index = real,
    __newindex = function(_, k, v)
      if k == "availableMercs" then error("whole-array assignment is forbidden") end
      real[k] = v
    end,
  })
  return engine, rows, regions
end

local function custom(name, over)
  local company = { name = name, units = { "inf", "bow" }, cost = 3000, region = "nus" }
  for k, v in pairs(over or {}) do company[k] = v end
  return { kind = "custom", name = name, company = company, cost = plan.LOCK_COST }
end

local function vanilla(name, cost, keep)
  return { kind = "vanilla", name = name, rowName = name, cost = cost, keep = keep or false }
end

local function entry(name, over)
  local e = { Name = name, cost = 1, units = F.array({ "inf" }), traits = F.array({}), banner = F.object({}), colorA = 0, colorB = 0, emblemA = 0, emblemB = 0, arrivesIn = 5 }
  for k, v in pairs(over or {}) do e[k] = v end
  return e
end

local function listNames(engine)
  local out = {}
  for i = 1, #engine.availableMercs do out[i] = engine.availableMercs[i].Name end
  return table.concat(out, ",")
end

T.run({
  rows_summarise_the_company_table = function()
    setup()
    local rows = list.rows()
    T.eq(#rows, 5, "five rows"); T.eq(rows[1].rowName, "a", "row name"); T.eq(rows[1].name, "a", "name"); T.eq(rows[1].cost, 10, "cost")
    T.eq(rows[2].quest, false, "other traits are not quest"); T.eq(rows[5].quest, true, "questOnly trait")
  end,
  read_reports_each_slot = function()
    local engine, _, regions = setup({ list = { entry("x", { cost = 7, units = F.array({ "inf", "bow" }) }), entry("y") } })
    engine.availableMercs[1].arrivalRegion = regions.nus
    local cur = list.read(engine)
    T.eq(#cur, 2, "two"); T.eq(cur[1].name, "x", "name"); T.eq(cur[1].cost, 7, "cost")
    T.eq(table.concat(cur[1].units, ","), "inf,bow", "units"); T.eq(cur[1].region, "nus", "region key")
    T.eq(cur[2].region, nil, "no arrival region")
  end,
  hired_names_lists_every_hired_company = function()
    local engine = setup({ hired = { "a", "토이박스" } })
    local names = list.hiredNames(engine)
    T.eq(names.a, true, "vanilla"); T.eq(names["토이박스"], true, "custom"); T.eq(names.b, nil, "not hired")
  end,
  rebuild_writes_customs_first_then_vanilla = function()
    local engine, rows, regions = setup()
    local made = list.rebuild(engine, { custom("토이박스"), vanilla("c", 30), vanilla("d", 40) }, 0)
    T.eq(made, 3, "three slots"); T.eq(engine.rerolls, 1, "one reroll"); T.eq(listNames(engine), "토이박스,c,d", "names")
    local first = engine.availableMercs[1]
    T.eq(table.concat(first.units, ","), "inf,bow", "custom units"); T.eq(#first.traits, 0, "no traits"); T.eq(first.cost, plan.LOCK_COST, "cost")
    T.eq(first.arrivesIn, list.CUSTOM_ARRIVES_IN, "arrival days"); T.eq(first.arrivalRegion, regions.nus, "region")
    local second = engine.availableMercs[2]
    T.eq(table.concat(second.units, ","), "spear", "row units copied"); T.eq(#second.traits, 0, "stale traits cleared")
    T.eq(second.banner, rows.c.banner, "row banner"); T.eq(second.cost, 30, "row cost"); T.eq(second.arrivesIn, 20, "a new pick keeps the rerolled arrival")
  end,
  rebuild_makes_room_by_renaming_hired_rows_and_restores_them = function()
    local engine, rows = setup({ hired = { "a", "b", "c", "d" } })
    local made = list.rebuild(engine, { custom("토이박스"), custom("궁수대") }, 2)
    T.eq(made, 2, "two slots"); T.eq(listNames(engine), "토이박스,궁수대", "customs")
    T.eq(rows.a.Name, "a", "row a restored"); T.eq(rows.b.Name, "b", "row b restored")
  end,
  rebuild_restores_row_names_when_the_reroll_fails = function()
    local engine, rows = setup({ hired = { "a" }, rerollFails = true })
    local ok = pcall(list.rebuild, engine, { custom("토이박스") }, 1)
    T.eq(ok, false, "the error surfaces"); T.eq(rows.a.Name, "a", "row restored")
  end,
  rebuild_restores_the_arrival_of_kept_vanilla = function()
    local engine, _, regions = setup({ list = { entry("b", { cost = 20, arrivesIn = 33 }) } })
    engine.availableMercs[1].arrivalRegion = regions.nus
    list.rebuild(engine, { vanilla("b", 20, true), vanilla("a", 10), vanilla("c", 30) }, 0)
    T.eq(listNames(engine), "b,a,c", "desired order")
    T.eq(engine.availableMercs[1].arrivesIn, 33, "kept arrival days"); T.eq(engine.availableMercs[1].arrivalRegion, regions.nus, "kept region")
    T.eq(table.concat(engine.availableMercs[1].traits, ","), "Looters", "row traits copied")
    T.eq(engine.availableMercs[2].arrivesIn, 20, "a new pick keeps the rerolled arrival")
  end,
  rebuild_writes_what_it_can_when_fewer_slots_come_back = function()
    local engine = setup({ hired = { "a", "b", "c" } })
    local made = list.rebuild(engine, { custom("토이박스"), vanilla("d", 40), vanilla("a", 10) }, 0)
    T.eq(made, 1, "only one slot came back"); T.eq(listNames(engine), "토이박스", "first desired entry written")
  end,
  without_row_copy_customs_take_the_temp_slots = function()
    local engine, rows = setup({ hired = { "a", "b", "c" }, canCopyRows = false })
    local made = list.rebuild(engine, { custom("토이박스"), vanilla("d", 40) }, 1)
    T.eq(made, 2, "two slots"); T.eq(listNames(engine), "토이박스,d", "custom replaced the temp slot, vanilla left as rerolled")
    T.eq(rows.a.Name, "a", "row restored")
  end,
  apply_in_place_writes_only_changed_fields = function()
    local engine, _, regions = setup({ list = {
      entry("토이박스", { cost = plan.LOCK_COST, units = F.array({ "inf", "bow" }), arrivesIn = 1 }),
      entry("b", { cost = 20 }),
    } })
    engine.availableMercs[1].arrivalRegion = regions.nus
    local units = engine.availableMercs[1].units
    local d = custom("토이박스"); d.cost = 3000
    list.applyInPlace(engine, { d })
    T.eq(engine.availableMercs[1].cost, 3000, "cost updated"); T.eq(engine.availableMercs[1].units, units, "unit array untouched")
    T.eq(engine.availableMercs[2].cost, 20, "other slot untouched"); T.eq(engine.rerolls, 0, "no reroll")
  end,
  apply_in_place_fixes_a_vanilla_cost = function()
    local engine = setup({ list = { entry("a", { cost = 0 }) } })
    list.applyInPlace(engine, { vanilla("a", 10, true) })
    T.eq(engine.availableMercs[1].cost, 10, "table cost restored")
  end,
  screen_state_reads_the_hire_screen_and_confirmation = function()
    setup()
    game.mercScreen = function() return nil end
    local s = list.screenState()
    T.eq(s.open, false, "no widget"); T.eq(s.confirming, false, "no widget 2")
    local visible, confirm = true, true
    local screen = F.object({ IsVisible = function() return visible end, HireConfirmation = F.object({ IsVisible = function() return confirm end }) })
    game.mercScreen = function() return screen end
    s = list.screenState(); T.eq(s.open, true, "open"); T.eq(s.confirming, true, "confirming")
    confirm = false
    s = list.screenState(); T.eq(s.confirming, false, "open without a confirmation")
    visible, confirm = false, true
    s = list.screenState(); T.eq(s.open, false, "closed"); T.eq(s.confirming, false, "a hidden screen is never confirming")
  end,
  refresh_screen_updates_only_an_open_screen = function()
    local updates, visible = 0, false
    local screen = F.object({ IsVisible = function() return visible end, updateCompanies = function() updates = updates + 1 end })
    game.mercScreen = function() return screen end
    list.refreshScreen(); T.eq(updates, 0, "closed")
    visible = true
    list.refreshScreen(); T.eq(updates, 1, "open")
    game.mercScreen = function() return nil end
    list.refreshScreen(); T.eq(updates, 1, "no widget")
  end,
})
```

- [ ] **Step 2: 실패 확인**

Run: `dotnet test E:\MLToybox\panel\MLToybox.sln --filter "DisplayName~merc_list_spec"`
Expected: FAIL, `module 'features.merc_list' not found`.

- [ ] **Step 3: `merc_list.lua` 구현**

`mod/MLToybox/Scripts/features/merc_list.lua`:
```lua
local game = require("core.game")
local datatable = require("core.datatable")
local safe = require("core.safe")

-- 용병 고용 창 목록(engine.availableMercs)의 게임 쪽 읽기와 쓰기 (spec 2026-09-30 §4.2, §4.3)
-- 배열 전체 대입(engine.availableMercs = {...})은 게임을 튕긴다(findings 용병 스파이크). 칸은 항상 제자리에서 고친다.
-- 칸 수는 Lua 로 바꿀 수 없어 rerollMercenaries() 로만 맞춘다.
local M = {
  QUEST_TRAIT = "questOnly",
  TEMP_SUFFIX = "#mlt",
  CUSTOM_ARRIVES_IN = 1,
  -- 표 행의 깃발·특성을 칸에 쓸 수 있는가 (findings "용병 실측 M1~M5" 의 M1). false 면 순정 칸은 다시 뽑기 결과를 그대로 둔다
  CAN_COPY_ROWS = true,
}

local function str(x)
  local ok, v = pcall(function() return x:ToString() end)
  if ok then return v end
  return tostring(x)
end

local function names(arr)
  local out = {}
  for i = 1, #arr do out[i] = str(arr[i]) end
  return out
end

local function fnames(list)
  local out = {}
  for i, n in ipairs(list) do out[i] = game.fname(n) end
  return out
end

local function clear(arr)
  if arr.Empty then arr:Empty() return end
  for i = #arr, 1, -1 do arr[i] = nil end
end

local function sameNames(a, b)
  if #a ~= #b then return false end
  for i = 1, #a do
    if a[i]:lower() ~= b[i]:lower() then return false end
  end
  return true
end

local function sameObject(a, b)
  return safe.valid(a) and safe.valid(b) and a:GetAddress() == b:GetAddress()
end

local function isQuest(row)
  for _, t in ipairs(names(row.traits)) do
    if t == M.QUEST_TRAIT then return true end
  end
  return false
end

local function isTemp(name)
  return name:sub(-#M.TEMP_SUFFIX) == M.TEMP_SUFFIX
end

function M.rows()
  local out = {}
  datatable.forEachRow("mercenaries", function(rowName, row)
    out[#out + 1] = { rowName = rowName, name = str(row.Name), cost = row.cost, quest = isQuest(row) }
  end)
  return out
end

function M.hiredNames(engine)
  local out = {}
  engine.hiredMercs:ForEach(function(_, v) out[str(game.unwrap(v).Name)] = true end)
  return out
end

function M.read(engine)
  local out, avail = {}, engine.availableMercs
  for i = 1, #avail do
    local c = avail[i]
    local region = nil
    if safe.valid(c.arrivalRegion) then region = game.regionKey(c.arrivalRegion) end
    out[i] = { name = str(c.Name), cost = c.cost, units = names(c.units), region = region }
  end
  return out
end

function M.screenState()
  local screen = game.mercScreen()
  if not screen then return { open = false, confirming = false } end
  local open = screen:IsVisible() == true
  local conf = screen.HireConfirmation
  local confirming = open and safe.valid(conf) and conf:IsVisible() == true
  return { open = open, confirming = confirming }
end

function M.refreshScreen()
  local screen = game.mercScreen()
  if screen and screen:IsVisible() == true then screen:updateCompanies() end
end

-- 다른 필드만 쓴다. 분대 목록을 다시 대입하면 배열이 새로 할당되므로, AI 잠금처럼 고용비만 바뀔 때는 고용비만 쓴다
local function writeCustom(c, d)
  local company = d.company
  if str(c.Name) ~= company.name then c.Name = company.name end
  if not sameNames(names(c.units), company.units) then c.units = fnames(company.units) end
  if #c.traits > 0 then clear(c.traits) end
  if c.cost ~= d.cost then c.cost = d.cost end
  if c.arrivesIn ~= M.CUSTOM_ARRIVES_IN then c.arrivesIn = M.CUSTOM_ARRIVES_IN end
  local region = game.regionByKey(company.region)
  if region and not sameObject(c.arrivalRegion, region) then c.arrivalRegion = region end
end

-- saved: 유지하는 순정 칸의 재구성 전 도착 정보({ arrivalRegion, arrivesIn }) 또는 nil
local function writeVanilla(c, d, saved)
  if str(c.Name) ~= d.name then
    local row = datatable.object("mercenaries"):FindRow(d.rowName)
    if not row then error("mercenary row not found: " .. tostring(d.rowName)) end
    c.Name = d.name
    c.units = fnames(names(row.units))
    local traits = names(row.traits)
    if #traits > 0 then c.traits = fnames(traits) elseif #c.traits > 0 then clear(c.traits) end
    c.banner = row.banner
    c.colorA, c.colorB, c.emblemA, c.emblemB = row.colorA, row.colorB, row.emblemA, row.emblemB
  end
  if c.cost ~= d.cost then c.cost = d.cost end
  if saved then
    c.arrivalRegion = saved.arrivalRegion
    c.arrivesIn = saved.arrivesIn
  end
end

local function writeSlot(c, d, saved)
  if d.kind == "custom" then writeCustom(c, d) else writeVanilla(c, d, saved) end
end

function M.applyInPlace(engine, slots)
  local avail = engine.availableMercs
  for i = 1, #avail do
    if slots[i] then writeSlot(avail[i], slots[i], nil) end
  end
end

-- 칸 수를 #desired 로 맞추고 내용을 덮어쓴다. 실제로 생긴 칸 수를 돌려준다 (spec §4.3)
function M.rebuild(engine, desired, renames)
  local saved = {}
  local before = engine.availableMercs
  for i = 1, #before do
    local c = before[i]
    saved[str(c.Name)] = { arrivalRegion = c.arrivalRegion, arrivesIn = c.arrivesIn }
  end

  -- 후보가 모자라면 고용 중 이름과 겹치는 표 행의 이름을 잠깐 바꿔 후보로 만든다
  local renamed = {}
  if renames > 0 then
    local hired = M.hiredNames(engine)
    datatable.forEachRow("mercenaries", function(_, row)
      local name = str(row.Name)
      if #renamed < renames and hired[name] and not isQuest(row) then
        renamed[#renamed + 1] = { row = row, name = name }
        row.Name = name .. M.TEMP_SUFFIX
      end
    end)
  end
  local ok, err = pcall(function() engine:rerollMercenaries() end)
  for _, r in ipairs(renamed) do pcall(function() r.row.Name = r.name end) end
  if not ok then error(err, 0) end

  local avail = engine.availableMercs
  local made = #avail
  if M.CAN_COPY_ROWS then
    for i = 1, math.min(made, #desired) do
      local d = desired[i]
      writeSlot(avail[i], d, d.keep and saved[d.name] or nil)
    end
  else
    -- 순정 칸은 다시 뽑기 결과를 그대로 둔다. 커스텀은 임시 이름 칸부터 덮어쓴다
    local order = {}
    for i = 1, made do if isTemp(str(avail[i].Name)) then order[#order + 1] = i end end
    for i = 1, made do if not isTemp(str(avail[i].Name)) then order[#order + 1] = i end end
    local k = 0
    for _, d in ipairs(desired) do
      if d.kind == "custom" then
        k = k + 1
        if order[k] then writeSlot(avail[order[k]], d, nil) end
      end
    end
  end
  return made
end

return M
```

- [ ] **Step 4: 통과 확인**

Run: `dotnet test E:\MLToybox\panel\MLToybox.sln --filter "DisplayName~merc_list_spec"`
Expected: PASS.

- [ ] **Step 5: M1 결과 반영**

`analysis/findings.md`의 "용병 실측 M1~M5" 표에서 M1을 본다.
- M1 PASS: 아무것도 바꾸지 않는다(`CAN_COPY_ROWS = true`).
- M1 FAIL: `merc_list.lua`의 `CAN_COPY_ROWS = true`를 `CAN_COPY_ROWS = false`로 바꾼다. 스펙은 케이스마다 값을 명시하므로 고칠 필요가 없다. Step 4를 다시 실행해 PASS를 확인한다.

- [ ] **Step 6: 커밋**

```bash
git -C E:/MLToybox add mod/MLToybox/Scripts/features/merc_list.lua mod/MLToybox/tests/merc_list_spec.lua
git -C E:/MLToybox commit -m "feat(mercenaries): read and rebuild the hire list in place

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 5: `mercenaries.lua` — 기능 등록, 환급, 상태 보고와 기존 모듈 연결

**Files:**
- Create: `mod/MLToybox/Scripts/features/mercenaries.lua`
- Modify: `mod/MLToybox/Scripts/features/military.lua`
- Modify: `mod/MLToybox/Scripts/core/registry.lua` (`status()`의 반환 표)
- Modify: `mod/MLToybox/Scripts/config.lua` (`featureModules`)
- Test: `mod/MLToybox/tests/mercenaries_spec.lua` (생성)
- Test: `mod/MLToybox/tests/military_spec.lua`, `registry_spec.lua`, `entrypoints_spec.lua` (수정)

**Interfaces:**
- Consumes:
  - Task 2: `plan.validate(companies, ctx)`, `plan.build(input)`, `plan.LOCK_COST`.
  - Task 4: `list.rows()`, `list.hiredNames(engine)`, `list.read(engine)`, `list.screenState()`, `list.applyInPlace(engine, slots)`, `list.rebuild(engine, desired, renames)`, `list.refreshScreen()`, `list.CAN_COPY_ROWS`.
  - 기존: `game.pawn()`, `game.engine()`, `game.cheat()`, `game.regionList()`(`{ { key, name } ... }`), `game.unwrap(x)`, `datatable.object("unitTemplates")`, `safe.valid(obj)`. 레지스트리는 `enable(state, settings)`, `disable(state, settings)`, `tick(state, settings)`를 부른다.
- Produces:
  - 기능 모듈 `{ name = "mercenaries", intervalSec = 1, enable, disable, tick }`. 교체 가능한 `M.clock`(기본 `os.time`)과 `M.pick(candidates, count)`.
  - `state.mercenaries = { slots = { { name, cost, custom } ... }, hiredMine, hiredAi, refunded, skipped = { { name, reason } ... }, note }` → `status.json`의 `mercenaries`.
  - 설정 `features.mercenaries = { enabled, refund, lockFromAi, companies = { { name, units, cost, region, enabled } ... } }`.

- [ ] **Step 1: 실패하는 스펙 작성**

`mod/MLToybox/tests/mercenaries_spec.lua`:
```lua
local T = require("t")
local F = require("fakes")
local game = require("core.game")
local datatable = require("core.datatable")
local plan = require("features.merc_plan")
local list = require("features.merc_list")
local merc = require("features.mercenaries")

local defaultPick = merc.pick

local ROWS = {
  { rowName = "a", name = "a", cost = 10, quest = false },
  { rowName = "b", name = "b", cost = 20, quest = false },
  { rowName = "c", name = "c", cost = 30, quest = false },
  { rowName = "q", name = "q", cost = 50, quest = true },
}
local function full() return { { name = "a", cost = 10 }, { name = "b", cost = 20 }, { name = "c", cost = 30 } } end
local ON = { enabled = true, refund = true, lockFromAi = true, companies = {} }

-- merc_list 는 통째로 가짜로 바꾼다(그 모듈의 동작은 merc_list_spec 이 본다). merc_plan 은 실제 것을 쓴다.
local function setup(opts)
  opts = opts or {}
  local pawn, other = F.object({}), F.object({})
  local calls = { inplace = {}, rebuild = {}, refresh = 0, treasury = {} }
  local hired = opts.hired and opts.hired(pawn, other) or { entries = {}, squads = {} }
  local engine = F.object({ hiredMercs = F.map(hired.entries), squads = hired.squads })
  local cheat = F.object({})
  cheat.ChangeTreasury = function(_, delta) calls.treasury[#calls.treasury + 1] = delta end
  game.pawn = function() return pawn end
  game.engine = function() return engine end
  game.cheat = function() return cheat end
  game.regionList = function() return { { key = "gold", name = "Mandlach" } } end
  datatable.find = function(p)
    if p == datatable.PATHS.unitTemplates then return F.datatable({ inf = {}, bow = {} }) end
  end
  list.rows = function() return ROWS end
  list.hiredNames = function() return {} end
  list.read = function() return opts.current or {} end
  list.screenState = function() return opts.screen or { open = false, confirming = false } end
  list.applyInPlace = function(_, slots) calls.inplace[#calls.inplace + 1] = slots end
  list.rebuild = function(_, desired, renames)
    calls.rebuild[#calls.rebuild + 1] = { desired = desired, renames = renames }
    return opts.rebuildReturns or #desired
  end
  list.refreshScreen = function() calls.refresh = calls.refresh + 1 end
  merc.pick = function(candidates, count)
    local out = {}
    for i = 1, count do out[i] = candidates[i] end
    return out
  end
  local now = 100
  merc.clock = function() return now end
  local state = {}
  merc.enable(state, {})
  return { state = state, calls = calls, pawn = pawn, other = other, cheat = cheat, advance = function(sec) now = now + sec end }
end

local function mineOnly(company)
  return function(pawn) return { entries = { { 0, company } }, squads = { { companyID = 0, ownerPawn = pawn } } } end
end

T.run({
  first_tick_zeroes_player_upkeep_without_refund = function()
    local mine = { Name = "x", cost = 50 }
    local s = setup({ current = full(), hired = mineOnly(mine) })
    merc.tick(s.state, ON)
    T.eq(mine.cost, 0, "upkeep removed"); T.eq(#s.calls.treasury, 0, "a company hired before the feature was on is not refunded")
    T.eq(s.state.mercenaries.refunded, 0, "nothing refunded"); T.eq(s.state.mercenaries.hiredMine, 1, "counted as mine")
  end,
  a_later_hire_is_refunded_once_then_zeroed = function()
    local entries, squads = {}, {}
    local s = setup({ current = full(), hired = function() return { entries = entries, squads = squads } end })
    merc.tick(s.state, ON)
    local hired = { Name = "토이박스", cost = 3000 }
    entries[1] = { 0, hired }
    squads[1] = { companyID = 0, ownerPawn = s.pawn }
    merc.tick(s.state, ON)
    T.eq(s.calls.treasury[1], 3000, "refunded"); T.eq(hired.cost, 0, "then free"); T.eq(s.state.mercenaries.refunded, 3000, "total")
    merc.tick(s.state, ON)
    T.eq(#s.calls.treasury, 1, "not refunded twice")
  end,
  ai_and_squadless_companies_are_left_alone = function()
    local ai, empty = { Name = "ai", cost = 60 }, { Name = "empty", cost = 70 }
    local s = setup({ current = full(), hired = function(_, other)
      return { entries = { { 0, ai }, { 1, empty } }, squads = { { companyID = 0, ownerPawn = other } } }
    end })
    merc.tick(s.state, ON); merc.tick(s.state, ON)
    T.eq(ai.cost, 60, "AI upkeep untouched"); T.eq(empty.cost, 70, "a company without squads is untouched"); T.eq(#s.calls.treasury, 0, "no refund")
    T.eq(s.state.mercenaries.hiredMine, 0, "mine"); T.eq(s.state.mercenaries.hiredAi, 1, "ai")
  end,
  refund_off_touches_nothing_and_rebaselines_when_turned_on = function()
    local mine = { Name = "x", cost = 50 }
    local s = setup({ current = full(), hired = mineOnly(mine) })
    local off = { enabled = true, refund = false, lockFromAi = true, companies = {} }
    merc.tick(s.state, off); merc.tick(s.state, off)
    T.eq(mine.cost, 50, "untouched"); T.eq(s.state.mercenaries.hiredMine, 1, "still counted")
    merc.tick(s.state, ON)
    T.eq(mine.cost, 0, "zeroed"); T.eq(#s.calls.treasury, 0, "hired while refund was off -> no refund")
  end,
  without_the_cheat_manager_the_refund_waits = function()
    local entries, squads = {}, {}
    local s = setup({ current = full(), hired = function() return { entries = entries, squads = squads } end })
    merc.tick(s.state, ON)
    local hired = { Name = "토이박스", cost = 3000 }
    entries[1] = { 0, hired }
    squads[1] = { companyID = 0, ownerPawn = s.pawn }
    game.cheat = function() return nil end
    merc.tick(s.state, ON)
    T.eq(hired.cost, 3000, "cost kept so the refund is not lost"); T.eq(#s.calls.treasury, 0, "nothing refunded yet")
    game.cheat = function() return s.cheat end
    merc.tick(s.state, ON)
    T.eq(s.calls.treasury[1], 3000, "refunded once the cheat manager is back"); T.eq(hired.cost, 0, "then free")
  end,
  nothing_changes_while_the_hire_confirmation_is_up = function()
    local s = setup({ current = {}, screen = { open = true, confirming = true } })
    merc.tick(s.state, ON)
    T.eq(#s.calls.rebuild, 0, "no rebuild"); T.eq(#s.calls.inplace, 0, "no in-place write"); T.eq(s.calls.refresh, 0, "no refresh")
  end,
  in_place_changes_are_applied_and_the_screen_refreshed = function()
    local s = setup({ current = { { name = "a", cost = 0 }, { name = "b", cost = 20 }, { name = "c", cost = 30 } } })
    merc.tick(s.state, ON)
    T.eq(#s.calls.inplace, 1, "applied"); T.eq(s.calls.inplace[1][1].cost, 10, "slot 1 cost fixed")
    T.eq(s.calls.refresh, 1, "refreshed"); T.eq(#s.calls.rebuild, 0, "no reroll")
  end,
  rebuilds_are_spaced_out = function()
    local s = setup({ current = {} })
    merc.tick(s.state, ON)
    T.eq(#s.calls.rebuild, 1, "first rebuild"); T.eq(#s.calls.rebuild[1].desired, 3, "three vanilla"); T.eq(s.calls.rebuild[1].renames, 0, "renames")
    s.advance(1); merc.tick(s.state, ON)
    T.eq(#s.calls.rebuild, 1, "too soon")
    s.advance(merc.REBUILD_MIN_INTERVAL); merc.tick(s.state, ON)
    T.eq(#s.calls.rebuild, 2, "after the interval")
  end,
  a_short_rebuild_is_reported_and_retried_later = function()
    local s = setup({ current = {}, rebuildReturns = 1 })
    merc.tick(s.state, ON)
    T.truthy(s.state.mercenaries.note:find("1 of 3", 1, true), "note")
    s.advance(merc.REBUILD_MIN_INTERVAL); merc.tick(s.state, ON)
    T.eq(#s.calls.rebuild, 1, "waits longer after a mismatch")
    s.advance(merc.MISMATCH_RETRY); merc.tick(s.state, ON)
    T.eq(#s.calls.rebuild, 2, "retried")
  end,
  status_lists_slots_and_skipped_definitions = function()
    local custom = { name = "토이박스", units = { "inf" }, cost = 3000, region = "gold", enabled = true }
    local bad = { name = "나쁜", units = { "dragon" }, cost = 1, enabled = true }
    local s = setup({ current = { { name = "토이박스", cost = plan.LOCK_COST, units = { "inf" }, region = "gold" }, { name = "a", cost = 10 }, { name = "b", cost = 20 } } })
    merc.tick(s.state, { enabled = true, refund = true, lockFromAi = true, companies = { custom, bad } })
    local st = s.state.mercenaries
    T.eq(#st.slots, 3, "slots"); T.eq(st.slots[1].name, "토이박스", "name"); T.eq(st.slots[1].cost, plan.LOCK_COST, "cost")
    T.eq(st.slots[1].custom, true, "custom flag"); T.eq(st.slots[2].custom, false, "vanilla flag")
    T.eq(st.skipped[1].name, "나쁜", "skipped"); T.truthy(st.skipped[1].reason:find("unknown unit", 1, true), "reason")
    T.eq(#s.calls.rebuild, 0, "list already right"); T.eq(#s.calls.inplace, 0, "nothing to write")
  end,
  the_lock_follows_the_setting_and_the_screen = function()
    local custom = { name = "토이박스", units = { "inf" }, cost = 3000, region = "gold", enabled = true }
    local locked = function() return { { name = "토이박스", cost = plan.LOCK_COST, units = { "inf" }, region = "gold" }, { name = "a", cost = 10 }, { name = "b", cost = 20 } } end
    local s = setup({ current = locked() })
    merc.tick(s.state, { enabled = true, refund = true, lockFromAi = false, companies = { custom } })
    T.eq(s.calls.inplace[1][1].cost, 3000, "lock off -> configured cost")
    s = setup({ current = locked(), screen = { open = true, confirming = false } })
    merc.tick(s.state, { enabled = true, refund = true, lockFromAi = true, companies = { custom } })
    T.eq(s.calls.inplace[1][1].cost, 3000, "screen open -> configured cost")
  end,
  tick_without_game_objects_is_a_noop = function()
    local s = setup({ current = {} })
    game.pawn = function() return nil end
    merc.tick(s.state, ON)
    T.eq(#s.calls.rebuild, 0, "no work"); T.eq(s.state.mercenaries, nil, "no status")
  end,
  disable_clears_the_status = function()
    local s = setup({ current = full() })
    merc.tick(s.state, ON)
    T.truthy(s.state.mercenaries, "status present")
    merc.disable(s.state, ON)
    T.eq(s.state.mercenaries, nil, "cleared")
  end,
  default_pick_returns_distinct_candidates = function()
    local picked = defaultPick({ "a", "b", "c", "d" }, 3)
    T.eq(#picked, 3, "count")
    local seen = {}
    for _, p in ipairs(picked) do
      T.eq(seen[p], nil, "distinct")
      seen[p] = true
    end
  end,
})
```

- [ ] **Step 2: 실패 확인**

Run: `dotnet test E:\MLToybox\panel\MLToybox.sln --filter "DisplayName~mercenaries_spec"`
Expected: FAIL, `module 'features.mercenaries' not found`.

- [ ] **Step 3: `mercenaries.lua` 구현**

`mod/MLToybox/Scripts/features/mercenaries.lua`:
```lua
local game = require("core.game")
local datatable = require("core.datatable")
local safe = require("core.safe")
local plan = require("features.merc_plan")
local list = require("features.merc_list")

-- 용병 고용 창 관리: 빈 칸 자동 보충, 커스텀 용병단 상시 등록, 플레이어 고용비 환급, AI 잠금 (spec 2026-09-30-mercenary-companies-design)
local M = { name = "mercenaries", intervalSec = 1, REBUILD_MIN_INTERVAL = 3, MISMATCH_RETRY = 10 }
M.clock = os.time

-- 새 순정 칸 고르기: 후보에서 중복 없이 count 개
M.pick = function(candidates, count)
  local pool, out = {}, {}
  for i, c in ipairs(candidates) do pool[i] = c end
  for _ = 1, count do out[#out + 1] = table.remove(pool, math.random(#pool)) end
  return out
end

local function fresh()
  return { baselineDone = false, refunded = 0, nextRebuild = 0, note = nil }
end

function M.enable(state)
  state.merc = fresh()
  state.mercenaries = nil
end

function M.disable(state)
  state.mercenaries = nil
end

-- 용병단 ID -> { mine = 플레이어 분대가 하나라도 있는가 }. 분대가 없는 용병단은 표에 없다
local function owners(engine, pawn)
  local out, addr = {}, pawn:GetAddress()
  local squads = engine.squads
  for i = 1, #squads do
    local squad = squads[i]
    local id = squad.companyID
    if id >= 0 then
      local o = out[id] or { mine = false }
      local owner = squad.ownerPawn
      if safe.valid(owner) and owner:GetAddress() == addr then o.mine = true end
      out[id] = o
    end
  end
  return out
end

-- 내 용병단의 고용비를 돌려주고 유지비를 0으로 만든다. 내 용병단 수와 AI 용병단 수를 돌려준다.
-- 환급을 켠 뒤 첫 점검은 환급 없이 0 으로만 만든다(켜기 전에 고용한 용병단). 그 뒤에 비용이 남은 내 용병단은 새 고용이다.
-- 치트 매니저가 없으면 비용을 그대로 둔다(0 으로 만들면 환급할 금액을 잃는다).
local function settle(st, settings, engine, pawn)
  local own = owners(engine, pawn)
  local mine, ai = 0, 0
  local refund = settings.refund == true
  local cheat = refund and game.cheat() or nil
  engine.hiredMercs:ForEach(function(k, v)
    local o = own[game.unwrap(k)]
    if not o then return end
    if not o.mine then
      ai = ai + 1
      return
    end
    mine = mine + 1
    local company = game.unwrap(v)
    if not refund or company.cost <= 0 then return end
    if not st.baselineDone then
      company.cost = 0
    elseif cheat then
      cheat:ChangeTreasury(company.cost)
      st.refunded = st.refunded + company.cost
      company.cost = 0
    end
  end)
  st.baselineDone = refund
  return mine, ai
end

function M.tick(state, settings)
  settings = settings or {}
  local pawn, engine = game.pawn(), game.engine()
  if not pawn or not engine then return end
  local st = state.merc
  if not st then
    st = fresh()
    state.merc = st
  end

  local mine, ai = settle(st, settings, engine, pawn)

  local rows = list.rows()
  local vanillaNames, regionKeys = {}, {}
  for _, r in ipairs(rows) do vanillaNames[r.name:lower()] = true end
  for _, r in ipairs(game.regionList()) do regionKeys[#regionKeys + 1] = r.key end
  local units = datatable.object("unitTemplates")
  local customs, skipped = plan.validate(settings.companies, {
    vanillaNames = vanillaNames,
    regionKeys = regionKeys,
    unitExists = function(u) return units:FindRow(u) ~= nil end,
  })

  -- 고용 확인 창이 떠 있는 동안 목록을 바꾸면 확인 중인 카드가 다른 용병단이 된다
  local screen = list.screenState()
  if not screen.confirming then
    local result = plan.build({
      rows = rows, hiredNames = list.hiredNames(engine), current = list.read(engine), customs = customs,
      screenOpen = screen.open, lockFromAi = settings.lockFromAi == true, canCopyRows = list.CAN_COPY_ROWS, pick = M.pick,
    })
    local now = M.clock()
    if result.action == "inplace" then
      list.applyInPlace(engine, result.slots)
      list.refreshScreen()
    elseif result.action == "rebuild" and now >= st.nextRebuild then
      local made = list.rebuild(engine, result.desired, result.renames)
      if made == #result.desired then
        st.note = nil
        st.nextRebuild = now + M.REBUILD_MIN_INTERVAL
      else
        st.note = string.format("rebuild produced %d of %d slots", made, #result.desired)
        st.nextRebuild = now + M.MISMATCH_RETRY
      end
      list.refreshScreen()
    end
  end

  local customNames, slots = {}, {}
  for _, c in ipairs(customs) do customNames[c.name] = true end
  for i, e in ipairs(list.read(engine)) do
    slots[i] = { name = e.name, cost = e.cost, custom = customNames[e.name] == true }
  end
  state.mercenaries = { slots = slots, hiredMine = mine, hiredAi = ai, refunded = st.refunded, skipped = skipped, note = st.note }
end

return M
```

- [ ] **Step 4: 통과 확인**

Run: `dotnet test E:\MLToybox\panel\MLToybox.sln --filter "DisplayName~mercenaries_spec"`
Expected: PASS.

- [ ] **Step 5: 기존 스펙을 새 동작에 맞게 고친다 (실패하게 만든다)**

`mod/MLToybox/tests/military_spec.lua`에서 세 군데를 바꾼다.

`enable_patches_templates_and_merc_table` 케이스 전체를 다음으로 바꾼다.
```lua
  enable_patches_templates_and_leaves_the_merc_table = function()
    local units, mercs = setup()
    military.enable({}, ALL)
    T.eq(#units.militiaFoot.requiredEquipment, 0, "equipment"); T.eq(units.militiaFoot.minHouseLv, 0, "house")
    T.eq(units.retinue_tier1.minMeleeTraining, 0, "melee"); T.eq(units.militiaFoot.minArcheryTraining, 0, "archery")
    T.eq(mercs.crazy_goose.cost, 90, "mercenary costs belong to features/mercenaries")
  end,
```

`tick_sets_squad_cap_and_zero_upkeep` 케이스 전체를 다음으로 바꾼다.
```lua
  tick_sets_squad_cap_and_recruit_cost_only = function()
    local _, _, hired, pawn = setup()
    military.tick({}, ALL)
    T.eq(pawn.maxNumOfMilitiaToSpawn, 99, "cap"); T.eq(pawn.recruitCost, 0, "recruit cost")
    T.eq(hired[1][2].cost, 50, "hired company 0 untouched"); T.eq(hired[2][2].cost, 60, "hired company 1 untouched")
  end,
```

`tick_respects_flags_off` 케이스의 마지막 줄을 다음으로 바꾼다.
```lua
    T.eq(pawn.maxNumOfMilitiaToSpawn, 6, "cap untouched"); T.eq(pawn.recruitCost, 25.0, "recruit cost untouched")
```

`mod/MLToybox/tests/entrypoints_spec.lua`의 `T.eq(#cfg.featureModules, 6, "six features")`를 다음으로 바꾼다.
```lua
    T.eq(#cfg.featureModules, 7, "seven features")
```

`mod/MLToybox/tests/registry_spec.lua`의 `status_includes_lord_values_when_present` 케이스 **앞**에 추가한다.
```lua
  status_includes_mercenaries_when_present = function()
    local r = registry.new(); r:add(fake("a")); r:setInGame(true)
    T.eq(r:status(1, nil, nil).mercenaries, nil, "absent")
    r.state.mercenaries = { hiredMine = 2 }
    T.eq(r:status(1, nil, nil).mercenaries.hiredMine, 2, "present")
  end,
```

- [ ] **Step 6: 실패 확인**

Run: `dotnet test E:\MLToybox\panel\MLToybox.sln --filter "FullyQualifiedName~LuaSpecTests"`
Expected: `military_spec.lua`(merc table cost 0), `entrypoints_spec.lua`(6 ≠ 7), `registry_spec.lua`(mercenaries nil) 세 개가 FAIL. 나머지는 PASS.

- [ ] **Step 7: 기존 모듈 수정**

`mod/MLToybox/Scripts/features/military.lua` 전체를 다음으로 바꾼다.
```lua
local datatable = require("core.datatable")
local game = require("core.game")

-- 주민 수를 넘는 징집은 네이티브 계층(spec §11)이, 용병 비용과 고용 창은 features/mercenaries.lua 가 담당한다
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
end

M.enable = M.apply
M.configure = M.apply

function M.tick(_, settings)
  local pawn = game.pawn()
  if not pawn then return end
  if settings.unlimitedSquads and pawn.maxNumOfMilitiaToSpawn < M.MAX_SQUADS then pawn.maxNumOfMilitiaToSpawn = M.MAX_SQUADS end
  -- zeroUpkeep 은 민병대 모집비만 0 으로 만든다
  if settings.zeroUpkeep and pawn.recruitCost ~= 0 then pawn.recruitCost = 0 end
end

return M
```

`mod/MLToybox/Scripts/config.lua`의 `featureModules` 줄을 다음으로 바꾼다.
```lua
  featureModules = { "resources", "lord", "build", "upgrade", "military", "mercenaries", "population" },
```

`mod/MLToybox/Scripts/core/registry.lua`의 `status()` 반환 표에서 `lord = st.lord,` 줄 **다음**에 추가한다.
```lua
      mercenaries = st.mercenaries,
```

- [ ] **Step 8: 전체 Lua 스펙 통과 확인**

Run: `dotnet test E:\MLToybox\panel\MLToybox.sln --filter "FullyQualifiedName~LuaSpecTests"`
Expected: 모든 스펙 PASS(`merc_plan_spec`, `merc_list_spec`, `mercenaries_spec` 포함).

- [ ] **Step 9: 커밋**

```bash
git -C E:/MLToybox add mod/MLToybox/Scripts mod/MLToybox/tests
git -C E:/MLToybox commit -m "feat(mercenaries): keep the hire list filled, refund player hires, lock customs from AI

military.zeroUpkeep now only zeroes the militia recruit cost.

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 6: `MLToybox.Panel.Core` — 설정, 상태, 검증 규칙

**Files:**
- Modify: `panel/MLToybox.Panel.Core/ControlDocument.cs`
- Modify: `panel/MLToybox.Panel.Core/StatusDocument.cs`
- Create: `panel/MLToybox.Panel.Core/MercCompanyRules.cs`
- Test: `panel/MLToybox.Tests/MercenaryTests.cs`

**Interfaces:**
- Consumes: 기존 `BridgeClient`(`SaveControl`, `LoadControl`, `ReadStatus`), `BridgeJson.Options`(camelCase), `UnitCatalog.Units`(`UnitOption(Id, Label)`). Task 1의 M2 결과(생성되지 않는 병종 id).
- Produces:
  - `FeaturesControl.Mercenaries : MercenariesControl { bool Enabled; bool Refund = true; bool LockFromAi = true; List<MercCompany> Companies }`
  - `MercCompany { string Name; List<string> Units; int Cost; string? Region; bool Enabled = true }`
  - `StatusDocument.Mercenaries : MercenaryStatus? { List<MercSlot>? Slots; int HiredMine; int HiredAi; int Refunded; List<MercSkipped>? Skipped; string? Note }`, `MercSlot { string Name; int Cost; bool Custom }`, `MercSkipped { string Name; string Reason }`
  - `MercCompanyRules`: `MaxSquads = 10`, `NameMax = 40`, `MaxEnabled = 3`, `VanillaNames`, `ExcludedUnits`, `Units`, `string? Validate(MercCompany c, IEnumerable<MercCompany> others)`, `string Summary(IEnumerable<string> units)`, `bool CanEnable(IEnumerable<MercCompany> all, MercCompany? target)`.

- [ ] **Step 1: 실패하는 테스트 작성**

`panel/MLToybox.Tests/MercenaryTests.cs`:
```csharp
using System.Text.Json;
using MLToybox.Panel.Core;
using Xunit;

namespace MLToybox.Tests;

public class MercenaryTests
{
    private static readonly DateTimeOffset Now = DateTimeOffset.FromUnixTimeSeconds(1_800_000_000);

    private static BridgeClient NewClient()
    {
        var dir = Path.Combine(Directory.CreateTempSubdirectory("mltb-merc-").FullName, "bridge");
        Directory.CreateDirectory(dir);
        return new BridgeClient(dir, () => Now);
    }

    private static MercCompany Company(string name = "토이박스 용병단") => new()
    {
        Name = name,
        Units = new List<string> { "mercenary_infantry", "mercenary_infantry", "mercenary_crossbowmen" },
        Cost = 3000,
        Region = "gold",
    };

    [Fact]
    public void SaveControl_WritesMercenariesProtocol()
    {
        var c = NewClient();
        var doc = new ControlDocument();
        doc.Features.Mercenaries.Enabled = true;
        doc.Features.Mercenaries.Companies.Add(Company());
        c.SaveControl(doc);
        using var json = JsonDocument.Parse(File.ReadAllText(c.ControlPath));
        var m = json.RootElement.GetProperty("features").GetProperty("mercenaries");
        Assert.True(m.GetProperty("enabled").GetBoolean());
        Assert.True(m.GetProperty("refund").GetBoolean());
        Assert.True(m.GetProperty("lockFromAi").GetBoolean());
        var company = m.GetProperty("companies")[0];
        Assert.Equal("토이박스 용병단", company.GetProperty("name").GetString());
        Assert.Equal(3, company.GetProperty("units").GetArrayLength());
        Assert.Equal("mercenary_crossbowmen", company.GetProperty("units")[2].GetString());
        Assert.Equal(3000, company.GetProperty("cost").GetInt32());
        Assert.Equal("gold", company.GetProperty("region").GetString());
        Assert.True(company.GetProperty("enabled").GetBoolean());
    }

    [Fact]
    public void SaveControl_OmitsRegionWhenNotChosen()
    {
        var c = NewClient();
        var doc = new ControlDocument();
        var company = Company();
        company.Region = null;
        doc.Features.Mercenaries.Companies.Add(company);
        c.SaveControl(doc);
        using var json = JsonDocument.Parse(File.ReadAllText(c.ControlPath));
        var saved = json.RootElement.GetProperty("features").GetProperty("mercenaries").GetProperty("companies")[0];
        Assert.False(saved.TryGetProperty("region", out _));
    }

    [Fact]
    public void LoadControl_WithoutMercenaries_UsesDefaults()
    {
        var c = NewClient();
        File.WriteAllText(c.ControlPath, """{"version":1,"seq":3,"features":{"military":{"enabled":true}}}""");
        var m = c.LoadControl().Features.Mercenaries;
        Assert.False(m.Enabled);
        Assert.True(m.Refund);
        Assert.True(m.LockFromAi);
        Assert.Empty(m.Companies);
    }

    [Fact]
    public void LoadControl_RoundTripsCompanies()
    {
        var c = NewClient();
        var doc = new ControlDocument();
        doc.Features.Mercenaries.Companies.Add(Company());
        c.SaveControl(doc);
        var loaded = c.LoadControl().Features.Mercenaries.Companies.Single();
        Assert.Equal("토이박스 용병단", loaded.Name);
        Assert.Equal(new[] { "mercenary_infantry", "mercenary_infantry", "mercenary_crossbowmen" }, loaded.Units);
        Assert.Equal(3000, loaded.Cost);
        Assert.Equal("gold", loaded.Region);
        Assert.True(loaded.Enabled);
    }

    [Fact]
    public void ReadStatus_ParsesMercenaries()
    {
        var c = NewClient();
        File.WriteAllText(c.StatusPath,
            """{"version":1,"heartbeat":1800000000,"inGame":true,"mercenaries":{"slots":[{"name":"토이박스 용병단","cost":10000000,"custom":true},{"name":"wayward_sons","cost":90,"custom":false}],"hiredMine":2,"hiredAi":3,"refunded":6000,"skipped":[{"name":"궁수대","reason":"unknown unit: foo"}],"note":"rebuild produced 1 of 3 slots"}}""");
        var m = c.ReadStatus()!.Mercenaries!;
        Assert.Equal(2, m.Slots!.Count);
        Assert.Equal("토이박스 용병단", m.Slots[0].Name);
        Assert.Equal(10_000_000, m.Slots[0].Cost);
        Assert.True(m.Slots[0].Custom);
        Assert.False(m.Slots[1].Custom);
        Assert.Equal(2, m.HiredMine);
        Assert.Equal(3, m.HiredAi);
        Assert.Equal(6000, m.Refunded);
        var skipped = m.Skipped!.Single();
        Assert.Equal("궁수대", skipped.Name);
        Assert.Equal("unknown unit: foo", skipped.Reason);
        Assert.Equal("rebuild produced 1 of 3 slots", m.Note);
    }

    [Fact]
    public void ReadStatus_ParsesEmptyMercenaryLists()
    {
        // 모드의 JSON 인코더는 빈 표를 [] 로 쓴다
        var c = NewClient();
        File.WriteAllText(c.StatusPath,
            """{"version":1,"heartbeat":1800000000,"inGame":true,"mercenaries":{"slots":[],"hiredMine":0,"hiredAi":0,"refunded":0,"skipped":[]}}""");
        var m = c.ReadStatus()!.Mercenaries!;
        Assert.Empty(m.Slots!);
        Assert.Empty(m.Skipped!);
        Assert.Null(m.Note);
    }

    [Fact]
    public void Rules_AcceptAGoodCompany() =>
        Assert.Null(MercCompanyRules.Validate(Company(), new List<MercCompany>()));

    [Fact]
    public void Rules_RejectBadCompanies()
    {
        var others = new List<MercCompany> { Company("기존 용병단") };
        string? V(Action<MercCompany> change)
        {
            var c = Company();
            change(c);
            return MercCompanyRules.Validate(c, others);
        }
        Assert.NotNull(V(c => c.Name = "   "));
        Assert.NotNull(V(c => c.Name = new string('가', 41)));
        Assert.Null(V(c => c.Name = new string('가', 40)));
        Assert.NotNull(V(c => c.Name = "Greencaps"));
        Assert.NotNull(V(c => c.Name = " 기존 용병단 "));
        Assert.NotNull(V(c => c.Units.Clear()));
        Assert.NotNull(V(c => c.Units = Enumerable.Repeat("mercenary_infantry", 11).ToList()));
        Assert.Null(V(c => c.Units = Enumerable.Repeat("mercenary_infantry", 10).ToList()));
        Assert.NotNull(V(c => c.Units.Add("dragon")));
        Assert.NotNull(V(c => c.Cost = -1));
        Assert.Null(V(c => c.Cost = 0));
    }

    [Fact]
    public void Rules_EditingACompanyDoesNotCollideWithItself()
    {
        var c = Company();
        Assert.Null(MercCompanyRules.Validate(c, new List<MercCompany> { c }));
    }

    [Fact]
    public void Rules_SummaryGroupsUnitsInFirstSeenOrder()
    {
        Assert.Equal("용병 - 보병 × 2, 용병 - 석궁병 × 1", MercCompanyRules.Summary(Company().Units));
        Assert.Equal("dragon × 1", MercCompanyRules.Summary(new[] { "dragon" }));
        Assert.Equal("", MercCompanyRules.Summary(Array.Empty<string>()));
    }

    [Fact]
    public void Rules_AtMostThreeEnabled()
    {
        var all = new List<MercCompany> { Company("1"), Company("2"), Company("3"), Company("4") };
        all[3].Enabled = false;
        Assert.False(MercCompanyRules.CanEnable(all, all[3]));
        Assert.False(MercCompanyRules.CanEnable(all, null));
        Assert.True(MercCompanyRules.CanEnable(all, all[0]));
        all[1].Enabled = false;
        Assert.True(MercCompanyRules.CanEnable(all, all[3]));
    }

    [Fact]
    public void Rules_VanillaNamesMatchTheGameTable()
    {
        Assert.Equal(11, MercCompanyRules.VanillaNames.Count);
        Assert.Contains("huntsmen", MercCompanyRules.VanillaNames);
        Assert.Contains("HILDEBOLTS_ARMY", MercCompanyRules.VanillaNames);
    }

    [Fact]
    public void Rules_UnitsLeaveOutTheOnesThatCannotBeHired()
    {
        var ids = MercCompanyRules.Units.Select(u => u.Id).ToList();
        Assert.Equal(UnitCatalog.Units.Count - MercCompanyRules.ExcludedUnits.Count, ids.Count);
        Assert.DoesNotContain(ids, id => MercCompanyRules.ExcludedUnits.Contains(id));
        Assert.Contains("mercenary_infantry", ids);
    }
}
```

- [ ] **Step 2: 실패 확인**

Run: `dotnet test E:\MLToybox\panel\MLToybox.sln --filter "FullyQualifiedName~MercenaryTests"`
Expected: 컴파일 오류(`MercCompany`, `MercCompanyRules` 등을 찾을 수 없음).

- [ ] **Step 3: `ControlDocument.cs` 수정**

`FeaturesControl` 클래스에서 `public MilitaryControl Military { get; set; } = new();` 줄 **다음**에 추가한다.
```csharp
    public MercenariesControl Mercenaries { get; set; } = new();
```

파일 끝(`MilitaryControl` 클래스 다음)에 추가한다.
```csharp

// 용병 고용 창 관리. 고용비는 원래 값을 유지하고(AI 도 같은 목록에서 고용한다) 플레이어 고용만 환급한다
public sealed class MercenariesControl
{
    public bool Enabled { get; set; }
    public bool Refund { get; set; } = true;       // 내 용병단 고용비 환급 + 유지비 0
    public bool LockFromAi { get; set; } = true;   // 커스텀 용병단은 고용 창이 닫혀 있는 동안 AI 가 살 수 없는 가격
    public List<MercCompany> Companies { get; set; } = new();
}

// 커스텀 용병단 정의. Units 는 분대마다 병종 id 하나(1~10개), Region 은 내 영지 키(null = 내 첫 영지)
public sealed class MercCompany
{
    public string Name { get; set; } = "";
    public List<string> Units { get; set; } = new();
    public int Cost { get; set; }
    [JsonIgnore(Condition = JsonIgnoreCondition.WhenWritingNull)] public string? Region { get; set; }
    public bool Enabled { get; set; } = true;
}
```

- [ ] **Step 4: `StatusDocument.cs` 수정**

`StatusDocument` 클래스에서 `public List<RegionInfo>? PlayerRegions { get; set; }` 줄 **다음**에 추가한다.
```csharp
    public MercenaryStatus? Mercenaries { get; set; }
```

파일 끝에 추가한다.
```csharp

public sealed class MercenaryStatus
{
    public List<MercSlot>? Slots { get; set; }
    public int HiredMine { get; set; }
    public int HiredAi { get; set; }
    public int Refunded { get; set; }   // 맵을 불러온 뒤의 환급 합계
    public List<MercSkipped>? Skipped { get; set; }
    public string? Note { get; set; }
}

public sealed class MercSlot
{
    public string Name { get; set; } = "";
    public int Cost { get; set; }
    public bool Custom { get; set; }
}

public sealed class MercSkipped
{
    public string Name { get; set; } = "";
    public string Reason { get; set; } = "";
}
```

- [ ] **Step 5: `MercCompanyRules.cs` 작성**

`panel/MLToybox.Panel.Core/MercCompanyRules.cs`:
```csharp
namespace MLToybox.Panel.Core;

// 커스텀 용병단 정의의 패널 쪽 검증. 모드도 같은 규칙으로 다시 검증한다(mod/MLToybox/Scripts/features/merc_plan.lua)
public static class MercCompanyRules
{
    public const int MaxSquads = 10;
    public const int NameMax = 40;
    public const int MaxEnabled = 3;

    // 용병 표(DT_MercenaryCompanies)의 Name 11개. findings "용병 고용 — 목록 보충과 커스텀 용병단"
    public static readonly IReadOnlySet<string> VanillaNames = new HashSet<string>(StringComparer.OrdinalIgnoreCase)
    {
        "brotherhood_of_the_forest", "crazy_goose", "brigands", "brigands_small", "wayward_sons", "greencaps",
        "vultures", "battle_brothers", "hildebolts_army", "hildebolts_army_large", "huntsmen",
    };

    // 용병 고용 경로로 생성되지 않는 병종 id. findings "용병 실측 M1~M5" 의 M2 에서 FAIL 인 것을 넣는다
    public static readonly IReadOnlySet<string> ExcludedUnits = new HashSet<string>();

    public static IReadOnlyList<UnitOption> Units => UnitCatalog.Units.Where(u => !ExcludedUnits.Contains(u.Id)).ToList();

    // 문제가 없으면 null, 있으면 사용자에게 보여 줄 이유. others 에 c 자신이 들어 있어도 된다
    public static string? Validate(MercCompany c, IEnumerable<MercCompany> others)
    {
        var name = c.Name.Trim();
        if (name.Length == 0) return "이름을 입력하세요.";
        if (name.Length > NameMax) return $"이름은 {NameMax}자 이하여야 합니다.";
        if (VanillaNames.Contains(name)) return "게임의 용병단 이름과 겹칩니다.";
        if (others.Any(o => !ReferenceEquals(o, c) && string.Equals(o.Name.Trim(), name, StringComparison.OrdinalIgnoreCase)))
            return "같은 이름의 용병단이 이미 있습니다.";
        if (c.Units.Count < 1 || c.Units.Count > MaxSquads) return $"분대는 1~{MaxSquads}개여야 합니다.";
        var known = Units.Select(u => u.Id).ToHashSet();
        var unknown = c.Units.FirstOrDefault(u => !known.Contains(u));
        if (unknown is not null) return $"쓸 수 없는 병종입니다: {unknown}";
        if (c.Cost < 0) return "고용비는 0 이상이어야 합니다.";
        return null;
    }

    // "용병 - 보병 × 2, 용병 - 석궁병 × 1" (처음 나온 순서). 모르는 병종 id 는 그대로 보여 준다
    public static string Summary(IEnumerable<string> units)
    {
        var labels = UnitCatalog.Units.ToDictionary(u => u.Id, u => u.Label);
        var order = new List<string>();
        var counts = new Dictionary<string, int>();
        foreach (var u in units)
        {
            if (!counts.ContainsKey(u))
            {
                counts[u] = 0;
                order.Add(u);
            }
            counts[u]++;
        }
        return string.Join(", ", order.Select(u => $"{labels.GetValueOrDefault(u, u)} × {counts[u]}"));
    }

    // target 을 사용으로 바꿔도 되는가(target 을 뺀 사용 수가 MaxEnabled 미만). target 이 null 이면 새 용병단
    public static bool CanEnable(IEnumerable<MercCompany> all, MercCompany? target) =>
        all.Count(c => c.Enabled && !ReferenceEquals(c, target)) < MaxEnabled;
}
```

- [ ] **Step 6: M2 결과 반영**

`analysis/findings.md`의 "용병 실측 M1~M5" 표에서 M2를 본다. FAIL인 병종 id가 있으면 `ExcludedUnits`의 초기값에 넣는다. 예를 들어 `retinue_tier3`와 `militia`가 FAIL이면:
```csharp
    public static readonly IReadOnlySet<string> ExcludedUnits = new HashSet<string> { "retinue_tier3", "militia" };
```
모두 PASS면 빈 집합 그대로 둔다.

- [ ] **Step 7: 통과 확인**

Run: `dotnet test E:\MLToybox\panel\MLToybox.sln --filter "FullyQualifiedName~MercenaryTests"`
Expected: PASS (13 tests).

- [ ] **Step 8: 커밋**

```bash
git -C E:/MLToybox add panel/MLToybox.Panel.Core panel/MLToybox.Tests/MercenaryTests.cs
git -C E:/MLToybox commit -m "feat(panel-core): mercenary settings, status and company rules

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 7: 패널 "용병" 탭

**Files:**
- Create: `panel/MLToybox.Panel/MercenaryTab.cs`
- Modify: `panel/MLToybox.Panel/MainForm.cs`

**Interfaces:**
- Consumes: Task 6의 `MercenariesControl`, `MercCompany`, `MercenaryStatus`, `MercSlot`, `MercSkipped`, `MercCompanyRules`. 기존 `StatusDocument.PlayerRegions`(`RegionInfo { Key, Name }`), `ScopeOption(string? Key, string Label)`, `UnitOption(Id, Label)`.
- Produces: `MercenaryTab : UserControl`의 `void LoadFrom(MercenariesControl control)`, `MercenariesControl Read()`, `void ShowStatus(StatusDocument? status)`, `event EventHandler? ApplyRequested`. `MainForm`이 이 네 개만 쓴다.

WinForms 화면은 자동 테스트가 없다. 이 태스크의 검증은 컴파일(`dotnet build`)이고, 화면 동작은 Task 8의 사용자 확인 항목으로 넘긴다.

- [ ] **Step 1: `MercenaryTab.cs` 작성**

`panel/MLToybox.Panel/MercenaryTab.cs`:
```csharp
using MLToybox.Panel.Core;

namespace MLToybox.Panel;

// [용병] 탭: 고용 창 관리 옵션과 커스텀 용병단 등록 (spec 2026-09-30-mercenary-companies-design §5.1)
// 옵션 체크박스는 하단 "적용"으로 반영하고, 등록·삭제·사용 체크는 ApplyRequested 로 바로 적용한다.
public sealed class MercenaryTab : UserControl
{
    public event EventHandler? ApplyRequested;

    // 편집 영역의 구성 목록 한 줄(병종 하나와 분대 수)
    private sealed record UnitGroup(string Id, int Count)
    {
        public override string ToString() => MercCompanyRules.Summary(Enumerable.Repeat(Id, Count));
    }

    private const string FirstRegionLabel = "내 첫 영지";

    private readonly CheckBox _enabled = new() { Text = "용병 기능 사용 (고용 창 자동 보충)", AutoSize = true };
    private readonly CheckBox _refund = new() { Text = "내 용병단 고용비 환급, 유지비 0", AutoSize = true };
    private readonly CheckBox _lock = new() { Text = "커스텀 용병단 AI 잠금 (고용 창을 열 때만 설정한 고용비)", AutoSize = true };
    private readonly DataGridView _grid = new()
    {
        Size = new Size(640, 120), AllowUserToAddRows = false, AllowUserToDeleteRows = false, AllowUserToResizeRows = false,
        RowHeadersVisible = false, MultiSelect = false, SelectionMode = DataGridViewSelectionMode.FullRowSelect,
        AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode.Fill,
    };
    private readonly TextBox _name = new() { Width = 260, MaxLength = MercCompanyRules.NameMax };
    private readonly ComboBox _unit = new() { DropDownStyle = ComboBoxStyle.DropDownList, Width = 180, DisplayMember = nameof(UnitOption.Label) };
    private readonly NumericUpDown _count = new() { Minimum = 1, Maximum = MercCompanyRules.MaxSquads, Value = 1, Width = 50 };
    private readonly ListBox _composition = new() { Width = 260, Height = 70 };
    private readonly Label _total = new() { AutoSize = true, Padding = new Padding(0, 6, 0, 0) };
    private readonly NumericUpDown _cost = new() { Minimum = 0, Maximum = 10_000_000, Value = 1000, Width = 110 };
    private readonly ComboBox _region = new() { DropDownStyle = ComboBoxStyle.DropDownList, Width = 220 };
    private readonly Label _error = new() { AutoSize = true, ForeColor = Color.Firebrick };
    private readonly Label _status = new() { AutoSize = true };

    private List<MercCompany> _companies = new();
    private MercCompany? _editing;                 // null = 새 용병단
    private List<string> _editUnits = new();
    private string _regionsKey = "";
    private bool _loading;

    public MercenaryTab()
    {
        _grid.Columns.Add(new DataGridViewCheckBoxColumn { Name = "Use", HeaderText = "사용", FillWeight = 12 });
        _grid.Columns.Add(new DataGridViewTextBoxColumn { Name = "Name", HeaderText = "이름", ReadOnly = true, FillWeight = 30 });
        _grid.Columns.Add(new DataGridViewTextBoxColumn { Name = "Units", HeaderText = "구성", ReadOnly = true, FillWeight = 44 });
        _grid.Columns.Add(new DataGridViewTextBoxColumn { Name = "Cost", HeaderText = "고용비", ReadOnly = true, FillWeight = 16 });
        _grid.Columns.Add(new DataGridViewTextBoxColumn { Name = "Region", HeaderText = "도착 영지", ReadOnly = true, FillWeight = 24 });
        // 체크박스는 셀을 떠나기 전에는 값이 확정되지 않는다. 누르는 즉시 확정시킨다
        _grid.CurrentCellDirtyStateChanged += (_, _) =>
        {
            if (_grid.IsCurrentCellDirty) _grid.CommitEdit(DataGridViewDataErrorContexts.Commit);
        };
        _grid.CellValueChanged += (_, e) => OnUseChanged(e.RowIndex, e.ColumnIndex);
        _grid.SelectionChanged += (_, _) => OnRowSelected();

        _unit.Items.AddRange(MercCompanyRules.Units.Cast<object>().ToArray());
        var infantry = MercCompanyRules.Units.ToList().FindIndex(u => u.Id == "mercenary_infantry");
        if (_unit.Items.Count > 0) _unit.SelectedIndex = Math.Max(0, infantry);
        _region.Items.Add(new ScopeOption(null, FirstRegionLabel));
        _region.SelectedIndex = 0;

        var newButton = new Button { Text = "새 용병단", AutoSize = true };
        newButton.Click += (_, _) => StartNew();
        var deleteButton = new Button { Text = "삭제", AutoSize = true };
        deleteButton.Click += (_, _) => DeleteSelected();
        var addUnit = new Button { Text = "추가", AutoSize = true };
        addUnit.Click += (_, _) => AddUnits();
        var removeUnit = new Button { Text = "선택 병종 빼기", AutoSize = true };
        removeUnit.Click += (_, _) => RemoveUnits();
        var register = new Button { Text = "등록", AutoSize = true };
        register.Click += (_, _) => Register();

        var root = new FlowLayoutPanel
        {
            Dock = DockStyle.Fill, FlowDirection = FlowDirection.TopDown, WrapContents = false, AutoScroll = true, Padding = new Padding(10),
        };
        root.Controls.AddRange(new Control[]
        {
            _enabled, _refund, _lock,
            Caption($"등록한 용병단 (사용 최대 {MercCompanyRules.MaxEnabled}개)"),
            _grid,
            Row(newButton, deleteButton),
            Caption("용병단 편집"),
            Row(Field("이름"), _name),
            Row(Field("분대 추가"), _unit, _count, Field("개 분대"), addUnit),
            Row(Field("구성"), _composition, removeUnit, _total),
            Row(Field("고용비"), _cost),
            Row(Field("도착 영지"), _region),
            Row(register, _error),
            _status,
        });
        Controls.Add(root);
        ClearEditor();
    }

    public void LoadFrom(MercenariesControl control)
    {
        _enabled.Checked = control.Enabled;
        _refund.Checked = control.Refund;
        _lock.Checked = control.LockFromAi;
        _companies = control.Companies.Select(Clone).ToList();
        ClearEditor();
        RebuildGrid(null);
    }

    public MercenariesControl Read() => new()
    {
        Enabled = _enabled.Checked,
        Refund = _refund.Checked,
        LockFromAi = _lock.Checked,
        Companies = _companies.Select(Clone).ToList(),
    };

    public void ShowStatus(StatusDocument? status)
    {
        var regions = status is { InGame: true } ? status.PlayerRegions ?? new List<RegionInfo>() : new List<RegionInfo>();
        var key = string.Join("|", regions.Select(r => $"{r.Key}={r.Name}"));
        if (key != _regionsKey)
        {
            _regionsKey = key;
            var selected = (_region.SelectedItem as ScopeOption)?.Key;
            var options = new List<ScopeOption> { new(null, FirstRegionLabel) };
            options.AddRange(regions.Select(r => new ScopeOption(r.Key, $"{r.Name} ({r.Key})")));
            _region.BeginUpdate();
            _region.Items.Clear();
            _region.Items.AddRange(options.Cast<object>().ToArray());
            _region.EndUpdate();
            _region.SelectedIndex = Math.Max(0, options.FindIndex(o => o.Key == selected));
            RebuildGrid(_editing);   // 영지 이름 표시를 새 목록으로 갱신
        }
        _status.Text = StatusText(status);
    }

    private static string StatusText(StatusDocument? status)
    {
        var m = status is { InGame: true } ? status.Mercenaries : null;
        if (m is null) return "고용 창: - (용병 기능이 꺼져 있거나 게임 밖)";
        var slots = m.Slots is null || m.Slots.Count == 0
            ? "(비어 있음)"
            : string.Join(", ", m.Slots.Select(s => $"{s.Name}{(s.Custom ? "(커스텀)" : "")} {s.Cost:N0}"));
        var lines = new List<string>
        {
            $"고용 창: {slots}",
            $"고용 중: 내 용병단 {m.HiredMine}개, AI {m.HiredAi}개 · 맵을 불러온 뒤 환급 {m.Refunded:N0}",
        };
        foreach (var s in m.Skipped ?? new List<MercSkipped>()) lines.Add($"띄우지 못함: {s.Name} — {s.Reason}");
        if (!string.IsNullOrEmpty(m.Note)) lines.Add($"참고: {m.Note}");
        return string.Join(Environment.NewLine, lines);
    }

    private static MercCompany Clone(MercCompany c) => new()
    {
        Name = c.Name, Units = new List<string>(c.Units), Cost = c.Cost, Region = c.Region, Enabled = c.Enabled,
    };

    private static Label Caption(string text) => new()
    {
        Text = text, AutoSize = true, Font = new Font(SystemFonts.DefaultFont, FontStyle.Bold), Margin = new Padding(3, 12, 3, 3),
    };

    private static Label Field(string text) => new() { Text = text, AutoSize = true, Padding = new Padding(0, 6, 0, 0) };

    private static FlowLayoutPanel Row(params Control[] controls)
    {
        var row = new FlowLayoutPanel { AutoSize = true, WrapContents = false };
        row.Controls.AddRange(controls);
        return row;
    }

    private string RegionLabel(string? key)
    {
        if (key is null) return FirstRegionLabel;
        var option = _region.Items.OfType<ScopeOption>().FirstOrDefault(o => o.Key == key);
        return option?.Label ?? key;
    }

    private void SelectRegion(string? key)
    {
        var index = _region.Items.OfType<ScopeOption>().ToList().FindIndex(o => o.Key == key);
        _region.SelectedIndex = Math.Max(0, index);
    }

    private void RebuildGrid(MercCompany? select)
    {
        _loading = true;
        _grid.Rows.Clear();
        foreach (var c in _companies)
        {
            var i = _grid.Rows.Add(c.Enabled, c.Name, MercCompanyRules.Summary(c.Units), c.Cost.ToString("N0"), RegionLabel(c.Region));
            _grid.Rows[i].Tag = c;
        }
        _grid.ClearSelection();
        foreach (DataGridViewRow row in _grid.Rows)
            if (ReferenceEquals(row.Tag, select)) row.Selected = true;
        _loading = false;
    }

    private void OnRowSelected()
    {
        if (_loading || _grid.SelectedRows.Count == 0) return;
        if (_grid.SelectedRows[0].Tag is MercCompany c) LoadEditor(c);
    }

    private void OnUseChanged(int rowIndex, int columnIndex)
    {
        if (_loading || rowIndex < 0 || columnIndex != 0) return;
        var row = _grid.Rows[rowIndex];
        if (row.Tag is not MercCompany c) return;
        var want = row.Cells[0].Value is true;
        if (want && !MercCompanyRules.CanEnable(_companies, c))
        {
            _loading = true;
            row.Cells[0].Value = false;
            _grid.RefreshEdit();
            _loading = false;
            _error.Text = $"사용은 최대 {MercCompanyRules.MaxEnabled}개입니다.";
            return;
        }
        c.Enabled = want;
        _error.Text = "";
        ApplyRequested?.Invoke(this, EventArgs.Empty);
    }

    private void LoadEditor(MercCompany c)
    {
        _editing = c;
        _name.Text = c.Name;
        _editUnits = new List<string>(c.Units);
        _cost.Value = Math.Clamp(c.Cost, 0, (int)_cost.Maximum);
        SelectRegion(c.Region);
        _error.Text = "";
        RefreshComposition();
    }

    private void ClearEditor()
    {
        _editing = null;
        _name.Text = "";
        _editUnits = new List<string>();
        _cost.Value = 1000;
        SelectRegion(null);
        _error.Text = "";
        RefreshComposition();
    }

    private void RefreshComposition()
    {
        _composition.Items.Clear();
        foreach (var id in _editUnits.Distinct())
            _composition.Items.Add(new UnitGroup(id, _editUnits.Count(u => u == id)));
        _total.Text = $"합계 {_editUnits.Count} / {MercCompanyRules.MaxSquads}";
    }

    private void StartNew()
    {
        ClearEditor();
        _loading = true;
        _grid.ClearSelection();
        _loading = false;
        _name.Focus();
    }

    private void AddUnits()
    {
        if (_unit.SelectedItem is not UnitOption unit) return;
        var count = (int)_count.Value;
        if (_editUnits.Count + count > MercCompanyRules.MaxSquads)
        {
            _error.Text = $"분대는 {MercCompanyRules.MaxSquads}개까지입니다.";
            return;
        }
        _editUnits.AddRange(Enumerable.Repeat(unit.Id, count));
        _error.Text = "";
        RefreshComposition();
    }

    private void RemoveUnits()
    {
        if (_composition.SelectedItem is not UnitGroup group) return;
        _editUnits.RemoveAll(u => u == group.Id);
        RefreshComposition();
    }

    private void Register()
    {
        var isNew = _editing is null;
        var candidate = new MercCompany
        {
            Name = _name.Text.Trim(),
            Units = new List<string>(_editUnits),
            Cost = (int)_cost.Value,
            Region = (_region.SelectedItem as ScopeOption)?.Key,
            Enabled = _editing?.Enabled ?? MercCompanyRules.CanEnable(_companies, null),
        };
        var error = MercCompanyRules.Validate(candidate, _companies.Where(c => !ReferenceEquals(c, _editing)));
        if (error is not null)
        {
            _error.Text = error;
            return;
        }
        if (_editing is null)
        {
            _companies.Add(candidate);
            _editing = candidate;
        }
        else
        {
            _editing.Name = candidate.Name;
            _editing.Units = candidate.Units;
            _editing.Cost = candidate.Cost;
            _editing.Region = candidate.Region;
        }
        _error.Text = isNew && !candidate.Enabled ? $"사용 중인 용병단이 {MercCompanyRules.MaxEnabled}개라 '사용'을 끈 채로 등록했습니다." : "";
        RebuildGrid(_editing);
        ApplyRequested?.Invoke(this, EventArgs.Empty);
    }

    private void DeleteSelected()
    {
        if (_editing is null) return;
        _companies.Remove(_editing);
        ClearEditor();
        RebuildGrid(null);
        ApplyRequested?.Invoke(this, EventArgs.Empty);
    }
}
```

- [ ] **Step 2: `MainForm.cs` 연결**

여섯 군데를 고친다.

1. `_zeroUpkeep` 선언의 `Text`를 바꾼다.
```csharp
    private readonly CheckBox _zeroUpkeep = new() { Text = "민병대 모집비 0", AutoSize = true };
```

2. `_unlimitedSquads` 선언 **다음 줄**에 추가한다.
```csharp
    private readonly MercenaryTab _mercTab = new() { Dock = DockStyle.Fill };
```

3. 생성자에서 `tabs.TabPages.Add(Page("군사", _milEnabled, _ignoreEquipment, _ignorePopulation, _zeroUpkeep, _unlimitedSquads, spawnRow, reformRow));` 줄 **다음**에 추가한다.
```csharp
        var mercPage = new TabPage("용병");
        mercPage.Controls.Add(_mercTab);
        tabs.TabPages.Add(mercPage);
        _mercTab.ApplyRequested += (_, _) => Apply();
```

4. `LoadControlIntoUi()`에서 `_unlimitedSquads.Checked = f.Military.UnlimitedSquads;` 줄 **다음**에 추가한다.
```csharp
        _mercTab.LoadFrom(f.Mercenaries);
```

5. `Apply()`에서 `f.Military.UnlimitedSquads = _unlimitedSquads.Checked;` 줄 **다음**에 추가한다.
```csharp
        f.Mercenaries = _mercTab.Read();
```

6. `RefreshStatus()`에서 `RefreshSpawnRegions(status);` 줄 **다음**에 추가한다(`status`가 null일 때도 불리는 위치다).
```csharp
        _mercTab.ShowStatus(status);
```

- [ ] **Step 3: 빌드와 테스트**

Run: `dotnet build E:\MLToybox\panel\MLToybox.sln`
Expected: `오류 0개`(경고는 기존 수준).

Run: `dotnet test E:\MLToybox\panel\MLToybox.sln`
Expected: 전부 PASS.

- [ ] **Step 4: 커밋**

```bash
git -C E:/MLToybox add panel/MLToybox.Panel/MercenaryTab.cs panel/MLToybox.Panel/MainForm.cs
git -C E:/MLToybox commit -m "feat(panel): mercenary tab for hire list options and custom companies

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 8: 배포, 인게임 검증, 문서, 병합

**Files:**
- Modify: `README.md`, `docs/CHANGELOG.md`, `analysis/findings.md`

**Interfaces:**
- Consumes:
  - Task 1의 `tools/lab.ps1 -Vars`, `tools/lab-load.ps1`, `tools/lab/merc_state.lua`(`available`, `slot`, `hired`, `treasury`, `screen`, `rows` 줄), `tools/lab/merc_screen.lua`(`ACTION`), `tools/lab/merc_hire.lua`(`NAME`, 마지막 줄 `HIRED`/`NOT_HIRED`).
  - Task 5의 설정 `features.mercenaries`와 상태 `status.json`의 `mercenaries`, `features.mercenaries.active`/`lastError`.
- Produces: 검증 결과가 적힌 문서, `develop`에 병합된 브랜치.

- [ ] **Step 1: 배포와 검증 준비**

게임과 패널이 꺼져 있어야 한다. 셸 상태는 도구 호출 사이에 유지되지 않으므로, 검증용 변수와 함수를 파일 하나에 두고 각 Step의 명령 맨 앞에서 불러온다.

`analysis/dumps/probes/merc-verify.ps1`(커밋하지 않음)을 Write 도구로 만든다.
```powershell
. E:\MLToybox\tools\common.ps1
$lab = 'E:\MLToybox\tools\lab.ps1'
$t = 'E:\MLToybox\tools\lab'
$bridge = Join-Path (Get-MLModsDir) 'MLToybox\bridge'
$control = Join-Path $bridge 'control.json'

# 다른 기능을 모두 끄고 용병 설정만 바꿔 쓴다(국고 유지 등이 환급 확인을 흐리지 않게)
function Set-Merc([hashtable]$merc) {
    $doc = Get-Content -Raw $control | ConvertFrom-Json -AsHashtable
    foreach ($k in @($doc.features.Keys)) { $doc.features[$k].enabled = $false }
    $doc.features.mercenaries = $merc
    $doc.seq = [long]$doc.seq + 1
    $doc.commands = @()
    $doc | ConvertTo-Json -Depth 20 | Set-Content "$control.tmp" -Encoding utf8NoBOM
    Move-Item -Force "$control.tmp" $control
}
function State { & $lab -File "$t\merc_state.lua" }
function Status { Get-Content -Raw (Join-Path $bridge 'status.json') | ConvertFrom-Json }
$custom = @{ name = '토이박스 용병단'; units = @('mercenary_infantry', 'mercenary_crossbowmen'); cost = 3000; region = $null; enabled = $true }
```

그다음 실행한다.
```powershell
Get-Process -Name 'ManorLords-Win64-Shipping', 'MLToybox.Panel' -ErrorAction SilentlyContinue | Stop-Process -Force
pwsh E:\MLToybox\tools\backup-saves.ps1
pwsh E:\MLToybox\tools\deploy.ps1 -Mod MLToybox
pwsh E:\MLToybox\tools\deploy.ps1 -Mod MLToybox -Panel
pwsh E:\MLToybox\tools\deploy.ps1 -Mod MLToyboxLab

. E:\MLToybox\analysis\dumps\probes\merc-verify.ps1
Copy-Item $control "$control.before-merc-test" -Force
Set-Merc @{ enabled = $true; refund = $true; lockFromAi = $true; companies = @() }
& E:\MLToybox\tools\lab-load.ps1 -Slot saveGame_8 -Start

# saveGame_8 은 국고가 적다. 고용비를 낼 수 있게 국고를 올려 둔다(저장하지 않으므로 세이브에는 남지 않는다)
$tmp = Join-Path $env:TEMP 'mltb-treasury.lua'
Set-Content $tmp 'FindFirstOf("MLCheatManager_C"):ChangeTreasury(100000) print("treasury +100000")' -Encoding utf8NoBOM
& $lab -File $tmp
```
Expected: `Loaded saveGame_8`과 `treasury +100000`.

**Step 2~8의 각 코드 블록은 맨 앞에 `. E:\MLToybox\analysis\dumps\probes\merc-verify.ps1` 줄을 붙여 실행한다.**

- [ ] **Step 2: 자동 보충**

```powershell
Start-Sleep -Seconds 5
State
(Status).features.mercenaries
```
통과 조건: `available 3`. `slot` 줄 세 개의 `cost`가 `rows` 줄에 나온 같은 이름의 값과 같고 0보다 크다. `features.mercenaries`가 `active=True`, `lastError`가 비어 있다.

- [ ] **Step 3: 커스텀 등록과 AI 잠금**

```powershell
Set-Merc @{ enabled = $true; refund = $true; lockFromAi = $true; companies = @($custom) }
Start-Sleep -Seconds 5
State | Select-String '^(available|slot)'
& $lab -File "$t\merc_screen.lua" -Vars @{ ACTION = 'open' }
Start-Sleep -Seconds 2
State | Select-String '^(slot|screen)'
```
통과 조건: 첫 출력에서 `available 3`이고 `slot 1`이 `name=토이박스 용병단 cost=10000000 arrivesIn=1`, `units=[mercenary_infantry,mercenary_crossbowmen]`, `traits=[]`, `region`이 내 영지 이름이다. 고용 창을 연 뒤에는 `slot 1`의 `cost=3000`, `screen visible=true`다.

- [ ] **Step 4: 고용, 환급, 상시 유지 (3회)**

```powershell
Start-Sleep -Seconds 10   # 국고 HUD 값이 자리 잡을 때까지
State | Select-String '^treasury'
1..3 | ForEach-Object {
    & $lab -File "$t\merc_hire.lua" -Vars @{ NAME = '토이박스 용병단' } | Select-Object -Last 1
    Start-Sleep -Seconds 6
    State | Select-String '^(available|slot\s+1|hired)'
}
Start-Sleep -Seconds 15
State | Select-String '^treasury'
(Status).mercenaries | Select-Object hiredMine, hiredAi, refunded
```
통과 조건: 세 번 모두 `HIRED`. 매번 `available 3`이고 `slot 1`이 다시 `토이박스 용병단`이다. `hired ... name=토이박스 용병단 cost=0 squads=2 mine=2` 줄이 세 개가 되고, 각 줄의 `units`에 병사 수가 0보다 큰 분대가 둘 있다. 마지막 `treasury`가 처음 값과 같다. `refunded`가 9000, `hiredMine`이 3이다.

- [ ] **Step 5: 잠금 복귀와 AI 관찰 (5분)**

```powershell
& $lab -File "$t\merc_screen.lua" -Vars @{ ACTION = 'close' }
Start-Sleep -Seconds 3
State | Select-String '^(slot\s+1|screen)'
1..10 | ForEach-Object { Start-Sleep -Seconds 30; State | Select-String '^(slot\s+1|hired)' | Out-String }
```
통과 조건: 닫은 뒤 `slot 1`의 `cost=10000000`, `screen visible=false`. 5분 동안 `slot 1`이 계속 `토이박스 용병단`이고, `name=토이박스 용병단`인 `hired` 줄 가운데 `mine=0`인 것이 생기지 않는다.

- [ ] **Step 6: 순정 전부 고용**

```powershell
& $lab -File "$t\merc_screen.lua" -Vars @{ ACTION = 'open' }
for ($i = 0; $i -lt 12; $i++) {
    Start-Sleep -Seconds 5
    $slot = State | Where-Object { $_ -match '^slot' -and $_ -notmatch '토이박스' } | Select-Object -First 1
    if (-not $slot) { break }
    $name = [regex]::Match($slot, 'name=(\S+)').Groups[1].Value
    & $lab -File "$t\merc_hire.lua" -Vars @{ NAME = $name } | Select-Object -Last 1
}
Start-Sleep -Seconds 5
State
(Status).mercenaries | Select-Object refunded, note
```
통과 조건: 순정 9개가 차례로 `HIRED`된다. 끝난 뒤 `available 1`이고 남은 칸이 `토이박스 용병단`이다. `rows` 줄의 이름 11개에 `#mlt`가 없다. `hired` 줄에 순정 이름 9개가 모두 있고 `mine`이 0보다 크며 `cost=0`이다. `note`가 비어 있다.

- [ ] **Step 7: 안정성, 다시 불러오기, 끄기**

```powershell
(Status).features.mercenaries
Select-String -Path (Join-Path (Get-MLWin64Dir) 'ue4ss\UE4SS.log') -Pattern 'mercenaries' | Select-Object -Last 5
Get-ChildItem (Join-Path $env:LOCALAPPDATA 'ManorLords\Saved\Crashes') -Directory | Sort-Object LastWriteTime -Descending | Select-Object -First 1 Name, LastWriteTime

Get-Process -Name 'ManorLords-Win64-Shipping' | Stop-Process -Force
Start-Sleep -Seconds 5
& E:\MLToybox\tools\lab-load.ps1 -Slot saveGame_8 -Start
Start-Sleep -Seconds 5
State | Select-String '^(available|slot|rows)'

Set-Merc @{ enabled = $false; refund = $true; lockFromAi = $true; companies = @($custom) }
Start-Sleep -Seconds 4
State | Select-String '^(available|slot)'
(Status).mercenaries
```
통과 조건: `features.mercenaries`가 `active=True`이고 `lastError`가 비어 있다. `UE4SS.log`에 `mercenaries` 오류 줄이 없다. 가장 최근 크래시 폴더의 시각이 이 검증을 시작하기 전이다. 다시 불러온 뒤 `available 3`이고 `slot 1`이 `토이박스 용병단`, `rows`에 `#mlt`가 없다. 기능을 끈 뒤에도 목록이 그대로이고(되돌리지 않음) `(Status).mercenaries`가 비어 있다.

- [ ] **Step 8: 정리**

```powershell
Get-Process -Name 'ManorLords-Win64-Shipping' | Stop-Process -Force
Copy-Item "$control.before-merc-test" $control -Force
Remove-Item "$control.before-merc-test"
pwsh E:\MLToybox\tools\deploy.ps1 -Mod MLToyboxLab -Remove
```
게임을 저장 없이 종료했으므로 `saveGame_8`에는 변화가 없다. `control.json`은 검증 전 내용으로 돌아간다.

- [ ] **Step 9: `analysis/findings.md`에 검증 결과 기록**

파일 끝에 추가하고, 결과 칸을 Step 2~7의 실제 결과로 채운다(통과하지 못한 항목은 관찰한 출력을 적는다).

```markdown
## 용병 기능 인게임 검증 (saveGame_8)
| 항목 | 결과 | 근거 |
|---|---|---|
| 자동 보충 | (통과 또는 실패) | (available, slot 줄) |
| 커스텀 등록 | (통과 또는 실패) | (slot 1 줄) |
| AI 잠금 | (통과 또는 실패) | (닫힘·열림 때 slot 1 의 cost, 5분 관찰) |
| 고용·상시 유지 | (통과 또는 실패) | (HIRED 3회, hired 줄) |
| 환급 | (통과 또는 실패) | (treasury 전후, refunded) |
| 전부 고용 | (통과 또는 실패) | (available 1, rows 줄) |
| 안정성·다시 불러오기 | (통과 또는 실패) | (active, lastError, 크래시 폴더 시각) |
| 끄기 | (통과 또는 실패) | (끈 뒤 slot 줄) |
```
통과하지 못한 항목이 있으면 여기서 멈추고 `superpowers:systematic-debugging`으로 원인을 찾는다. 병합하지 않는다.

- [ ] **Step 10: README와 CHANGELOG**

`README.md`에서 다음을 고친다.

"### 군사" 절의 첫 항목을 바꾼다.
```markdown
- 부대 수 상한을 99로 올리고, 민병대 모집비를 0으로 만들고, 장비·훈련·집 레벨 요구를 풉니다. 용병 비용은 [용병] 탭이 맡습니다.
```

"### 인구 (영지별)" 절 **앞**에 새 절을 넣는다.
```markdown
### 용병
- **고용 창 자동 보충**: 게임은 용병 고용 창을 새 게임을 시작할 때 한 번만 채웁니다. 모드는 빈 칸을 고용 중이 아닌 순정 용병단으로 채웁니다(최대 3칸).
- **커스텀 용병단**: 이름, 분대 구성(1~10개), 고용비, 도착 영지를 정해 등록하면 고용 창에 카드로 뜹니다.
  - 고용해도 같은 카드가 다시 채워지므로 몇 번이든 고용할 수 있습니다.
  - 사용 중인 것은 최대 3개이고 앞 칸을 차지합니다. 순정 용병단이 모두 고용 중이어도 뜹니다.
  - 깃발과 색은 덮어쓴 칸의 것을 물려받습니다.
- **환급**: 고용비는 원래 값을 유지합니다. AI 영주도 같은 목록에서 고용하고, 국고가 고용비 이상이면 고용하기 때문입니다. 내가 고용한 용병단만 고용비를 국고에 돌려주고 유지비를 0으로 만듭니다.
- **AI 잠금**: 커스텀 카드의 고용비를 고용 창이 닫혀 있는 동안 10,000,000으로 두고, 창을 열면 설정한 고용비로 바꿉니다. 창을 연 직후 최대 1초 동안 잠금 가격이 보일 수 있습니다.
- 목록을 다시 만들 때마다 게임의 "새 용병단" 알림이 한 번 뜹니다.
```
`findings.md`의 M3가 PASS면 "커스텀 용병단" 항목의 하위 목록에 `  - 고용한 뒤 부대 패널의 용병단 표시에도 입력한 이름이 쓰입니다.`를 더한다. `CAN_COPY_ROWS`가 `false`면 "고용 창 자동 보충" 항목 끝에 ` 다른 칸이 바뀌면 순정 카드도 함께 바뀔 수 있습니다.`를 붙인다.

"데이터 표(업그레이드·건물·유닛·용병) 변경은" 문장의 괄호를 `(업그레이드·건물·유닛)`으로 바꾼다.

"## 개발 도구"의 "인게임 Lua 실행기" 목록 끝에 항목을 더한다.
```markdown
  4. `& tools/lab-load.ps1 -Slot saveGame_8 -Start`는 게임을 켜고 그 세이브를 불러옵니다(화면 조작 불필요). `tools/lab/`에는 용병 상태 읽기(`merc_state.lua`), 고용 창 열기·닫기(`merc_screen.lua`), 카드 고용(`merc_hire.lua`) 스크립트가 있습니다. `lab.ps1 -Vars @{ NAME = '...' }`는 스크립트의 `__NAME__`을 바꿔 넣습니다.
```

"## 주의" 절 끝에 항목을 더한다.
```markdown
- 용병 고용 창 목록과 고용한 커스텀 용병단은 세이브에 남습니다. AI 잠금을 켠 채 저장한 세이브에는 커스텀 카드가 10,000,000 고용비로 남습니다.
```

`docs/CHANGELOG.md`의 첫 `##` 제목 **앞**에 넣는다. 날짜는 병합하는 날로 쓴다.
```markdown
## 2026-09-30 — 용병
### 추가
- **용병 탭**: 고용 창의 빈 칸을 순정 용병단으로 자동 보충합니다. 패널에서 등록한 커스텀 용병단(이름, 분대 구성, 고용비, 도착 영지)이 고용 창에 상시 뜨고, 고용하면 다시 채워집니다. 순정 용병단이 모두 고용 중이어도 커스텀은 뜹니다.
- **플레이어 환급**: 내가 고용한 용병단의 고용비를 국고에 돌려주고 유지비를 0으로 만듭니다. AI가 고용한 용병단은 건드리지 않습니다.
- **AI 잠금**: 커스텀 용병단은 고용 창이 닫혀 있는 동안 AI가 살 수 없는 가격으로 둡니다.
- 개발 도구: `tools/lab-load.ps1`(세이브 자동 로드), `tools/lab/`(용병 상태·고용 스크립트), `lab.ps1 -Vars`.
### 변경
- **군사 탭 "용병 비용·모집비 0"이 "민병대 모집비 0"이 됐습니다.** 예전에는 용병 표의 고용비를 0으로 만들어 AI 영주도 공짜로 고용했습니다. 용병을 무료로 쓰려면 [용병] 탭의 환급을 켜세요.
```

- [ ] **Step 11: 전체 확인**

```powershell
dotnet test E:\MLToybox\panel\MLToybox.sln
dotnet build E:\MLToybox\panel\MLToybox.sln
pwsh E:\MLToybox\tools\tests\Tools.Tests.ps1
```
Expected: 테스트 전부 PASS, 빌드 `오류 0개`, 마지막 줄 `ALL PASS`.

- [ ] **Step 12: 커밋, 병합, 푸시**

```bash
git -C E:/MLToybox add README.md docs/CHANGELOG.md analysis/findings.md
git -C E:/MLToybox commit -m "docs: mercenary tab, in-game verification results and changelog

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
git -C E:/MLToybox checkout develop
git -C E:/MLToybox merge --no-ff feat/mercenary-companies -m "Merge branch 'feat/mercenary-companies' into develop"
git -C E:/MLToybox push origin develop
git -C E:/MLToybox log --oneline -3
```
Expected: 맨 위가 `Merge branch 'feat/mercenary-companies' into develop`이고 푸시가 성공한다.

- [ ] **Step 13: 사용자에게 넘길 확인 항목 보고**

자동으로 확인하지 못한 것을 최종 보고에 적는다.
- 패널 [용병] 탭 화면: 등록, 목록에서 골라 고치기, 삭제, 사용 체크(네 번째 체크가 막히는지), 상태 줄 표시.
- 고용 창과 부대 패널에서 커스텀 용병단이 실제로 어떻게 보이는지(이름, 깃발).
- 테스트 세이브 `saveGame_900`("MLToybox test")이 세이브 폴더에 남아 있다는 것.
