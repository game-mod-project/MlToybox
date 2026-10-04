local T = require("t")
local F = require("fakes")
local game = require("core.game")
local native = require("core.native")
local region = require("features.region")

local function fname(s) return { ToString = function() return s end } end

local function makeRegion(tag, wait)
  local r = F.object({ nextLivestockOrderIn = wait or 0 })
  r.regionUniqueTag = fname(tag)
  r.regionName = fname(tag:upper())
  return r
end

-- 덩어리형 매장지: 덩어리마다 양(amt)과 용량(capacity). 철에 따라 다시 차는 것(seasonal)만 용량이 있다
local function clumpNode(owner, resType, seasonal, amounts)
  local clumps = {}
  for i, a in ipairs(amounts) do
    clumps[i] = F.object({ amt = a[1], capacity = a[2], bSeasonal = seasonal, resType = fname(resType), Region = owner })
  end
  return F.object({ resourceClumps = F.array(clumps) })
end

-- 광물 매장지: 덩어리가 없다. 남은 양은 리플렉션에 없어 네이티브가 알려 준다
local function mineralNode() return F.object({ resourceClumps = F.array({}) }) end

local finds = 0
local function world(regions, nodes)
  finds = 0
  game.playerRegions = function() return regions end
  game.find.all = function(cls)
    if cls == "ResourceNode" then finds = finds + 1 return nodes end
    return {}
  end
  native.nodes = nil
end

local function sum(node, field)
  local total = 0
  for _, c in ipairs(node.resourceClumps) do total = total + c[field] end
  return total
end

local function hex(r) return string.format("%X", r:GetAddress()) end

T.run({
  livestock_trader_wait_is_cleared_only_when_the_option_is_on = function()
    local mine = makeRegion("eich", 19)
    world({ mine }, {})
    region.tick({}, { enabled = true })
    T.eq(mine.nextLivestockOrderIn, 19, "option off: untouched")
    region.tick({}, { enabled = true, noLivestockWait = true })
    T.eq(mine.nextLivestockOrderIn, 0, "option on: no wait")
  end,
  clump_deposits_below_the_target_are_filled_evenly = function()
    local mine = makeRegion("imm")
    local fish = clumpNode(mine, "res_fish", true, { { 20, 33 }, { 25, 33 }, { 30, 33 } })
    world({ mine }, { fish })
    region.tick({}, { enabled = true, targets = { Fish = 900 } })
    T.eq(sum(fish, "amt"), 900, "amount reaches the target")
    T.eq(sum(fish, "capacity"), 900, "seasonal clumps get room for it")
    T.eq(fish.resourceClumps[1].amt, 300, "an even share per clump")
  end,
  stone_has_no_capacity_and_only_its_amount_is_raised = function()
    local mine = makeRegion("imm")
    local stone = clumpNode(mine, "Stone", false, { { 5, 0 }, { 3, 0 } })
    world({ mine }, { stone })
    region.tick({}, { enabled = true, targets = { Stone = 500 } })
    T.eq(sum(stone, "amt"), 500, "amount")
    T.eq(sum(stone, "capacity"), 0, "capacity untouched")
  end,
  a_deposit_at_or_above_the_target_is_left_alone = function()
    local mine = makeRegion("imm")
    local fish = clumpNode(mine, "res_fish", true, { { 500, 500 }, { 100, 500 } })
    world({ mine }, { fish })
    region.tick({}, { enabled = true, targets = { Fish = 600 } })
    T.eq(fish.resourceClumps[1].amt, 500, "first clump")
    T.eq(fish.resourceClumps[2].amt, 100, "second clump")
  end,
  a_region_target_wins_and_zero_turns_that_region_off = function()
    local eich, imm, hof = makeRegion("eich"), makeRegion("imm"), makeRegion("hof")
    local a = clumpNode(eich, "mushrooms", true, { { 1, 9 } })
    local b = clumpNode(imm, "mushrooms", true, { { 1, 9 } })
    local c = clumpNode(hof, "mushrooms", true, { { 1, 9 } })
    world({ eich, imm, hof }, { a, b, c })
    region.tick({}, {
      enabled = true, targets = { Mushrooms = 100 },
      regionTargets = { imm = { Mushrooms = 700 }, hof = { Mushrooms = 0 } },
    })
    T.eq(sum(a, "amt"), 100, "eich follows the common target")
    T.eq(sum(b, "amt"), 700, "imm has its own")
    T.eq(sum(c, "amt"), 1, "hof is turned off")
  end,
  deposits_of_other_lords_are_not_touched_or_listed = function()
    local mine, theirs = makeRegion("eich"), makeRegion("gold")
    local a = clumpNode(theirs, "berries", true, { { 1, 9 } })
    world({ mine }, { a })
    local state = {}
    region.tick(state, { enabled = true, targets = { Berries = 100 } })
    T.eq(sum(a, "amt"), 1, "untouched")
    T.eq(#state.region.regions[1].deposits, 0, "not listed under my region")
  end,
  status_lists_my_regions_with_the_wait_and_their_deposits = function()
    local eich, imm = makeRegion("eich", 7), makeRegion("imm")
    local fish = clumpNode(imm, "res_fish", true, { { 20, 33 }, { 25, 33 } })
    local stone = clumpNode(eich, "Stone", false, { { 39, 0 } })
    world({ eich, imm }, { fish, stone, mineralNode() })
    -- 네이티브가 날짜가 넘어갈 때 적어 둔 광물 매장지: 영지의 주소(16진수), 종류 번호, 남은 양
    native.nodes = {
      { region = hex(eich), type = 3, amount = 25 }, { region = hex(eich), type = 1, amount = 119 },
      { region = hex(imm), type = 2, amount = 118 }, { region = "DEAD", type = 2, amount = 5 },
    }
    local state = {}
    region.tick(state, { enabled = true })
    local r1, r2 = state.region.regions[1], state.region.regions[2]
    T.eq(r1.key, "eich", "key")
    T.eq(r1.name, "EICH", "name")
    T.eq(r1.livestockWait, 7, "days until the livestock trader")
    T.eq(#r1.deposits, 3, "salt, clay, stone")
    T.eq(r1.deposits[1].kind, "Salt", "listed in table order")
    T.eq(r1.deposits[1].amount, 119, "salt amount")
    T.eq(r1.deposits[2].kind, "Clay", "clay")
    T.eq(r1.deposits[2].amount, 25, "clay amount")
    T.eq(r1.deposits[3].kind, "Stone", "stone")
    T.eq(r1.deposits[3].amount, 39, "stone amount")
    T.eq(#r2.deposits, 2, "iron, fish")
    T.eq(r2.deposits[1].kind, "Iron", "iron")
    T.eq(r2.deposits[2].kind, "Fish", "fish")
    T.eq(r2.deposits[2].amount, 45, "fish amount")
    T.eq(r2.deposits[2].capacity, 66, "fish capacity")
  end,
  observe_only_reads = function()
    local mine = makeRegion("imm", 12)
    local fish = clumpNode(mine, "res_fish", true, { { 20, 33 } })
    world({ mine }, { fish })
    local state = {}
    region.observe(state)
    T.eq(mine.nextLivestockOrderIn, 12, "wait untouched")
    T.eq(sum(fish, "amt"), 20, "amount untouched")
    T.eq(state.region.regions[1].livestockWait, 12, "wait reported")
    T.eq(state.region.regions[1].deposits[1].amount, 20, "amount reported")
  end,
  the_node_list_is_found_once_per_map_and_again_after_a_node_goes_away = function()
    local mine = makeRegion("imm")
    local fish = clumpNode(mine, "res_fish", true, { { 20, 33 } })
    world({ mine }, { fish })
    local state = {}
    region.tick(state, { enabled = true })
    region.tick(state, { enabled = true })
    T.eq(finds, 1, "searched once")
    fish.IsValid = function() return false end   -- 맵이 바뀌는 등으로 객체가 사라졌다
    region.tick(state, { enabled = true })
    region.tick(state, { enabled = true })
    T.eq(finds, 2, "searched again after an invalid node")
    region.tick({}, { enabled = true })           -- 새 맵: 공유 상태가 비워진다
    T.eq(finds, 3, "a fresh state searches again")
  end,
  targets_out_of_range_or_not_numbers_are_ignored_or_capped = function()
    local mine = makeRegion("imm")
    local fish = clumpNode(mine, "res_fish", true, { { 20, 33 } })
    world({ mine }, { fish })
    region.tick({}, { enabled = true, targets = { Fish = "x" } })
    T.eq(sum(fish, "amt"), 20, "not a number")
    region.tick({}, { enabled = true, targets = { Fish = -5 } })
    T.eq(sum(fish, "amt"), 20, "negative")
    region.tick({}, { enabled = true, targets = { Fish = 99999999 } })
    T.eq(sum(fish, "amt"), region.MAX_TARGET, "capped")
  end,
})
