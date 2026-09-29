local game = require("core.game")
local catalog = require("features.resources_catalog")

-- 국고·영향력·왕의 총애는 영주 전체 값이라 features/lord.lua 가 관리한다
local M = { name = "resources", intervalSec = 2 }

local function shortfall(current, target)
  if type(target) ~= "number" then return 0 end
  if current < target then return target - current end
  return 0
end

function M.enable(state)
  state.resourceIds = catalog.ids()
  state.resources = {}
end

-- 영지 식별자: regionUniqueTag(세이브 간 고정, 예 "hof"), 표시 이름: regionName(플레이어가 바꿀 수 있음)
local function regionKey(r)
  local ok, tag = pcall(function() return r.regionUniqueTag:ToString() end)
  return ok and tag or nil
end

local function regionName(r)
  local ok, name = pcall(function() return r.regionName:ToString() end)
  return ok and name or nil
end

-- 영지별 목표(regionTargets[영지][자원])가 있으면 그것을, 없으면 공통 목표를 쓴다
local function targetFor(targets, override, id)
  if override and override[id] ~= nil then return override[id] end
  return targets[id]
end

function M.tick(state, settings)
  local targets = (settings and settings.targets) or {}
  local regionTargets = (settings and settings.regionTargets) or {}
  local regions = game.playerRegions()
  local cur = {}
  local perRegion = {}
  for i, r in ipairs(regions) do
    local key = regionKey(r)
    perRegion[i] = { key = key, name = regionName(r), values = {}, override = key and regionTargets[key] or nil }
  end

  for _, it in ipairs(catalog.items) do
    local total = 0
    for i, r in ipairs(regions) do
      local stock = r:getStockOfGood(it.type, false, false)
      local need = shortfall(stock, targetFor(targets, perRegion[i].override, it.id))
      if need > 0 then
        r:grantResources({ { Type = it.type, amt = need } }, false)
        stock = stock + need
      end
      perRegion[i].values[it.id] = stock
      total = total + stock
    end
    cur[it.id] = total
  end

  local wealth = 0
  for i, r in ipairs(regions) do
    local target = targetFor(targets, perRegion[i].override, "RegionalWealth")
    if shortfall(r.regionalWealth, target) > 0 then r.regionalWealth = target end
    perRegion[i].values.RegionalWealth = r.regionalWealth
    wealth = wealth + r.regionalWealth
  end
  cur.RegionalWealth = wealth

  state.regions = {}
  for i, p in ipairs(perRegion) do
    if p.key then state.regions[#state.regions + 1] = { key = p.key, name = p.name or p.key, values = p.values } end
  end

  state.resources = cur
end

return M
