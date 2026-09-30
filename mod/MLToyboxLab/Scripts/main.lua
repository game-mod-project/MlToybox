-- MLToyboxLab: dev-only in-game Lua runner. Executes lab\run.lua on the game thread and writes lab\out.txt. Deploy only while developing.
local src = debug.getinfo(1, "S").source:gsub("^@", "")
local modDir = src:match("^(.*)[\\/][^\\/]+[\\/][^\\/]+$")
local lab = modDir .. "\\lab"
local runPath, outPath = lab .. "\\run.lua", lab .. "\\out.txt"
local busy = false

local function readAll(p) local f = io.open(p, "rb"); if not f then return nil end; local s = f:read("a"); f:close(); return s end

local function fmt(v, depth)
  depth = depth or 0
  local t = type(v)
  if t == "table" and depth < 3 then
    local parts = {}
    for k, x in pairs(v) do parts[#parts + 1] = tostring(k) .. "=" .. fmt(x, depth + 1) end
    return "{" .. table.concat(parts, ", ") .. "}"
  elseif t == "userdata" then
    local ok, s = pcall(function() return v:GetFullName() end)
    if ok and s then return "<" .. s .. ">" end
    local ok2, s2 = pcall(function() return v:ToString() end)
    if ok2 and s2 then return "'" .. s2 .. "'" end
  end
  return tostring(v)
end

local function run(code)
  local out = {}
  local env = setmetatable({}, { __index = _G })
  env.print = function(...)
    local a = table.pack(...)
    local s = {}
    for i = 1, a.n do s[#s + 1] = fmt(a[i]) end
    out[#out + 1] = table.concat(s, "\t")
  end
  env.fmt = fmt
  local fn, err = load(code, "run", "t", env)
  if not fn then
    out[#out + 1] = "COMPILE ERROR: " .. tostring(err)
  else
    local ok, e = xpcall(fn, debug.traceback)
    if not ok then out[#out + 1] = "RUNTIME ERROR: " .. tostring(e) end
  end
  out[#out + 1] = "<<END>>"
  local f = io.open(outPath .. ".tmp", "wb"); f:write(table.concat(out, "\n")); f:close()
  os.remove(outPath); os.rename(outPath .. ".tmp", outPath)
end

LoopAsync(500, function()
  if busy then return false end
  local code = readAll(runPath)
  if code then
    busy = true
    os.remove(runPath)
    ExecuteInGameThread(function() pcall(run, code); busy = false end)
  end
  return false
end)
print("[MLToyboxLab] loaded, lab=" .. lab .. "\n")
