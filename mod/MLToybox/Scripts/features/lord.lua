local game = require("core.game")

-- 영주 전체 값(국고·영향력·왕의 총애). 영지별 자원과 달리 영주 한 명에게 하나뿐이라 별도 기능으로 관리한다.
-- 설정값이 없으면(nil) 관리하지 않고 현재값만 보고한다. 목표보다 적을 때만 채운다.
local M = { name = "lord", intervalSec = 2, TREASURY_PENDING_TIMEOUT = 30 }
M.clock = os.time

local function shortfall(current, target)
  if type(target) ~= "number" or type(current) ~= "number" then return 0 end
  if current < target then return target - current end
  return 0
end

function M.enable(state)
  state.treasuryPending = nil
  state.treasuryLastReading = nil
  state.lord = nil
end

-- ChangeTreasury 는 증감이고 HUD 값은 늦게(수 초) 반영되거나 로드 직후 카운트업 애니메이션을 한다.
-- 그래서 (1) 같은 값이 두 번 연속 읽힐 때만 보충하고, (2) 보충 후에는 HUD 값이 바뀌거나 시간이 초과될 때까지 다시 보충하지 않는다.
local function keepTreasury(state, target, treasury, now)
  local stable = state.treasuryLastReading == treasury
  state.treasuryLastReading = treasury
  local pending = state.treasuryPending
  if pending then
    if treasury ~= pending.before or now >= pending.deadline then
      state.treasuryPending = nil
    else
      return treasury + pending.added
    end
  end
  local need = shortfall(treasury, target)
  if need <= 0 or not stable then return treasury end
  local cheat = game.cheat()
  if not cheat then return treasury end
  local added = math.floor(need)
  cheat:ChangeTreasury(added)
  state.treasuryPending = { before = treasury, added = added, deadline = now + M.TREASURY_PENDING_TIMEOUT }
  return treasury + added
end

function M.tick(state, settings)
  settings = settings or {}
  local cur = {}

  local pawn = game.pawn()
  if pawn then
    local changed = false
    if shortfall(pawn.influence, settings.influence) > 0 then pawn.influence = settings.influence; changed = true end
    cur.influence = pawn.influence
    local need = shortfall(pawn.kingsFavour, settings.kingsFavour)
    if need > 0 then pawn:changeKingsFavour(math.floor(need)); changed = true end
    if changed then game.refreshLordHud() end
    cur.kingsFavour = pawn.kingsFavour
  end

  local treasury = game.treasury()
  if treasury then cur.treasury = keepTreasury(state, settings.treasury, treasury, M.clock()) end

  state.lord = cur
end

-- 기능 켜짐과 무관하게 현재값만 읽는다(main 이 매 루프 상태로 보고)
function M.read()
  local out = {}
  local pawn = game.pawn()
  if pawn then
    out.influence = pawn.influence
    out.kingsFavour = pawn.kingsFavour
  end
  out.treasury = game.treasury()
  return out
end

-- 일회성 명령 setLord { key, value }: 목표 유지와 달리 올리든 내리든 그 값으로 한 번 맞춘다
local SETTERS = {
  influence = function(value)
    local pawn = game.pawn()
    if not pawn then return "game objects not ready" end
    pawn.influence = value
    game.refreshLordHud()
  end,
  kingsFavour = function(value)
    local pawn = game.pawn()
    if not pawn then return "game objects not ready" end
    local delta = value - pawn.kingsFavour
    if delta ~= 0 then pawn:changeKingsFavour(delta) end
    game.refreshLordHud()
  end,
  treasury = function(value)
    local treasury, cheat = game.treasury(), game.cheat()
    if not treasury or not cheat then return "treasury not readable" end
    local delta = math.floor(value - treasury)
    if delta ~= 0 then cheat:ChangeTreasury(delta) end
  end,
}

function M.set(command, ctx)
  if not ctx.inGame then return { ok = false, error = "not in game" } end
  local setter = SETTERS[command.key]
  if not setter then return { ok = false, error = "unknown key: " .. tostring(command.key) } end
  local value = tonumber(command.value)
  if not value or value < 0 or value ~= math.floor(value) then return { ok = false, error = "value must be a non-negative integer" } end
  local err = setter(value)
  if err then return { ok = false, error = err } end
  return { ok = true }
end

return M
