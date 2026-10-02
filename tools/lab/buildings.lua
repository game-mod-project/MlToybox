-- 개발용: 게임의 건물 표(buildingStats)에서 저장 한도가 있는 행을 적는다.
-- 한 줄에 건물 종류 번호(행 이름), 표의 이름, 건물 기능(EBuildingFunction), 일반·목재·식량 한도.
-- 저장 용량 기능의 건물 목록(features/storage_catalog.lua)과 견본(tests/fixtures/dt_buildings.tsv)을 만들 때 쓴다.
-- 건물의 한글 이름은 tools/lab/translations.lua 로 BuildingNames 표를 읽는다(buildingID_<번호>).
local dt = StaticFindObject("/Game/NotStronghold/Data/buildingStats.buildingStats")
if not dt or not dt:IsValid() then print("NO_TABLE") return end

local function text(v)
  if type(v) == "string" then return v end
  local ok, s = pcall(function() return v:ToString() end)
  if ok then return s end
  return tostring(v)
end

local rows, shown = 0, 0
for _, raw in ipairs(dt:GetRowNames()) do
  local name = text(raw)
  local row = dt:FindRow(name)
  if row then
    rows = rows + 1
    local ok, line = pcall(function()
      local g, l, p = row.storageLimitGeneric, row.storageLimitLarge, row.storageLimitPantry
      if g == 0 and l == 0 and p == 0 then return nil end
      return string.format("%s\t%s\tfn=%d\tgeneric=%d\tlarge=%d\tpantry=%d", name, text(row.DisplayName), row.buildingFunction, g, l, p)
    end)
    if not ok then print(name .. "\tERR " .. tostring(line))
    elseif line then shown = shown + 1 print(line) end
  end
end
print("rows", rows, "withLimit", shown)
