local T = require("t")
local json = require("lib.json")

T.run({
  roundtrip_object = function()
    local s = json.encode({ a = 1, b = "x" })
    local d = json.decode(s)
    T.eq(d.a, 1, "a")
    T.eq(d.b, "x", "b")
  end,
  temp_dir_is_writable = function()
    local p = TEST_TMP .. "\\probe.txt"
    local f = assert(io.open(p, "wb")); f:write("ok"); f:close()
    T.truthy(io.open(p, "rb"), "readable")
  end,
})
