local source = debug.getinfo(1, "S").source
local scriptsDir = source:gsub("^@", ""):match("^(.*)[\\/][^\\/]+$")
package.path = scriptsDir .. "\\?.lua;" .. package.path

local paths = require("core.paths")
local log = require("core.log")
local safe = require("core.safe")
local bridgeLib = require("core.bridge")
local registryLib = require("core.registry")
local config = require("config")

safe.setThreshold(config.failureThreshold)

local bridgeDir = paths.parentDir(scriptsDir) .. "\\bridge"
local bridge = bridgeLib.new(bridgeDir)
local registry = registryLib.new()
local appliedSeq = nil

for _, name in ipairs(config.featureModules) do
  local ok, mod = pcall(require, "features." .. name)
  if ok then registry:add(mod) else log.error("load feature %s: %s", name, tostring(mod)) end
end
registry:apply(config.defaults)

local function isGameplayState(fullName)
  if config.gameStateClassPattern == nil then return true end
  return fullName:find(config.gameStateClassPattern) ~= nil
end

RegisterLoadMapPreHook(function()
  safe.call("core", function() registry:setInGame(false) end)
end)

RegisterInitGameStatePostHook(function(context)
  safe.call("core", function()
    local gameState = context:get()
    local fullName = gameState:GetFullName()
    local inGame = isGameplayState(fullName)
    log.info("GameState: %s inGame=%s", fullName, tostring(inGame))
    registry:setInGame(inGame)
  end)
end)

LoopAsync(config.pollIntervalMs, function()
  local ok, err = pcall(function()
    local control = bridge:poll()
    ExecuteInGameThread(function()
      safe.call("core", function()
        if control then
          registry:apply(control.features)
          appliedSeq = control.seq
        end
        local now = os.time()
        registry:tick(now)
        local wrote, werr = bridge:writeStatus(registry:status(now, appliedSeq, bridge.lastError))
        if not wrote then log.error("status write: %s", tostring(werr)) end
      end)
    end)
  end)
  if not ok then log.error("loop: %s", tostring(err)) end
  return false
end)

log.info("loaded (scripts=%s, bridge=%s)", scriptsDir, bridgeDir)
