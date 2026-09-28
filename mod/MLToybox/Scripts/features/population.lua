local game = require("core.game")
local safe = require("core.safe")

-- 인구: ASMBuildingMaster.spawnManorServantsInside(1) 로 주거지에 정상 규모 가족을 들인다(집당 최대 2가족).
-- 배율은 자연 이민으로 늘어난 가족 수를 관찰해 (배율-1)배를 추가로 들인다. 모드가 들인 가족에는 배율을 다시 걸지 않는다.
local M = { name = "population", intervalSec = 5, MAX_FAMILIES_PER_HOUSE = 2, MAX_COMMAND = 20, MAX_MULTIPLIER = 10 }

local function residentialHouses()
  local houses = {}
  for _, region in ipairs(game.playerRegions()) do
    for _, w in ipairs(region:GetBuildings()) do
      local b = game.unwrap(w)
      if safe.valid(b) and b:IsConstructed() and b:isResidentialBuilding() then houses[#houses + 1] = b end
    end
  end
  return houses
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
function M.addFamilies(n)
  local added = 0
  for pass = 0, M.MAX_FAMILIES_PER_HOUSE - 1 do
    for _, b in ipairs(residentialHouses()) do
      if added >= n then return added end
      if #b.occupantFamilyIDs == pass then
        local before = #b.occupantFamilyIDs
        b:spawnManorServantsInside(1)
        if #b.occupantFamilyIDs > before then added = added + 1 end
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
  state.population = nil
end

function M.tick(state, settings)
  local multiplier = math.min(M.MAX_MULTIPLIER, math.max(1, math.floor(tonumber(settings.multiplier) or 1)))
  local families = countFamilies()
  if state.populationLast and families > state.populationLast and multiplier > 1 then
    M.addFamilies((families - state.populationLast) * (multiplier - 1))
  end
  local target = tonumber(settings.targetFamilies)
  if target and target > 0 then
    local current = countFamilies()
    if current < target then M.addFamilies(target - current) end
  end
  local f, pop, homeless = countFamilies()
  state.populationLast = f
  state.population = { families = f, population = pop, homeless = homeless, freeSlots = freeSlots() }
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
