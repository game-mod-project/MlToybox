local source = debug.getinfo(1, "S").source:gsub("^@", "")
local scriptsDir = source:match("^(.*)[\\/][^\\/]+$")
local modDir = scriptsDir:match("^(.*)[\\/][^\\/]+$")
local requestPath = modDir .. "\\dump_request.txt"
local donePath = modDir .. "\\dump_done.txt"
local eventsPath = modDir .. "\\events.txt"
local busy = false

local function log(msg) print("[MLToyboxDump] " .. msg .. "\n") end

local function append(path, line)
  local f = io.open(path, "ab")
  if f then f:write(os.date("!%Y-%m-%dT%H:%M:%SZ") .. " " .. line .. "\n"); f:close() end
end

local function exists(path)
  local f = io.open(path, "rb")
  if f then f:close() return true end
  return false
end

RegisterLoadMapPreHook(function() append(eventsPath, "LoadMapPre") end)
RegisterInitGameStatePostHook(function(context)
  local ok, name = pcall(function() return context:get():GetFullName() end)
  append(eventsPath, "InitGameState " .. (ok and name or ("<error " .. tostring(name) .. ">")))
end)

local function runDump()
  local results = {}
  for _, step in ipairs({
    { "DumpAllObjects", DumpAllObjects },
    { "GenerateSDK", GenerateSDK },
    { "GenerateLuaTypes", GenerateLuaTypes },
  }) do
    local started = os.time()
    local ok, err = pcall(step[2])
    results[#results + 1] = string.format("%s ok=%s sec=%d err=%s", step[1], tostring(ok), os.time() - started, tostring(err))
    log(results[#results])
  end
  os.remove(requestPath)
  local f = io.open(donePath, "wb")
  if f then f:write(table.concat(results, "\n") .. "\n"); f:close() end
end

LoopAsync(2000, function()
  if not busy and exists(requestPath) then
    busy = true
    os.remove(donePath)
    ExecuteInGameThread(function()
      local ok, err = pcall(runDump)
      if not ok then log("dump failed: " .. tostring(err)) end
      busy = false
    end)
  end
  return false
end)

log("loaded, waiting for " .. requestPath)
