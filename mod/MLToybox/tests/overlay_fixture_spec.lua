local T = require("t")
local bridge = require("core.bridge")
local fileio = require("core.fileio")

-- 오버레이 코어(C++)가 쓴 control.json 견본을 모드가 읽을 수 있는가.
-- 견본은 네이티브 테스트(overlay_fixture_for_the_lua_spec_matches_core_output)가 코어의 출력과 같은지 지킨다.
T.run({
  control_written_by_the_overlay_core_is_valid_for_the_mod = function()
    local text = fileio.read(SCRIPTS_DIR .. "\\..\\tests\\fixtures\\control_from_overlay.json")
    T.truthy(text, "fixture exists")
    local control, err = bridge.parseControl(text)
    T.truthy(control, "parses: " .. tostring(err))
    T.eq(control.version, 1, "version"); T.eq(control.seq, 7, "seq")
    local f = control.features
    T.eq(f.build.enabled, true, "build.enabled"); T.eq(f.build.instantRepair, false, "build.instantRepair"); T.eq(f.build.instantBuild, true, "build.instantBuild")
    T.eq(f.upgrade.enabled, true, "upgrade.enabled")
    T.eq(f.lord.enabled, true, "lord.enabled"); T.eq(f.lord.intervalSec, 2, "lord.intervalSec")
    T.eq(f.lord.treasury, 150000, "lord.treasury"); T.eq(f.lord.influence, nil, "unmanaged key is absent"); T.eq(f.lord.kingsFavour, 0, "zero is a value")
    T.eq(#control.commands, 1, "one command")
    local c = control.commands[1]
    T.eq(c.type, "setLord", "type"); T.eq(c.key, "influence", "key"); T.eq(c.value, 20000, "value")
    T.eq(c.id, "0123456789abcdef0123456789abcdef", "id"); T.eq(c.issuedAt, 1790000000, "issuedAt")
  end,
})
