local T = require("t")
local F = require("fakes")
local bridge = require("core.bridge")
local fileio = require("core.fileio")
local game = require("core.game")
local native = require("core.native")
local plan = require("features.merc_plan")
local mood = require("features.mood")
local region = require("features.region")

-- 오버레이 코어(C++)가 쓴 control.json 견본을 모드가 읽을 수 있는가.
-- 견본은 네이티브 테스트(overlay_fixture_for_the_lua_spec_matches_core_output)가 코어의 출력과 같은지 지킨다.
local function load()
  local text = fileio.read(SCRIPTS_DIR .. "\\..\\tests\\fixtures\\control_from_overlay.json")
  T.truthy(text, "fixture exists")
  local control, err = bridge.parseControl(text)
  T.truthy(control, "parses: " .. tostring(err))
  return control
end

local function fname(s) return { ToString = function() return s end } end

-- 가짜 내 영지(자격 40, 공공질서 70, 가축 상인 대기 19일). 모드가 내 영지로 보는 목록을 이것들로 바꾼다
local function myRegions(...)
  local out = {}
  for i, tag in ipairs({ ... }) do
    out[i] = F.object({ regionUniqueTag = fname(tag), regionName = fname(tag), Approval = 40, publicOrder = 70, nextLivestockOrderIn = 19 })
  end
  game.playerRegions = function() return out end
  return table.unpack(out)
end

-- 덩어리 하나짜리 매장지(철마다 다시 차는 종류)
local function deposit(owner, resType, amt)
  return F.object({ resourceClumps = F.array({ F.object({ amt = amt, capacity = amt, bSeasonal = true, resType = fname(resType), Region = owner }) }) })
end

T.run({
  control_written_by_the_overlay_core_is_valid_for_the_mod = function()
    local control = load()
    T.eq(control.version, 1, "version"); T.eq(control.seq, 7, "seq")
    local f = control.features
    T.eq(f.build.enabled, true, "build.enabled"); T.eq(f.build.instantRepair, false, "build.instantRepair"); T.eq(f.build.instantBuild, true, "build.instantBuild")
    T.eq(f.upgrade.enabled, true, "upgrade.enabled")
    T.eq(f.lord.enabled, true, "lord.enabled"); T.eq(f.lord.intervalSec, 2, "lord.intervalSec")
    T.eq(f.lord.treasury, 150000, "lord.treasury"); T.eq(f.lord.influence, nil, "unmanaged key is absent"); T.eq(f.lord.kingsFavour, 0, "zero is a value")
  end,
  military_population_and_resources_keep_their_shape = function()
    local f = load().features
    T.eq(f.military.enabled, true, "military.enabled"); T.eq(f.military.zeroUpkeep, false, "military.zeroUpkeep"); T.eq(f.military.unlimitedSquads, true, "military.unlimitedSquads")
    T.eq(f.population.enabled, true, "population.enabled"); T.eq(f.population.multiplier, 3, "population.multiplier")
    T.eq(f.population.targetFamilies, 12, "population.targetFamilies"); T.eq(f.population.regionTargets.nus, 30, "population.regionTargets")
    T.eq(f.resources.enabled, true, "resources.enabled"); T.eq(f.resources.intervalSec, 2, "resources.intervalSec")
    T.eq(f.resources.targets.Timber, 500, "resources.targets"); T.eq(f.resources.regionTargets.gold.Timber, 2000, "resources.regionTargets")
  end,
  storage_limits_are_keyed_by_building_type_and_kind = function()
    local s = load().features.storage
    T.eq(s.enabled, true, "enabled"); T.eq(s.intervalSec, 5, "intervalSec")
    T.eq(s.limits["99"].generic, 5000, "large storehouse"); T.eq(s.limits["99"].pantry, nil, "a kind without a value is absent")
    T.eq(s.limits["69"].generic, 3000, "farmhouse generic"); T.eq(s.limits["69"].pantry, 6000, "farmhouse pantry")
  end,
  mercenary_companies_pass_the_mods_own_validation = function()
    local m = load().features.mercenaries
    T.eq(m.enabled, true, "enabled"); T.eq(m.refund, true, "refund"); T.eq(m.lockFromAi, true, "lockFromAi")
    T.eq(#m.companies, 3, "three companies")
    T.eq(m.companies[1].name, "토이박스 용병단", "korean name survives"); T.eq(m.companies[2].region, nil, "unset region is absent"); T.eq(m.companies[2].enabled, false, "disabled")
    -- 오버레이가 받아 주는 가장 긴 이름(한글 40자, 120바이트)을 모드도 40자로 센다
    T.eq(utf8.len(m.companies[3].name), 40, "forty characters"); T.eq(#m.companies[3].name, 120, "in 120 bytes")
    local ctx = { vanillaNames = { greencaps = true }, unitExists = function() return true end, regionKeys = { "nus", "gold" } }
    local valid, skipped = plan.validate(m.companies, ctx)
    T.eq(#skipped, 0, "nothing skipped")
    T.eq(#valid, 2, "the two companies in use")
    T.eq(valid[1].name, "토이박스 용병단", "name"); T.eq(#valid[1].units, 2, "units"); T.eq(valid[1].cost, 3000, "cost")
    T.eq(valid[1].region, "gold", "region"); T.eq(valid[1].banner, "greencaps", "banner")
    T.eq(valid[2].name, m.companies[3].name, "the longest name passes the mod's validation")
  end,
  commands_keep_the_fields_the_handlers_read = function()
    local commands = load().commands
    T.eq(#commands, 2, "two commands")
    local c = commands[1]
    T.eq(c.type, "setLord", "type"); T.eq(c.key, "influence", "key"); T.eq(c.value, 20000, "value")
    T.eq(c.id, "0123456789abcdef0123456789abcdef", "id"); T.eq(c.issuedAt, 1790000000, "issuedAt")
    local s = commands[2]
    T.eq(s.type, "spawnSquads", "type"); T.eq(s.unit, "spearMilitia", "unit"); T.eq(s.count, 2, "count"); T.eq(s.region, "nus", "region")
  end,
  mood_settings_fix_the_values_the_overlay_asked_for = function()
    local m = load().features.mood
    T.eq(m.enabled, true, "enabled")
    T.eq(m.approval.fixed, 80, "approval.fixed"); T.eq(m.approval.good, 1, "approval.good"); T.eq(m.approval.bad, 100, "approval.bad")
    T.eq(m.order.fixed, 0, "order.fixed"); T.eq(m.order.good, 3, "order.good"); T.eq(m.order.bad, 50, "order.bad")
    T.eq(m.regions.nus.approval.fixed, 0, "nus approval is not fixed"); T.eq(m.regions.nus.order.fixed, 33, "nus order")
    -- 모드가 그 설정으로 고정값을 쓴다: 공통을 따르는 gold 는 자격만, 따로 지정한 nus 는 공공질서만
    local gold, nus = myRegions("gold", "nus")
    mood.tick({}, m)
    T.eq(gold.Approval, 80, "gold follows the common fixed approval"); T.eq(gold.publicOrder, 70, "the common order is not fixed")
    T.eq(nus.Approval, 40, "nus has its own setting: approval stays"); T.eq(nus.publicOrder, 33, "nus order is fixed")
  end,
  region_settings_fill_the_deposits_the_overlay_asked_for = function()
    local r = load().features.region
    T.eq(r.enabled, true, "enabled"); T.eq(r.intervalSec, 5, "intervalSec")
    T.eq(r.noLivestockWait, true, "noLivestockWait"); T.eq(r.richDeposits, true, "richDeposits")
    T.eq(r.targets.Fish, 900, "targets"); T.eq(r.regionTargets.gold.Mushrooms, 700, "regionTargets")
    T.eq(r.regionTargets.nus.Fish, 0, "zero is a value: that region is off")
    -- 모드가 그 설정으로 채운다: 공통 목표, 영지 목표, 0 으로 끈 영지
    local gold, nus = myRegions("gold", "nus")
    local goldFish, nusFish, goldMushrooms = deposit(gold, "res_fish", 10), deposit(nus, "res_fish", 10), deposit(gold, "mushrooms", 10)
    game.find.all = function(cls) return cls == "ResourceNode" and { goldFish, nusFish, goldMushrooms } or {} end
    native.nodes, native.nodesDay = nil, nil
    region.tick({}, r)
    T.eq(gold.nextLivestockOrderIn, 0, "no livestock trader wait"); T.eq(nus.nextLivestockOrderIn, 0, "in every region of mine")
    T.eq(goldFish.resourceClumps[1].amt, 900, "gold follows the common target")
    T.eq(nusFish.resourceClumps[1].amt, 10, "nus is turned off for fish")
    T.eq(goldMushrooms.resourceClumps[1].amt, 700, "gold's own target")
  end,
})
