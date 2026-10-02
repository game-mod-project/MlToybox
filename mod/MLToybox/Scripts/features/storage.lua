local datatable = require("core.datatable")
local game = require("core.game")
local safe = require("core.safe")

-- 건물의 저장 한도(일반·목재·식량)를 건물 종류별로 바꾼다. settings.limits["<건물 종류 번호>"] = { generic, large, pantry }.
-- 실측(findings "저장 용량"): 게임은 건물 인스턴스의 storageLimitGeneric/Large/Pantry 를 쓴다(입고 판정과 건물 창의 표시).
-- 건물 표(buildingStats)를 바꿔도 지어진 건물에는 반영되지 않으므로 표는 기본값을 읽는 데만 쓰고, 내 영지의 건물마다 값을 쓴다.
-- 한도는 세이브에 저장되지 않는다. 불러올 때마다 다시 적용하고, 끄면 게임이 준 값으로 되돌린다.
local M = { name = "storage", intervalSec = 5, MAX = 1000000 }

local KINDS = {
  { key = "generic", field = "storageLimitGeneric" },
  { key = "large", field = "storageLimitLarge" },
  { key = "pantry", field = "storageLimitPantry" },
}

-- 건물 주소 -> { type, base = 게임이 준 한도, applied = 우리가 써 둔 한도 }
local tracked = {}
-- 건물 종류 -> 표의 기본 한도(행이 없으면 false)
local defaults = {}

local function defaultsOf(btype)
  local d = defaults[btype]
  if d == nil then
    local row = datatable.object("buildingStats"):FindRow(tostring(btype))
    d = row and { generic = row.storageLimitGeneric, large = row.storageLimitLarge, pantry = row.storageLimitPantry } or false
    defaults[btype] = d
  end
  return d
end

-- 그 건물에 쓸 한도. base 는 게임이 그 건물에 준 값, default 는 표의 기본값, value 는 설정값(그 종류의 기본값을 대신한다).
-- 게임이 보너스를 준 건물(base 가 기본값과 다르다)은 그 비율을 유지한다. 원래 0 인 저장실은 고치지 않는다
function M.target(base, default, value)
  if type(value) ~= "number" or value < 0 or type(default) ~= "number" or default <= 0 then return base end
  value = math.min(math.floor(value), M.MAX)
  if base == default then return value end
  return math.min(math.floor(base * value / default + 0.5), M.MAX)
end

-- 우리가 쓴 값이 그대로 있을 때만 게임이 준 값으로 되돌린다(그사이 게임이 다시 계산했으면 그 값을 둔다)
local function restore(b, t)
  for _, k in ipairs(KINDS) do
    if t.applied[k.key] ~= nil and b[k.field] == t.applied[k.key] then b[k.field] = t.base[k.key] end
  end
end

local function apply(b, addr, limits)
  local btype = b:GetType()
  local want = limits[tostring(btype)]
  local t = tracked[addr]
  if t and t.type ~= btype then t = nil end   -- 같은 주소에 다른 건물이 들어섰다
  if type(want) ~= "table" or next(want) == nil then
    if t then restore(b, t) end
    tracked[addr] = nil
    return
  end
  local def = defaultsOf(btype)
  if not def then return end
  if not t then t = { type = btype, base = {}, applied = {} } end
  tracked[addr] = t
  for _, k in ipairs(KINDS) do
    local current = b[k.field]
    -- 처음 보는 건물이거나, 게임이 값을 다시 계산했다(업그레이드 등): 지금 값이 게임이 준 값이다
    if t.applied[k.key] ~= current then t.base[k.key] = current end
    local target = M.target(t.base[k.key], def[k.key], want[k.key])
    if target ~= current then b[k.field] = target end
    t.applied[k.key] = target
  end
end

local function forEachBuilding(fn)
  for _, region in ipairs(game.playerRegions()) do
    for _, w in ipairs(region:GetBuildings()) do
      local b = game.unwrap(w)
      if safe.valid(b) then fn(b, b:GetAddress()) end
    end
  end
end

function M.enable()
  tracked = {}
  defaults = {}
end

function M.tick(_, settings)
  local limits = (type(settings) == "table" and type(settings.limits) == "table") and settings.limits or {}
  local seen = {}
  forEachBuilding(function(b, addr)
    seen[addr] = true
    apply(b, addr, limits)
  end)
  for addr in pairs(tracked) do
    if not seen[addr] then tracked[addr] = nil end   -- 철거됐거나 남의 것이 된 건물
  end
end

M.configure = M.tick

-- 껐을 때는 게임이 준 값으로 되돌린다. 맵을 떠날 때(state.leaving)는 건물이 곧 사라지므로 게임 객체를 건드리지 않고 잊기만 한다
function M.disable(state)
  local kept = tracked
  tracked = {}
  if type(state) == "table" and state.leaving then return end
  forEachBuilding(function(b, addr)
    local t = kept[addr]
    if t and t.type == b:GetType() then restore(b, t) end
  end)
end

function M.trackedCount()
  local n = 0
  for _ in pairs(tracked) do n = n + 1 end
  return n
end

return M
