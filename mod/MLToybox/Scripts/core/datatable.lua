local safe = require("core.safe")

local M = {}

M.PATHS = {
  upgrades = "/Game/NotStronghold/Data/DT_Upgrades.DT_Upgrades",
  buildingStats = "/Game/NotStronghold/Data/buildingStats.buildingStats",
  unitTemplates = "/Game/NotStronghold/Data/DT_UnitTemplates.DT_UnitTemplates",
  mercenaries = "/Game/NotStronghold/Data/DT_MercenaryCompanies.DT_MercenaryCompanies",
  items = "/Game/NotStronghold/Data/DT_Items.DT_Items",
  residentialSettings = "/Script/ManorLords.Default__ResidentialRequirementSettings",
}

M.find = function(path) return StaticFindObject(path) end

function M.object(key)
  local o = M.find(M.PATHS[key])
  if not safe.valid(o) then error("datatable not found: " .. key, 2) end
  return o
end

local function nameOf(n)
  if type(n) == "string" then return n end
  local ok, s = pcall(function() return n:ToString() end)
  if ok then return s end
  return tostring(n)
end

-- FindRow 는 참조를 돌려주므로 fn 안에서의 필드 쓰기가 표에 남는다 (findings: 스파이크 결과)
function M.forEachRow(key, fn)
  local dt = M.object(key)
  local n = 0
  for _, raw in ipairs(dt:GetRowNames()) do
    local name = nameOf(raw)
    local row = dt:FindRow(name)
    if row then
      n = n + 1
      fn(name, row)
    end
  end
  return n
end

return M
