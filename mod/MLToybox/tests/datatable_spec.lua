local T = require("t")
local F = require("fakes")
local datatable = require("core.datatable")

T.run({
  forEachRow_visits_rows_by_reference = function()
    local rows = { ["2"] = { v = 1 }, ["4"] = { v = 2 } }
    datatable.find = function(p) if p == datatable.PATHS.upgrades then return F.datatable(rows) end end
    local n = datatable.forEachRow("upgrades", function(name, row) row.v = row.v * 10 end)
    T.eq(n, 2, "count"); T.eq(rows["2"].v, 10, "row 2 mutated"); T.eq(rows["4"].v, 20, "row 4 mutated")
  end,
  forEachRow_missing_table_errors = function()
    datatable.find = function() return nil end
    local ok, err = pcall(datatable.forEachRow, "upgrades", function() end)
    T.eq(ok, false, "errors"); T.truthy(tostring(err):find("datatable not found: upgrades", 1, true), "message")
  end,
  row_names_accept_fname_userdata = function()
    local rows = { a = { v = 1 } }
    local dt = F.datatable(rows)
    dt.GetRowNames = function() return { { ToString = function() return "a" end } } end
    datatable.find = function() return dt end
    local seen
    datatable.forEachRow("upgrades", function(name) seen = name end)
    T.eq(seen, "a", "ToString used")
  end,
  object_errors_when_missing = function()
    datatable.find = function() return nil end
    T.eq((pcall(datatable.object, "residentialSettings")), false, "errors")
  end,
})
