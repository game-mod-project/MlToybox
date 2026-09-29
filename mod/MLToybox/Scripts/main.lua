local source = debug.getinfo(1, "S").source
local scriptsDir = source:gsub("^@", ""):match("^(.*)[\\/][^\\/]+$")
package.path = scriptsDir .. "\\?.lua;" .. package.path

local paths = require("core.paths")
local log = require("core.log")
local safe = require("core.safe")
local bridgeLib = require("core.bridge")
local registryLib = require("core.registry")
local gamemode = require("core.gamemode")
local native = require("core.native")
local commandsLib = require("core.commands")
local spawnSquads = require("features.spawn_squads")
local population = require("features.population")
local config = require("config")

safe.setThreshold(config.failureThreshold)

local bridgeDir = paths.parentDir(scriptsDir) .. "\\bridge"
local bridge = bridgeLib.new(bridgeDir)
local nativeLoaded, nativeErr = native.load(paths.parentDir(scriptsDir) .. "\\native")
log.info("native: %s", nativeLoaded and "loaded" or tostring(nativeErr))
local registry = registryLib.new()
local commands = commandsLib.new({ spawnSquads = spawnSquads.spawn, reformSquads = spawnSquads.reform, addFamilies = population.command })
local appliedSeq = nil

for _, name in ipairs(config.featureModules) do
  local ok, mod = pcall(require, "features." .. name)
  if ok then registry:add(mod) else log.error("load feature %s: %s", name, tostring(mod)) end
end
registry:apply(config.defaults)

RegisterLoadMapPreHook(function()
  safe.call("core", function() registry:setInGame(false) end)
end)

RegisterInitGameStatePostHook(function(context)
  safe.call("core", function()
    local gameState = context:get()
    local fullName = gameState:GetFullName()
    local inGame = gamemode.isGameplay(fullName, config)
    log.info("GameState: %s inGame=%s", fullName, tostring(inGame))
    registry:setInGame(inGame)
  end)
end)

LoopAsync(config.pollIntervalMs, function()
  local ok, err = pcall(function()
    local control = bridge:poll()
    ExecuteInGameThread(function()
      safe.call("core", function()
        local now = os.time()
        if control then
          registry:apply(control.features)
          commands:run(control.commands, { now = now, inGame = registry.state.inGame })
          appliedSeq = control.seq
        end
        registry:tick(now)
        safe.call("spawnSquads", function() spawnSquads.tick(registry.state.inGame) end)
        local status = registry:status(now, appliedSeq, bridge.lastError)
        status.native = native.status(bridgeDir .. "\\native_status.json", now, nativeLoaded, nativeErr)
        status.commands = commands:status()
        local spawnOk, spawnStatus = safe.call("spawnSquads", spawnSquads.status, registry.state.inGame)
        status.spawn = spawnOk and spawnStatus or nil
        local wrote, werr = bridge:writeStatus(status)
        if not wrote then log.error("status write: %s", tostring(werr)) end
      end)
    end)
  end)
  if not ok then log.error("loop: %s", tostring(err)) end
  return false
end)

log.info("loaded (scripts=%s, bridge=%s)", scriptsDir, bridgeDir)
