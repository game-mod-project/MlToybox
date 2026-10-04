local game = require("core.game")
local native = require("core.native")
local safe = require("core.safe")

-- 인구: ASMBuildingMaster.spawnManorServantsInside(1) 로 주거지에 가족을 들인다(집당 최대 2가족).
-- 모든 계산은 영지별이다: 목표 가족 수는 영지마다 최소값.
-- 자연 이민의 수와 속도(settings.monthlyFamilies, settings.multiplier)는 네이티브 DLL 이 게임의 월간 인구 변화를 내 영지에서만 바꿔서 한다
-- (findings "자연 이민 — 게임의 방식"). 네이티브가 못 맡으면 배율만 예전 방식으로 한다: 그 영지의 자연 이민만큼 그 영지에 따로 더 들인다.
-- spawnManorServantsInside 는 새 가족을 그 집의 일꾼으로 배치하므로(일터 = 자기 집), 자연 이민처럼 미배치로 바꾼다.
local M = { name = "population", intervalSec = 5, MAX_FAMILIES_PER_HOUSE = 2, MAX_COMMAND = 20, MAX_MULTIPLIER = 10 }
-- 네이티브가 월간 인구 변화를 맡을 때 설치돼 있어야 하는 항목(core/native.lua 의 installed)
M.SCOPE = { "immigration", "immigration_space", "immigration_owner" }

-- 모드가 들인 가족 수(영지별). 다음 틱의 자연 이민 계산에서 빼서 배율이 다시 걸리지 않게 한다
local modAdded = {}

local regionKey, regionName = game.regionKey, game.regionName

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

local function freeIn(count) return math.max(0, M.MAX_FAMILIES_PER_HOUSE - count) end

-- 영지 하나의 집과, 집마다의 가족 수와, 빈 자리의 합. 가족 목록은 리플렉션으로 읽으므로 한 번만 읽어 두고,
-- 가족을 들일 때 그 집의 수만 고친다(가족마다 모든 집을 다시 읽으면 집과 가족이 많을 때 게임이 멈칫한다)
local function entryFor(region)
  local e = { region = region, houses = housesOf(region), counts = {}, free = 0 }
  for i, b in ipairs(e.houses) do
    e.counts[i] = #b.occupantFamilyIDs
    e.free = e.free + freeIn(e.counts[i])
  end
  return e
end

-- 영지 하나에 한 가족: 빈 집(가족 0)을 먼저, 그다음 가족 1인 집. 성공하면 true
local function addOne(e)
  for pass = 0, M.MAX_FAMILIES_PER_HOUSE - 1 do
    for i, b in ipairs(e.houses) do
      if e.counts[i] == pass then
        local before, ids = {}, b.occupantFamilyIDs
        for k = 1, #ids do before[ids[k]] = true end
        b:spawnManorServantsInside(1)
        ids = b.occupantFamilyIDs
        local count = #ids
        if count > pass then
          for k = 1, count do
            if not before[ids[k]] then e.region:unassignFamily(ids[k]) end
          end
          e.free = e.free - freeIn(pass) + freeIn(count)
          e.counts[i] = count
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
    if key == nil or regionKey(r) == key then entries[#entries + 1] = entryFor(r) end
  end
  local added = 0
  while added < n do
    local best, bestFree = nil, 0
    for _, e in ipairs(entries) do
      if e.free > bestFree then best, bestFree = e, e.free end
    end
    if not best or not addOne(best) then break end
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
  -- 네이티브가 맡았으면 게임이 직접 더 들인다. 못 맡을 때만 여기서 들이고, 아직 모르면 이번에는 넘어간다(둘 다 하면 배율이 두 번 걸린다)
  local addHere = multiplier > 1 and native.installed(M.SCOPE) == false

  local entries = {}
  for _, r in ipairs(game.playerRegions()) do
    local key = regionKey(r)
    local families = r:getTotalNumFamilies()
    local last = key and state.populationLast[key]
    local grown = last and (families - last - (modAdded[key] or 0)) or 0
    if grown > 0 then
      state.populationNatural = (state.populationNatural or 0) + grown
      if addHere then
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
