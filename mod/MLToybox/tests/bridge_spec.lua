local T = require("t")
local bridge = require("core.bridge")
local json = require("lib.json")

local n = 0
local function fresh()
  n = n + 1
  local dir = TEST_TMP .. "\\b" .. n
  os.execute('mkdir "' .. dir .. '"')
  return bridge.new(dir)
end
local function writeControl(b, text)
  local f = assert(io.open(b.controlPath, "wb")); f:write(text); f:close()
end
local function ctl(seq, extra)
  local t = { version = 1, seq = seq, features = extra or { build = { enabled = true } } }
  return json.encode(t)
end

T.run({
  poll_missing_file_returns_nil = function()
    local b = fresh()
    T.eq(b:poll(), nil, "nil")
    T.eq(b.lastError, nil, "no error")
  end,
  poll_returns_new_seq_once = function()
    local b = fresh()
    writeControl(b, ctl(1))
    local c = b:poll()
    T.eq(c.seq, 1, "seq")
    T.eq(c.features.build.enabled, true, "payload")
    T.eq(b:poll(), nil, "same seq again")
    writeControl(b, ctl(2))
    T.eq(b:poll().seq, 2, "next seq")
  end,
  poll_bad_json_keeps_last_seq = function()
    local b = fresh()
    writeControl(b, ctl(5)); b:poll()
    writeControl(b, "{not json")
    T.eq(b:poll(), nil, "nil on bad json")
    T.truthy(b.lastError and b.lastError:find("parse"), "parse error reported")
    T.eq(b.lastSeq, 5, "last seq kept")
    writeControl(b, ctl(5))
    T.eq(b:poll(), nil, "same seq still ignored")
    T.eq(b.lastError, nil, "error cleared on good file")
  end,
  poll_version_mismatch = function()
    local b = fresh()
    writeControl(b, '{"version":2,"seq":1,"features":{}}')
    T.eq(b:poll(), nil, "rejected")
    T.truthy(b.lastError:find("version"), "version error")
  end,
  parse_rejects_missing_fields = function()
    T.eq((bridge.parseControl('{"version":1,"features":{}}')), nil, "no seq")
    T.eq((bridge.parseControl('{"version":1,"seq":1}')), nil, "no features")
    T.eq((bridge.parseControl('[]')), nil, "array")
    T.eq((bridge.parseControl('')), nil, "empty")
  end,
  write_status_roundtrip = function()
    local b = fresh()
    T.eq(b:writeStatus({ version = 1, heartbeat = 123, inGame = false }), true, "written")
    local f = assert(io.open(b.statusPath, "rb")); local s = f:read("a"); f:close()
    local d = json.decode(s)
    T.eq(d.heartbeat, 123, "heartbeat")
    T.eq(d.inGame, false, "inGame")
  end,
})
