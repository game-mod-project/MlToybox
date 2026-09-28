local datatable = require("core.datatable")
local game = require("core.game")

-- ignorePlacement / instantBuild 는 네이티브 계층(spec §11)이 해석한다
local M = { name = "build", intervalSec = 10 }

local function clear(arr) if arr and arr.Empty then arr:Empty() end end

function M.apply(_, settings)
  if not settings.noMaterials then return end
  datatable.forEachRow("buildingStats", function(_, row) clear(row.constructionGoods) end)
  for _, region in ipairs(game.playerRegions()) do
    for _, w in ipairs(region:GetBuildings()) do
      local b = game.unwrap(w)
      if b:IsValid() and not b:IsConstructed() then clear(b.constructionGoods) end
    end
  end
end

M.enable = M.apply
M.configure = M.apply

function M.tick(_, settings)
  if not settings.instantRepair then return end
  local cheat = game.cheat()
  if cheat then cheat:MaintainAllBuildings() end
end

return M
