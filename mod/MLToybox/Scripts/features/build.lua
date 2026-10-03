local datatable = require("core.datatable")
local game = require("core.game")
local safe = require("core.safe")

-- ignorePlacement 는 네이티브 계층(spec §11)이 해석한다.
-- instantBuild 는 두 가지로 한다(findings "즉시 완공 — instaBuild 플래그").
--  새로 놓는 건물: 엔진의 디버그 플래그 목록(drawDebugFlags)에 instaBuild 가 있으면 게임이 건물을 놓는 순간 완공 상태로 만든다(SetupBuilding).
--    플래그는 AI 영주의 건물에도 적용되므로, 내가 건물이나 밭을 배치하는 동안만 넣는다(poll, 1초마다).
--  이미 공사 중인 건물(업그레이드 포함): 플래그가 닿지 않는다. 네이티브가 getConstructionProgress 를 후킹해 파츠 hp 를 채우고
--    게임의 완공 처리 함수를 부른다. Lua 는 게임 스레드에서 그 함수를 호출해 발동시킨다(poll, 1초마다. Plan 3 부록 A.1).
--    네이티브는 낼 자재가 없는 건물만 완공 처리하므로(게임과 같은 조건) 자재 목록을 먼저 비운다.
local M = { name = "build", intervalSec = 10, FLAG = "instaBuild" }

-- 지난 poll 에 미완공이던 내 건물의 주소. 처음 보는 현장은 한 번 건너뛴다(막 생긴 건물은 파츠가 아직 올라오는 중일 수 있다)
local pending = {}

local function clear(arr) if arr and arr.Empty then arr:Empty() end end

-- 배치 모드인가: 게임의 isInAnyConstructionMode() 가 보는 셋(placeBuilding > 0, placeFieldMode, roadmode) 가운데 건물이 생기는 둘
local function placing()
  local pawn = game.pawn()
  if not pawn then return false end
  return pawn.placeBuilding > 0 or pawn.placeFieldMode == true
end

-- 플래그 목록에 instaBuild 를 넣거나 뺀다. 게임이 넣어 둔 다른 플래그는 그대로 두고, 이미 그 상태면 쓰지 않는다
local function setFlag(on)
  local engine = game.engine()
  if not engine then return end
  local flags = engine.drawDebugFlags
  local kept, has = {}, false
  for i = 1, #flags do
    local name = flags[i]:ToString()
    if name == M.FLAG then has = true else kept[#kept + 1] = game.fname(name) end
  end
  if has == on then return end
  if on then kept[#kept + 1] = game.fname(M.FLAG) end
  if #kept == 0 then flags:Empty() else engine.drawDebugFlags = kept end
end

-- 로드 직후 enable 시점에는 지역이 아직 없을 수 있으므로 공사 현장 처리는 tick 에서도 반복한다
local function forEachUnbuilt(fn)
  for _, region in ipairs(game.playerRegions()) do
    for _, w in ipairs(region:GetBuildings()) do
      local b = game.unwrap(w)
      if safe.valid(b) and not b:IsConstructed() then fn(b) end
    end
  end
end

local function clearConstructionSites()
  forEachUnbuilt(function(b) clear(b.constructionGoods) end)
end

function M.apply(_, settings)
  -- 지역당 개수 제한: buildingStats.maxInRegion (실측 9개 행이 1 — 영주 저택 모듈(수비용 탑 등)·세금 징수소·장작/식량 수레, 나머지는 0 = 제한 없음)
  if settings.noRegionLimit then
    datatable.forEachRow("buildingStats", function(_, row)
      if row.maxInRegion ~= 0 then row.maxInRegion = 0 end
    end)
  end
  if not settings.noMaterials then return end
  datatable.forEachRow("buildingStats", function(_, row) clear(row.constructionGoods) end)
  clearConstructionSites()
end

function M.enable(state, settings)
  pending = {}
  return M.apply(state, settings)
end
M.configure = M.apply

local function finishSites()
  local seen = {}
  forEachUnbuilt(function(b)
    local addr = b:GetAddress()
    seen[addr] = true
    if pending[addr] then
      clear(b.constructionGoods)
      b:getConstructionProgress()
    end
  end)
  pending = seen
end

function M.poll(_, settings)
  local on = settings.instantBuild == true
  setFlag(on and placing())
  if on then finishSites() else pending = {} end
end

-- 껐을 때는 플래그를 뺀다. 맵을 떠날 때(state.leaving)는 엔진이 곧 사라지므로 건드리지 않는다
function M.disable(state)
  pending = {}
  if type(state) == "table" and state.leaving then return end
  setFlag(false)
end

function M.tick(_, settings)
  if settings.noMaterials then clearConstructionSites() end
  if not settings.instantRepair then return end
  local cheat = game.cheat()
  if cheat then cheat:MaintainAllBuildings() end
end

return M
