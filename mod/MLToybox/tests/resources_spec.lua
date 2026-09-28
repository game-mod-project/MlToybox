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

T.run({
  enable_publishes_ids_special_first = function()
    local st = {}
    resources.enable(st, {})
    T.eq(st.resourceIds[1], "RegionalWealth", "special first")
    T.eq(#st.resourceIds, #catalog.special + #catalog.items, "all ids")
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
  wealth_and_influence_raised_to_target = function()
    local r = region({}, 10)
    local pawn = F.object({ influence = 3 })
    world({ regions = { r }, pawn = pawn })
    local st = {}
    resources.tick(st, { targets = { RegionalWealth = 100, Influence = 50 } })
    T.eq(r.regionalWealth, 100, "wealth"); T.eq(pawn.influence, 50, "influence")
    T.eq(st.resources.RegionalWealth, 100, "wealth reported"); T.eq(st.resources.Influence, 50, "influence reported")
  end,
  treasury_topup_uses_delta = function()
    resources.clock = function() return 1000 end
    local cheat = world({ regions = {}, treasury = 300.0 })
    local st = {}
    resources.tick(st, { targets = { Treasury = 1000 } })
    T.eq(cheat.changes[1], 700, "delta"); T.eq(st.resources.Treasury, 1000, "reported as topped")
  end,
  treasury_topup_has_cooldown = function()
    local now = 2000
    resources.clock = function() return now end
    local cheat = world({ regions = {}, treasury = 300.0 })
    local st = {}
    resources.tick(st, { targets = { Treasury = 1000 } })
    now = 2002
    resources.tick(st, { targets = { Treasury = 1000 } })   -- HUD 가 아직 300 을 보여줘도 다시 더하지 않는다
    T.eq(#cheat.changes, 1, "once within cooldown")
    now = 2006
    resources.tick(st, { targets = { Treasury = 1000 } })
    T.eq(#cheat.changes, 2, "again after cooldown")
  end,
  tick_without_regions_is_noop = function()
    world({ regions = {} })
    local st = {}
    resources.tick(st, { targets = { Timber = 500, RegionalWealth = 10, Influence = 1, Treasury = 5 } })
    T.eq(st.resources.Timber, 0, "zero")
    T.eq(st.resources.Treasury, nil, "no hud -> not reported")
  end,
})
