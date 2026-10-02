-- 자원 ID(EItemType 이름) ↔ 값. 게임의 품목 표(DT_Items)가 자원으로 분류한 것 전부다:
-- 분류(EItemCategory)가 건설 1, 식량 2, 제작 재료 3, 일용품 4, 군사 6 인 행. 영지 창이 보여 주는 묶음과 같다.
-- 가축(5), 과도기(8), 공성(11), 조리법(12), 분류 없음(0)은 넣지 않는다(분류 없음에는 옛 meat, vegetables_DEPREC 등이 있다).
-- 출처: 표는 tools/lab/items.lua 로 읽어 tests/fixtures/dt_items.tsv 에 두었다(2026-10-02, Steam buildid 24905706).
-- 이름은 CXXHeaderDump/ManorLords_enums.hpp 의 enum class EItemType. tests/resources_catalog_spec.lua 가 이 목록과 표를 맞춰 본다.
-- 게임이 업데이트되면 표를 다시 읽어 견본을 바꾸고, 스펙이 알려 주는 대로 여기를 고친다.
local M = {}

M.special = { "RegionalWealth" }   -- 국고·영향력은 features/lord.lua

M.items = {
  -- 건설
  { id = "Timber", type = 16 }, { id = "planks", type = 17 },   -- 목재 작업물
  { id = "Rubble", type = 15 }, { id = "DressedStone", type = 283 }, { id = "Mortar", type = 329 },   -- 석재 작업물
  { id = "Irontools", type = 6 },   -- 유지보수
  { id = "clayTILES", type = 269 },   -- 지붕 공사
  -- 식량
  { id = "Berries", type = 171 }, { id = "mushrooms", type = 279 },   -- 채집한 상품
  { id = "sausage", type = 320 }, { id = "SmallGame", type = 330 }, { id = "Mutton", type = 332 }, { id = "Chevon", type = 333 }, { id = "Pork", type = 334 }, { id = "Beef", type = 335 }, { id = "Chicken", type = 336 },   -- 고기
  { id = "fish", type = 30 }, { id = "Eel", type = 32 }, { id = "smokedfish", type = 323 },   -- 물고기
  { id = "Carrots", type = 337 }, { id = "Cabbages", type = 338 }, { id = "Beetroots", type = 339 },   -- 채소
  { id = "Pears", type = 45 }, { id = "Quinces", type = 46 }, { id = "apples", type = 296 },   -- 과일
  { id = "RyeBread", type = 170 }, { id = "WheatBread", type = 172 }, { id = "lebkuchen", type = 326 }, { id = "OatGrain", type = 349 },   -- 곡물
  { id = "Eggs", type = 220 }, { id = "Milk", type = 221 }, { id = "Cheese", type = 348 },   -- 동물 생산물
  -- 제작 재료
  { id = "WheatFlour", type = 5 }, { id = "Herbs", type = 18 }, { id = "RyeFlour", type = 33 }, { id = "Honey", type = 141 }, { id = "Wax", type = 142 }, { id = "Salt", type = 145 }, { id = "Spices", type = 176 },   -- 원재료
  { id = "WheatGrain", type = 1 }, { id = "Flax", type = 11 }, { id = "Malt", type = 29 }, { id = "Barley", type = 165 }, { id = "WheatSheaves", type = 217 }, { id = "RyeSheaves", type = 298 }, { id = "RyeGrain", type = 299 }, { id = "OatSheaves", type = 350 },   -- 작물
  { id = "IronOre", type = 14 }, { id = "RoughStone", type = 27 }, { id = "Clay", type = 146 },   -- 광물
  { id = "Hides", type = 3 }, { id = "Leather", type = 4 }, { id = "Pelts", type = 9 }, { id = "Cloth_Linen", type = 12 }, { id = "Wool", type = 23 }, { id = "Yarn", type = 148 },   -- 섬유
  { id = "IronSlabs", type = 35 }, { id = "iron_parts", type = 317 },   -- 금속 작업물
  { id = "WoodenParts", type = 7 },   -- 유지보수(영지 창에서는 건설 아래)
  -- 일용품
  { id = "Charcoal", type = 13 }, { id = "Firewood", type = 216 },   -- 연료
  { id = "Clothes", type = 149 },   -- 의류
  { id = "Shoes", type = 10 },   -- 신발
  { id = "Ale", type = 28 }, { id = "mead", type = 325 }, { id = "cider", type = 327 },   -- 음료
  { id = "AnimalFeed", type = 351 },   -- 사료
  -- 군사
  { id = "spears", type = 133 }, { id = "weapons_sidearms", type = 177 }, { id = "weapons_polearms", type = 178 },   -- 근접 무기
  { id = "warbows", type = 205 }, { id = "crossbows", type = 206 },   -- 원거리 무기
  { id = "shields_small", type = 270 }, { id = "shields_large", type = 271 },   -- 방패
  { id = "gambesons", type = 163 }, { id = "mail_armor", type = 164 }, { id = "militia_helmets_resource", type = 273 }, { id = "PlateArmor", type = 293 },   -- 갑옷
}

function M.ids()
  local out = {}
  for _, s in ipairs(M.special) do out[#out + 1] = s end
  for _, it in ipairs(M.items) do out[#out + 1] = it.id end
  return out
end

return M
