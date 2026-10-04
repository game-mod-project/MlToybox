local game = require("core.game")

-- 영주 전체 값(국고·영향력·왕의 총애). 영지별 자원과 달리 영주 한 명에게 하나뿐이라 별도 기능으로 관리한다.
-- 설정값이 없으면(nil) 관리하지 않는다. 목표보다 적을 때만 채운다. 현재값은 read() 로 보고한다.
local M = { name = "lord", intervalSec = 2, TREASURY_PENDING_TIMEOUT = 30 }
M.clock = os.time

local function shortfall(current, target)
  if type(target) ~= "number" or type(current) ~= "number" then return 0 end
  if current < target then return target - current end
  return 0
end

-- 국고는 HUD 숫자로만 읽을 수 있고, ChangeTreasury(증감)로 바꾼 값은 HUD 에 늦게(수 초) 나타나거나 숫자가 올라가는 애니메이션을 거친다.
-- 그래서 모드가 바꾼 뒤의 값을 기억해 두고, HUD 가 그 값이 되거나 시간이 다 될 때까지 그 값을 국고로 본다.
-- 그러지 않으면 HUD 가 따라오기 전의 다음 변경(목표 유지의 보충, '지금 설정')이 옛 숫자를 기준으로 한 번 더 더한다
local ledger = nil   -- { value = 모드가 바꾼 뒤의 국고, deadline }

-- 맵을 떠나면 잊는다(main.lua 가 부른다). 다음 맵의 국고는 다른 값이다
function M.forget() ledger = nil end

local function believedTreasury(hud, now)
  if ledger then
    if hud ~= ledger.value and now < ledger.deadline then return ledger.value end
    ledger = nil   -- HUD 가 따라왔거나 너무 오래 기다렸다. 다시 HUD 를 믿는다
  end
  return hud
end

local function changeTreasury(cheat, from, delta, now)
  cheat:ChangeTreasury(delta)
  ledger = { value = from + delta, deadline = now + M.TREASURY_PENDING_TIMEOUT }
end

function M.enable(state)
  state.treasuryLastReading = nil
end

-- 목표보다 적으면 채운다. HUD 는 로드 직후 카운트업 애니메이션을 하므로 같은 값이 두 번 연속 읽힐 때만 채우고,
-- 모드가 바꾼 값이 HUD 에 아직 나타나지 않았으면 기다린다
local function keepTreasury(state, target, hud, now)
  local stable = state.treasuryLastReading == hud
  state.treasuryLastReading = hud
  if believedTreasury(hud, now) ~= hud then return end
  local need = shortfall(hud, target)
  if need <= 0 or not stable then return end
  local cheat = game.cheat()
  if not cheat then return end
  changeTreasury(cheat, hud, math.floor(need), now)
end

function M.tick(state, settings)
  settings = settings or {}

  local pawn = game.pawn()
  if pawn then
    local changed = false
    if shortfall(pawn.influence, settings.influence) > 0 then pawn.influence = settings.influence; changed = true end
    local need = shortfall(pawn.kingsFavour, settings.kingsFavour)
    if need > 0 then pawn:changeKingsFavour(math.floor(need)); changed = true end
    if changed then game.refreshLordHud() end
  end

  local hud = game.treasury()
  if hud then keepTreasury(state, settings.treasury, hud, M.clock()) end
end

-- 기능 켜짐과 무관하게 현재값만 읽는다(main 이 매 루프 상태로 보고). 국고는 모드가 방금 바꿨으면 바꾼 뒤의 값이다
function M.read()
  local out = {}
  local pawn = game.pawn()
  if pawn then
    out.influence = pawn.influence
    out.kingsFavour = pawn.kingsFavour
  end
  local hud = game.treasury()
  if hud then out.treasury = believedTreasury(hud, M.clock()) end
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
    local hud, cheat = game.treasury(), game.cheat()
    if not hud or not cheat then return "treasury not readable" end
    local now = M.clock()
    local current = believedTreasury(hud, now)
    local delta = math.floor(value - current)
    if delta ~= 0 then changeTreasury(cheat, current, delta, now) end
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
