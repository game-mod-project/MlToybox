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
  local nodes, nodesDay = nil, nil
  local text = fileio.read(statusPath)
  if text then
    local ok, d = pcall(json.decode, text)
    if ok and type(d) == "table" and type(d.heartbeat) == "number" then
      out.heartbeat = d.heartbeat
      out.stale = (now - d.heartbeat) > M.STALE_SEC
      if type(d.features) == "table" and next(d.features) then out.features = d.features end
      if not out.stale and type(d.nodes) == "table" then nodes, nodesDay = d.nodes, tonumber(d.nodesDay) end
    end
  end
  M.last = out
  -- 내 영지의 광물 매장지(영지 주소, 종류 번호, 남은 양). 네이티브가 날짜가 넘어갈 때 적는다. status.json 에는 영지 기능이 정리해서 싣는다.
  -- nodesDay 는 모을 때마다 오르는 번호다(영지 기능이 "이 맵에서 모은 것인가"를 가린다)
  M.nodes, M.nodesDay = nodes, nodesDay
  return out
end

-- 네이티브 DLL 이 그 항목들을 모두 설치했는가(마지막으로 읽은 상태로 본다).
-- true: 네이티브가 그 일을 맡았다. false: 못 맡는다(DLL 을 올리지 못했거나 항목이 설치되지 않았다). nil: 아직 모른다(상태를 못 읽었다)
function M.installed(names)
  local s = M.last
  if not s then return nil end
  if not s.loaded then return false end
  if s.stale or type(s.features) ~= "table" then return nil end
  for _, name in ipairs(names) do
    local f = s.features[name]
    if not (type(f) == "table" and f.installed == true) then return false end
  end
  return true
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
