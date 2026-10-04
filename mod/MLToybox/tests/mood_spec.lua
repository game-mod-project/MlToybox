local T = require("t")
local F = require("fakes")
local game = require("core.game")
local mood = require("features.mood")

-- 가짜 영지: 자격(Approval)과 공공질서(publicOrder) 필드를 들고, 쓰인 횟수를 센다
local function region(tag, approval, order)
  local values = { Approval = approval, publicOrder = order }
  local r = F.object({ writes = 0 })
  r.regionUniqueTag = { ToString = function() return tag end }
  r.regionName = { ToString = function() return tag:upper() end }
  return setmetatable({}, {
    __index = function(_, k) if values[k] ~= nil then return values[k] end return r[k] end,
    __newindex = function(_, k, v)
      if values[k] ~= nil then values[k] = v; r.writes = r.writes + 1 else r[k] = v end
    end,
  })
end

local function world(...)
  local regions = { ... }
  game.playerRegions = function() return regions end
  return regions
end

local function stat(fixed, good, bad) return { fixed = fixed, good = good or 1, bad = bad or 100 } end

T.run({
  fixed_values_are_written_to_every_region_of_mine = function()
    local rs = world(region("eich", 40, 70), region("imm", 91, 100))
    local state = {}
    mood.tick(state, { enabled = true, approval = stat(80), order = stat(95) })
    T.eq(rs[1].Approval, 80, "eich approval")
    T.eq(rs[1].publicOrder, 95, "eich order")
    T.eq(rs[2].Approval, 80, "imm approval")
    T.eq(rs[2].publicOrder, 95, "imm order")
  end,
  a_value_that_is_already_right_is_not_written_again = function()
    local r = world(region("eich", 80, 95))[1]
    mood.tick({}, { enabled = true, approval = stat(80), order = stat(95) })
    T.eq(r.writes, 0, "no writes")
  end,
  region_settings_replace_the_common_ones = function()
    local rs = world(region("eich", 40, 70), region("imm", 40, 70))
    mood.tick({}, {
      enabled = true, approval = stat(80), order = stat(0),
      regions = { imm = { approval = stat(0), order = stat(33) } },
    })
    T.eq(rs[1].Approval, 80, "eich follows the common value")
    T.eq(rs[1].publicOrder, 70, "eich order untouched")
    T.eq(rs[2].Approval, 40, "imm has its own setting: no fixed approval")
    T.eq(rs[2].publicOrder, 33, "imm order")
  end,
  multipliers_are_left_to_the_native_hook = function()
    -- 배율은 게임이 하루에 한 번 다시 계산할 때 네이티브가 건다. Lua 는 요인 목록 전부를 읽지 못해 값을 쓰지 않는다
    local r = world(region("eich", 40, 70))[1]
    mood.tick({}, { enabled = true, approval = stat(0, 5, 0), order = stat(0, 1, 50) })
    T.eq(r.writes, 0, "no writes")
  end,
  fixed_values_stay_between_1_and_100 = function()
    local r = world(region("eich", 40, 70))[1]
    mood.tick({}, { enabled = true, approval = stat(250), order = stat(-5) })
    T.eq(r.Approval, 100, "above 100 is 100")
    T.eq(r.publicOrder, 70, "below 1 means not fixed")
    mood.tick({}, { enabled = true, approval = { fixed = "x" }, order = "y" })
    T.eq(r.Approval, 100, "not a number means not fixed")
  end,
  settings_apply_as_soon_as_they_arrive = function()
    local r = world(region("eich", 40, 70))[1]
    mood.enable({}, { enabled = true, approval = stat(60), order = stat(0) })
    T.eq(r.Approval, 60, "on enable")
    mood.configure({}, { enabled = true, approval = stat(75), order = stat(0) })
    T.eq(r.Approval, 75, "on a settings change")
  end,
  status_lists_the_current_values_of_my_regions = function()
    world(region("eich", 40, 70), region("imm", 91, 100))
    local state = {}
    mood.tick(state, { enabled = true, approval = stat(80), order = stat(0) })
    T.eq(#state.mood.regions, 2, "two regions")
    T.eq(state.mood.regions[1].key, "eich", "key")
    T.eq(state.mood.regions[1].name, "EICH", "name")
    T.eq(state.mood.regions[1].approval, 80, "the value after this tick")
    T.eq(state.mood.regions[1].order, 70, "order")
    T.eq(state.mood.regions[2].approval, 80, "imm")
  end,
  the_tab_shows_current_values_while_the_feature_is_off = function()
    local r = world(region("eich", 40, 70))[1]
    local state = {}
    mood.observe(state)
    T.eq(state.mood.regions[1].approval, 40, "approval")
    T.eq(state.mood.regions[1].order, 70, "order")
    T.eq(r.writes, 0, "observe only reads")
  end,
})
