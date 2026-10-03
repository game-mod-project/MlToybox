local T = require("t")
local F = require("fakes")
local datatable = require("core.datatable")
local native = require("core.native")
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

-- 네이티브가 "내 건물에 대한 호출 동안만 행을 고치는 일"을 맡았는가: true 맡음, false 못 맡음(DLL 없음), nil 아직 모름
local function nativeCan(v)
  if v == nil then native.last = nil
  elseif v then
    local features = {}
    for _, name in ipairs(upgrade.SCOPE) do features[name] = { installed = true } end
    native.last = { loaded = true, stale = false, features = features }
  else native.last = { loaded = false, stale = true } end
end

local function tables()
  local rows = { ["2"] = upRow(), ["4"] = upRow() }
  local s = residential()
  datatable.find = function(p)
    if p == datatable.PATHS.upgrades then return F.datatable(rows) end
    if p == datatable.PATHS.residentialSettings then return F.object(s) end
  end
  return rows, s
end

local function gameValuesKept(r)
  return #r.cost == 1 and r.regionalWealth == 25 and r.treasury == 10 and r.minimumSettlementLevel == 2 and r.minimumHouseLv == 2
end
local function screenValuesCleared(r)
  return #r.requiresBuilding == 0 and #r.requiredPerks == 0 and r.minimumProsperity == 0 and r.lockedInOutposts == false
end

T.run({
  patch_upgrade_row_clears_everything = function()
    local r = upRow()
    upgrade.patchUpgradeRow(r)
    T.eq(#r.cost, 0, "cost"); T.eq(#r.requiresBuilding, 0, "requiresBuilding"); T.eq(#r.requiredPerks, 0, "perks")
    T.eq(r.regionalWealth, 0, "wealth"); T.eq(r.treasury, 0, "treasury"); T.eq(r.minimumSettlementLevel, 0, "settle")
    T.eq(r.minimumProsperity, 0, "prosperity"); T.eq(r.minimumHouseLv, 0, "house"); T.eq(r.lockedInOutposts, false, "outposts")
  end,
  -- 화면(블루프린트)만 읽는 값: 선행 건물, 해금, 번영도, 전초기지 잠금. AI 영주는 읽지 않는다(게임 코드에 읽는 곳이 없다)
  patch_screen_row_clears_only_what_the_screens_read = function()
    local r = upRow()
    upgrade.patchScreenRow(r)
    T.truthy(screenValuesCleared(r), "screen values cleared")
    T.truthy(gameValuesKept(r), "what the game code reads is left for native to scope to the player")
  end,
  patch_residential_zeroes_values_keeps_arrays = function()
    local s = residential()
    T.eq(upgrade.patchResidential(s), 2, "two changed")
    local reqs = s.UpgradeRequirementsPerLevel[2].Requirements
    T.eq(#reqs, 3, "array kept"); T.eq(reqs[1].VarietyRequired, 0, "zeroed"); T.eq(reqs[3].VarietyRequired, 0, "zeroed")
  end,
  -- 네이티브가 맡으면 표의 비용·레벨 조건과 주거 요구 설정은 그대로 둔다(AI 영주가 같은 함수로 읽는 값이다)
  with_native_only_the_screen_values_change = function()
    local rows, s = tables()
    nativeCan(true)
    local st = {}
    upgrade.enable(st, { enabled = true })
    T.truthy(screenValuesCleared(rows["4"]) and gameValuesKept(rows["4"]), "row 4")
    T.truthy(screenValuesCleared(rows["2"]) and gameValuesKept(rows["2"]), "row 2")
    T.eq(s.UpgradeRequirementsPerLevel[2].Requirements[1].VarietyRequired, 1, "residential settings untouched")
    T.eq(st.upgradePatch.rows, 2, "rows"); T.eq(st.upgradePatch.requirements, 0, "no settings change"); T.eq(st.upgradePatch.scoped, true, "scoped to the player")
  end,
  -- 못 맡으면(DLL 을 올리지 못했다, 게임 업데이트로 함수를 못 찾았다) 예전처럼 표와 설정을 바꾼다. 이때는 AI 영주에게도 적용된다
  without_native_all_rows_and_the_settings_change = function()
    local rows, s = tables()
    nativeCan(false)
    local st = {}
    upgrade.enable(st, { enabled = true })
    T.eq(rows["4"].regionalWealth, 0, "row patched"); T.eq(#rows["2"].cost, 0, "cost cleared")
    T.eq(s.UpgradeRequirementsPerLevel[2].Requirements[1].VarietyRequired, 0, "settings patched")
    T.eq(st.upgradePatch.rows, 2, "rows"); T.eq(st.upgradePatch.requirements, 2, "reqs"); T.eq(st.upgradePatch.scoped, false, "not scoped")
  end,
  the_game_values_wait_until_the_native_state_is_known = function()
    -- 표를 바꾸면 되돌릴 수 없으므로 네이티브 상태를 알 때까지 기다린다. 화면 값은 AI 와 무관하니 바로 바꾼다
    local rows, s = tables()
    nativeCan(nil)
    local st = {}
    upgrade.enable(st, { enabled = true })
    T.truthy(screenValuesCleared(rows["4"]) and gameValuesKept(rows["4"]), "only the screen values so far")
    upgrade.poll(st, { enabled = true })
    T.truthy(gameValuesKept(rows["4"]), "still unknown")
    nativeCan(false)
    upgrade.poll(st, { enabled = true })
    T.eq(rows["4"].regionalWealth, 0, "native cannot: the table after all")
    T.eq(s.UpgradeRequirementsPerLevel[2].Requirements[3].VarietyRequired, 0, "and the settings")
  end,
  poll_does_nothing_once_native_took_over = function()
    local rows = tables()
    nativeCan(true)
    local st = {}
    upgrade.enable(st, { enabled = true })
    datatable.find = function() error("the tables must not be read again") end
    upgrade.poll(st, { enabled = true })
    T.truthy(gameValuesKept(rows["4"]), "kept")
  end,
  enable_fails_when_table_missing = function()
    datatable.find = function() return nil end
    nativeCan(false)
    T.eq((pcall(upgrade.enable, {}, { enabled = true })), false, "error propagates to registry")
  end,
})