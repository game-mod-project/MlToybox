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

local function named(r, tag, name)
  r.regionUniqueTag = { ToString = function() return tag end }
  r.regionName = { ToString = function() return name end }
  return r
end

T.run({
  region_target_overrides_common_target = function()
    local hof = named(region({ [16] = 100 }), "hof", "Klainau")
    local sel = named(region({ [16] = 100 }), "sel", "Furdau")
    world({ regions = { hof, sel } })
    resources.tick({}, { targets = { Timber = 500 }, regionTargets = { hof = { Timber = 2000 } } })
    T.eq(hof.stock[16], 2000, "override"); T.eq(sel.stock[16], 500, "common for other region")
  end,
  region_target_zero_stops_topping_up_that_region = function()
    local hof = named(region({ [16] = 100 }), "hof", "Klainau")
    world({ regions = { hof } })
    resources.tick({}, { targets = { Timber = 500 }, regionTargets = { hof = { Timber = 0 } } })
    T.eq(#hof.grants, 0, "zero override means no top-up")
  end,
  region_only_target_without_common = function()
    local hof = named(region({ [16] = 100 }), "hof", "Klainau")
    local sel = named(region({ [16] = 100 }), "sel", "Furdau")
    world({ regions = { hof, sel } })
    resources.tick({}, { targets = {}, regionTargets = { sel = { Timber = 300 } } })
    T.eq(hof.stock[16], 100, "unmanaged"); T.eq(sel.stock[16], 300, "region only")
  end,
  region_wealth_uses_region_target = function()
    local hof = named(region({}, 10), "hof", "Klainau")
    local sel = named(region({}, 10), "sel", "Furdau")
    world({ regions = { hof, sel } })
    resources.tick({}, { targets = { RegionalWealth = 100 }, regionTargets = { sel = { RegionalWealth = 900 } } })
    T.eq(hof.regionalWealth, 100, "common"); T.eq(sel.regionalWealth, 900, "override")
  end,
  reports_per_region_stock_with_names = function()
    local hof = named(region({ [16] = 700 }, 5), "hof", "Klainau")
    local sel = named(region({ [16] = 40 }, 6), "sel", "Furdau")
    world({ regions = { hof, sel } })
    local st = {}
    resources.tick(st, { targets = {} })
    T.eq(#st.regions, 2, "two regions")
    T.eq(st.regions[1].key, "hof", "key"); T.eq(st.regions[1].name, "Klainau", "name")
    T.eq(st.regions[1].values.Timber, 700, "hof timber"); T.eq(st.regions[2].values.Timber, 40, "sel timber")
    T.eq(st.regions[2].values.RegionalWealth, 6, "wealth per region")
    T.eq(st.resources.Timber, 740, "total still reported")
  end,
  enable_publishes_ids_special_first = function()
    local st = {}
    resources.enable(st, {})
    T.eq(st.resourceIds[1], "RegionalWealth", "special first")
    T.eq(#st.resourceIds, #catalog.special + #catalog.items, "all ids")
  end,
  -- 꺼져 있는 동안: 자원 탭이 영지 목록과 현재 값을 보이도록 읽기만 한다
  observe_reports_regions_and_stock_without_granting = function()
    local hof = named(region({ [16] = 100 }, 5), "hof", "Klainau")
    world({ regions = { hof } })
    local st = {}
    resources.observe(st, { enabled = false, targets = { Timber = 500, RegionalWealth = 900 }, regionTargets = { hof = { Timber = 2000 } } })
    T.eq(#hof.grants, 0, "nothing granted"); T.eq(hof.stock[16], 100, "stock untouched"); T.eq(hof.regionalWealth, 5, "wealth untouched")
    T.eq(#st.regions, 1, "region listed"); T.eq(st.regions[1].key, "hof", "key"); T.eq(st.regions[1].name, "Klainau", "name")
    T.eq(st.regions[1].values.Timber, 100, "stock reported"); T.eq(st.resources.Timber, 100, "total reported")
    T.eq(#st.resourceIds, #catalog.special + #catalog.items, "ids for the table")
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
  wealth_raised_to_target = function()
    local r = region({}, 10)
    world({ regions = { r } })
    local st = {}
    resources.tick(st, { targets = { RegionalWealth = 100 } })
    T.eq(r.regionalWealth, 100, "wealth"); T.eq(st.resources.RegionalWealth, 100, "wealth reported")
  end,
  lord_wide_values_are_not_handled_here = function()
    -- 국고·영향력은 lord 기능이 관리한다(예전 control.json 의 targets.Treasury/Influence 는 무시)
    local pawn = F.object({ influence = 3 })
    local cheat = world({ regions = {}, pawn = pawn, treasury = 10.0 })
    local st = {}
    resources.tick(st, { targets = { Treasury = 1000, Influence = 50 } })
    resources.tick(st, { targets = { Treasury = 1000, Influence = 50 } })
    T.eq(pawn.influence, 3, "influence untouched"); T.eq(#cheat.changes, 0, "treasury untouched")
    T.eq(st.resources.Treasury, nil, "not reported"); T.eq(st.resources.Influence, nil, "not reported")
    T.eq(#catalog.special, 1, "only RegionalWealth is special")
  end,
  tick_without_regions_is_noop = function()
    world({ regions = {} })
    local st = {}
    resources.tick(st, { targets = { Timber = 500, RegionalWealth = 10 } })
    T.eq(st.resources.Timber, 0, "zero")
  end,
})
