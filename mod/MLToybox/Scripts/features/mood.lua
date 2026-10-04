local game = require("core.game")

-- 자격(Approval)과 공공질서. 게임은 영지마다 하루에 한 번 두 값을 다시 계산한다(자격 = 50 + 요인의 합, 공공질서 = 100 + 요인의 합).
-- 고정값과 배율(오르는 요인 good 배, 깎이는 요인 bad %)은 네이티브 DLL 이 그 계산 직후에 내 영지에만 건다
-- (native/src/features/mood, findings "자격·공공질서 — 게임의 방식").
-- 여기서는 두 가지만 한다: 고정값을 바로 써 넣는 일(설정을 바꾼 직후와, 게임이 다시 계산하기 전까지)과 지금 값을 상태에 적는 일.
-- 배율은 건드리지 않는다: 요인 목록의 일부(정책 효과)가 리플렉션에 없어 Lua 로는 게임과 같은 값을 계산할 수 없다.
-- 설정: { approval = { fixed, good, bad }, order = { ... }, regions = { [영지 키] = { approval, order } } }. 영지에 따로 둔 것이 있으면 공통 대신 쓴다
local M = { name = "mood" }

-- 그 영지에 쓸 묶음: 영지 설정이 있으면 그것, 없으면 공통(settings 자체)
local function setFor(settings, key)
  local own = type(settings.regions) == "table" and settings.regions[key]
  if type(own) == "table" then return own end
  return settings
end

-- 고정값(1~100). 없으면 nil
local function fixedOf(set, stat)
  local s = set[stat]
  local v = type(s) == "table" and tonumber(s.fixed) or nil
  if not v or v < 1 then return nil end
  return math.min(math.floor(v), 100)
end

local function enforce(settings)
  for _, r in ipairs(game.playerRegions()) do
    local key = game.regionKey(r)
    if key then
      local set = setFor(settings, key)
      local approval, order = fixedOf(set, "approval"), fixedOf(set, "order")
      if approval and r.Approval ~= approval then r.Approval = approval end
      if order and r.publicOrder ~= order then r.publicOrder = order end
    end
  end
end

-- 내 영지의 지금 값을 state.mood 에 적는다(게임 안 창의 탭이 읽는다)
local function publish(state)
  local regions = {}
  for _, r in ipairs(game.playerRegions()) do
    local key = game.regionKey(r)
    if key then
      regions[#regions + 1] = { key = key, name = game.regionName(r) or key, approval = r.Approval, order = r.publicOrder }
    end
  end
  state.mood = { regions = regions }
end

function M.tick(state, settings)
  enforce(settings)
  publish(state)
end

M.enable = M.tick
M.configure = M.tick

-- 꺼져 있는 동안 registry 가 부른다. 탭이 지금 값을 보이도록 읽기만 한다
function M.observe(state)
  publish(state)
end

return M
