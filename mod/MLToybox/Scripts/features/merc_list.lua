local game = require("core.game")
local datatable = require("core.datatable")
local safe = require("core.safe")

-- 용병 고용 창 목록(engine.availableMercs)의 게임 쪽 읽기와 쓰기 (spec 2026-09-30 §4.2, §4.3)
-- 배열 전체 대입(engine.availableMercs = {...})은 게임을 튕긴다(findings 용병 스파이크). 칸은 항상 제자리에서 고친다.
-- 칸 수는 Lua 로 바꿀 수 없어 rerollMercenaries() 로만 맞춘다.
local M = {
  QUEST_TRAIT = "questOnly",
  TEMP_SUFFIX = "#mlt",
  CUSTOM_ARRIVES_IN = 1,
  -- 표 행의 깃발·특성을 칸에 쓸 수 있는가 (findings "용병 실측 M1~M5" 의 M1). false 면 순정 칸은 다시 뽑기 결과를 그대로 둔다
  CAN_COPY_ROWS = true,
}

local function str(x)
  local ok, v = pcall(function() return x:ToString() end)
  if ok then return v end
  return tostring(x)
end

local function names(arr)
  local out = {}
  for i = 1, #arr do out[i] = str(arr[i]) end
  return out
end

local function fnames(list)
  local out = {}
  for i, n in ipairs(list) do out[i] = game.fname(n) end
  return out
end

local function clear(arr)
  if arr.Empty then arr:Empty() return end
  for i = #arr, 1, -1 do arr[i] = nil end
end

local function sameNames(a, b)
  if #a ~= #b then return false end
  for i = 1, #a do
    if a[i]:lower() ~= b[i]:lower() then return false end
  end
  return true
end

local function sameObject(a, b)
  return safe.valid(a) and safe.valid(b) and a:GetAddress() == b:GetAddress()
end

local function isQuest(row)
  for _, t in ipairs(names(row.traits)) do
    if t == M.QUEST_TRAIT then return true end
  end
  return false
end

local function isTemp(name)
  return name:sub(-#M.TEMP_SUFFIX) == M.TEMP_SUFFIX
end

function M.rows()
  local out = {}
  datatable.forEachRow("mercenaries", function(rowName, row)
    out[#out + 1] = { rowName = rowName, name = str(row.Name), cost = row.cost, quest = isQuest(row) }
  end)
  return out
end

function M.hiredNames(engine)
  local out = {}
  engine.hiredMercs:ForEach(function(_, v) out[str(game.unwrap(v).Name)] = true end)
  return out
end

function M.read(engine)
  local out, avail = {}, engine.availableMercs
  for i = 1, #avail do
    local c = avail[i]
    local region = nil
    if safe.valid(c.arrivalRegion) then region = game.regionKey(c.arrivalRegion) end
    out[i] = { name = str(c.Name), cost = c.cost, units = names(c.units), region = region }
  end
  return out
end

function M.screenState()
  local screen = game.mercScreen()
  if not screen then return { open = false, confirming = false } end
  local open = screen:IsVisible() == true
  local conf = screen.HireConfirmation
  local confirming = open and safe.valid(conf) and conf:IsVisible() == true
  return { open = open, confirming = confirming }
end

function M.refreshScreen()
  local screen = game.mercScreen()
  if screen and screen:IsVisible() == true then screen:updateCompanies() end
end

-- 다른 필드만 쓴다. 분대 목록을 다시 대입하면 배열이 새로 할당되므로, AI 잠금처럼 고용비만 바뀔 때는 고용비만 쓴다
local function writeCustom(c, d)
  local company = d.company
  if str(c.Name) ~= company.name then c.Name = company.name end
  if not sameNames(names(c.units), company.units) then c.units = fnames(company.units) end
  if #c.traits > 0 then clear(c.traits) end
  if c.cost ~= d.cost then c.cost = d.cost end
  if c.arrivesIn ~= M.CUSTOM_ARRIVES_IN then c.arrivesIn = M.CUSTOM_ARRIVES_IN end
  local region = game.regionByKey(company.region)
  if region and not sameObject(c.arrivalRegion, region) then c.arrivalRegion = region end
end

-- saved: 유지하는 순정 칸의 재구성 전 도착 정보({ arrivalRegion, arrivesIn }) 또는 nil
local function writeVanilla(c, d, saved)
  if str(c.Name) ~= d.name then
    local row = datatable.object("mercenaries"):FindRow(d.rowName)
    if not row then error("mercenary row not found: " .. tostring(d.rowName)) end
    c.Name = d.name
    c.units = fnames(names(row.units))
    local traits = names(row.traits)
    if #traits > 0 then c.traits = fnames(traits) elseif #c.traits > 0 then clear(c.traits) end
    c.banner = row.banner
    c.colorA, c.colorB, c.emblemA, c.emblemB = row.colorA, row.colorB, row.emblemA, row.emblemB
  end
  if c.cost ~= d.cost then c.cost = d.cost end
  if saved then
    c.arrivalRegion = saved.arrivalRegion
    c.arrivesIn = saved.arrivesIn
  end
end

local function writeSlot(c, d, saved)
  if d.kind == "custom" then writeCustom(c, d) else writeVanilla(c, d, saved) end
end

function M.applyInPlace(engine, slots)
  local avail = engine.availableMercs
  -- 유지하는 순정 칸이 다른 자리로 옮겨 갈 수 있다(커스텀이 앞 칸을 차지할 때). 도착 정보를 쓰기 전에 기억해 둔다
  local before, saved = {}, {}
  for i = 1, #avail do
    local c = avail[i]
    before[i] = str(c.Name)
    saved[before[i]] = { arrivalRegion = c.arrivalRegion, arrivesIn = c.arrivesIn }
  end
  for i = 1, #avail do
    local d = slots[i]
    if d then writeSlot(avail[i], d, d.keep and before[i] ~= d.name and saved[d.name] or nil) end
  end
end

-- 칸 수를 #desired 로 맞추고 내용을 덮어쓴다. 실제로 생긴 칸 수를 돌려준다 (spec §4.3)
function M.rebuild(engine, desired, renames)
  local saved = {}
  local before = engine.availableMercs
  for i = 1, #before do
    local c = before[i]
    saved[str(c.Name)] = { arrivalRegion = c.arrivalRegion, arrivesIn = c.arrivesIn }
  end

  -- 후보가 모자라면 고용 중 이름과 겹치는 표 행의 이름을 잠깐 바꿔 후보로 만든다
  local renamed = {}
  if renames > 0 then
    local hired = M.hiredNames(engine)
    datatable.forEachRow("mercenaries", function(_, row)
      local name = str(row.Name)
      if #renamed < renames and hired[name] and not isQuest(row) then
        renamed[#renamed + 1] = { row = row, name = name }
        row.Name = name .. M.TEMP_SUFFIX
      end
    end)
  end
  local ok, err = pcall(function() engine:rerollMercenaries() end)
  for _, r in ipairs(renamed) do pcall(function() r.row.Name = r.name end) end
  if not ok then error(err, 0) end

  local avail = engine.availableMercs
  local made = #avail
  if M.CAN_COPY_ROWS then
    for i = 1, math.min(made, #desired) do
      local d = desired[i]
      writeSlot(avail[i], d, d.keep and saved[d.name] or nil)
    end
  else
    -- 순정 칸은 다시 뽑기 결과를 그대로 둔다. 커스텀은 임시 이름 칸부터 덮어쓴다
    local order = {}
    for i = 1, made do if isTemp(str(avail[i].Name)) then order[#order + 1] = i end end
    for i = 1, made do if not isTemp(str(avail[i].Name)) then order[#order + 1] = i end end
    local k = 0
    for _, d in ipairs(desired) do
      if d.kind == "custom" then
        k = k + 1
        if order[k] then writeSlot(avail[order[k]], d, nil) end
      end
    end
  end
  return made
end

return M
