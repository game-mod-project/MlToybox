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
  -- 맵을 떠날 때의 disable 은 state.leaving 으로 알 수 있다: 사라질 게임 객체를 되돌려 놓을 필요가 없다(features/storage.lua)
  disable_can_tell_leaving_the_map_from_being_turned_off = function()
    local r = registry.new()
    local seen = {}
    local a = { name = "a", enable = function() end, disable = function(state) seen[#seen + 1] = state.leaving == true end }
    r:add(a); r:setInGame(true)
    r:apply({ a = { enabled = true } }); r:apply({ a = { enabled = false } })
    r:apply({ a = { enabled = true } }); r:setInGame(false)
    T.eq(#seen, 2, "disabled twice"); T.eq(seen[1], false, "turned off by the user"); T.eq(seen[2], true, "the map is going away")
    T.eq(r.state.leaving, nil, "cleared with the rest of the state")
    r:setInGame(true); r:apply({ a = { enabled = false } })
    T.eq(seen[3], false, "not leaving in the next map")
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
  -- poll 은 간격과 무관하게 tick 을 부를 때마다(1초) 돈다: 화면 상태를 바로 따라가야 하는 일(features/build.lua 의 배치 모드)
  poll_runs_on_every_tick_whatever_the_interval = function()
    local r = registry.new()
    local polls, ticks, got = 0, 0, nil
    local a = { name = "a", intervalSec = 10, poll = function(state, s) polls = polls + 1; got = s end, tick = function() ticks = ticks + 1 end }
    r:add(a); r:setInGame(true)
    r:tick(99)
    T.eq(polls, 0, "not while the feature is off")
    r:apply({ a = { enabled = true, x = 7 } })
    r:tick(100); r:tick(101); r:tick(102)
    T.eq(polls, 3, "every tick"); T.eq(ticks, 1, "tick keeps its interval"); T.eq(got.x, 7, "poll gets the settings")
    r:apply({ a = { enabled = false } }); r:tick(103)
    T.eq(polls, 3, "stops with the feature")
  end,
  -- observe 는 기능이 꺼져 있는 동안 게임 안에서 tick 의 간격으로 돈다: 읽기만 하는 일(features/resources.lua 의 영지와 현재 값)
  observe_runs_in_game_while_the_feature_is_off = function()
    local r = registry.new()
    local seen, ticks = 0, 0
    local a = { name = "a", observe = function(state) seen = seen + 1; state.seen = true end, tick = function() ticks = ticks + 1 end }
    r:add(a)
    r:tick(98)
    T.eq(seen, 0, "not in the menu")
    r:setInGame(true)
    r:tick(100); r:tick(101); r:tick(102)
    T.eq(seen, 2, "t=100 and t=102"); T.eq(ticks, 0, "tick stays off"); T.eq(r.state.seen, true, "observe gets the state")
    r:apply({ a = { enabled = true } })
    r:tick(103); r:tick(104)
    T.eq(seen, 2, "tick takes over while the feature is on"); T.eq(ticks, 1, "ticked")
    r:apply({ a = { enabled = false, intervalSec = 5 } })
    r:tick(105); r:tick(108)
    T.eq(seen, 3, "back to observe, on the interval in the settings: t=108")
  end,
  observe_stops_once_it_trips = function()
    safe.setThreshold(2)
    local r = registry.new()
    local seen = 0
    local bad = { name = "bad", intervalSec = 1, observe = function() seen = seen + 1; error("observe failed") end }
    local good = fake("good")
    r:add(bad); r:add(good); r:setInGame(true)
    r:apply({ good = { enabled = true, intervalSec = 1 } })
    for t = 1, 5 do r:tick(t) end
    T.eq(seen, 2, "stopped at the threshold"); T.eq(r.tripped.bad, true, "tripped")
    T.eq(count(good, "tick"), 5, "the others keep ticking")
    safe.setThreshold(5)
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
  -- 실패는 호출 종류마다 따로 센다. 기능 이름 하나로 세면 매초 성공하는 poll 이 계속 실패하는 tick 의 횟수를 지워 영영 꺼지지 않는다
  a_failing_tick_trips_even_while_poll_keeps_succeeding = function()
    safe.setThreshold(3)
    local r = registry.new()
    local ticks = 0
    local mixed = { name = "mixed", intervalSec = 1, poll = function() end, tick = function() ticks = ticks + 1; error("tick failed") end }
    r:add(mixed); r:setInGame(true); r:apply({ mixed = { enabled = true } })
    for t = 1, 6 do r:tick(t) end
    T.eq(ticks, 3, "stopped at the threshold"); T.eq(r.tripped.mixed, true, "tripped"); T.eq(r.active.mixed, false, "off")
    safe.setThreshold(5)
  end,
  -- 같은 이름으로 밖에서 부른 호출(main.lua 의 safe.call("lord", lord.read))이 그 기능의 실패 횟수를 건드리지 않는다
  outside_calls_under_the_feature_name_do_not_touch_its_count = function()
    safe.setThreshold(3)
    local r = registry.new(); local bad, good = fake("lord", { fail = "tick" }), fake("other"); r:add(bad); r:add(good)
    r:setInGame(true); r:apply({ lord = { enabled = true, intervalSec = 1 } })
    for t = 1, 4 do
      r:tick(t)
      safe.call("lord", function() end)          -- 성공해도 tick 의 연속 실패는 그대로다
    end
    T.eq(r.tripped.lord, true, "the tick still tripped")
    for _ = 1, 5 do safe.call("other", function() error("read failed") end) end
    T.eq(r.tripped.other, nil, "an outside failure does not trip the feature")
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
  -- 기능이 켜지고 꺼질 때를 알린다(main.lua 가 로그에 한 줄 남긴다). 자동으로 꺼졌으면 이유도 준다
  activation_changes_are_reported = function()
    safe.setThreshold(1)
    local seen = {}
    local r = registry.new(); local a, bad = fake("a"), fake("bad", { fail = "tick" }); r:add(a); r:add(bad)
    r.onChange = function(name, active, reason) seen[#seen + 1] = name .. (active and " on" or " off") .. (reason and (" " .. reason) or "") end
    r:apply({ a = { enabled = true }, bad = { enabled = true } })
    T.eq(#seen, 0, "nothing in the menu")
    r:setInGame(true)
    T.eq(table.concat(seen, "|"), "a on|bad on", "both on")
    r:tick(1)
    T.eq(#seen, 3, "the failing one went off"); T.truthy(seen[3]:find("^bad off auto%-disabled"), "with the reason: " .. seen[3])
    r:apply({ a = { enabled = false } })
    T.eq(seen[4], "a off", "turned off by the user")
    r:setInGame(false)
    T.eq(#seen, 4, "already off: nothing more to report")
    r.onChange = function() error("reporter broke") end
    r:apply({ a = { enabled = true } }); r:setInGame(true)
    T.eq(r.active.a, true, "a failing reporter does not stop the feature")
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
  status_includes_mercenaries_when_present = function()
    local r = registry.new(); r:add(fake("a")); r:setInGame(true)
    T.eq(r:status(1, nil, nil).mercenaries, nil, "absent")
    r.state.mercenaries = { hiredMine = 2 }
    T.eq(r:status(1, nil, nil).mercenaries.hiredMine, 2, "present")
  end,
  -- 영주 값은 main.lua 가 lord.read() 로 싣는다(기능이 꺼져 있어도 보고한다). 공유 상태에서 가져오지 않는다
  status_leaves_lord_values_to_main = function()
    local r = registry.new(); r:add(fake("a")); r:setInGame(true)
    r.state.lord = { treasury = 5, kingsFavour = 2 }
    T.eq(r:status(1, nil, nil).lord, nil, "not taken from the shared state")
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
