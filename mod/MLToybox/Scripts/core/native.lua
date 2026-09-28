local json = require("lib.json")
local fileio = require("core.fileio")

-- 독립 네이티브 DLL(spec §11) 로드와 native_status.json 병합
local M = { STALE_SEC = 5 }
M.loadlib = package.loadlib

function M.load(nativeDir)
  local path = nativeDir .. "\\mltoybox_native.dll"
  local f = io.open(path, "rb")
  if not f then return false, "not deployed" end
  f:close()
  local ok, err = M.loadlib(path, "*")
  if not ok then return false, tostring(err) end
  return true, nil
end

function M.status(statusPath, now, loaded, loadErr)
  local out = { loaded = loaded, error = loadErr, stale = true }
  local text = fileio.read(statusPath)
  if text then
    local ok, d = pcall(json.decode, text)
    if ok and type(d) == "table" and type(d.heartbeat) == "number" then
      out.heartbeat = d.heartbeat
      out.stale = (now - d.heartbeat) > M.STALE_SEC
      if type(d.features) == "table" and next(d.features) then out.features = d.features end
    end
  end
  return out
end

return M
