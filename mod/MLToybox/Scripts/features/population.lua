local game = require("core.game")
local safe = require("core.safe")

-- 인구: ASMBuildingMaster.spawnManorServantsInside(1) 로 주거지에 가족을 들인다(집당 최대 2가족).
-- 모든 계산은 영지별이다: 배율은 그 영지의 자연 이민만큼 그 영지에, 목표 가족 수는 영지마다 최소값.
-- spawnManorServantsInside 는 새 가족을 그 집의 일꾼으로 배치하므로(일터 = 자기 집), 자연 이민처럼 미배치로 바꾼다.
local M = { name = "population", intervalSec = 5, MAX_FAMILIES_PER_HOUSE = 2, MAX_COMMAND = 20, MAX_MULTIPLIER = 10 }

-- 모드가 들인 가족 수(영지별). 다음 틱의 자연 이민 계산에서 빼서 배율이 다시 걸리지 않게 한다
local modAdded = {}

local function regionKey(r)
  local ok, tag = pcall(function() return r.regionUniqueTag:ToString() end)
  return ok and tag or nil
end

local function regionName(r)
  local ok, name = pcall(function() return r.regionName:ToString() end)
  return ok and name or nil
end

local function housesOf(region)
  local houses = {}
  for _, w in ipairs(region:GetBuildings()) do
    local b = game.unwrap(w)
    if safe.valid(b) and b:IsConstructed() and b:isResidentialBuilding() then houses[#houses + 1] = b end
  end
  return houses
end

local function freeSlotsOf(houses)
  local free = 0
  for _, b in ipairs(houses) do free = free + math.max(0, M.MAX_FAMILIES_PER_HOUSE - #b.occupantFamilyIDs) end
  return free
end

-- 영지 하나에 한 가족: 빈 집(가족 0)을 먼저, 그다음 가족 1인 집. 성공하면 true
local function addOne(region, houses)
  for pass = 0, M.MAX_FAMILIES_PER_HOUSE - 1 do
    for _, b in ipairs(houses) do
      if #b.occupantFamilyIDs == pass then
        local before = {}
        for i = 1, #b.occupantFamilyIDs do before[b.occupantFamilyIDs[i]] = true end
        b:spawnManorServantsInside(1)
        local ids = b.occupantFamilyIDs
        if #ids > pass then
          for i = 1, #ids do
            if not before[ids[i]] then region:unassignFamily(ids[i]) end
          end
          return true
        end
      end
    end
  end
  return false
end

-- n 가족을 들인다. regionKey 가 있으면 그 영지에만, 없으면 가족마다 빈 자리가 가장 많은 영지에. 실제로 들인 수를 돌려준다
function M.addFamilies(n, key)
  local entries = {}
  for _, r in ipairs(game.playerRegions()) do
    if key == nil or regionKey(r) == key then entries[#entries + 1] = { region = r, houses = housesOf(r) } end
  end
  local added = 0
  while added < n do
    local best, bestFree = nil, 0
    for _, e in ipairs(entries) do
      local free = freeSlotsOf(e.houses)
      if free > bestFree then best, bestFree = e, free end
    end
    if not best or not addOne(best.region, best.houses) then break end
    added = added + 1
    local k = regionKey(best.region)
    if k then modAdded[k] = (modAdded[k] or 0) + 1 end
  end
  return added
end

function M.enable(state)
  modAdded = {}
  state.populationLast = {}
  state.populationNatural = 0
  state.populationMultiplied = 0
  state.population = nil
end

-- 영지 하나의 지금 값
local function entryOf(r, key)
  return {
    key = key, name = regionName(r) or key,
    families = r:getTotalNumFamilies(), population = r:getNumTotalPopulation(),
    homeless = r:getNumHomelessFamilies(), freeSlots = freeSlotsOf(housesOf(r)),
    unassigned = r:getNumUnassignedFamilies(),
  }
end

-- 영지별 값과 합계를 state.population 에 적는다(키가 없는 영지는 합계에만 든다)
local function publish(state, entries)
  local totals = { families = 0, population = 0, homeless = 0, freeSlots = 0, unassigned = 0 }
  local perRegion = {}
  for _, entry in ipairs(entries) do
    if entry.key then perRegion[#perRegion + 1] = entry end
    for k in pairs(totals) do totals[k] = totals[k] + entry[k] end
  end
  totals.natural = state.populationNatural or 0        -- 이번 세션 누계
  totals.multiplied = state.populationMultiplied or 0
  totals.regions = perRegion
  state.population = totals
end

-- 꺼져 있는 동안 registry 가 부른다. [인구] 탭이 지금 값을 보이도록 읽기만 한다(가족을 들이지 않는다).
-- 배율의 기준(populationLast)은 두지 않는다: 다시 켜면 enable 이 기준을 비우므로, 꺼져 있는 동안 늘어난 가족에는 배율이 걸리지 않는다
function M.observe(state)
  local entries = {}
  for _, r in ipairs(game.playerRegions()) do entries[#entries + 1] = entryOf(r, regionKey(r)) end
  publish(state, entries)
end

function M.tick(state, settings)
  local multiplier = math.min(M.MAX_MULTIPLIER, math.max(1, math.floor(tonumber(settings.multiplier) or 1)))
  local common = tonumber(settings.targetFamilies)
  local overrides = settings.regionTargets or {}
  state.populationLast = state.populationLast or {}

  local entries = {}
  for _, r in ipairs(game.playerRegions()) do
    local key = regionKey(r)
    local families = r:getTotalNumFamilies()
    local last = key and state.populationLast[key]
    local grown = last and (families - last - (modAdded[key] or 0)) or 0
    if grown > 0 then
      state.populationNatural = (state.populationNatural or 0) + grown
      if multiplier > 1 then
        state.populationMultiplied = (state.populationMultiplied or 0) + M.addFamilies(grown * (multiplier - 1), key)
      end
    end
    local target = key and tonumber(overrides[key])
    if target == nil then target = common end
    if key and target and target > 0 then
      local current = r:getTotalNumFamilies()
      if current < target then M.addFamilies(target - current, key) end
    end

    local entry = entryOf(r, key)
    if key then
      state.populationLast[key] = entry.families
      modAdded[key] = 0
    end
    entries[#entries + 1] = entry
  end
  publish(state, entries)
end

-- 일회성 명령 addFamilies { count, region? } (core.commands 처리기). region 이 없으면 빈 자리가 많은 영지부터
function M.command(command, ctx)
  if not ctx.inGame then return { ok = false, error = "not in game" } end
  local count = tonumber(command.count)
  if not count or count ~= math.floor(count) or count < 1 or count > M.MAX_COMMAND then
    return { ok = false, error = "count must be 1.." .. M.MAX_COMMAND }
  end
  return { ok = true, requested = count, added = M.addFamilies(count, command.region) }
end

return M
