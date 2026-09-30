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
  status_merges_fresh_native_status = function()
    local p = TEST_TMP .. "\\ns.json"
    write(p, '{"version":1,"heartbeat":100,"appliedSeq":3,"features":{"instant_build":{"installed":true,"active":false}}}')
    local s = native.status(p, 102, true, nil)
    T.eq(s.loaded, true, "loaded"); T.eq(s.stale, false, "fresh"); T.eq(s.heartbeat, 100, "hb")
    T.eq(s.features.instant_build.installed, true, "feature")
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
})
