local T = require("t")
local F = require("fakes")
local game = require("core.game")
local population = require("features.population")

-- 가짜 지역: 집마다 거주 가족 수를 들고, spawnManorServantsInside(1) 이 호출되면 +1 (최대 2)
local function world(houseOccupants, extra)
  local region = F.object({ spawned = 0 })
  local houses = {}
  for i, occ in ipairs(houseOccupants) do
    local h = F.object({ occupantFamilyIDs = F.array({}), calls = 0 })
    for k = 1, occ do h.occupantFamilyIDs[k] = k end
    h.IsConstructed = function() return true end
    h.isResidentialBuilding = function() return true end
    h.spawnManorServantsInside = function(self, n)
      self.calls = self.calls + 1
      for _ = 1, n do
        if #self.occupantFamilyIDs < 2 then self.occupantFamilyIDs[#self.occupantFamilyIDs + 1] = 99; region.spawned = region.spawned + 1 end
      end
    end
    houses[i] = h
  end
  local natural = (extra and extra.natural) or 0
  region.GetBuildings = function() local out = {} for i, h in ipairs(houses) do out[i] = F.wrap(h) end return out end
  region.getTotalNumFamilies = function() return 10 + natural + region.spawned end
  region.getNumTotalPopulation = function() return 30 end
  region.getNumHomelessFamilies = function() return 0 end
  region.setNatural = function(n) natural = n end
  game.playerRegions = function() return { region } end
  return region, houses
end

T.run({
  add_families_fills_empty_houses_first = function()
    local _, houses = world({ 1, 0, 0 })
    local added = population.addFamilies(2)
    T.eq(added, 2, "added two")
    T.eq(#houses[1].occupantFamilyIDs, 1, "half-full house untouched")
    T.eq(#houses[2].occupantFamilyIDs, 1, "empty 1 filled"); T.eq(#houses[3].occupantFamilyIDs, 1, "empty 2 filled")
  end,
  add_families_then_uses_half_full_houses_and_stops_when_full = function()
    local _, houses = world({ 1, 2 })
    T.eq(population.addFamilies(5), 1, "only one free slot")
    T.eq(#houses[1].occupantFamilyIDs, 2, "filled to two"); T.eq(houses[2].calls, 0, "full house skipped")
  end,
  multiplier_adds_extra_for_natural_growth_only = function()
    local region = world({ 0, 0, 0, 0, 0, 0 })
    local st = {}
    population.enable(st, {})
    population.tick(st, { enabled = true, multiplier = 3 })   -- 기준선만 기록
    T.eq(region.spawned, 0, "baseline")
    region.setNatural(1)                                      -- 자연 이민 1가족
    population.tick(st, { enabled = true, multiplier = 3 })
    T.eq(region.spawned, 2, "1 x (3-1) extra")
    population.tick(st, { enabled = true, multiplier = 3 })
    T.eq(region.spawned, 2, "mod-added families are not multiplied again")
  end,
  target_families_tops_up_shortfall = function()
    local region = world({ 0, 0, 0 })
    local st = {}
    population.enable(st, {})
    population.tick(st, { enabled = true, multiplier = 1, targetFamilies = 12 })
    T.eq(region.spawned, 2, "10 -> 12")
    population.tick(st, { enabled = true, multiplier = 1, targetFamilies = 5 })
    T.eq(region.spawned, 2, "never removes")
  end,
  status_reports_counts_and_free_slots = function()
    world({ 0, 1, 2 })
    local st = {}
    population.enable(st, {})
    population.tick(st, { enabled = true, multiplier = 1 })
    T.eq(st.population.families, 10, "families"); T.eq(st.population.population, 30, "pop")
    T.eq(st.population.homeless, 0, "homeless"); T.eq(st.population.freeSlots, 3, "2 + 1 + 0")
  end,
  command_validates_and_reports = function()
    world({ 1, 1 })   -- 빈 자리 2개
    T.eq(population.command({ count = 0 }, { inGame = true }).ok, false, "zero")
    T.eq(population.command({ count = 21 }, { inGame = true }).ok, false, "too many")
    T.eq(population.command({ count = 1 }, { inGame = false }).ok, false, "not in game")
    local r = population.command({ count = 3 }, { inGame = true })
    T.eq(r.ok, true, "ok"); T.eq(r.added, 2, "limited by slots"); T.eq(r.requested, 3, "requested")
  end,
  tick_without_regions_is_noop = function()
    game.playerRegions = function() return {} end
    local st = {}
    population.enable(st, {})
    population.tick(st, { enabled = true, multiplier = 5, targetFamilies = 50 })
    T.eq(st.population.families, 0, "zero")
  end,
})
