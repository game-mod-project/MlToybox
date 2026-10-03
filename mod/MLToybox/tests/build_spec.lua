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
  no_region_limit_clears_max_in_region = function()
    -- 실측: 수비용 탑(manor_keep_lv1) 등 9개 행이 maxInRegion=1, 나머지 93개 행은 0(제한 없음)
    local rows = setup()
    rows["57"] = { constructionGoods = F.array({}), maxInRegion = 1 }
    rows["3"].maxInRegion = 0
    build.enable({}, { enabled = true, noRegionLimit = true })
    T.eq(rows["57"].maxInRegion, 0, "limit removed"); T.eq(rows["3"].maxInRegion, 0, "unlimited stays")
  end,
  no_region_limit_off_leaves_data = function()
    local rows = setup()
    rows["57"] = { constructionGoods = F.array({}), maxInRegion = 1 }
    build.enable({}, { enabled = true, noRegionLimit = false })
    T.eq(rows["57"].maxInRegion, 1, "untouched")
  end,  no_materials_clears_stats_and_unbuilt_only = function()
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
  no_materials_clears_sites_on_tick_when_enable_ran_before_regions_existed = function()
    local rows, unbuilt = setup()
    local regions = game.playerRegions
    game.playerRegions = function() return {} end          -- 로드 직후: 아직 지역 없음
    build.enable({}, { enabled = true, noMaterials = true })
    T.eq(#unbuilt.constructionGoods, 1, "not reachable at enable")
    game.playerRegions = regions
    build.tick({}, { enabled = true, noMaterials = true, instantRepair = false })
    T.eq(#unbuilt.constructionGoods, 0, "cleared on tick")
  end,
  invalid_building_elements_are_skipped = function()
    setup()
    local region = F.object({})
    region.GetBuildings = function() return { F.wrap(nil), F.wrap(F.invalid()) } end
    game.playerRegions = function() return { region } end
    build.tick({}, { enabled = true, noMaterials = true })
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
  tick_without_cheat_is_noop = function()
    setup()
    game.cheat = function() return nil end
    build.tick({}, { enabled = true, instantRepair = true })
  end,
})
