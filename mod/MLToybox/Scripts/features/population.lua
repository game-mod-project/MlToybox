local game = require("core.game")
local safe = require("core.safe")

-- 인구: ASMBuildingMaster.spawnManorServantsInside(1) 로 주거지에 정상 규모 가족을 들인다(집당 최대 2가족).
-- 배율은 자연 이민으로 늘어난 가족 수를 관찰해 (배율-1)배를 추가로 들인다. 모드가 들인 가족에는 배율을 다시 걸지 않는다.
local M = { name = "population", intervalSec = 5, MAX_FAMILIES_PER_HOUSE = 2, MAX_COMMAND = 20, MAX_MULTIPLIER = 10 }

local function residentialHouses()
  local houses, regionOf = {}, {}
  for _, region in ipairs(game.playerRegions()) do
    for _, w in ipairs(region:GetBuildings()) do
      local b = game.unwrap(w)
      if safe.valid(b) and b:IsConstructed() and b:isResidentialBuilding() then
        houses[#houses + 1] = b
        regionOf[b] = region
      end
    end
  end
  return houses, regionOf
end

local function idSet(arr)
  local s = {}
  for i = 1, #arr do s[arr[i]] = true end
  return s
end

local function countFamilies()
  local families, pop, homeless = 0, 0, 0
  for _, region in ipairs(game.playerRegions()) do
    families = families + region:getTotalNumFamilies()
    pop = pop + region:getNumTotalPopulation()
    homeless = homeless + region:getNumHomelessFamilies()
  end
  return families, pop, homeless
end

-- 빈 집(가족 0)부터, 그다음 가족 1인 집을 채운다. 실제로 들인 수를 돌려준다.
-- spawnManorServantsInside 는 새 가족을 그 집의 일꾼으로 배치하므로(일터 = 자기 집), 자연 이민처럼 미배치로 바꾼다.
function M.addFamilies(n)
  local added = 0
  for pass = 0, M.MAX_FAMILIES_PER_HOUSE - 1 do
    local houses, regionOf = residentialHouses()
    for _, b in ipairs(houses) do
      if added >= n then return added end
      if #b.occupantFamilyIDs == pass then
        local before = idSet(b.occupantFamilyIDs)
        local count = #b.occupantFamilyIDs
        b:spawnManorServantsInside(1)
        if #b.occupantFamilyIDs > count then
          added = added + 1
          local ids = b.occupantFamilyIDs
          for i = 1, #ids do
            if not before[ids[i]] then regionOf[b]:unassignFamily(ids[i]) end
          end
        end
      end
    end
  end
  return added
end

local function freeSlots()
  local free = 0
  for _, b in ipairs(residentialHouses()) do
    free = free + math.max(0, M.MAX_FAMILIES_PER_HOUSE - #b.occupantFamilyIDs)
  end
  return free
end

function M.enable(state)
  state.populationLast = nil
  state.populationNatural = 0
  state.populationMultiplied = 0
  state.population = nil
end

function M.tick(state, settings)
  local multiplier = math.min(M.MAX_MULTIPLIER, math.max(1, math.floor(tonumber(settings.multiplier) or 1)))
  local families = countFamilies()
  if state.populationLast and families > state.populationLast then
    local grown = families - state.populationLast
    state.populationNatural = (state.populationNatural or 0) + grown
    if multiplier > 1 then
      state.populationMultiplied = (state.populationMultiplied or 0) + M.addFamilies(grown * (multiplier - 1))
    end
  end
  local target = tonumber(settings.targetFamilies)
  if target and target > 0 then
    local current = countFamilies()
    if current < target then M.addFamilies(target - current) end
  end
  local f, pop, homeless = countFamilies()
  state.populationLast = f
  state.population = {
    families = f, population = pop, homeless = homeless, freeSlots = freeSlots(),
    natural = state.populationNatural or 0, multiplied = state.populationMultiplied or 0,   -- 이번 세션 누계
  }
end

-- 일회성 명령 addFamilies { count } (core.commands 처리기)
function M.command(command, ctx)
  if not ctx.inGame then return { ok = false, error = "not in game" } end
  local count = tonumber(command.count)
  if not count or count ~= math.floor(count) or count < 1 or count > M.MAX_COMMAND then
    return { ok = false, error = "count must be 1.." .. M.MAX_COMMAND }
  end
  return { ok = true, requested = count, added = M.addFamilies(count) }
end

return M
