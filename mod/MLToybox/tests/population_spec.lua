local T = require("t")
local F = require("fakes")
local game = require("core.game")
local population = require("features.population")

local nextId = 100

-- 가짜 영지: 집마다 거주 가족 id 목록을 들고, spawnManorServantsInside(1) 이 호출되면 새 id 를 넣는다(최대 2)
local function region(tag, houseOccupants, base)
  local r = F.object({ spawned = 0, unassigned = {}, natural = 0, base = base or 10 })
  r.regionUniqueTag = { ToString = function() return tag end }
  r.regionName = { ToString = function() return tag:upper() end }
  r.unassignFamily = function(self, id) self.unassigned[#self.unassigned + 1] = id end
  local houses = {}
  for i, occ in ipairs(houseOccupants) do
    local h = F.object({ occupantFamilyIDs = F.array({}), calls = 0 })
    for k = 1, occ do h.occupantFamilyIDs[k] = k end
    h.IsConstructed = function() return true end
    h.isResidentialBuilding = function() return true end
    h.spawnManorServantsInside = function(self, n)
      self.calls = self.calls + 1
      for _ = 1, n do
        if #self.occupantFamilyIDs < 2 then
          nextId = nextId + 1
          self.occupantFamilyIDs[#self.occupantFamilyIDs + 1] = nextId
          r.spawned = r.spawned + 1
        end
      end
    end
    houses[i] = h
  end
  r.houses = houses
  r.GetBuildings = function() local out = {} for i, h in ipairs(houses) do out[i] = F.wrap(h) end return out end
  r.getTotalNumFamilies = function(self) return self.base + self.natural + self.spawned end
  r.getNumTotalPopulation = function(self) return 3 * self:getTotalNumFamilies() end
  r.getNumHomelessFamilies = function() return 0 end
  r.getNumUnassignedFamilies = function(self) return #self.unassigned end
  return r
end

local function world(...)
  local regions = { ... }
  game.playerRegions = function() return regions end
  return regions
end

local IN_GAME = { inGame = true }

T.run({
  added_families_are_unassigned_not_working_at_their_house = function()
    local r = world(region("hof", { 1, 0 }))[1]
    T.eq(population.addFamilies(2), 2, "added")
    T.eq(#r.unassigned, 2, "both unassigned")
    T.eq(r.unassigned[1], r.houses[2].occupantFamilyIDs[1], "new family id")
    for _, id in ipairs(r.unassigned) do T.truthy(id > 100, "never the existing family") end
  end,
  add_families_fills_empty_houses_first = function()
    local r = world(region("hof", { 1, 0, 0 }))[1]
    T.eq(population.addFamilies(2), 2, "added two")
    T.eq(#r.houses[1].occupantFamilyIDs, 1, "half-full house untouched")
    T.eq(#r.houses[2].occupantFamilyIDs, 1, "empty 1"); T.eq(#r.houses[3].occupantFamilyIDs, 1, "empty 2")
  end,
  add_families_stops_when_full = function()
    local r = world(region("hof", { 1, 2 }))[1]
    T.eq(population.addFamilies(5), 1, "only one free slot")
    T.eq(r.houses[2].calls, 0, "full house skipped")
  end,
  add_families_to_one_region_only = function()
    local hof, sel = table.unpack(world(region("hof", { 0, 0 }), region("sel", { 0, 0 })))
    T.eq(population.addFamilies(2, "sel"), 2, "added")
    T.eq(sel.spawned, 2, "sel got them"); T.eq(hof.spawned, 0, "hof untouched")
    T.eq(population.addFamilies(1, "nope"), 0, "unknown region adds nothing")
  end,
  add_families_common_spreads_to_region_with_most_free_slots = function()
    local hof, sel = table.unpack(world(region("hof", { 2, 1 }), region("sel", { 0, 0, 0 })))   -- 빈 자리 1 vs 6
    T.eq(population.addFamilies(3), 3, "added")
    T.eq(sel.spawned, 3, "all to the emptier region while it has more room"); T.eq(hof.spawned, 0, "none to hof")
    population.addFamilies(4)                                                                      -- sel 빈 자리 3, hof 1
    T.eq(sel.spawned + hof.spawned, 7, "total"); T.eq(hof.spawned, 1, "hof gets one once sel is no roomier")
  end,
  multiplier_applies_per_region_to_that_region = function()
    local hof, sel = table.unpack(world(region("hof", { 0, 0, 0 }), region("sel", { 0, 0, 0 })))
    local st = {}
    population.enable(st, {})
    population.tick(st, { enabled = true, multiplier = 3 })   -- 기준선
    sel.natural = 1                                           -- sel 에만 자연 이민 1가족
    population.tick(st, { enabled = true, multiplier = 3 })
    T.eq(sel.spawned, 2, "1 x (3-1) extra into sel"); T.eq(hof.spawned, 0, "hof untouched")
    population.tick(st, { enabled = true, multiplier = 3 })
    T.eq(sel.spawned, 2, "mod-added families are not multiplied again")
    T.eq(st.population.natural, 1, "natural counted"); T.eq(st.population.multiplied, 2, "multiplied counted")
  end,
  common_target_is_minimum_per_region = function()
    local hof, sel = table.unpack(world(region("hof", { 0, 0 }, 10), region("sel", { 0, 0 }, 11)))
    local st = {}
    population.enable(st, {})
    population.tick(st, { enabled = true, multiplier = 1, targetFamilies = 12 })
    T.eq(hof.spawned, 2, "hof 10 -> 12"); T.eq(sel.spawned, 1, "sel 11 -> 12")
    population.tick(st, { enabled = true, multiplier = 1, targetFamilies = 5 })
    T.eq(hof.spawned + sel.spawned, 3, "never removes")
  end,
  region_target_overrides_common = function()
    local hof, sel = table.unpack(world(region("hof", { 0, 0 }, 10), region("sel", { 0, 0 }, 10)))
    local st = {}
    population.enable(st, {})
    population.tick(st, { enabled = true, multiplier = 1, targetFamilies = 11, regionTargets = { sel = 13, hof = 0 } })
    T.eq(sel.spawned, 3, "sel override 13"); T.eq(hof.spawned, 0, "hof override 0 = off")
  end,
  status_reports_totals_and_each_region = function()
    world(region("hof", { 0, 1, 2 }, 10), region("sel", { 2 }, 4))
    local st = {}
    population.enable(st, {})
    population.tick(st, { enabled = true, multiplier = 1 })
    local p = st.population
    T.eq(p.families, 14, "total families"); T.eq(p.freeSlots, 3, "total free")
    T.eq(#p.regions, 2, "two regions")
    T.eq(p.regions[1].key, "hof", "key"); T.eq(p.regions[1].name, "HOF", "name")
    T.eq(p.regions[1].freeSlots, 3, "hof free"); T.eq(p.regions[2].freeSlots, 0, "sel full")
    T.eq(p.regions[2].families, 4, "sel families"); T.eq(p.regions[1].unassigned, 0, "unassigned")
  end,
  command_validates_and_targets_region = function()
    local hof, sel = table.unpack(world(region("hof", { 1 }), region("sel", { 1, 1 })))
    T.eq(population.command({ count = 0 }, IN_GAME).ok, false, "zero")
    T.eq(population.command({ count = 21 }, IN_GAME).ok, false, "too many")
    T.eq(population.command({ count = 1 }, { inGame = false }).ok, false, "not in game")
    local r = population.command({ count = 3, region = "sel" }, IN_GAME)
    T.eq(r.ok, true, "ok"); T.eq(r.added, 2, "limited by sel slots"); T.eq(r.requested, 3, "requested")
    T.eq(hof.spawned, 0, "hof untouched"); T.eq(sel.spawned, 2, "sel filled")
  end,
  tick_without_regions_is_noop = function()
    world()
    local st = {}
    population.enable(st, {})
    population.tick(st, { enabled = true, multiplier = 5, targetFamilies = 50 })
    T.eq(st.population.families, 0, "zero")
  end,
})
