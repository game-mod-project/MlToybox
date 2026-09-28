local T = require("t")
local fileio = require("core.fileio")

T.run({
  read_missing_returns_nil = function()
    T.eq(fileio.read(TEST_TMP .. "\\nope.json"), nil, "missing")
  end,
  write_atomic_creates_and_overwrites = function()
    local p = TEST_TMP .. "\\s.json"
    T.eq(fileio.writeAtomic(p, "one"), true, "first")
    T.eq(fileio.writeAtomic(p, "two"), true, "overwrite")
    T.eq(fileio.read(p), "two", "content")
    T.eq(fileio.read(p .. ".tmp"), nil, "tmp removed")
  end,
  write_atomic_into_missing_dir_fails_cleanly = function()
    local ok, err = fileio.writeAtomic(TEST_TMP .. "\\no\\such\\dir\\x.json", "x")
    T.eq(ok, false, "fails")
    T.truthy(err, "has error")
  end,
})
