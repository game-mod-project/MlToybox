-- 개발용: 게임의 번역 표(DT_Translation_*)의 행을 파일에 적는다. 한 줄에 표 이름, 행 이름(번역 키), 영어, 한국어.
-- 자원의 한글 이름(features/resources_catalog.lua)을 게임이 보여 주는 글과 맞출 때 쓴다.
-- 쓰는 법: pwsh tools/lab.ps1 -File tools/lab/translations.lua -Vars @{ OUT = '<적을 파일의 전체 경로>'; TABLES = 'Items,MainUI,Military' }
local OUT = [[__OUT__]]
local TABLES = "__TABLES__"

local function text(v)
  if type(v) == "string" then return v end
  local ok, s = pcall(function() return v:ToString() end)
  if ok and type(s) == "string" then return s end
  return tostring(v)
end

local function oneLine(s)
  return (text(s):gsub("[\r\n\t]+", " "))
end

local f, err = io.open(OUT, "wb")
if not f then print("NO_FILE", err) return end
for name in TABLES:gmatch("[^,]+") do
  local path = "/Game/Translation/HoodedHorse/DT_Translation_" .. name .. ".DT_Translation_" .. name
  local dt = StaticFindObject(path)
  if not dt or not dt:IsValid() then
    print("NO_TABLE", name)
  else
    local n = 0
    for _, raw in ipairs(dt:GetRowNames()) do
      local key = text(raw)
      local row = dt:FindRow(key)
      if row then
        local ok, line = pcall(function() return name .. "\t" .. key .. "\t" .. oneLine(row.en_US) .. "\t" .. oneLine(row.ko_KR) end)
        f:write((ok and line or (name .. "\t" .. key .. "\tERR " .. tostring(line))) .. "\n")
        n = n + 1
      end
    end
    print(name, n)
  end
end
f:close()
print("written", OUT)
