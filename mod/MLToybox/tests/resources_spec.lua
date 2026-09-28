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
  treasury_topup_uses_delta_after_stable_reading = function()
    resources.clock = function() return 1000 end
    local cheat = world({ regions = {}, treasury = 300.0 })
    local st = {}
    resources.tick(st, { targets = { Treasury = 1000 } })
    T.eq(#cheat.changes, 0, "first reading only observed")
    resources.tick(st, { targets = { Treasury = 1000 } })
    T.eq(cheat.changes[1], 700, "delta once reading is stable"); T.eq(st.resources.Treasury, 1000, "reported as topped")
  end,
  treasury_stale_hud_never_double_adds = function()
    local now = 2000
    resources.clock = function() return now end
    local opts = { regions = {}, treasury = 300.0 }
    local cheat = world(opts)
    local st = {}
    for _ = 1, 8 do resources.tick(st, { targets = { Treasury = 1000 } }); now = now + 2 end  -- HUD 가 16초 동안 300 그대로
    T.eq(#cheat.changes, 1, "single top-up while HUD is stale")
    opts.treasury = 1000.0                                                                    -- HUD 반영
    for _ = 1, 2 do resources.tick(st, { targets = { Treasury = 1000 } }); now = now + 2 end
    T.eq(#cheat.changes, 1, "no more once target reached")
    opts.treasury = 400.0                                                                     -- 이후 소비
    for _ = 1, 2 do resources.tick(st, { targets = { Treasury = 1000 } }); now = now + 2 end
    T.eq(#cheat.changes, 2, "tops up again after HUD moved and settled")
  end,
  treasury_unsettled_hud_is_ignored = function()
    resources.clock = function() return 3000 end
    local opts = { regions = {}, treasury = 0.0 }
    local cheat = world(opts)
    local st = {}
    for _, v in ipairs({ 0.0, 12000.0, 45000.0, 66000.0 }) do opts.treasury = v; resources.tick(st, { targets = { Treasury = 70000 } }) end
    T.eq(#cheat.changes, 0, "no top-up while HUD value keeps changing (count-up after load)")
  end,
  treasury_pending_expires = function()
    local now = 5000
    resources.clock = function() return now end
    local cheat = world({ regions = {}, treasury = 300.0 })
    local st = {}
    resources.tick(st, { targets = { Treasury = 1000 } }); resources.tick(st, { targets = { Treasury = 1000 } })
    now = now + resources.TREASURY_PENDING_TIMEOUT + 1
    resources.tick(st, { targets = { Treasury = 1000 } })
    T.eq(#cheat.changes, 2, "retries after pending timeout if HUD never moved")
  end,
  tick_without_regions_is_noop = function()
    world({ regions = {} })
    local st = {}
    resources.tick(st, { targets = { Timber = 500, RegionalWealth = 10, Influence = 1, Treasury = 5 } })
    T.eq(st.resources.Timber, 0, "zero")
    T.eq(st.resources.Treasury, nil, "no hud -> not reported")
  end,
})
