local T = require("t")
local safe = require("core.safe")

T.run({
  success_returns_results_and_resets = function()
    safe.reset("a")
    local ok, x, y = safe.call("a", function(p) return p + 1, "y" end, 1)
    T.eq(ok, true, "ok"); T.eq(x, 2, "x"); T.eq(y, "y", "y")
    T.eq(safe.failures("a"), 0, "failures")
  end,
  failure_counts_and_returns_error = function()
    safe.reset("b")
    local ok, err = safe.call("b", function() error("boom") end)
    T.eq(ok, false, "ok")
    T.truthy(tostring(err):find("boom"), "err text")
    T.eq(safe.failures("b"), 1, "failures")
  end,
  trip_fires_once_at_threshold_then_resets = function()
    safe.setThreshold(3)
    safe.reset("c")
    local trips = 0
    safe.onTrip("c", function() trips = trips + 1 end)
    for _ = 1, 3 do safe.call("c", function() error("x") end) end
    T.eq(trips, 1, "tripped once")
    T.eq(safe.failures("c"), 0, "reset after trip")
    safe.setThreshold(5)
  end,
  success_between_failures_resets_counter = function()
    safe.setThreshold(2)
    safe.reset("d")
    local trips = 0
    safe.onTrip("d", function() trips = trips + 1 end)
    safe.call("d", function() error("x") end)
    safe.call("d", function() end)
    safe.call("d", function() error("x") end)
    T.eq(trips, 0, "not consecutive")
    safe.setThreshold(5)
  end,
  valid_handles_nil_and_throwing_objects = function()
    T.eq(safe.valid(nil), false, "nil")
    T.eq(safe.valid({ IsValid = function() return true end }), true, "valid")
    T.eq(safe.valid({ IsValid = function() error("dead") end }), false, "throws")
    T.eq(safe.valid({}), false, "no method")
  end,
})
