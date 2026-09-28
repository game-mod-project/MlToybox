local game = require("core.game")
local catalog = require("features.resources_catalog")

local M = { name = "resources", intervalSec = 2, TREASURY_COOLDOWN = 5 }
M.clock = os.time

local function shortfall(current, target)
  if type(target) ~= "number" then return 0 end
  if current < target then return target - current end
  return 0
end

function M.enable(state)
  state.resourceIds = catalog.ids()
  state.resources = {}
  state.treasuryCooldownUntil = nil
end

function M.tick(state, settings)
  local targets = (settings and settings.targets) or {}
  local regions = game.playerRegions()
  local cur = {}

  for _, it in ipairs(catalog.items) do
    local total = 0
    for _, r in ipairs(regions) do
      local stock = r:getStockOfGood(it.type, false, false)
      local need = shortfall(stock, targets[it.id])
      if need > 0 then
        r:grantResources({ { Type = it.type, amt = need } }, false)
        stock = stock + need
      end
      total = total + stock
    end
    cur[it.id] = total
  end

  local wealth = 0
  for _, r in ipairs(regions) do
    if shortfall(r.regionalWealth, targets.RegionalWealth) > 0 then r.regionalWealth = targets.RegionalWealth end
    wealth = wealth + r.regionalWealth
  end
  cur.RegionalWealth = wealth

  local pawn = game.pawn()
  if pawn then
    if shortfall(pawn.influence, targets.Influence) > 0 then pawn.influence = targets.Influence end
    cur.Influence = pawn.influence
  end

  -- ChangeTreasury 는 증감이고 HUD 반영이 늦으므로 보충 후 쿨다운 동안 다시 더하지 않는다
  local treasury = game.treasury()
  if treasury then
    local now = M.clock()
    local need = shortfall(treasury, targets.Treasury)
    local cooling = state.treasuryCooldownUntil and now < state.treasuryCooldownUntil
    if need > 0 and not cooling then
      local cheat = game.cheat()
      if cheat then
        cheat:ChangeTreasury(math.floor(need))
        state.treasuryCooldownUntil = now + M.TREASURY_COOLDOWN
        treasury = treasury + need
      end
    end
    cur.Treasury = treasury
  end

  state.resources = cur
end

return M
