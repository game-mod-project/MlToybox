local safe = require("core.safe")

local M = {}

M.find = {
  first = function(className) return FindFirstOf(className) end,
  all = function(className) return FindAllOf(className) or {} end,
}

local function firstValid(className)
  local o = M.find.first(className)
  if safe.valid(o) then return o end
  return nil
end

function M.pawn() return firstValid("MyPawnCPP_BP3_C") end
function M.cheat() return firstValid("MLCheatManager_C") end
function M.engine() return firstValid("MyRTSMultiEngineCPP_BP_C") end

function M.playerRegions()
  local pawn = M.pawn()
  if not pawn then return {} end
  local addr = pawn:GetAddress()
  local out = {}
  for _, r in ipairs(M.find.all("BP_Region_C")) do
    if safe.valid(r) then
      local owner = r.ownerPawn
      if safe.valid(owner) and owner:GetAddress() == addr then out[#out + 1] = r end
    end
  end
  return out
end

-- 금고 필드는 리플렉션에 없어 HUD 숫자 위젯에서 읽는다 (findings: 스파이크 결과 ①)
function M.treasury()
  for _, w in ipairs(M.find.all("W_HUD_LordPanel_V2_C")) do
    if safe.valid(w) then
      local n = w.TreasuryNumeric
      if safe.valid(n) then return n.CurrentNumericValue end
    end
  end
  return nil
end

M.fname = function(s) return FName(s) end

-- 분대 생성 위치: 내 첫 지역의 영주 저택, 없으면 첫 완공 건물
function M.anchorLocation()
  for _, region in ipairs(M.playerRegions()) do
    if safe.valid(region.manor) then return region.manor:K2_GetActorLocation() end
    for _, w in ipairs(region:GetBuildings()) do
      local b = M.unwrap(w)
      if safe.valid(b) and b:IsConstructed() then return b:K2_GetActorLocation() end
    end
  end
  return nil
end

function M.unwrap(x)
  local ok, v = pcall(function() return x:get() end)
  if ok then return v end
  return x
end

return M
