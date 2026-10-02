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
local retinueEditor = require("features.retinue_editor")
local population = require("features.population")
local lord = require("features.lord")
local resourcesCatalog = require("features.resources_catalog")
local game = require("core.game")
local config = require("config")

safe.setThreshold(config.failureThreshold)

local bridgeDir = paths.parentDir(scriptsDir) .. "\\bridge"
local bridge = bridgeLib.new(bridgeDir)
-- 오버레이가 읽을 자원 이름 표. 오버레이 DLL 을 올리기 전에 써 둔다
local catalogOk, catalogErr = bridge:writeCatalog(resourcesCatalog.describe())
if not catalogOk then log.error("catalog write: %s", tostring(catalogErr)) end
local nativeDir = paths.parentDir(scriptsDir) .. "\\native"
local nativeLoaded, nativeErr = native.load(nativeDir)
log.info("native: %s", nativeLoaded and "loaded" or tostring(nativeErr))
local overlayLoaded, overlayErr = false, "disabled in config"
if config.overlay then overlayLoaded, overlayErr = native.loadFile(nativeDir, native.OVERLAY_DLL) end
log.info("overlay: %s", overlayLoaded and "loaded" or tostring(overlayErr))
local registry = registryLib.new()
-- 오버레이의 [로그] 탭에 보이는 줄: 기능이 켜지고 꺼질 때, 명령을 처리했을 때
registry.onChange = function(name, active, reason)
  if active then log.info("feature %s: on", name)
  else log.info("feature %s: off%s", name, reason and (" (" .. tostring(reason) .. ")") or "") end
end
local commands = commandsLib.new({
  spawnSquads = spawnSquads.spawn, reformSquads = spawnSquads.reform, addFamilies = population.command, setLord = lord.set,
  customizeRetinue = retinueEditor.open,
}, function(command, result)
  if result.ok then log.info("command %s: ok", tostring(command.type))
  else log.error("command %s: %s", tostring(command.type), tostring(result.error)) end
end)
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
        safe.call("retinueEditor", function() retinueEditor.tick(registry.state.inGame) end)
        local status = registry:status(now, appliedSeq, bridge.lastError)
        status.native = native.status(bridgeDir .. "\\native_status.json", now, nativeLoaded, nativeErr)
        status.overlay = native.overlayStatus(bridgeDir .. "\\overlay_status.json", now, overlayLoaded, overlayErr)
        status.commands = commands:status()
        local spawnOk, spawnStatus = safe.call("spawnSquads", spawnSquads.status, registry.state.inGame)
        status.spawn = spawnOk and spawnStatus or nil
        local retinueOk, retinueStatus = safe.call("retinueEditor", retinueEditor.status, registry.state.inGame)
        status.retinue = retinueOk and retinueStatus or nil
        if registry.state.inGame then   -- 영주 기능이 꺼져 있어도 현재값은 보고한다
          local lordOk, lordValues = safe.call("lord", lord.read)
          if lordOk then status.lord = lordValues end
          local regionsOk, regionList = safe.call("regions", game.regionList)   -- 패널 영지 선택 목록(병력 생성 위치 등)
          if regionsOk and #regionList > 0 then status.playerRegions = regionList end
        end
        local wrote, werr = bridge:writeStatus(status)
        if not wrote then log.error("status write: %s", tostring(werr)) end
      end)
    end)
  end)
  if not ok then log.error("loop: %s", tostring(err)) end
  return false
end)

log.info("loaded (scripts=%s, bridge=%s)", scriptsDir, bridgeDir)
