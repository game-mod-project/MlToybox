local T = require("t")
local registry = require("core.registry")
local safe = require("core.safe")

local function fake(name, opts)
  opts = opts or {}
  local f = { name = name, calls = {}, intervalSec = opts.intervalSec }
  local function rec(m) return function(state, s) f.calls[#f.calls + 1] = m; if opts.fail == m then error(m .. " failed") end end end
  f.enable, f.disable, f.configure, f.tick = rec("enable"), rec("disable"), rec("configure"), rec("tick")
  return f
end
local function count(f, m) local c = 0 for _, x in ipairs(f.calls) do if x == m then c = c + 1 end end return c end

T.run({
  not_enabled_until_in_game = function()
    local r = registry.new(); local a = fake("a"); r:add(a)
    r:apply({ a = { enabled = true } })
    T.eq(count(a, "enable"), 0, "menu")
    r:setInGame(true)
    T.eq(count(a, "enable"), 1, "in game")
    T.eq(r.active.a, true, "active")
  end,
  apply_toggles_and_configures = function()
    local r = registry.new(); local a = fake("a"); r:add(a); r:setInGame(true)
    r:apply({ a = { enabled = true } })
    r:apply({ a = { enabled = true, x = 1 } })
    T.eq(count(a, "configure"), 1, "configure on change while active")
    r:apply({ a = { enabled = false } })
    T.eq(count(a, "disable"), 1, "disabled")
    T.eq(r.active.a, false, "inactive")
  end,
  unknown_feature_in_control_is_ignored = function()
    local r = registry.new(); r:add(fake("a")); r:setInGame(true)
    r:apply({ zzz = { enabled = true } })
    T.eq(r.active.zzz, nil, "ignored")
  end,
  leaving_game_disables_all = function()
    local r = registry.new(); local a, b = fake("a"), fake("b"); r:add(a); r:add(b)
    r:setInGame(true); r:apply({ a = { enabled = true }, b = { enabled = true } })
    r:setInGame(false)
    T.eq(count(a, "disable") + count(b, "disable"), 2, "both disabled")
    T.eq(r.active.a or r.active.b, false, "none active")
  end,
  reentering_game_reenables = function()
    local r = registry.new(); local a = fake("a"); r:add(a)
    r:apply({ a = { enabled = true } }); r:setInGame(true); r:setInGame(false); r:setInGame(true)
    T.eq(count(a, "enable"), 2, "enabled twice")
  end,
  tick_respects_interval = function()
    local r = registry.new(); local a = fake("a"); r:add(a); r:setInGame(true)
    r:apply({ a = { enabled = true, intervalSec = 3 } })
    r:tick(100); r:tick(101); r:tick(102); r:tick(103)
    T.eq(count(a, "tick"), 2, "t=100 and t=103")
  end,
  tick_default_interval_is_2 = function()
    local r = registry.new(); local a = fake("a"); r:add(a); r:setInGame(true)
    r:apply({ a = { enabled = true } })
    r:tick(10); r:tick(11); r:tick(12)
    T.eq(count(a, "tick"), 2, "t=10 and t=12")
  end,
  trip_disables_only_that_feature = function()
    safe.setThreshold(3)
    local r = registry.new(); local bad, good = fake("bad", { fail = "tick" }), fake("good"); r:add(bad); r:add(good)
    r:setInGame(true); r:apply({ bad = { enabled = true, intervalSec = 1 }, good = { enabled = true, intervalSec = 1 } })
    for t = 1, 5 do r:tick(t) end
    T.eq(r.active.bad, false, "bad off")
    T.eq(r.tripped.bad, true, "tripped")
    T.truthy(r.errors.bad:find("auto%-disabled"), "reason kept")
    T.eq(r.active.good, true, "good on")
    T.eq(count(good, "tick"), 5, "good kept ticking")
    safe.setThreshold(5)
  end,
  apply_clears_trip = function()
    safe.setThreshold(1)
    local r = registry.new(); local bad = fake("bad", { fail = "tick" }); r:add(bad)
    r:setInGame(true); r:apply({ bad = { enabled = true } }); r:tick(1)
    T.eq(r.tripped.bad, true, "tripped")
    r:setInGame(false); r:setInGame(true)
    T.eq(r.active.bad, false, "stays off across maps")
    r:apply({ bad = { enabled = true } })
    T.eq(r.active.bad, true, "retry after apply")
    safe.setThreshold(5)
  end,
  enable_failure_reports_error_and_stays_off = function()
    local r = registry.new(); local a = fake("a", { fail = "enable" }); r:add(a); r:setInGame(true)
    r:apply({ a = { enabled = true } })
    T.eq(r.active.a, false, "off")
    T.truthy(r.errors.a:find("enable failed"), "error")
  end,
  status_shape = function()
    local r = registry.new(); r:add(fake("a")); r:setInGame(true); r:apply({ a = { enabled = true } })
    local s = r:status(42, 7, "oops")
    T.eq(s.version, 1, "version"); T.eq(s.heartbeat, 42, "hb"); T.eq(s.inGame, true, "inGame")
    T.eq(s.appliedSeq, 7, "seq"); T.eq(s.bridgeError, "oops", "bridgeError")
    T.eq(s.features.a.active, true, "feature active")
    T.eq(s.resources, nil, "empty resources omitted")
    T.eq(s.resourceIds, nil, "empty ids omitted")
  end,
  failed_enable_is_retried_on_next_tick = function()
    local r = registry.new()
    local attempts = 0
    local a = { name = "late", enable = function() attempts = attempts + 1; if attempts == 1 then error("targets not ready") end end }
    r:add(a); r:apply({ late = { enabled = true } }); r:setInGame(true)
    T.eq(r.active.late, false, "first enable failed")
    r:tick(1)
    T.eq(attempts, 2, "retried on tick")
    T.eq(r.active.late, true, "active after retry")
    T.eq(r.errors.late, nil, "error cleared")
  end,
  leaving_game_clears_shared_state = function()
    local r = registry.new(); local a = fake("a"); r:add(a)
    r:apply({ a = { enabled = true } }); r:setInGame(true)
    local stateRef = r.state
    r.state.resourceIds = { "Timber" }; r.state.resources = { Timber = 5 }; r.state.cachedObj = {}
    r:setInGame(false)
    local s = r:status(1, nil, nil)
    T.eq(s.resourceIds, nil, "ids cleared"); T.eq(s.resources, nil, "resources cleared")
    T.eq(stateRef.cachedObj, nil, "feature cache cleared in the same table")
    T.eq(r.state, stateRef, "table identity kept for features holding it")
    T.eq(r.state.inGame, false, "inGame kept")
  end,
  status_includes_population_when_present = function()
    local r = registry.new(); r:add(fake("a")); r:setInGame(true)
    T.eq(r:status(1, nil, nil).population, nil, "absent")
    r.state.population = { families = 3 }
    T.eq(r:status(1, nil, nil).population.families, 3, "present")
  end,
  status_includes_lord_values_when_present = function()
    local r = registry.new(); r:add(fake("a")); r:setInGame(true)
    T.eq(r:status(1, nil, nil).lord, nil, "absent")
    r.state.lord = { treasury = 5, kingsFavour = 2 }
    T.eq(r:status(1, nil, nil).lord.kingsFavour, 2, "present")
  end,
  status_includes_per_region_resources_when_present = function()
    local r = registry.new(); r:add(fake("a")); r:setInGame(true)
    T.eq(r:status(1, nil, nil).regions, nil, "absent")
    r.state.regions = {}
    T.eq(r:status(1, nil, nil).regions, nil, "empty omitted")
    r.state.regions = { { key = "hof", name = "Klainau", values = { Timber = 5 } } }
    T.eq(r:status(1, nil, nil).regions[1].key, "hof", "present")
  end,
  status_without_features_omits_map = function()
    local s = registry.new():status(1, nil, nil)
    T.eq(s.features, nil, "no features -> nil (avoids [] encoding)")
  end,
})
