local M = {}
local PREFIX = "[MLToybox] "

M.sink = print

function M.info(fmt, ...)
  M.sink(PREFIX .. string.format(fmt, ...) .. "\n")
end

function M.error(fmt, ...)
  M.sink(PREFIX .. "ERROR " .. string.format(fmt, ...) .. "\n")
end

return M
