-- 개발용(읽기 전용): 용병 고용 창 목록, 고용 중 용병단과 분대 소유, 국고, 고용 창 상태, 용병 표 행 이름
local function s(x) local ok, v = pcall(function() return x:ToString() end); return ok and v or tostring(x) end
local function names(arr)
  local out = {}
  pcall(function() for i = 1, #arr do out[#out + 1] = s(arr[i]) end end)
  return table.concat(out, ",")
end
local engine = FindFirstOf("MyRTSMultiEngineCPP_BP_C")
local pawn = FindFirstOf("MyPawnCPP_BP3_C")
if not engine:IsValid() or not pawn:IsValid() then print("NOT_IN_GAME") return end

local avail = engine.availableMercs
print("available", #avail)
for i = 1, #avail do
  local c = avail[i]
  local region = "nil"
  pcall(function() if c.arrivalRegion:IsValid() then region = s(c.arrivalRegion.regionName) end end)
  print("slot", i, "name=" .. s(c.Name), "cost=" .. tostring(c.cost), "arrivesIn=" .. tostring(c.arrivesIn), "region=" .. region,
    "units=[" .. names(c.units) .. "]", "traits=[" .. names(c.traits) .. "]")
end

local owners = {}
local squads = engine.squads
for i = 1, #squads do
  local sq = squads[i]
  if sq.companyID >= 0 then
    local mine = false
    pcall(function() mine = sq.ownerPawn:GetAddress() == pawn:GetAddress() end)
    local o = owners[sq.companyID] or { squads = 0, mine = 0, units = {} }
    o.squads = o.squads + 1
    if mine then o.mine = o.mine + 1 end
    o.units[#o.units + 1] = s(sq.unitType) .. ":" .. #sq.unitArr
    owners[sq.companyID] = o
  end
end
engine.hiredMercs:ForEach(function(k, v)
  local id, c = k:get(), v:get()
  local o = owners[id] or { squads = 0, mine = 0, units = {} }
  print("hired", id, "name=" .. s(c.Name), "cost=" .. tostring(c.cost), "squads=" .. o.squads, "mine=" .. o.mine, "units=[" .. table.concat(o.units, ",") .. "]")
end)

local treasury = "?"
for _, w in ipairs(FindAllOf("W_HUD_LordPanel_V2_C") or {}) do
  if w:IsValid() then
    pcall(function() if w.TreasuryNumeric:IsValid() then treasury = tostring(w.TreasuryNumeric.CurrentNumericValue) end end)
  end
end
print("treasury", treasury)

for _, x in ipairs(FindAllOf("mercenaryScreen_C") or {}) do
  if x:IsValid() and x:GetFullName():find("/Engine/Transient", 1, true) then
    local confirm = "?"
    pcall(function() confirm = tostring(x.HireConfirmation:IsVisible()) end)
    print("screen", "visible=" .. tostring(x:IsVisible()), "confirmVisible=" .. confirm)
  end
end

local dt = StaticFindObject("/Game/NotStronghold/Data/DT_MercenaryCompanies.DT_MercenaryCompanies")
local rowNames = {}
dt:ForEachRow(function(name, row) rowNames[#rowNames + 1] = tostring(name) .. "=" .. s(row.Name) .. ":" .. tostring(row.cost) end)
print("rows", table.concat(rowNames, " "))
