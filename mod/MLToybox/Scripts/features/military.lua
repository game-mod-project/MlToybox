local datatable = require("core.datatable")
local game = require("core.game")

-- 주민 수를 넘는 징집은 네이티브 계층(spec §11)이 담당한다
local M = { name = "military", intervalSec = 5, MAX_SQUADS = 99 }

local function clear(arr) if arr and arr.Empty then arr:Empty() end end

function M.apply(_, settings)
  datatable.forEachRow("unitTemplates", function(_, row)
    if settings.ignoreEquipment then clear(row.requiredEquipment) end
    if settings.ignorePopulation then
      row.minHouseLv = 0
      row.minMeleeTraining = 0
      row.minArcheryTraining = 0
    end
  end)
  if settings.zeroUpkeep then
    datatable.forEachRow("mercenaries", function(_, row) row.cost = 0 end)
  end
end

M.enable = M.apply
M.configure = M.apply

function M.tick(_, settings)
  local pawn = game.pawn()
  if pawn then
    if settings.unlimitedSquads and pawn.maxNumOfMilitiaToSpawn < M.MAX_SQUADS then pawn.maxNumOfMilitiaToSpawn = M.MAX_SQUADS end
    if settings.zeroUpkeep and pawn.recruitCost ~= 0 then pawn.recruitCost = 0 end
  end
  if settings.zeroUpkeep then
    local engine = game.engine()
    if engine then
      engine.hiredMercs:ForEach(function(_, v)
        local company = game.unwrap(v)
        if company.cost ~= 0 then company.cost = 0 end
      end)
    end
  end
end

return M
