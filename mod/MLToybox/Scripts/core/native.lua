local json = require("lib.json")
local fileio = require("core.fileio")

-- 독립 네이티브 DLL(spec §11)과 오버레이 DLL 로드, native_status.json / overlay_status.json 병합
local M = { STALE_SEC = 5, NATIVE_DLL = "mltoybox_native.dll", OVERLAY_DLL = "mltoybox_overlay.dll" }
M.loadlib = package.loadlib

function M.loadFile(nativeDir, fileName)
  local path = nativeDir .. "\\" .. fileName
  local f = io.open(path, "rb")
  if not f then return false, "not deployed" end
  f:close()
  local ok, err = M.loadlib(path, "*")
  if not ok then return false, tostring(err) end
  return true, nil
end

function M.load(nativeDir)
  return M.loadFile(nativeDir, M.NATIVE_DLL)
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

-- 오버레이 DLL 이 쓰는 overlay_status.json 을 합친다. 올리지 못했으면 파일을 보지 않는다(지난 실행의 파일이 남아 있을 수 있다)
function M.overlayStatus(statusPath, now, loaded, loadErr)
  local out = { loaded = loaded, error = loadErr, stale = true }
  if not loaded then return out end
  local text = fileio.read(statusPath)
  if text then
    local ok, d = pcall(json.decode, text)
    if ok and type(d) == "table" and type(d.heartbeat) == "number" then
      out.stale = (now - d.heartbeat) > M.STALE_SEC
      if type(d.state) == "string" then out.state = d.state end
      if type(d.reason) == "string" then out.reason = d.reason end
    end
  end
  return out
end

return M
