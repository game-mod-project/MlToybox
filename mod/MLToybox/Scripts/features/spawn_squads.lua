local game = require("core.game")
local datatable = require("core.datatable")

-- 주민 수를 넘는 병력: 용병 생성 경로(spawnArmy)로 플레이어 소유 분대를 새로 만든다 (Plan 3 부록 A.2 대안)
local M = { MAX_COUNT = 5, OFFSET_X = 800, MILITIA_COMPANY = -1 }

function M.spawn(command, ctx)
  if not ctx.inGame then return { ok = false, error = "not in game" } end
  local count = tonumber(command.count)
  if not count or count ~= math.floor(count) or count < 1 or count > M.MAX_COUNT then
    return { ok = false, error = "count must be 1.." .. M.MAX_COUNT }
  end
  local unit = command.unit
  if type(unit) ~= "string" or not datatable.object("unitTemplates"):FindRow(unit) then
    return { ok = false, error = "unknown unit: " .. tostring(unit) }
  end
  local pawn, engine = game.pawn(), game.engine()
  if not pawn or not engine then return { ok = false, error = "game objects not ready" } end
  local loc = game.anchorLocation()
  if not loc then return { ok = false, error = "no anchor building in player region" } end

  local names = {}
  for i = 1, count do names[i] = game.fname(unit) end
  local ids = engine:spawnArmy({ X = loc.X + M.OFFSET_X, Y = loc.Y, Z = loc.Z }, names, pawn, M.MILITIA_COMPANY, 0)
  local squads = {}
  for _, w in ipairs(ids or {}) do squads[#squads + 1] = game.unwrap(w) end
  return { ok = true, squads = squads }
end

return M
