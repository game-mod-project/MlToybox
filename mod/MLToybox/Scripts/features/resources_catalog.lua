-- 자원 ID(EItemType 이름) ↔ 값, 묶음, 한글 이름. 게임의 품목 표(DT_Items)가 자원으로 분류한 것 전부다:
-- 분류(EItemCategory)가 건설 1, 식량 2, 제작 재료 3, 일용품 4, 군사 6 인 행. 영지 창이 보여 주는 묶음과 같다.
-- 가축(5), 과도기(8), 공성(11), 조리법(12), 분류 없음(0)은 넣지 않는다(분류 없음에는 옛 meat, vegetables_DEPREC 등이 있다).
-- 출처: 표는 tools/lab/items.lua 로 읽어 tests/fixtures/dt_items.tsv 에 두었다(2026-10-02, Steam buildid 24905706).
-- 이름은 CXXHeaderDump/ManorLords_enums.hpp 의 enum class EItemType. tests/resources_catalog_spec.lua 가 이 목록과 표를 맞춰 본다.
-- 순서는 영지 창의 순서다: 분류, 그 안의 묶음(EItemSubcategory), 묶음 안에서는 품목 번호. sub 는 묶음 번호, name 은 게임의 한글 이름.
-- 게임이 업데이트되면 표를 다시 읽어 견본을 바꾸고, 스펙이 알려 주는 대로 여기를 고친다.
local M = {}

M.special = { "RegionalWealth" }   -- 국고·영향력은 features/lord.lua
M.specialInfo = { RegionalWealth = { name = "지역 재화", category = "영지" } }

-- 분류(EItemCategory)의 이름
M.categories = { [1] = "건설", [2] = "식량", [3] = "제작 재료", [4] = "일용품", [6] = "군사" }

-- 묶음(EItemSubcategory)의 이름과, 영지 창에서 그 묶음이 놓이는 분류
M.groups = {
  [1] = { name = "목재 작업물", category = 1 }, [2] = { name = "석재 작업물", category = 1 }, [3] = { name = "유지보수", category = 1 }, [4] = { name = "지붕 공사", category = 1 },
  [8] = { name = "채집한 상품", category = 2 }, [6] = { name = "고기", category = 2 }, [12] = { name = "물고기", category = 2 }, [5] = { name = "채소", category = 2 }, [9] = { name = "과일", category = 2 }, [7] = { name = "곡물", category = 2 }, [10] = { name = "동물 생산물", category = 2 },
  [13] = { name = "원재료", category = 3 }, [18] = { name = "작물", category = 3 }, [15] = { name = "광물", category = 3 }, [14] = { name = "섬유", category = 3 }, [16] = { name = "금속 작업물", category = 3 },
  [17] = { name = "연료", category = 4 }, [19] = { name = "의류", category = 4 }, [20] = { name = "신발", category = 4 }, [21] = { name = "음료", category = 4 }, [28] = { name = "사료", category = 4 },
  [22] = { name = "MeleeWeapons", category = 6 }, [23] = { name = "RangedWeapons", category = 6 }, [24] = { name = "Shields", category = 6 }, [25] = { name = "Armor", category = 6 },
}

M.items = {
  -- 건설
  { id = "Timber", type = 16, sub = 1, name = "목재" }, { id = "planks", type = 17, sub = 1, name = "판자" },   -- 목재 작업물
  { id = "Rubble", type = 15, sub = 2, name = "잔해 돌" }, { id = "DressedStone", type = 283, sub = 2, name = "다듬은 돌" }, { id = "Mortar", type = 329, sub = 2, name = "모르타르" },   -- 석재 작업물
  { id = "Irontools", type = 6, sub = 3, name = "도구" }, { id = "WoodenParts", type = 7, sub = 3, name = "목제 부품" },   -- 유지보수
  { id = "clayTILES", type = 269, sub = 4, name = "지붕 타일" },   -- 지붕 공사
  -- 식량
  { id = "Berries", type = 171, sub = 8, name = "열매" }, { id = "mushrooms", type = 279, sub = 8, name = "버섯" },   -- 채집한 상품
  { id = "sausage", type = 320, sub = 6, name = "소시지" }, { id = "SmallGame", type = 330, sub = 6, name = "소형 사냥감" }, { id = "Mutton", type = 332, sub = 6, name = "양고기" }, { id = "Chevon", type = 333, sub = 6, name = "염소고기" }, { id = "Pork", type = 334, sub = 6, name = "돼지고기" }, { id = "Beef", type = 335, sub = 6, name = "소고기" }, { id = "Chicken", type = 336, sub = 6, name = "닭고기" },   -- 고기
  { id = "fish", type = 30, sub = 12, name = "잉어" }, { id = "Eel", type = 32, sub = 12, name = "장어" }, { id = "smokedfish", type = 323, sub = 12, name = "훈제 생선" },   -- 물고기
  { id = "Carrots", type = 337, sub = 5, name = "당근" }, { id = "Cabbages", type = 338, sub = 5, name = "양배추" }, { id = "Beetroots", type = 339, sub = 5, name = "비트" },   -- 채소
  { id = "Pears", type = 45, sub = 9, name = "배" }, { id = "Quinces", type = 46, sub = 9, name = "마르멜로" }, { id = "apples", type = 296, sub = 9, name = "사과" },   -- 과일
  { id = "RyeBread", type = 170, sub = 7, name = "호밀빵" }, { id = "WheatBread", type = 172, sub = 7, name = "밀빵" }, { id = "lebkuchen", type = 326, sub = 7, name = "렙쿠헨" }, { id = "OatGrain", type = 349, sub = 7, name = "귀리" },   -- 곡물
  { id = "Eggs", type = 220, sub = 10, name = "달걀" }, { id = "Milk", type = 221, sub = 10, name = "우유" }, { id = "Cheese", type = 348, sub = 10, name = "치즈" },   -- 동물 생산물
  -- 제작 재료
  { id = "WheatFlour", type = 5, sub = 13, name = "밀가루" }, { id = "Herbs", type = 18, sub = 13, name = "약초" }, { id = "RyeFlour", type = 33, sub = 13, name = "호밀 가루" }, { id = "Honey", type = 141, sub = 13, name = "꿀" }, { id = "Wax", type = 142, sub = 13, name = "밀랍" }, { id = "Salt", type = 145, sub = 13, name = "소금" }, { id = "Spices", type = 176, sub = 13, name = "향신료" },   -- 원재료
  { id = "WheatGrain", type = 1, sub = 18, name = "밀 낟알" }, { id = "Flax", type = 11, sub = 18, name = "아마" }, { id = "Malt", type = 29, sub = 18, name = "맥아" }, { id = "Barley", type = 165, sub = 18, name = "보리" }, { id = "WheatSheaves", type = 217, sub = 18, name = "밀 다발" }, { id = "RyeSheaves", type = 298, sub = 18, name = "호밀 다발" }, { id = "RyeGrain", type = 299, sub = 18, name = "호밀 낟알" }, { id = "OatSheaves", type = 350, sub = 18, name = "귀리 다발" },   -- 작물
  { id = "IronOre", type = 14, sub = 15, name = "철광석" }, { id = "RoughStone", type = 27, sub = 15, name = "잡석" }, { id = "Clay", type = 146, sub = 15, name = "점토" },   -- 광물
  { id = "Hides", type = 3, sub = 14, name = "생가죽" }, { id = "Leather", type = 4, sub = 14, name = "가죽" }, { id = "Pelts", type = 9, sub = 14, name = "털가죽" }, { id = "Cloth_Linen", type = 12, sub = 14, name = "리넨" }, { id = "Wool", type = 23, sub = 14, name = "양모" }, { id = "Yarn", type = 148, sub = 14, name = "실" },   -- 섬유
  { id = "IronSlabs", type = 35, sub = 16, name = "철판" }, { id = "iron_parts", type = 317, sub = 16, name = "철제 부품" },   -- 금속 작업물
  -- 일용품
  { id = "Charcoal", type = 13, sub = 17, name = "숯" }, { id = "Firewood", type = 216, sub = 17, name = "장작" },   -- 연료
  { id = "Clothes", type = 149, sub = 19, name = "의류" },   -- 의류
  { id = "Shoes", type = 10, sub = 20, name = "신발" },   -- 신발
  { id = "Ale", type = 28, sub = 21, name = "맥주" }, { id = "mead", type = 325, sub = 21, name = "벌꿀주" }, { id = "cider", type = 327, sub = 21, name = "사이더" },   -- 음료
  { id = "AnimalFeed", type = 351, sub = 28, name = "동물 사료" },   -- 사료
  -- 군사
  { id = "spears", type = 133, sub = 22 }, { id = "weapons_sidearms", type = 177, sub = 22 }, { id = "weapons_polearms", type = 178, sub = 22 },   -- MeleeWeapons
  { id = "warbows", type = 205, sub = 23 }, { id = "crossbows", type = 206, sub = 23 },   -- RangedWeapons
  { id = "shields_small", type = 270, sub = 24 }, { id = "shields_large", type = 271, sub = 24 },   -- Shields
  { id = "gambesons", type = 163, sub = 25 }, { id = "mail_armor", type = 164, sub = 25 }, { id = "militia_helmets_resource", type = 273, sub = 25 }, { id = "PlateArmor", type = 293, sub = 25 },   -- Armor
}

function M.ids()
  local out = {}
  for _, s in ipairs(M.special) do out[#out + 1] = s end
  for _, it in ipairs(M.items) do out[#out + 1] = it.id end
  return out
end

-- 오버레이가 표에 쓸 것: id, 한글 이름, 분류, 묶음. ids() 와 같은 순서(영지 창의 순서)다. 이름이 없으면 id 를 쓴다
function M.describe()
  local out = {}
  for _, s in ipairs(M.special) do
    local info = M.specialInfo[s] or {}
    out[#out + 1] = { id = s, name = info.name or s, category = info.category }
  end
  for _, it in ipairs(M.items) do
    local g = M.groups[it.sub]
    out[#out + 1] = { id = it.id, name = it.name or it.id, category = g and M.categories[g.category] or nil, group = g and g.name or nil }
  end
  return out
end

return M
