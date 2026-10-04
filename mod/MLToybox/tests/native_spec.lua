local T = require("t")
local native = require("core.native")

local function write(path, text) local f = assert(io.open(path, "wb")); f:write(text); f:close() end

T.run({
  load_missing_dll_reports_not_deployed = function()
    local ok, err = native.load(TEST_TMP .. "\\nope")
    T.eq(ok, false, "not ok"); T.eq(err, "not deployed", "reason")
  end,
  load_failure_reports_error = function()
    os.execute('mkdir "' .. TEST_TMP .. '\\n1"')
    write(TEST_TMP .. "\\n1\\mltoybox_native.dll", "x")
    native.loadlib = function() return nil, "blocked", "open" end
    local ok, err = native.load(TEST_TMP .. "\\n1")
    T.eq(ok, false, "not ok"); T.truthy(err:find("blocked", 1, true), "reason")
  end,
  load_success = function()
    os.execute('mkdir "' .. TEST_TMP .. '\\n2"')
    write(TEST_TMP .. "\\n2\\mltoybox_native.dll", "x")
    native.loadlib = function() return true end
    T.eq((native.load(TEST_TMP .. "\\n2")), true, "ok")
  end,
  load_file_loads_the_named_dll = function()
    os.execute('mkdir "' .. TEST_TMP .. '\\n3"')
    write(TEST_TMP .. "\\n3\\mltoybox_overlay.dll", "x")
    local seen
    native.loadlib = function(path) seen = path; return true end
    T.eq((native.loadFile(TEST_TMP .. "\\n3", native.OVERLAY_DLL)), true, "ok")
    T.truthy(seen:find("mltoybox_overlay.dll", 1, true), "path: " .. tostring(seen))
    local ok, err = native.loadFile(TEST_TMP .. "\\n3", "missing.dll")
    T.eq(ok, false, "missing file"); T.eq(err, "not deployed", "reason")
  end,
  overlay_status_merges_state_and_reason = function()
    local p = TEST_TMP .. "\\os1.json"
    write(p, '{"heartbeat":100,"state":"disabled","reason":"present queue not found","visible":false,"frames":0,"font":"malgun"}')
    local s = native.overlayStatus(p, 103, true, nil)
    T.eq(s.loaded, true, "loaded"); T.eq(s.stale, false, "fresh"); T.eq(s.state, "disabled", "state"); T.eq(s.reason, "present queue not found", "reason")
    write(p, '{"heartbeat":100,"state":"ready","reason":null,"visible":true,"frames":9,"font":"malgun"}')
    s = native.overlayStatus(p, 106, true, nil)
    T.eq(s.stale, true, "old heartbeat"); T.eq(s.state, "ready", "state kept"); T.eq(s.reason, nil, "null reason")
  end,
  overlay_status_without_the_dll_ignores_a_leftover_file = function()
    local p = TEST_TMP .. "\\os2.json"
    write(p, '{"heartbeat":100,"state":"ready"}')
    local s = native.overlayStatus(p, 100, false, "disabled in config")
    T.eq(s.loaded, false, "not loaded"); T.eq(s.error, "disabled in config", "error"); T.eq(s.state, nil, "leftover state ignored"); T.eq(s.stale, true, "stale")
    s = native.overlayStatus(TEST_TMP .. "\\none.json", 1, true, nil)
    T.eq(s.loaded, true, "loaded without a file yet"); T.eq(s.stale, true, "no heartbeat"); T.eq(s.state, nil, "no state")
    write(p, "{broken")
    T.eq(native.overlayStatus(p, 1, true, nil).state, nil, "broken file")
  end,
  status_merges_fresh_native_status = function()
    local p = TEST_TMP .. "\\ns.json"
    write(p, '{"version":1,"heartbeat":100,"appliedSeq":3,"features":{"instant_build":{"installed":true,"active":false}}}')
    local s = native.status(p, 102, true, nil)
    T.eq(s.loaded, true, "loaded"); T.eq(s.stale, false, "fresh"); T.eq(s.heartbeat, 100, "hb")
    T.eq(s.features.instant_build.installed, true, "feature")
  end,
  -- 네이티브가 날짜가 넘어갈 때 적는 광물 매장지 목록. status.json 에 그대로 실리지 않게 따로 둔다(영지 기능이 읽는다)
  status_keeps_the_mineral_nodes_aside_and_drops_them_when_stale = function()
    local p = TEST_TMP .. "\\ns_nodes.json"
    write(p, '{"version":1,"heartbeat":100,"appliedSeq":3,"nodes":[{"region":"20E9D490010","type":3,"amount":500}]}')
    local s = native.status(p, 102, true, nil)
    T.eq(s.nodes, nil, "not part of the merged status")
    T.eq(#native.nodes, 1, "kept aside")
    T.eq(native.nodes[1].region, "20E9D490010", "region address")
    T.eq(native.nodes[1].amount, 500, "amount")
    native.status(p, 106, true, nil)
    T.eq(native.nodes, nil, "stale: dropped")
    write(p, '{"version":1,"heartbeat":100,"appliedSeq":3}')
    native.status(p, 101, true, nil)
    T.eq(native.nodes, nil, "no nodes in the file")
  end,
  status_marks_stale_heartbeat = function()
    local p = TEST_TMP .. "\\ns2.json"
    write(p, '{"version":1,"heartbeat":100,"appliedSeq":-1}')
    local s = native.status(p, 106, true, nil)
    T.eq(s.stale, true, "stale"); T.eq(s.features, nil, "no features")
  end,
  status_without_file_or_load = function()
    local s = native.status(TEST_TMP .. "\\none.json", 1, false, "not deployed")
    T.eq(s.loaded, false, "not loaded"); T.eq(s.error, "not deployed", "error"); T.eq(s.stale, true, "stale when missing")
  end,
  -- 기능이 "네이티브가 이 일을 맡았는가"를 묻는다. 맡았으면 Lua 는 표를 바꾸지 않고, 못 맡으면 예전 방식으로 돌아간다
  installed_is_unknown_until_a_status_was_read = function()
    native.last = nil
    T.eq(native.installed({ "placement" }), nil, "nothing read yet")
  end,
  installed_tells_whether_every_named_item_is_installed = function()
    local p = TEST_TMP .. "\\ns3.json"
    write(p, '{"version":1,"heartbeat":100,"appliedSeq":3,"features":{"placement":{"installed":true,"active":true},"building_row":{"installed":true,"active":true},"placement_rows":{"installed":false,"active":false,"lastError":"layout check failed"}}}')
    native.status(p, 101, true, nil)
    T.eq(native.installed({ "placement", "building_row" }), true, "both installed")
    T.eq(native.installed({ "placement", "placement_rows" }), false, "one failed its layout check")
    T.eq(native.installed({ "placement", "nope" }), false, "an item this DLL does not have (older DLL)")
  end,
  installed_is_false_without_the_dll_and_unknown_while_the_status_is_stale = function()
    native.status(TEST_TMP .. "\\none.json", 1, false, "not deployed")
    T.eq(native.installed({ "placement" }), false, "the DLL is not there: nothing will ever be installed")
    local p = TEST_TMP .. "\\ns4.json"
    write(p, '{"version":1,"heartbeat":100,"appliedSeq":3,"features":{"placement":{"installed":true,"active":true}}}')
    native.status(p, 200, true, nil)
    T.eq(native.installed({ "placement" }), nil, "old heartbeat: wait")
    native.status(TEST_TMP .. "\\none.json", 200, true, nil)
    T.eq(native.installed({ "placement" }), nil, "loaded but no status file yet: wait")
  end,
})
