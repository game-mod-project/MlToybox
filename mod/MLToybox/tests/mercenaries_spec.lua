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
