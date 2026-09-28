local T = require("t")
local commands = require("core.commands")

local function cmd(id, extra)
  local c = { id = id, type = "echo", issuedAt = 1000 }
  for k, v in pairs(extra or {}) do c[k] = v end
  return c
end

local function runner()
  local calls = {}
  local c = commands.new({
    echo = function(command, ctx) calls[#calls + 1] = command.id; return { ok = true, value = ctx.tag } end,
    boom = function() error("kaboom") end,
  })
  return c, calls
end

T.run({
  runs_each_command_once = function()
    local c, calls = runner()
    local list = { cmd("a"), cmd("b") }
    c:run(list, { now = 1010, tag = "x" })
    c:run(list, { now = 1011, tag = "x" })
    T.eq(#calls, 2, "two calls total")
    T.eq(c:status().a.ok, true, "a ok"); T.eq(c:status().a.value, "x", "ctx passed")
  end,
  stale_commands_are_ignored_but_reported = function()
    local c, calls = runner()
    c:run({ cmd("old", { issuedAt = 900 }) }, { now = 1000 })
    T.eq(#calls, 0, "not executed")
    T.eq(c:status().old.ok, false, "reported"); T.truthy(c:status().old.error:find("stale", 1, true), "reason")
  end,
  missing_issued_at_is_stale = function()
    local c, calls = runner()
    c:run({ { id = "n", type = "echo" } }, { now = 1000 })
    T.eq(#calls, 0, "not executed")
  end,
  unknown_type_and_handler_error_are_reported = function()
    local c = runner()
    c:run({ cmd("u", { type = "nope" }), cmd("e", { type = "boom" }) }, { now = 1000 })
    T.truthy(c:status().u.error:find("unknown command", 1, true), "unknown")
    T.truthy(c:status().e.error:find("kaboom", 1, true), "error text")
  end,
  malformed_entries_are_skipped = function()
    local c, calls = runner()
    c:run({ "x", { type = "echo", issuedAt = 1000 }, cmd("ok") }, { now = 1000 })
    T.eq(#calls, 1, "only valid one")
    c:run("not a list", { now = 1000 })
  end,
  results_are_capped = function()
    local c = runner()
    local list = {}
    for i = 1, commands.MAX_RESULTS + 3 do list[#list + 1] = cmd("c" .. i) end
    c:run(list, { now = 1000 })
    local n = 0
    for _ in pairs(c:status()) do n = n + 1 end
    T.eq(n, commands.MAX_RESULTS, "capped")
    T.eq(c:status().c1, nil, "oldest dropped")
  end,
  status_is_nil_when_empty = function()
    T.eq(runner():status(), nil, "nil avoids [] encoding")
  end,
})
