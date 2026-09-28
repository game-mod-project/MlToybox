local M = {}

function M.parentDir(path)
  return path:match("^(.*)[\\/][^\\/]+$")
end

function M.scriptsDirFromSource(source)
  return M.parentDir((source:gsub("^@", "")))
end

return M
