local datatable = require("core.datatable")
local game = require("core.game")
local native = require("core.native")
local safe = require("core.safe")

-- ignorePlacement 는 네이티브 계층(spec §11)이 해석한다.
-- noRegionLimit, noMaterials 의 배치 판정도 네이티브가 맡는다: 플레이어의 배치 판정 동안만 표의 행을 고친다
--   (findings "표를 바꾸는 기능을 플레이어에게만 — 방법 조사"). 표를 바꾸면 AI 영주도 읽으므로 Lua 는 표를 바꾸지 않는다.
--   네이티브가 못 맡을 때만(DLL 없음, 게임 업데이트로 함수를 못 찾음) 예전처럼 표를 바꾼다.
--   표의 자재가 그대로이므로 새로 놓은 건물은 자재 목록을 갖고 생긴다. 내 영지의 공사 현장 자재 목록은 poll(1초)마다 비운다.
-- instantBuild 는 두 가지로 한다(findings "즉시 완공 — instaBuild 플래그").
--  새로 놓는 건물: 엔진의 디버그 플래그 목록(drawDebugFlags)에 instaBuild 가 있으면 게임이 건물을 놓는 순간 완공 상태로 만든다(SetupBuilding).
--    플래그는 AI 영주의 건물에도 적용되므로, 내가 건물이나 밭을 배치하는 동안만 넣는다(poll, 1초마다).
--  이미 공사 중인 건물(업그레이드 포함): 플래그가 닿지 않는다. 네이티브가 getConstructionProgress 를 후킹해 파츠 hp 를 채우고
--    게임의 완공 처리 함수를 부른다. Lua 는 게임 스레드에서 그 함수를 호출해 발동시킨다(poll, 1초마다. Plan 3 부록 A.1).
--    네이티브는 낼 자재가 없는 건물만 완공 처리하므로(게임과 같은 조건) 자재 목록을 먼저 비운다.
local M = { name = "build", intervalSec = 10, FLAG = "instaBuild",
  -- 네이티브가 행을 플레이어에게만 고치는 데 필요한 항목: 배치 갱신 후킹, 건물 표의 행 함수, 행 오프셋 확인
  ROW_SCOPE = { "placement", "building_row", "placement_rows" } }

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

-- 로드 직후 enable 시점에는 지역이 아직 없을 수 있으므로 공사 현장 처리는 poll 에서도 반복한다
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

-- 네이티브가 못 맡을 때의 예전 방식: 표를 바꾼다(게임 전체에 하나라 AI 영주에게도 적용되고, 게임을 다시 켜야 원래대로 돌아온다).
-- 네이티브 상태를 아직 모르면 기다린다(poll 이 다시 부른다)
local function patchTableWithoutNative(settings)
  if not (settings.noRegionLimit or settings.noMaterials) then return end
  if native.installed(M.ROW_SCOPE) ~= false then return end
  datatable.forEachRow("buildingStats", function(_, row)
    -- 지역당 개수 제한: maxInRegion (실측 9개 행이 1 — 영주 저택 모듈(수비용 탑 등)·세금 징수소·장작/식량 수레, 나머지는 0 = 제한 없음)
    if settings.noRegionLimit and row.maxInRegion ~= 0 then row.maxInRegion = 0 end
    if settings.noMaterials then clear(row.constructionGoods) end
  end)
end

function M.apply(_, settings)
  patchTableWithoutNative(settings)
  if settings.noMaterials then clearConstructionSites() end
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
  patchTableWithoutNative(settings)
  local on = settings.instantBuild == true
  setFlag(on and placing())
  if on then finishSites() else pending = {} end
  if settings.noMaterials then clearConstructionSites() end
end

-- 껐을 때는 플래그를 뺀다. 맵을 떠날 때(state.leaving)는 엔진이 곧 사라지므로 건드리지 않는다
function M.disable(state)
  pending = {}
  if type(state) == "table" and state.leaving then return end
  setFlag(false)
end

-- 즉시 수리가 건물에 남기는 유지보수 물자 (findings "즉시 수리 — 건물에 남는 유지보수 물자").
-- 치트(MaintainAllBuildings)는 물자를 쓰지 않고 건물을 채운다. 그때 일꾼이 이미 유지보수 물자를 나르고 있었으면 도착한 물자가 건물에 남는다.
-- 그 물자의 보관 방식(일반·목재·식량)에 해당하는 저장 한도가 0 인 건물(벌목장의 일반 저장실 등)에서는 게임이 "저장실 가득 참"을 붙인다
-- (실측: 벌목장에 철제 도구 1개). 치트가 대신 한 유지보수에 쓰였어야 할 물자이므로 그 건물에서 소모시킨다. 게임이 저장 문제를 스스로 다시 검사한다.
local STORAGE = {
  [0] = { limit = "storageLimitGeneric", stored = "numStoredGeneric" },
  [1] = { limit = "storageLimitLarge", stored = "numStoredLarge" },
  [2] = { limit = "storageLimitPantry", stored = "numStoredPantry" },
}

-- 한도가 0 인 저장실에 물자가 든 건물인가(값만 읽는 싼 검사. 대부분의 건물이 여기서 걸러진다)
local function holdsWithoutRoom(b)
  for _, k in pairs(STORAGE) do
    if b[k.limit] == 0 and b[k.stored] > 0 then return true end
  end
  return false
end

-- 그 건물의 유지보수 물자 종류. 유지보수 대상이 아니면 nil
local function maintenanceGoods(b)
  local comp = b.MaintenanceComponent
  if not safe.valid(comp) then return nil end
  local out
  for _, w in ipairs(comp:GetTrackedMaintenanceTypes()) do
    local goods = game.unwrap(w).goodTypes
    for i = 1, #goods do
      out = out or {}
      out[goods[i]] = true
    end
  end
  return out
end

local function hasNoRoomFor(b, goodType)
  local row = datatable.object("items"):FindRow(tostring(goodType))
  local kind = row and STORAGE[row.storageType]
  return kind ~= nil and b[kind.limit] == 0
end

local function useUpLeftoverSupplies()
  for _, region in ipairs(game.playerRegions()) do
    for _, w in ipairs(region:GetBuildings()) do
      local b = game.unwrap(w)
      if safe.valid(b) and holdsWithoutRoom(b) then
        local supplies = maintenanceGoods(b)
        if supplies then
          -- 소모하면 인벤토리가 바뀌므로 먼저 모은 뒤 소모한다
          local found = {}
          local inventory = b.Inventory
          for i = 1, #inventory do
            local g = inventory[i]
            if supplies[g.Type] and g.amt > 0 and hasNoRoomFor(b, g.Type) then found[#found + 1] = { type = g.Type, amt = g.amt } end
          end
          for _, g in ipairs(found) do region:consumeGood(g.type, g.amt, b, false, false, false) end
        end
      end
    end
  end
end

-- 게임의 유지보수 치트(MaintainAllBuildings)는 폰의 currentRegion 인 영지에만 듣는다(실측 2026-10-03, findings "즉시 수리 — 내 영지 전체").
-- 내 영지를 하나씩 현재 영지로 두고 부른 뒤 원래 값으로 돌려놓는다. 한 번의 게임 스레드 호출 안에서 끝나므로 게임은 바뀐 값을 보지 못한다.
-- 화면의 영지가 내 것이 아니면 그 영지에는 부르지 않는다. 현재 영지를 읽지 못하면 예전처럼 한 번만 부른다
local function maintainMyRegions(cheat)
  local pawn = game.pawn()
  local current = pawn and pawn.currentRegion
  if not safe.valid(current) then
    cheat:MaintainAllBuildings()
    return
  end
  local here = current:GetAddress()
  local viewingMine = false
  local ok, err = pcall(function()
    for _, region in ipairs(game.playerRegions()) do
      if region:GetAddress() == here then
        viewingMine = true
      else
        pawn.currentRegion = region
        cheat:MaintainAllBuildings()
      end
    end
  end)
  pawn.currentRegion = current
  if not ok then error(err, 0) end
  if viewingMine then cheat:MaintainAllBuildings() end
end

function M.tick(_, settings)
  if not settings.instantRepair then return end
  local cheat = game.cheat()
  if not cheat then return end
  maintainMyRegions(cheat)
  useUpLeftoverSupplies()
end

return M
