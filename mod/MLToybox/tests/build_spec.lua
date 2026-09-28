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
  instant_build_triggers_progress_read_on_unbuilt = function()
    local _, unbuilt, built = setup()
    local calls = 0
    unbuilt.getConstructionProgress = function() calls = calls + 1; return 0.5 end
    built.getConstructionProgress = function() error("must not be called on built") end
    build.tick({}, { enabled = true, instantBuild = true })
    T.eq(calls, 1, "triggered once")
  end,
  tick_without_cheat_is_noop = function()
    setup()
    game.cheat = function() return nil end
    build.tick({}, { enabled = true, instantRepair = true })
  end,
})
