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
  customs_take_the_front_slots_in_place = function()
    local c = company()
    local r = plan.build(input({ customs = { c }, current = { { name = "b", cost = 20 }, listed(c, plan.LOCK_COST), { name = "c", cost = 30 } } }))
    T.eq(r.action, "inplace", "the same entries in another order are reordered")
    T.eq(r.slots[1].name, "토이박스", "slot 1"); T.eq(r.slots[2].name, "b", "slot 2"); T.eq(r.slots[3].name, "c", "slot 3")
    T.eq(r.slots[2].keep, true, "a moved vanilla entry is still a kept one")
  end,
  new_custom_pushes_listed_vanilla_back_in_place = function()
    local c = company()
    local r = plan.build(input({ customs = { c }, current = { { name = "a", cost = 10 }, { name = "b", cost = 20 }, { name = "c", cost = 30 } } }))
    T.eq(r.action, "inplace", "action")
    T.eq(r.slots[1].name, "토이박스", "custom in front"); T.eq(r.slots[2].name, "a", "kept"); T.eq(r.slots[3].name, "b", "kept 2")
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
