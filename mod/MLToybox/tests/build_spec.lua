local T = require("t")
local F = require("fakes")
local datatable = require("core.datatable")
local game = require("core.game")
local native = require("core.native")
local build = require("features.build")

-- 네이티브가 "플레이어의 배치 판정 동안만 행을 고치는 일"을 맡았는가: true 맡음, false 못 맡음(DLL 없음), nil 아직 모름
local function nativeCan(v)
  if v == nil then native.last = nil
  elseif v then
    native.last = { loaded = true, stale = false, features = {
      placement = { installed = true }, building_row = { installed = true }, placement_rows = { installed = true } } }
  else native.last = { loaded = false, stale = true } end
end

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

-- 엔진의 디버그 플래그 목록(drawDebugFlags)과 플레이어 폰의 배치 상태. 게임처럼 Lua 표를 대입하면 배열이 통째로 바뀐다
local function fname(s) return { ToString = function() return s end } end
local function placement(opts)
  opts = opts or {}
  local items = {}
  for i, s in ipairs(opts.flags or {}) do items[i] = fname(s) end
  local store, writes = F.array(items), 0
  local engine = setmetatable(F.object({}), {
    __index = function(_, k) if k == "drawDebugFlags" then return store end end,
    __newindex = function(t, k, v)
      if k == "drawDebugFlags" then store = F.array(v); writes = writes + 1 else rawset(t, k, v) end
    end,
  })
  local pawn = F.object({ placeBuilding = opts.placeBuilding or 0, placeFieldMode = opts.placeFieldMode or false })
  game.engine = function() return engine end
  game.pawn = function() return pawn end
  game.fname = fname
  game.playerRegions = function() return {} end   -- 공사 현장이 없는 맵. 현장이 필요한 테스트는 이 뒤에 setup() 을 부른다
  local function flags()
    local out = {}
    for i = 1, #store do out[i] = store[i]:ToString() end
    return table.concat(out, ",")
  end
  return pawn, flags, function() return writes end
end

-- 유지보수 물자가 남은 건물이 있는 영지. 벌목장: 일반 한도 0, 일반 물자 1개(철제 도구 6), 목재(16)는 목재 저장실에 있다.
-- opts.supply / opts.kind 로 유지보수 물자와 그 보관 방식을 바꾼다(기본 6, 일반 = 0)
local STORED = { [0] = "numStoredGeneric", [1] = "numStoredLarge", [2] = "numStoredPantry" }
local function supplySite(opts)
  opts = opts or {}
  local supply, kind = opts.supply or 6, opts.kind or 0
  local items = { ["6"] = { storageType = 0 }, ["16"] = { storageType = 1 }, ["172"] = { storageType = 2 } }
  items[tostring(supply)] = { storageType = kind }
  datatable.find = function(p) if p == datatable.PATHS.items then return F.datatable(items) end end
  local function maintained(goods)
    local comp = F.object({})
    comp.GetTrackedMaintenanceTypes = function() return { F.wrap({ goodTypes = F.array(goods) }) } end
    return comp
  end
  local function building(fields)
    local b = F.object(fields)
    b.IsConstructed = function() return true end
    return b
  end
  local camp = building({ storageLimitGeneric = 0, storageLimitLarge = 28, storageLimitPantry = 0, numStoredGeneric = 0, numStoredLarge = 4, numStoredPantry = 0,
    Inventory = F.array({ { Type = supply, amt = 1 }, { Type = 16, amt = 4 } }), MaintenanceComponent = maintained({ supply }) })
  camp[STORED[kind]] = (kind == 1) and 5 or 1
  local list = { F.wrap(camp) }
  if opts.roomy then
    list[#list + 1] = F.wrap(building({ storageLimitGeneric = 50, storageLimitLarge = 0, storageLimitPantry = 0, numStoredGeneric = 1, numStoredLarge = 0, numStoredPantry = 0,
      Inventory = F.array({ { Type = 6, amt = 1 } }), MaintenanceComponent = maintained({ 6 }) }))
  end
  if opts.pile then
    local none = F.object({})
    none.GetTrackedMaintenanceTypes = function() return {} end
    list[#list + 1] = F.wrap(building({ storageLimitGeneric = 0, storageLimitLarge = 0, storageLimitPantry = 0, numStoredGeneric = 470, numStoredLarge = 0, numStoredPantry = 0,
      Inventory = F.array({ { Type = 6, amt = 470 } }), MaintenanceComponent = none }))
  end
  local site = { camp = camp, consumed = {} }
  local region = F.object({})
  region.GetBuildings = function() return list end
  region.consumeGood = function(_, goodType, amt, b, scramble, respectReservation, redistribute)
    site.consumed[#site.consumed + 1] = { type = goodType, amt = amt, building = b, scramble = scramble, respectReservation = respectReservation, redistribute = redistribute }
    return true
  end
  game.playerRegions = function() return { region } end
  local cheat = { MaintainAllBuildings = function() end }
  game.cheat = function() return cheat end
  return site
end
T.run({
  -- 새로 놓는 건물: 게임은 엔진의 디버그 플래그에 instaBuild 가 있으면 건물을 놓는 순간 완공 상태로 만든다(findings "즉시 완공 — instaBuild 플래그").
  -- 플래그는 AI 영주의 건물에도 적용되므로 내가 배치하는 동안만 넣는다
  placing_a_building_puts_the_insta_build_flag_on_the_engine = function()
    local _, flags, writes = placement({ placeBuilding = 4 })
    build.poll({}, { enabled = true, instantBuild = true })
    T.eq(flags(), "instaBuild", "flag on while placing")
    build.poll({}, { enabled = true, instantBuild = true })
    T.eq(writes(), 1, "the list is not rewritten every second")
  end,
  leaving_placement_mode_takes_the_flag_off = function()
    local pawn, flags = placement({ placeBuilding = 4 })
    build.poll({}, { enabled = true, instantBuild = true })
    pawn.placeBuilding = 0
    build.poll({}, { enabled = true, instantBuild = true })
    T.eq(flags(), "", "flag off once the building is placed or the mode is cancelled")
  end,
  placing_a_field_or_plot_counts_as_placing = function()
    local _, flags = placement({ placeFieldMode = true })
    build.poll({}, { enabled = true, instantBuild = true })
    T.eq(flags(), "instaBuild", "placeFieldMode")
  end,
  not_placing_leaves_the_engine_alone = function()
    local _, flags, writes = placement()
    build.poll({}, { enabled = true, instantBuild = true })
    T.eq(flags(), "", "no flag"); T.eq(writes(), 0, "nothing written")
  end,
  instant_build_off_never_sets_the_flag_and_clears_a_leftover = function()
    local _, flags = placement({ placeBuilding = 4 })
    build.poll({}, { enabled = true, instantBuild = false })
    T.eq(flags(), "", "not set")
    _, flags = placement({ placeBuilding = 4, flags = { "instaBuild" } })
    build.poll({}, { enabled = true, instantBuild = false })
    T.eq(flags(), "", "a flag left from before the option was turned off is removed")
  end,
  flags_the_game_already_had_are_kept = function()
    local pawn, flags = placement({ placeBuilding = 4, flags = { "showPaths" } })
    build.poll({}, { enabled = true, instantBuild = true })
    T.eq(flags(), "showPaths,instaBuild", "added after the others")
    pawn.placeBuilding = 0
    build.poll({}, { enabled = true, instantBuild = true })
    T.eq(flags(), "showPaths", "only ours is removed")
  end,
  disable_takes_the_flag_off_unless_the_map_is_going_away = function()
    local _, flags, writes = placement({ placeBuilding = 4, flags = { "instaBuild" } })
    build.disable({ leaving = true }, { enabled = true, instantBuild = true })
    T.eq(flags(), "instaBuild", "leaving the map: the engine is about to go, leave it"); T.eq(writes(), 0, "untouched")
    build.disable({}, { enabled = true, instantBuild = true })
    T.eq(flags(), "", "turned off by the user")
  end,
  poll_without_pawn_or_engine_does_nothing = function()
    local _, flags = placement({ placeBuilding = 4 })
    game.pawn = function() return nil end
    build.poll({}, { enabled = true, instantBuild = true })
    T.eq(flags(), "", "no pawn: not placing")
    placement({ placeBuilding = 4 })
    game.engine = function() return nil end
    build.poll({}, { enabled = true, instantBuild = true })
    build.disable({}, { enabled = true, instantBuild = true })
  end,
  -- 지역당 개수 제한과 건설 자재: 네이티브가 플레이어의 배치 판정 동안만 표의 행을 고친다
  -- (findings "표를 바꾸는 기능을 플레이어에게만 — 방법 조사"). 표를 바꾸면 AI 영주도 읽으므로 Lua 는 표를 건드리지 않는다
  the_stats_table_is_left_alone_when_native_scopes_the_rows_to_the_player = function()
    placement()
    local rows, unbuilt, built = setup()
    rows["57"] = { constructionGoods = F.array({}), maxInRegion = 1 }
    nativeCan(true)
    local settings = { enabled = true, noRegionLimit = true, noMaterials = true }
    build.enable({}, settings)
    build.poll({}, settings)
    T.eq(rows["57"].maxInRegion, 1, "limit kept in the table"); T.eq(#rows["3"].constructionGoods, 1, "goods kept in the table")
    T.eq(#unbuilt.constructionGoods, 0, "my site still loses its goods list"); T.eq(#built.constructionGoods, 1, "built untouched")
  end,
  -- 네이티브가 못 맡으면(DLL 을 올리지 못했다, 게임 업데이트로 함수를 못 찾았다) 예전처럼 표를 바꾼다. 이때는 AI 영주에게도 적용된다
  without_native_no_region_limit_clears_max_in_region_in_the_table = function()
    -- 실측: 수비용 탑(manor_keep_lv1) 등 9개 행이 maxInRegion=1, 나머지 93개 행은 0(제한 없음)
    local rows = setup()
    rows["57"] = { constructionGoods = F.array({}), maxInRegion = 1 }
    rows["3"].maxInRegion = 0
    nativeCan(false)
    build.enable({}, { enabled = true, noRegionLimit = true })
    T.eq(rows["57"].maxInRegion, 0, "limit removed"); T.eq(rows["3"].maxInRegion, 0, "unlimited stays")
    T.eq(#rows["3"].constructionGoods, 1, "goods are another option")
  end,
  without_native_no_materials_clears_the_goods_in_the_table = function()
    local rows, unbuilt, built = setup()
    nativeCan(false)
    build.enable({}, { enabled = true, noMaterials = true })
    T.eq(#rows["3"].constructionGoods, 0, "stats row"); T.eq(#rows["72"].constructionGoods, 0, "stats row 2")
    T.eq(#unbuilt.constructionGoods, 0, "unbuilt cleared"); T.eq(#built.constructionGoods, 1, "built untouched")
  end,
  the_options_off_leave_the_table_alone_either_way = function()
    local rows = setup()
    rows["57"] = { constructionGoods = F.array({}), maxInRegion = 1 }
    nativeCan(false)
    build.enable({}, { enabled = true, noRegionLimit = false, noMaterials = false })
    T.eq(rows["57"].maxInRegion, 1, "limit untouched"); T.eq(#rows["3"].constructionGoods, 1, "goods untouched")
  end,
  the_table_waits_until_the_native_state_is_known = function()
    -- 맵을 불러온 직후에는 네이티브 상태를 아직 읽지 못했을 수 있다. 표를 바꾸면 되돌릴 수 없으므로 알 때까지 기다린다
    placement()
    local rows = setup()
    rows["57"] = { constructionGoods = F.array({}), maxInRegion = 1 }
    local settings = { enabled = true, noRegionLimit = true, noMaterials = true }
    nativeCan(nil)
    build.enable({}, settings)
    build.poll({}, settings)
    T.eq(rows["57"].maxInRegion, 1, "not yet"); T.eq(#rows["3"].constructionGoods, 1, "not yet")
    nativeCan(false)
    build.poll({}, settings)
    T.eq(rows["57"].maxInRegion, 0, "native cannot: the table after all"); T.eq(#rows["3"].constructionGoods, 0, "goods too")
  end,
  an_option_turned_on_later_still_reaches_the_table_without_native = function()
    placement()
    local rows = setup()
    rows["57"] = { constructionGoods = F.array({}), maxInRegion = 1 }
    nativeCan(false)
    build.enable({}, { enabled = true, noRegionLimit = true, noMaterials = false })
    T.eq(#rows["3"].constructionGoods, 1, "goods option is off")
    build.configure({}, { enabled = true, noRegionLimit = true, noMaterials = true })
    T.eq(#rows["3"].constructionGoods, 0, "turned on from the panel")
  end,  instant_repair_maintains_on_tick = function()
    local _, _, _, cheat = setup()
    build.tick({}, { enabled = true, instantRepair = true })
    T.eq(cheat.maintained, 1, "maintained")
    build.tick({}, { enabled = true, instantRepair = false })
    T.eq(cheat.maintained, 1, "not when off")
  end,
  -- 표의 자재를 그대로 두므로 새로 놓은 건물은 자재 목록을 갖고 생긴다. 운반이 시작되기 전에 비우도록 매 poll(1초)에 비운다
  no_materials_clears_my_sites_on_every_poll = function()
    placement()
    local _, unbuilt, built = setup()
    nativeCan(true)
    local regions = game.playerRegions
    game.playerRegions = function() return {} end          -- 로드 직후: 아직 지역 없음
    build.enable({}, { enabled = true, noMaterials = true })
    T.eq(#unbuilt.constructionGoods, 1, "not reachable at enable")
    game.playerRegions = regions
    build.poll({}, { enabled = true, noMaterials = true })
    T.eq(#unbuilt.constructionGoods, 0, "cleared on the next poll"); T.eq(#built.constructionGoods, 1, "built untouched")
    unbuilt.constructionGoods = F.array({ { Type = 16, amt = 2 } })   -- 새로 놓은 건물
    build.tick({}, { enabled = true, noMaterials = true, instantRepair = false })
    T.eq(#unbuilt.constructionGoods, 1, "the 10 second tick no longer does it")
    build.poll({}, { enabled = true, noMaterials = true })
    T.eq(#unbuilt.constructionGoods, 0, "poll does")
  end,
  no_materials_off_keeps_the_goods_of_my_sites = function()
    placement()
    local _, unbuilt = setup()
    nativeCan(true)
    build.enable({}, { enabled = true, noMaterials = false })
    build.poll({}, { enabled = true, noMaterials = false })
    T.eq(#unbuilt.constructionGoods, 1, "kept")
  end,
  invalid_building_elements_are_skipped = function()
    placement()
    setup()
    local region = F.object({})
    region.GetBuildings = function() return { F.wrap(nil), F.wrap(F.invalid()) } end
    game.playerRegions = function() return { region } end
    build.poll({}, { enabled = true, noMaterials = true })
  end,
  -- 이미 공사 중인 건물(업그레이드 포함): 진행도 함수를 부르면 네이티브가 파츠 hp 를 채우고 게임의 완공 함수를 부른다.
  -- 네이티브는 낼 자재가 없는 건물만 완공 처리하므로 자재 목록을 먼저 비운다. 완공된 건물은 건드리지 않는다
  instant_build_finishes_sites_seen_on_the_previous_poll = function()
    placement()
    local _, unbuilt, built = setup()
    local calls = 0
    unbuilt.getConstructionProgress = function() calls = calls + 1; T.eq(#unbuilt.constructionGoods, 0, "goods cleared before the call"); return 0.5 end
    built.getConstructionProgress = function() error("must not be called on built") end
    build.enable({}, { enabled = true, instantBuild = true })
    build.poll({}, { enabled = true, instantBuild = true })
    T.eq(calls, 0, "a site seen for the first time is left alone: its parts may still be on their way")
    T.eq(#unbuilt.constructionGoods, 1, "and keeps its goods")
    build.poll({}, { enabled = true, instantBuild = true })
    T.eq(calls, 1, "finished on the next poll")
    T.eq(#built.constructionGoods, 1, "built untouched")
  end,
  instant_build_off_leaves_sites_alone = function()
    placement()
    local _, unbuilt = setup()
    unbuilt.getConstructionProgress = function() error("must not be called") end
    build.enable({}, { enabled = true, instantBuild = false })
    build.poll({}, { enabled = true, instantBuild = false }); build.poll({}, { enabled = true, instantBuild = false })
    T.eq(#unbuilt.constructionGoods, 1, "goods kept")
    build.tick({}, { enabled = true, instantBuild = true })   -- 10초 tick 은 더는 진행도 함수를 부르지 않는다(poll 이 맡는다)
  end,
  sites_seen_before_are_forgotten_when_the_feature_starts_again = function()
    placement()
    local _, unbuilt = setup()
    local calls = 0
    unbuilt.getConstructionProgress = function() calls = calls + 1; return 0.5 end
    build.enable({}, { enabled = true, instantBuild = true })
    build.poll({}, { enabled = true, instantBuild = true })
    build.enable({}, { enabled = true, instantBuild = true })   -- 새 맵: 같은 주소에 다른 건물이 있을 수 있다
    build.poll({}, { enabled = true, instantBuild = true })
    T.eq(calls, 0, "first poll of the new map")
    build.poll({}, { enabled = true, instantBuild = false })    -- 옵션을 껐다 켜도 처음부터 다시 센다
    build.poll({}, { enabled = true, instantBuild = true })
    T.eq(calls, 0, "first poll after turning the option back on")
  end,
  -- 즉시 수리의 치트는 물자를 쓰지 않고 건물을 채운다. 그때 일꾼이 이미 유지보수 물자를 나르고 있었으면 도착한 물자가 건물에 남는다
  -- (실측 2026-10-03: 벌목장에 철제 도구 1개). 그 종류의 저장 한도가 0 인 건물이면 게임이 "저장실 가득 참"을 붙인다.
  -- 치트가 대신 한 유지보수에 쓰였어야 할 물자이므로 그 건물에서 소모시킨다
  instant_repair_uses_up_a_supply_left_in_a_building_with_no_room_for_it = function()
    local site = supplySite()
    build.tick({}, { enabled = true, instantRepair = true })
    T.eq(#site.consumed, 1, "one consume call")
    local c = site.consumed[1]
    T.eq(c.type, 6, "the iron tools"); T.eq(c.amt, 1, "all of them"); T.eq(c.building, site.camp, "from that building")
    T.eq(c.scramble, false, "not from anywhere else"); T.eq(c.respectReservation, false, "reservation flag"); T.eq(c.redistribute, false, "market flag")
  end,
  instant_repair_leaves_other_goods_and_buildings_alone = function()
    local site = supplySite({
      roomy = true,        -- 일반 한도가 있는 건물에 든 도구: 자리가 있으니 표시가 붙지 않는다
      pile = true,         -- 유지보수 대상이 아닌 건물(야적 물자 등)에 든 도구
    })
    build.tick({}, { enabled = true, instantRepair = true })
    T.eq(#site.consumed, 1, "only the camp's tools: not its timber, not the workshop's tools, not the pile's")
    T.eq(site.consumed[1].building, site.camp, "the camp")
  end,
  instant_repair_off_leaves_the_supplies_alone = function()
    local site = supplySite()
    build.tick({}, { enabled = true, instantRepair = false })
    T.eq(#site.consumed, 0, "that is the game's own maintenance then")
  end,
  a_supply_kept_in_another_kind_of_storage_is_handled_the_same_way = function()
    -- 식량 저장실(종류 2)에 들어가는 물자가 유지보수 물자인 건물, 식량 한도 0
    local site = supplySite({ supply = 172, kind = 2 })
    build.tick({}, { enabled = true, instantRepair = true })
    T.eq(#site.consumed, 1, "consumed"); T.eq(site.consumed[1].type, 172, "the pantry good")
  end,
  a_building_that_cannot_tell_its_supplies_is_skipped = function()
    local site = supplySite()
    site.camp.MaintenanceComponent = F.invalid()
    build.tick({}, { enabled = true, instantRepair = true })
    T.eq(#site.consumed, 0, "no component: nothing to go by")
  end,
  tick_without_cheat_is_noop = function()
    setup()
    game.cheat = function() return nil end
    build.tick({}, { enabled = true, instantRepair = true })
  end,
})
