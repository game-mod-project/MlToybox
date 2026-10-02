-- 개발용: 게임의 품목 표(DT_Items)의 행을 모두 적는다. 한 줄에 행 이름, 표시 이름 키, 분류, 하위 분류, 거래 가능, 보관 방식.
-- 자원 목록(features/resources_catalog.lua)을 게임의 분류와 맞출 때 쓴다. 분류 번호는 EItemCategory / EItemSubcategory.
local dt = StaticFindObject("/Game/NotStronghold/Data/DT_Items.DT_Items")
if not dt or not dt:IsValid() then print("NO_TABLE") return end

local function text(v)
  if type(v) == "string" then return v end
  local ok, s = pcall(function() return v:ToString() end)
  if ok then return s end
  return tostring(v)
end

local n = 0
for _, raw in ipairs(dt:GetRowNames()) do
  local name = text(raw)
  local row = dt:FindRow(name)
  if row then
    n = n + 1
    local ok, line = pcall(function()
      return string.format("%s\t%s\tcat=%d\tsub=%d\ttradeable=%s\tstorage=%d", name, text(row.Name), row.ItemCategory, row.Subcategory,
        tostring(row.tradeable), row.storageType)
    end)
    print(ok and line or (name .. "\tERR " .. tostring(line)))
  end
end
print("rows", n)
