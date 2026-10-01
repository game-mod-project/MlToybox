local T = require("t")
local bridge = require("core.bridge")
local fileio = require("core.fileio")
local plan = require("features.merc_plan")

-- 오버레이 코어(C++)가 쓴 control.json 견본을 모드가 읽을 수 있는가.
-- 견본은 네이티브 테스트(overlay_fixture_for_the_lua_spec_matches_core_output)가 코어의 출력과 같은지 지킨다.
local function load()
  local text = fileio.read(SCRIPTS_DIR .. "\\..\\tests\\fixtures\\control_from_overlay.json")
  T.truthy(text, "fixture exists")
  local control, err = bridge.parseControl(text)
  T.truthy(control, "parses: " .. tostring(err))
  return control
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
  mercenary_companies_pass_the_mods_own_validation = function()
    local m = load().features.mercenaries
    T.eq(m.enabled, true, "enabled"); T.eq(m.refund, true, "refund"); T.eq(m.lockFromAi, true, "lockFromAi")
    T.eq(#m.companies, 2, "two companies")
    T.eq(m.companies[1].name, "토이박스 용병단", "korean name survives"); T.eq(m.companies[2].region, nil, "unset region is absent"); T.eq(m.companies[2].enabled, false, "disabled")
    local ctx = { vanillaNames = { greencaps = true }, unitExists = function() return true end, regionKeys = { "nus", "gold" } }
    local valid, skipped = plan.validate(m.companies, ctx)
    T.eq(#skipped, 0, "nothing skipped")
    T.eq(#valid, 1, "only the enabled company is used")
    T.eq(valid[1].name, "토이박스 용병단", "name"); T.eq(#valid[1].units, 2, "units"); T.eq(valid[1].cost, 3000, "cost")
    T.eq(valid[1].region, "gold", "region"); T.eq(valid[1].banner, "greencaps", "banner")
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
})
