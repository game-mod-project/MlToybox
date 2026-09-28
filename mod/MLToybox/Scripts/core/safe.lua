local log = require("core.log")

local M = { threshold = 5 }
local failures = {}
local tripHandlers = {}

function M.setThreshold(n) M.threshold = n end
function M.onTrip(name, fn) tripHandlers[name] = fn end
function M.reset(name) failures[name] = 0 end
function M.failures(name) return failures[name] or 0 end

function M.call(name, fn, ...)
  local results = table.pack(pcall(fn, ...))
  if results[1] then
    failures[name] = 0
    return true, table.unpack(results, 2, results.n)
  end
  local err = tostring(results[2])
  failures[name] = (failures[name] or 0) + 1
  log.error("%s: %s", name, err)
  if failures[name] >= M.threshold then
    failures[name] = 0
    local handler = tripHandlers[name]
    if handler then
      local ok, herr = pcall(handler, err)
      if not ok then log.error("%s trip handler: %s", name, tostring(herr)) end
    end
  end
  return false, err
end

function M.valid(obj)
  if obj == nil then return false end
  local ok, v = pcall(function() return obj:IsValid() end)
  return ok and v == true
end

return M
