local T = require("t")
local F = require("fakes")
local game = require("core.game")
local native = require("core.native")
local population = require("features.population")

-- 네이티브가 "내 영지의 월간 인구 변화를 바꾸는 일"을 맡았는가: true 맡음, false 못 맡음(DLL 없음), nil 아직 모름
local function nativeCan(v)
  if v == nil then native.last = nil
  elseif v then
    native.last = { loaded = true, stale = false, features = {
      immigration = { installed = true }, immigration_space = { installed = true }, immigration_owner = { installed = true } } }
  else native.last = { loaded = false, stale = true } end
end

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
  -- 네이티브가 못 맡을 때의 배율(예전 방식): 게임이 들인 만큼 모드가 따로 더 들인다
  multiplier_applies_per_region_to_that_region = function()
    nativeCan(false)
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
  command_added_families_are_not_multiplied_as_natural_growth = function()
    nativeCan(false)
    -- 실측: 명령으로 1가족 추가 → 다음 틱이 자연 이민으로 보고 배율 3 으로 2가족을 더 들였다
    local hof = world(region("hof", { 0, 0, 0, 0 }))[1]
    local st = {}
    population.enable(st, {})
    population.tick(st, { enabled = true, multiplier = 3 })   -- 기준선
    T.eq(population.command({ count = 1, region = "hof" }, IN_GAME).added, 1, "command added one")
    population.tick(st, { enabled = true, multiplier = 3 })
    T.eq(hof.spawned, 1, "no multiplier bonus for mod-added family")
    T.eq(st.population.natural, 0, "not counted as natural")
    hof.natural = 1
    population.tick(st, { enabled = true, multiplier = 3 })
    T.eq(hof.spawned, 3, "real natural growth still multiplied")
  end,
  -- 네이티브가 맡으면 게임이 직접 더 들인다(월간 인구 변화에 곱한다). 모드는 따로 들이지 않고 자연 이민만 센다
  multiplier_is_left_to_the_native_hook_when_it_is_installed = function()
    nativeCan(true)
    local hof = world(region("hof", { 0, 0, 0, 0 }))[1]
    local st = {}
    population.enable(st, {})
    population.tick(st, { enabled = true, multiplier = 3 })   -- 기준선
    hof.natural = 3                                           -- 게임이 배율만큼 들였다
    population.tick(st, { enabled = true, multiplier = 3 })
    T.eq(hof.spawned, 0, "the mod adds nothing on top"); T.eq(st.population.natural, 3, "arrivals counted"); T.eq(st.population.multiplied, 0, "nothing multiplied by the mod")
  end,
  -- 네이티브 상태를 아직 모르면 이번 틱에는 더 들이지 않는다(맡았는데 또 들이면 배율이 두 번 걸린다)
  multiplier_waits_while_the_native_state_is_unknown = function()
    nativeCan(nil)
    local hof = world(region("hof", { 0, 0, 0, 0 }))[1]
    local st = {}
    population.enable(st, {})
    population.tick(st, { enabled = true, multiplier = 3 })
    hof.natural = 1
    population.tick(st, { enabled = true, multiplier = 3 })
    T.eq(hof.spawned, 0, "nothing added while unknown")
  end,
  -- 목표 가족 수는 네이티브와 무관하게 모드가 채운다
  common_target_is_minimum_per_region = function()
    nativeCan(true)
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
    T.eq(p.unassigned, 0, "total unassigned reported")
  end,
  -- 꺼져 있는 동안: [인구] 탭이 지금 값을 보이도록 읽기만 한다
  observe_reports_numbers_without_adding_families = function()
    local hof, sel = table.unpack(world(region("hof", { 0, 1, 2 }, 10), region("sel", { 2 }, 4)))
    local st = {}
    population.observe(st, { enabled = false, multiplier = 3, targetFamilies = 50, regionTargets = { sel = 99 } })
    T.eq(hof.spawned + sel.spawned, 0, "nobody moved in")
    local p = st.population
    T.eq(p.families, 14, "total families"); T.eq(p.population, 42, "total population"); T.eq(p.freeSlots, 3, "total free")
    T.eq(#p.regions, 2, "two regions"); T.eq(p.regions[2].key, "sel", "key"); T.eq(p.regions[2].name, "SEL", "name")
    T.eq(p.regions[2].families, 4, "sel families"); T.eq(p.regions[1].freeSlots, 3, "hof free")
    T.eq(p.natural, 0, "no session count yet"); T.eq(p.multiplied, 0, "no session count yet")
  end,
  -- 켰다 끈 뒤: 값은 계속 따라가고 세션 누계는 그대로 보인다. 꺼져 있는 동안 늘어난 가족에는 다시 켜도 배율을 걸지 않는다
  observe_follows_the_game_after_the_feature_is_turned_off = function()
    nativeCan(false)
    local hof = world(region("hof", { 0, 0, 0, 0 }))[1]
    local st = {}
    population.enable(st, {})
    population.tick(st, { enabled = true, multiplier = 3 })   -- 기준선
    hof.natural = 1
    population.tick(st, { enabled = true, multiplier = 3 })
    T.eq(hof.spawned, 2, "multiplied while on")
    hof.natural = 2                                            -- 끈 뒤의 자연 이민
    population.observe(st, { enabled = false, multiplier = 3 })
    T.eq(hof.spawned, 2, "nothing added while off"); T.eq(st.population.families, 14, "follows the game")
    T.eq(st.population.natural, 1, "session count kept"); T.eq(st.population.multiplied, 2, "session count kept")
    population.enable(st, {})                                  -- 다시 켬
    population.tick(st, { enabled = true, multiplier = 3 })
    T.eq(hof.spawned, 2, "growth while off is the new baseline")
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
