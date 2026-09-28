local datatable = require("core.datatable")
local game = require("core.game")
local safe = require("core.safe")

-- ignorePlacement / instantBuild 는 네이티브 계층(spec §11)이 해석한다
local M = { name = "build", intervalSec = 10 }

local function clear(arr) if arr and arr.Empty then arr:Empty() end end

-- 로드 직후 enable 시점에는 지역이 아직 없을 수 있으므로 공사 현장 정리는 tick 에서도 반복한다
local function clearConstructionSites()
  for _, region in ipairs(game.playerRegions()) do
    for _, w in ipairs(region:GetBuildings()) do
      local b = game.unwrap(w)
      if safe.valid(b) and not b:IsConstructed() then clear(b.constructionGoods) end
    end
  end
end

function M.apply(_, settings)
  if not settings.noMaterials then return end
  datatable.forEachRow("buildingStats", function(_, row) clear(row.constructionGoods) end)
  clearConstructionSites()
end

M.enable = M.apply
M.configure = M.apply

function M.tick(_, settings)
  if settings.noMaterials then clearConstructionSites() end
  if not settings.instantRepair then return end
  local cheat = game.cheat()
  if cheat then cheat:MaintainAllBuildings() end
end

return M
