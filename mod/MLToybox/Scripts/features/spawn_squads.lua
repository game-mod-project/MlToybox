local game = require("core.game")
local datatable = require("core.datatable")

-- 주민 수를 넘는 병력: 용병 생성 경로(spawnArmy)로 플레이어 소유 분대를 새로 만든다 (Plan 3 부록 A.2 대안)
local M = { MAX_COUNT = 5, OFFSET_X = 800, MILITIA_COMPANY = -1 }

-- 생성 위치: 명령의 region(영지 키)의 영주 저택. 없으면 내 첫 영지
function M.noAnchor(region)
  if region then return "no anchor building in region " .. tostring(region) end
  return "no anchor building in player region"
end

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
  local loc = game.anchorLocation(command.region)
  if not loc then return { ok = false, error = M.noAnchor(command.region) } end

  local names = {}
  for i = 1, count do names[i] = game.fname(unit) end
  local ids = engine:spawnArmy({ X = loc.X + M.OFFSET_X, Y = loc.Y, Z = loc.Z }, names, pawn, M.MILITIA_COMPANY, 0)
  local squads = {}
  for _, w in ipairs(ids or {}) do squads[#squads + 1] = game.unwrap(w) end
  return { ok = true, squads = squads }
end

-- 해제·재구성 대행: 생성 분대 병사는 집이 없어 해제하면 사라지고(0/N), 게임의 집결 버튼도 없어진다.
-- 해제된 생성 분대(유령)는 세이브에 병종과 함께 남으므로 그것을 예비 기록으로 쓴다.
-- 재구성 = 같은 병종으로 새로 생성 + 유령 제거. removeSquad 는 뒤쪽 분대 ID 를 하나씩 당기므로
-- 유령은 틱마다 하나씩, 높은 ID 부터, 새 분대보다 앞 번호만 지운다.
local reform = { pending = {}, boundary = nil }

local function isPlayer(s, pawn)
  local owner = s.ownerPawn
  return owner and owner:IsValid() and owner:GetAddress() == pawn:GetAddress()
end

local function isGhost(s, pawn)
  -- 생성 분대는 type 0(None). 용병(2)이면서 용병단이 없는 경우도 생성 분대다(순정 용병은 company >= 0)
  local t = s.squadType
  return isPlayer(s, pawn) and (t == 0 or t == 2) and s.companyID == M.MILITIA_COMPANY
    and #s.unitArr == 0 and #s.assignedRecruits == 0
end

function M.ghosts(pawn, engine)
  local out = {}
  local squads = engine.squads
  for i = #squads, 1, -1 do
    local s = squads[i]
    if isGhost(s, pawn) then out[#out + 1] = { id = s.ID, unit = s.unitType:ToString() } end
  end
  return out
end

function M.resetReform() reform = { pending = {}, boundary = nil } end
function M.pendingRemovals() return #reform.pending end

function M.reform(command, ctx)
  command = command or {}
  if not ctx.inGame then return { ok = false, error = "not in game" } end
  local pawn, engine = game.pawn(), game.engine()
  if not pawn or not engine then return { ok = false, error = "game objects not ready" } end
  -- 앞선 재구성의 유령이 아직 배열에 남아 있으면(제거 대기 중이거나, 제거를 요청했지만 게임이 배열을 줄이기 전)
  -- 그 유령을 다시 세어 같은 분대를 또 만들게 된다. 연달아 온 명령은 거절한다
  if #reform.pending > 0 or (reform.awaitBelow and #engine.squads >= reform.awaitBelow) then
    return { ok = false, error = "reform already in progress" }
  end
  local ghosts = M.ghosts(pawn, engine)
  if #ghosts == 0 then return { ok = false, error = "no disbanded spawned squads" } end
  local loc = game.anchorLocation(command.region)
  if not loc then return { ok = false, error = M.noAnchor(command.region) } end

  local names = {}
  for i, g in ipairs(ghosts) do names[i] = game.fname(g.unit) end
  local ids = engine:spawnArmy({ X = loc.X + M.OFFSET_X, Y = loc.Y, Z = loc.Z }, names, pawn, M.MILITIA_COMPANY, 0)
  local squads, first = {}, nil
  for _, w in ipairs(ids or {}) do
    local id = game.unwrap(w)
    squads[#squads + 1] = id
    if not first or id < first then first = id end
  end
  if not first then return { ok = false, error = "spawnArmy returned no squads" } end
  for _, g in ipairs(ghosts) do reform.pending[#reform.pending + 1] = g.unit end
  reform.boundary = first
  -- 새 분대만큼 배열이 늘어나므로 남아 있는 제거 대기 기준도 같이 올린다
  if reform.awaitBelow then reform.awaitBelow = reform.awaitBelow + #squads end
  return { ok = true, squads = squads, reformed = #ghosts }
end

function M.tick(inGame)
  if not inGame then M.resetReform() return end
  if #reform.pending == 0 and not reform.awaitBelow then return end
  local pawn, engine = game.pawn(), game.engine()
  if not pawn or not engine then return end
  -- removeSquad 는 요청만 등록한다. 게임이 배열을 줄이기 전에는 다음 유령을 지우지 않는다.
  -- 대기는 대기열이 비어도 배열이 줄면 바로 푼다(남겨 두면 다음 재구성의 생성으로 배열이 늘어 영영 대기한다)
  if reform.awaitBelow and #engine.squads >= reform.awaitBelow then return end
  reform.awaitBelow = nil
  if #reform.pending == 0 then return end
  for _, g in ipairs(M.ghosts(pawn, engine)) do
    if g.id < reform.boundary then
      for k, unit in ipairs(reform.pending) do
        if unit == g.unit then
          reform.awaitBelow = #engine.squads
          pawn:removeSquad(g.id)
          table.remove(reform.pending, k)
          reform.boundary = reform.boundary - 1   -- 지운 유령 뒤의 새 분대 ID 가 하나 당겨진다
          return
        end
      end
    end
  end
end

function M.status(inGame)
  local out = { disbanded = 0, byUnit = {}, pending = #reform.pending }
  if not inGame then return out end
  local pawn, engine = game.pawn(), game.engine()
  if not pawn or not engine then return out end
  for _, g in ipairs(M.ghosts(pawn, engine)) do
    out.disbanded = out.disbanded + 1
    out.byUnit[g.unit] = (out.byUnit[g.unit] or 0) + 1
  end
  return out
end

return M
