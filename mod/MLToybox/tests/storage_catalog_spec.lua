local T = require("t")
local fileio = require("core.fileio")
local catalog = require("features.storage_catalog")

-- 게임의 건물 표(buildingStats)에서 저장 한도가 있는 행을 적은 견본: tests/fixtures/dt_buildings.tsv
-- (tools/lab/buildings.lua 로 만든다. 2026-10-02, Steam buildid 24905706).
-- 한 줄: 건물 종류 번호(행 이름) \t 표의 이름 \t fn=건물 기능 \t generic=일반 \t large=목재 \t pantry=식량
local function rows()
  local text = fileio.read(SCRIPTS_DIR .. "\\..\\tests\\fixtures\\dt_buildings.tsv")
  T.truthy(text, "fixture exists")
  local out = {}
  for line in text:gmatch("[^\r\n]+") do
    local id, name, fn, generic, large, pantry = line:match("^(%d+)\t([^\t]*)\tfn=(%d+)\tgeneric=(%d+)\tlarge=(%d+)\tpantry=(%d+)")
    if id then out[tonumber(id)] = { name = name, fn = tonumber(fn), generic = tonumber(generic), large = tonumber(large), pantry = tonumber(pantry) } end
  end
  return out
end

-- 게임의 번역 표 견본에서 건물 이름(BuildingNames 의 buildingID_<번호>)
local function names()
  local text = fileio.read(SCRIPTS_DIR .. "\\..\\tests\\fixtures\\dt_translations.tsv")
  T.truthy(text, "fixture exists")
  local out = {}
  for line in text:gmatch("[^\r\n]+") do
    local id, korean = line:match("^BuildingNames\tbuildingID_(%d+)\t[^\t]*\t([^\t]*)")
    if id then out[tonumber(id)] = korean end
  end
  return out
end

T.run({
  catalog_has_every_named_building_that_stores_goods = function()
    local known, named = rows(), names()
    local have = {}
    for _, b in ipairs(catalog.buildings) do have[b.type] = true end
    local missing = {}
    for type in pairs(known) do
      if named[type] and not have[type] then missing[#missing + 1] = string.format("%03d:%s", type, named[type]) end
    end
    table.sort(missing)
    T.eq(table.concat(missing, ", "), "", "buildings with a limit and a name that the catalog lacks")
    T.eq(#catalog.buildings, 52, "buildings in this build")
  end,
  catalog_has_nothing_the_game_does_not_name = function()
    local known, named = rows(), names()
    local extra, seen = {}, {}
    for _, b in ipairs(catalog.buildings) do
      if not known[b.type] or not named[b.type] then extra[#extra + 1] = tostring(b.type) end
      T.eq(seen[b.type], nil, "type used once: " .. b.type)
      seen[b.type] = true
    end
    T.eq(table.concat(extra, ", "), "", "catalog buildings without a limit or a name in the game")
  end,
  limits_and_names_are_the_games = function()
    local known, named = rows(), names()
    for _, b in ipairs(catalog.buildings) do
      local row = known[b.type]
      T.eq(b.generic, row.generic, "generic of " .. b.type); T.eq(b.large, row.large, "large of " .. b.type); T.eq(b.pantry, row.pantry, "pantry of " .. b.type)
      T.eq(b.name, named[b.type], "name of " .. b.type)
      T.truthy(b.generic > 0 or b.large > 0 or b.pantry > 0, "stores something: " .. b.type)
    end
  end,
  buildings_the_user_showed_have_the_values_on_their_panels = function()
    -- 사용자가 올린 건물 창 캡처(2026-10-02): 대형 식량 비축고 식량 2,500, 대형 창고 일반 2,500, 벌목장 목재 28, 농가 일반 1,200 + 식량 1,200
    local by = {}
    for _, b in ipairs(catalog.buildings) do by[b.name] = b end
    T.eq(by["대형 식량 비축고"].pantry, 2500, "large granary"); T.eq(by["대형 식량 비축고"].type, 68, "its type")
    T.eq(by["대형 창고"].generic, 2500, "large storehouse"); T.eq(by["대형 창고"].type, 99, "its type")
    T.eq(by["벌목장"].large, 28, "logging camp"); T.eq(by["벌목장"].generic, 0, "no generic storage there")
    T.eq(by["농가"].generic, 1200, "farmhouse generic"); T.eq(by["농가"].pantry, 1200, "farmhouse pantry")
  end,
  storage_buildings_come_first_then_the_rest_by_number = function()
    local b = catalog.buildings
    T.eq(b[1].name, "창고", "storehouse"); T.eq(b[2].name, "대형 창고", "large storehouse")
    T.eq(b[3].name, "식량 비축고", "granary"); T.eq(b[4].name, "대형 식량 비축고", "large granary")
    for i = 6, #b do T.truthy(b[i - 1].type < b[i].type, "ascending after the storage buildings: " .. b[i].type) end
  end,
  describe_uses_the_settings_key_as_the_id = function()
    local d = catalog.describe()
    T.eq(#d, #catalog.buildings, "every building")
    T.eq(d[1].id, "72", "id is the type number as text"); T.eq(d[1].name, "창고", "name"); T.eq(d[1].generic, 250, "generic")
    T.eq(d[1].large, 0, "no timber storage"); T.eq(d[1].pantry, 0, "no pantry")
    for i, entry in ipairs(d) do T.eq(entry.id, tostring(catalog.buildings[i].type), "same order") end
  end,
})
