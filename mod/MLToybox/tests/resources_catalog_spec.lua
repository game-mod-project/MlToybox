local T = require("t")
local fileio = require("core.fileio")
local catalog = require("features.resources_catalog")

-- 게임의 품목 표(DT_Items)를 그대로 적은 견본: tests/fixtures/dt_items.tsv (tools/lab/items.lua 로 만든다. 2026-10-02, Steam buildid 24905706).
-- 한 줄: 품목 번호(EItemType 값) \t 표의 이름 \t cat=분류 \t sub=하위 분류 \t tradeable \t storage
-- 자원 목록을 손으로 골랐다가 게임이 쓰는 품목 30종이 빠지고 쓰지 않는 품목 7종이 들어 있었다. 목록은 이 표와 맞아야 한다.
local function rows()
  local text = fileio.read(SCRIPTS_DIR .. "\\..\\tests\\fixtures\\dt_items.tsv")
  T.truthy(text, "fixture exists")
  local out = {}
  for line in text:gmatch("[^\r\n]+") do
    local id, name, cat, sub = line:match("^(%-?%d+)\t([^\t]*)\tcat=(%d+)\tsub=(%d+)")
    if id then out[tonumber(id)] = { name = name, cat = tonumber(cat), sub = tonumber(sub) } end
  end
  return out
end

-- 자원으로 다루는 분류(EItemCategory): 건설 1, 식량 2, 제작 재료 3, 일용품 4, 군사 6. 게임의 영지 창이 보여 주는 묶음이다.
-- 가축(5)은 재고가 아니라 동물이고, 과도기(8)·조리법(12)·공성(11)·분류 없음(0)은 영지 창에 없다
local GOODS = { [1] = true, [2] = true, [3] = true, [4] = true, [6] = true }

T.run({
  catalog_has_every_good_the_game_classifies = function()
    local have = {}
    for _, it in ipairs(catalog.items) do have[it.type] = true end
    local missing = {}
    for type, row in pairs(rows()) do
      if GOODS[row.cat] and not have[type] then missing[#missing + 1] = string.format("%03d:%s", type, row.name) end
    end
    table.sort(missing)
    T.eq(table.concat(missing, ", "), "", "goods in the game's table that the catalog lacks")
  end,
  catalog_has_nothing_the_game_does_not_show_as_a_good = function()
    local known = rows()
    local extra = {}
    for _, it in ipairs(catalog.items) do
      local row = known[it.type]
      if not row or not GOODS[row.cat] then extra[#extra + 1] = it.id end
    end
    T.eq(table.concat(extra, ", "), "", "catalog items the game does not classify as goods")
  end,
  catalog_ids_and_types_are_unique = function()
    local ids, types = {}, {}
    for _, it in ipairs(catalog.items) do
      T.eq(ids[it.id], nil, "id used once: " .. it.id); T.eq(types[it.type], nil, "type used once: " .. it.type)
      ids[it.id] = true; types[it.type] = true
    end
    T.eq(#catalog.items, 78, "goods in this build")
  end,
  ids_already_saved_in_settings_keep_their_item = function()
    -- control.json 의 목표는 이 이름을 키로 쓴다. 이름이 가리키는 품목이 바뀌면 사용자의 목표가 다른 품목에 걸린다
    local by = {}
    for _, it in ipairs(catalog.items) do by[it.id] = it.type end
    local saved = { Timber = 16, planks = 17, Firewood = 216, RoughStone = 27, DressedStone = 283, clayTILES = 269, WheatGrain = 1, WheatBread = 172,
      RyeBread = 170, fish = 30, Eggs = 220, apples = 296, Ale = 28, Irontools = 6, Cloth_Linen = 12, spears = 133, PlateArmor = 293 }
    for id, type in pairs(saved) do T.eq(by[id], type, id) end
  end,
  new_goods_use_the_games_own_names = function()
    local by = {}
    for _, it in ipairs(catalog.items) do by[it.id] = it.type end
    T.eq(by.Mortar, 329, "Mortar"); T.eq(by.WoodenParts, 7, "WoodenParts"); T.eq(by.Rubble, 15, "Rubble"); T.eq(by.SmallGame, 330, "SmallGame")
    T.eq(by.Carrots, 337, "Carrots"); T.eq(by.Cheese, 348, "Cheese"); T.eq(by.OatGrain, 349, "OatGrain"); T.eq(by.AnimalFeed, 351, "AnimalFeed")
    T.eq(by.meat, nil, "the old combined meat item is gone"); T.eq(by.vegetables, nil, "the deprecated vegetables item is gone")
  end,
})
