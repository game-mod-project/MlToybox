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

  -- 오버레이가 자원 표에 쓸 이름 표(id, 한글 이름, 분류, 묶음)와 저장 용량 표에 쓸 건물 목록(id, 한글 이름, 기본 한도).
  -- 바뀌지 않는 값이라 status 에 싣지 않고 시작할 때 한 번 쓴다
  function b:writeCatalog(resources, buildings)
    return fileio.writeAtomic(self.dir .. "\\catalog.json", json.encode({ version = M.VERSION, resources = resources, buildings = buildings }))
  end

  return b
end

return M
