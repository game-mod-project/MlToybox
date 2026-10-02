local T = require("t")
local F = require("fakes")
local datatable = require("core.datatable")
local game = require("core.game")
local storage = require("features.storage")

-- 건물 표의 기본 한도(실측 2026-10-02): 대형 창고 99 일반 2500, 대형 식량 비축고 68 식량 2500, 벌목장 4 목재 28,
-- 거주 구획 3레벨 60 일반 75·식량 75, 농가 69 일반 1200·식량 1200
local function row(generic, large, pantry)
  return { storageLimitGeneric = generic, storageLimitLarge = large, storageLimitPantry = pantry }
end

local function building(type, generic, large, pantry)
  local b = F.object(row(generic, large, pantry))
  b.GetType = function() return type end
  return b
end

local function limits(b)
  return string.format("%d/%d/%d", b.storageLimitGeneric, b.storageLimitLarge, b.storageLimitPantry)
end

-- 내 영지 하나와 그 건물들. 돌려준 목록에 건물을 더하거나 빼면 영지의 건물이 바뀐다
local function setup(buildings)
  local rows = { ["99"] = row(2500, 0, 0), ["68"] = row(0, 0, 2500), ["4"] = row(0, 28, 0), ["60"] = row(75, 0, 75), ["69"] = row(1200, 0, 1200) }
  datatable.find = function(p) if p == datatable.PATHS.buildingStats then return F.datatable(rows) end end
  local region = F.object({})
  region.GetBuildings = function()
    local out = {}
    for i, b in ipairs(buildings) do out[i] = F.wrap(b) end
    return out
  end
  game.playerRegions = function() return { region } end
  storage.enable({}, {})
  return rows
end

local function on(limitsByType) return { enabled = true, limits = limitsByType } end

T.run({
  sets_the_limit_of_every_building_of_that_type = function()
    local a, b, camp = building(99, 2500, 0, 0), building(99, 2500, 0, 0), building(4, 0, 28, 0)
    setup({ a, b, camp })
    storage.tick({}, on({ ["99"] = { generic = 5000 }, ["4"] = { large = 100 } }))
    T.eq(limits(a), "5000/0/0", "first storehouse"); T.eq(limits(b), "5000/0/0", "second storehouse"); T.eq(limits(camp), "0/100/0", "logging camp")
  end,
  each_kind_of_storage_has_its_own_value = function()
    local farm = building(69, 1200, 0, 1200)
    setup({ farm })
    storage.tick({}, on({ ["69"] = { generic = 3000 } }))
    T.eq(limits(farm), "3000/0/1200", "only the generic storage changes")
    storage.tick({}, on({ ["69"] = { generic = 3000, pantry = 6000 } }))
    T.eq(limits(farm), "3000/0/6000", "both")
  end,
  buildings_without_a_setting_are_left_alone = function()
    local granary, camp = building(68, 0, 0, 2500), building(4, 0, 28, 0)
    setup({ granary, camp })
    storage.tick({}, on({ ["68"] = { pantry = 9000 } }))
    T.eq(limits(camp), "0/28/0", "no setting for logging camps"); T.eq(limits(granary), "0/0/9000", "granary")
    storage.tick({}, on({}))
    storage.tick({}, { enabled = true })
  end,
  a_storage_the_building_does_not_have_stays_at_zero = function()
    -- 원래 0 인 분류(대형 창고의 식량·목재)는 고치지 않는다: 게임에서 재지 않았다
    local a = building(99, 2500, 0, 0)
    setup({ a })
    storage.tick({}, on({ ["99"] = { generic = 5000, large = 400, pantry = 700 } }))
    T.eq(limits(a), "5000/0/0", "only the storage it has")
  end,
  a_bonus_the_game_gave_is_kept_in_proportion = function()
    -- 실측: 거주 구획 3레벨의 기본은 75인데 업그레이드 12가 있는 집은 112(1.5배)였다
    local plain, upgraded = building(60, 75, 0, 75), building(60, 112, 0, 112)
    setup({ plain, upgraded })
    storage.tick({}, on({ ["60"] = { generic = 150, pantry = 300 } }))
    T.eq(limits(plain), "150/0/300", "plain house gets the value"); T.eq(limits(upgraded), "224/0/448", "upgraded house keeps its 1.5 times")
  end,
  changing_the_value_starts_from_the_games_limit_not_from_ours = function()
    local upgraded = building(60, 112, 0, 112)
    setup({ upgraded })
    storage.tick({}, on({ ["60"] = { generic = 150 } }))
    storage.tick({}, on({ ["60"] = { generic = 300 } }))
    T.eq(upgraded.storageLimitGeneric, 448, "112 * 300 / 75, not 224 * 300 / 75")
    storage.tick({}, on({ ["60"] = { generic = 300 } }))
    T.eq(upgraded.storageLimitGeneric, 448, "stays on the next pass")
  end,
  a_limit_the_game_recomputed_becomes_the_new_base = function()
    local house = building(60, 75, 0, 75)
    setup({ house })
    storage.tick({}, on({ ["60"] = { generic = 150 } }))
    T.eq(house.storageLimitGeneric, 150, "applied")
    house.storageLimitGeneric = 112   -- 게임이 업그레이드 뒤에 다시 계산했다
    storage.tick({}, on({ ["60"] = { generic = 150 } }))
    T.eq(house.storageLimitGeneric, 224, "scaled from the new base")
    storage.disable({}, {})
    T.eq(house.storageLimitGeneric, 112, "turning off gives back what the game last set")
  end,
  turning_off_gives_the_games_limits_back = function()
    local a, camp = building(99, 2500, 0, 0), building(4, 0, 28, 0)
    setup({ a, camp })
    storage.tick({}, on({ ["99"] = { generic = 5000 }, ["4"] = { large = 100 } }))
    storage.disable({}, {})
    T.eq(limits(a), "2500/0/0", "storehouse"); T.eq(limits(camp), "0/28/0", "logging camp")
    T.eq(storage.trackedCount(), 0, "nothing remembered")
  end,
  removing_a_setting_gives_that_types_limits_back = function()
    local a, granary = building(99, 2500, 0, 0), building(68, 0, 0, 2500)
    setup({ a, granary })
    storage.tick({}, on({ ["99"] = { generic = 5000 }, ["68"] = { pantry = 9000 } }))
    storage.tick({}, on({ ["68"] = { pantry = 9000 } }))
    T.eq(limits(a), "2500/0/0", "storehouse is back"); T.eq(limits(granary), "0/0/9000", "granary keeps its value")
    storage.tick({}, on({ ["68"] = {} }))
    T.eq(limits(granary), "0/0/2500", "an empty entry is no setting")
  end,
  giving_back_does_not_undo_what_the_game_changed_meanwhile = function()
    local house = building(60, 75, 0, 75)
    setup({ house })
    storage.tick({}, on({ ["60"] = { generic = 150 } }))
    house.storageLimitGeneric = 112   -- 끄기 직전에 게임이 다시 계산했다
    storage.disable({}, {})
    T.eq(house.storageLimitGeneric, 112, "the game's newer value stays")
  end,
  a_new_building_gets_the_value_on_the_next_pass = function()
    local list = { building(99, 2500, 0, 0) }
    setup(list)
    local settings = on({ ["99"] = { generic = 5000 } })
    storage.tick({}, settings)
    local new = building(99, 2500, 0, 0)
    list[#list + 1] = new
    storage.tick({}, settings)
    T.eq(new.storageLimitGeneric, 5000, "new storehouse"); T.eq(storage.trackedCount(), 2, "both remembered")
  end,
  a_building_that_is_gone_is_forgotten = function()
    local list = { building(99, 2500, 0, 0), building(99, 2500, 0, 0) }
    setup(list)
    local settings = on({ ["99"] = { generic = 5000 } })
    storage.tick({}, settings)
    list[2] = nil
    storage.tick({}, settings)
    T.eq(storage.trackedCount(), 1, "one left")
  end,
  configure_applies_right_away = function()
    local a = building(99, 2500, 0, 0)
    setup({ a })
    storage.configure({}, on({ ["99"] = { generic = 4000 } }))
    T.eq(a.storageLimitGeneric, 4000, "applied without waiting for the next pass")
  end,
  values_that_are_not_usable_are_ignored = function()
    local a, granary, camp = building(99, 2500, 0, 0), building(68, 0, 0, 2500), building(4, 0, 28, 0)
    setup({ a, granary, camp })
    storage.tick({}, on({ ["99"] = { generic = "many" }, ["68"] = { pantry = -5 }, ["4"] = { large = 99999999 } }))
    T.eq(limits(a), "2500/0/0", "text"); T.eq(limits(granary), "0/0/2500", "negative")
    T.eq(camp.storageLimitLarge, storage.MAX, "capped"); T.eq(storage.MAX, 1000000, "the cap")
    storage.tick({}, on({ ["99"] = { generic = 3000.7 } }))
    T.eq(a.storageLimitGeneric, 3000, "whole numbers only")
    storage.tick({}, on({ ["99"] = "all of it" }))
    T.eq(a.storageLimitGeneric, 2500, "an entry that is not a table is no setting")
  end,
  a_type_the_table_does_not_have_is_left_alone = function()
    local odd = building(7777, 40, 0, 0)
    setup({ odd })
    storage.tick({}, on({ ["7777"] = { generic = 500 } }))
    T.eq(limits(odd), "40/0/0", "no row, no default to scale from")
  end,
  invalid_building_elements_are_skipped = function()
    local a = building(99, 2500, 0, 0)
    setup({ a })
    local region = F.object({})
    region.GetBuildings = function() return { F.wrap(nil), F.wrap(F.invalid()), F.wrap(a) } end
    game.playerRegions = function() return { region } end
    storage.tick({}, on({ ["99"] = { generic = 5000 } }))
    T.eq(a.storageLimitGeneric, 5000, "the valid one")
  end,
  the_building_table_is_not_changed = function()
    -- 실측: 표를 바꿔도 지어진 건물에는 반영되지 않는다. 표는 기본값을 읽는 데만 쓴다
    local a = building(99, 2500, 0, 0)
    local rows = setup({ a })
    storage.tick({}, on({ ["99"] = { generic = 5000 } }))
    T.eq(rows["99"].storageLimitGeneric, 2500, "row untouched")
  end,
  enabling_again_forgets_the_previous_map = function()
    local a = building(99, 2500, 0, 0)
    setup({ a })
    storage.tick({}, on({ ["99"] = { generic = 5000 } }))
    storage.enable({}, {})   -- 다른 세이브를 불러왔다: 예전 건물은 사라졌다
    T.eq(storage.trackedCount(), 0, "nothing carried over")
  end,
})
