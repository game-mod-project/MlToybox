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
  every_item_sits_in_the_games_group = function()
    local known = rows()
    for _, it in ipairs(catalog.items) do
      T.eq(it.sub, known[it.type].sub, "subcategory of " .. it.id)
      local g = catalog.groups[it.sub]
      T.truthy(g, "group of " .. it.id)
      T.truthy(catalog.categories[g.category], "category of group " .. tostring(it.sub))
      -- 묶음이 놓이는 분류는 품목의 분류와 같다. 목제 부품만 다르다: 표에서는 제작 재료(3)인데 영지 창은 유지보수 묶음을 건설 아래에 보여 준다
      if it.id ~= "WoodenParts" then T.eq(g.category, known[it.type].cat, "category of " .. it.id) end
    end
  end,
  describe_lists_every_resource_in_the_region_panels_order = function()
    local d = catalog.describe()
    T.eq(#d, 79, "regional wealth and 78 goods")
    T.eq(d[1].id, "RegionalWealth", "special first"); T.eq(d[1].name, "지역 재화", "its name"); T.eq(d[1].category, "영지", "its category")
    local ids = catalog.ids()
    local at = {}
    for i, entry in ipairs(d) do
      T.eq(entry.id, ids[i], "same order as ids()")
      T.truthy(type(entry.name) == "string" and #entry.name > 0, "name of " .. entry.id)
      T.truthy(type(entry.category) == "string" and #entry.category > 0, "category of " .. entry.id)
      at[entry.id] = i
    end
    -- 건설의 유지보수 묶음에 도구와 목제 부품이 나란히 있다
    T.eq(at.WoodenParts, at.Irontools + 1, "wooden parts next to tools")
    T.eq(d[at.WoodenParts].category, "건설", "shown under construction"); T.eq(d[at.WoodenParts].group, "유지보수", "maintenance")
    T.eq(d[at.Beef].category, "식량", "beef category"); T.eq(d[at.Beef].group, "고기", "beef group")
    -- 분류는 건설, 식량, 제작 재료, 일용품, 군사 순서로 이어지고, 한 분류의 품목은 흩어지지 않는다
    local order = {}
    for i = 2, #d do
      if d[i].category ~= order[#order] then order[#order + 1] = d[i].category end
    end
    T.eq(table.concat(order, ","), "건설,식량,제작 재료,일용품,군사", "category order")
    -- 묶음 안에서는 품목 번호 순서(영지 창의 순서)
    T.eq(at.Timber + 1, at.planks, "timber then planks"); T.eq(at.fish + 1, at.Eel, "carp then eel"); T.eq(at.Eel + 1, at.smokedfish, "then smoked fish")
  end,
  names_are_the_ones_the_region_panel_shows = function()
    -- 사용자가 올린 영지 창 캡처(2026-10-02)에서 읽은 이름. 묶음 안에서 품목 번호 순서로 놓여 있었다
    local by = {}
    for _, entry in ipairs(catalog.describe()) do by[entry.id] = entry.name end
    local shown = { Timber = "목재", planks = "판자", Rubble = "잔해 돌", DressedStone = "다듬은 돌", Mortar = "모르타르", Irontools = "도구",
      WoodenParts = "목제 부품", clayTILES = "지붕 타일", Berries = "열매", sausage = "소시지", SmallGame = "소형 사냥감", Chevon = "염소고기",
      fish = "잉어", smokedfish = "훈제 생선", Beetroots = "비트", Quinces = "마르멜로", RyeBread = "호밀빵", lebkuchen = "렙쿠헨", OatGrain = "귀리",
      Eggs = "달걀", WheatFlour = "밀가루", WheatGrain = "밀 낟알", WheatSheaves = "밀 다발", RoughStone = "잡석", Hides = "생가죽", Pelts = "털가죽",
      Cloth_Linen = "리넨", Yarn = "실", IronSlabs = "철판", iron_parts = "철제 부품", Charcoal = "숯", Ale = "맥주", mead = "벌꿀주", AnimalFeed = "동물 사료" }
    for id, name in pairs(shown) do T.eq(by[id], name, id) end
  end,
  new_goods_use_the_games_own_names = function()
    local by = {}
    for _, it in ipairs(catalog.items) do by[it.id] = it.type end
    T.eq(by.Mortar, 329, "Mortar"); T.eq(by.WoodenParts, 7, "WoodenParts"); T.eq(by.Rubble, 15, "Rubble"); T.eq(by.SmallGame, 330, "SmallGame")
    T.eq(by.Carrots, 337, "Carrots"); T.eq(by.Cheese, 348, "Cheese"); T.eq(by.OatGrain, 349, "OatGrain"); T.eq(by.AnimalFeed, 351, "AnimalFeed")
    T.eq(by.meat, nil, "the old combined meat item is gone"); T.eq(by.vegetables, nil, "the deprecated vegetables item is gone")
  end,
})
