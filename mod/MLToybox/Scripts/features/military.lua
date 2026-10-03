local datatable = require("core.datatable")
local game = require("core.game")

-- 주민 수와 무관한 병력은 features/spawn_squads.lua(병력 생성)가, 용병 비용과 고용 창은 features/mercenaries.lua 가 담당한다
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
end

M.enable = M.apply
M.configure = M.apply

function M.tick(_, settings)
  local pawn = game.pawn()
  if not pawn then return end
  if settings.unlimitedSquads and pawn.maxNumOfMilitiaToSpawn < M.MAX_SQUADS then pawn.maxNumOfMilitiaToSpawn = M.MAX_SQUADS end
  -- zeroUpkeep 은 민병대 모집비만 0 으로 만든다
  if settings.zeroUpkeep and pawn.recruitCost ~= 0 then pawn.recruitCost = 0 end
end

return M
