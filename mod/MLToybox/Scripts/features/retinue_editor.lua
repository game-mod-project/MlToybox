local game = require("core.game")
local safe = require("core.safe")

-- 모드가 만든 수행원 분대(생성 분대, 커스텀 용병 고용 분대)에 게임의 수행원 꾸미기 화면을 연다 (findings "수행원 꾸미기" 스파이크).
-- 게임은 영지의 retinueSquadID 가 가리키는 분대를 대상으로 화면을 연다. 그 번호를 잠깐 모드 분대로 바꿔 열고, 화면이 닫히면 되돌린다.
local M = { RETINUE_PREFIX = "retinue", REAL_RETINUE_TYPE = 3, MERCENARY_TYPE = 2 }

local session = nil   -- { region, original, squad }

function M.reset() session = nil end

local function unitName(squad)
  local ok, name = pcall(function() return squad.unitType:ToString() end)
  return ok and name or ""
end

local function isPlayer(squad, pawn)
  local owner = squad.ownerPawn
  return safe.valid(owner) and owner:GetAddress() == pawn:GetAddress()
end

-- 대상: 내 분대, 진짜 수행원 분대가 아니고, 병종이 수행원(retinue_*)인 것
local function eligible(squad, pawn)
  return isPlayer(squad, pawn) and squad.squadType ~= M.REAL_RETINUE_TYPE
    and unitName(squad):lower():sub(1, #M.RETINUE_PREFIX) == M.RETINUE_PREFIX
end

local function findSquad(engine, id)
  local squads = engine.squads
  for i = 1, #squads do
    if squads[i].ID == id then return squads[i] end
  end
  return nil
end

local function editorOpen()
  local editor = game.retinueEditor()
  return editor ~= nil and editor:IsVisible() == true
end

local function restore()
  if not session then return end
  local s = session
  session = nil
  pcall(function()
    if safe.valid(s.region) then s.region.retinueSquadID = s.original end
  end)
end

function M.open(command, ctx)
  if not ctx.inGame then return { ok = false, error = "not in game" } end
  local id = tonumber(command.value)
  if not id or id ~= math.floor(id) then return { ok = false, error = "value must be a squad id" } end
  local pawn, engine = game.pawn(), game.engine()
  if not pawn or not engine then return { ok = false, error = "game objects not ready" } end
  local squad = findSquad(engine, id)
  if not squad or not eligible(squad, pawn) then return { ok = false, error = "squad " .. id .. " is not a retinue squad made by the mod" } end
  local editor = game.retinueEditor()
  if not editor then return { ok = false, error = "retinue editor widget not found" } end
  if session or editor:IsVisible() == true then return { ok = false, error = "retinue editor is already open" } end
  -- 화면을 열 저택: 지정한 영지, 없으면 내 첫 영지
  local region
  if command.region ~= nil then
    region = game.regionByKey(command.region)
    if not region then return { ok = false, error = "region not found: " .. tostring(command.region) } end
  else
    region = game.playerRegions()[1]
    if not region then return { ok = false, error = "no player region" } end
  end
  if not safe.valid(region.manor) then return { ok = false, error = "region has no manor" } end

  session = { region = region, original = region.retinueSquadID, squad = id }
  region.retinueSquadID = id
  local ok, err = pcall(function() editor:Open(region.manor) end)
  if not ok then
    restore()
    return { ok = false, error = "open failed: " .. tostring(err) }
  end
  return { ok = true, squad = id }
end

-- 화면이 닫혔거나 맵을 떠나면 영지의 수행원 분대 번호를 되돌린다
function M.tick(inGame)
  if not session then return end
  if not inGame or not editorOpen() then restore() end
end

function M.status(inGame)
  local out = { squads = {}, editing = session and session.squad or nil }
  if not inGame then return out end
  local pawn, engine = game.pawn(), game.engine()
  if not pawn or not engine then return out end
  local squads = engine.squads
  for i = 1, #squads do
    local s = squads[i]
    if eligible(s, pawn) then
      out.squads[#out.squads + 1] = { id = s.ID, unit = unitName(s), count = #s.unitArr, kind = s.squadType == M.MERCENARY_TYPE and "mercenary" or "spawned" }
    end
  end
  return out
end

return M
