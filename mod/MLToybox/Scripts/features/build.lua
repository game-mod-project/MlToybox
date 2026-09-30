local datatable = require("core.datatable")
local game = require("core.game")
local safe = require("core.safe")

-- ignorePlacement 는 네이티브 계층(spec §11)이 해석한다.
-- instantBuild 는 네이티브가 getConstructionProgress 를 후킹하고, Lua 가 게임 스레드에서 그 함수를 호출해 발동시킨다(Plan 3 부록 A.1).
local M = { name = "build", intervalSec = 10 }

local function clear(arr) if arr and arr.Empty then arr:Empty() end end

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

M.enable = M.apply
M.configure = M.apply

function M.tick(_, settings)
  if settings.noMaterials then clearConstructionSites() end
  if settings.instantBuild then forEachUnbuilt(function(b) b:getConstructionProgress() end) end
  if not settings.instantRepair then return end
  local cheat = game.cheat()
  if cheat then cheat:MaintainAllBuildings() end
end

return M
