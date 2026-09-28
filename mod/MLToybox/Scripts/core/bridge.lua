local json = require("lib.json")
local fileio = require("core.fileio")

local M = { VERSION = 1 }

function M.parseControl(text)
  if text == nil or text == "" then return nil, "empty control" end
  local ok, data = pcall(json.decode, text)
  if not ok then return nil, "parse error: " .. tostring(data) end
  if type(data) ~= "table" then return nil, "control is not an object" end
  if data.version ~= M.VERSION then return nil, "version mismatch: " .. tostring(data.version) end
  if type(data.seq) ~= "number" then return nil, "missing seq" end
  if type(data.features) ~= "table" then return nil, "missing features" end
  return data
end

function M.new(dir)
  local b = {
    dir = dir,
    controlPath = dir .. "\\control.json",
    statusPath = dir .. "\\status.json",
    lastSeq = nil,
    lastError = nil,
  }

  function b:poll()
    local text = fileio.read(self.controlPath)
    if text == nil then return nil end
    local control, err = M.parseControl(text)
    if not control then
      self.lastError = err
      return nil
    end
    self.lastError = nil
    if control.seq == self.lastSeq then return nil end
    self.lastSeq = control.seq
    return control
  end

  function b:writeStatus(status)
    return fileio.writeAtomic(self.statusPath, json.encode(status))
  end

  return b
end

return M
