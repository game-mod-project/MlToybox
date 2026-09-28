-- 자원 ID(EItemType 이름) ↔ 값. 출처: CXXHeaderDump/ManorLords_enums.hpp enum class EItemType
local M = {}

M.special = { "RegionalWealth", "Treasury", "Influence" }

M.items = {
  { id = "Timber", type = 16 }, { id = "planks", type = 17 }, { id = "Firewood", type = 216 }, { id = "Charcoal", type = 13 },
  { id = "RoughStone", type = 27 }, { id = "DressedStone", type = 283 }, { id = "Clay", type = 146 }, { id = "clayTILES", type = 269 },
  { id = "IronOre", type = 14 }, { id = "IronSlabs", type = 35 }, { id = "Salt", type = 145 },
  { id = "WheatGrain", type = 1 }, { id = "WheatFlour", type = 5 }, { id = "WheatBread", type = 172 },
  { id = "RyeGrain", type = 299 }, { id = "RyeFlour", type = 33 }, { id = "RyeBread", type = 170 },
  { id = "Berries", type = 171 }, { id = "mushrooms", type = 279 }, { id = "meat", type = 147 }, { id = "fish", type = 30 },
  { id = "Eggs", type = 220 }, { id = "vegetables", type = 230 }, { id = "apples", type = 296 }, { id = "Honey", type = 141 },
  { id = "Pastries", type = 173 }, { id = "Barley", type = 165 }, { id = "Malt", type = 29 }, { id = "Ale", type = 28 },
  { id = "Hops", type = 166 }, { id = "Beer", type = 167 },
  { id = "Hides", type = 3 }, { id = "Leather", type = 4 }, { id = "Pelts", type = 9 }, { id = "Shoes", type = 10 },
  { id = "Flax", type = 11 }, { id = "Cloth_Linen", type = 12 }, { id = "Wool", type = 23 }, { id = "Yarn", type = 148 },
  { id = "Clothes", type = 149 }, { id = "dyes", type = 302 }, { id = "Wax", type = 142 }, { id = "Candle", type = 153 },
  { id = "Irontools", type = 6 },
  { id = "spears", type = 133 }, { id = "weapons_sidearms", type = 177 }, { id = "weapons_polearms", type = 178 },
  { id = "warbows", type = 205 }, { id = "crossbows", type = 206 }, { id = "shields_small", type = 270 }, { id = "shields_large", type = 271 },
  { id = "militia_helmets_resource", type = 273 }, { id = "gambesons", type = 163 }, { id = "mail_armor", type = 164 }, { id = "PlateArmor", type = 293 },
}

function M.ids()
  local out = {}
  for _, s in ipairs(M.special) do out[#out + 1] = s end
  for _, it in ipairs(M.items) do out[#out + 1] = it.id end
  return out
end

return M
