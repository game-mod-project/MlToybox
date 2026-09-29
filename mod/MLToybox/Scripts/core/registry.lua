local safe = require("core.safe")

local M = { DEFAULT_INTERVAL = 2 }

function M.new()
  local r = {
    features = {}, order = {}, active = {}, settings = {},
    errors = {}, tripped = {}, lastTick = {},
    state = { inGame = false },
  }

  function r:add(feature)
    assert(type(feature) == "table" and type(feature.name) == "string", "feature needs a name")
    local name = feature.name
    self.features[name] = feature
    self.order[#self.order + 1] = name
    self.active[name] = false
    safe.reset(name)
    safe.onTrip(name, function(err)
      self.tripped[name] = true
      self:_deactivate(name, "auto-disabled: " .. tostring(err))
    end)
  end

  function r:_invoke(name, method)
    local fn = self.features[name][method]
    if fn == nil then return true end
    local ok, err = safe.call(name, fn, self.state, self.settings[name])
    if not ok and not self.tripped[name] then self.errors[name] = tostring(err) end
    return ok
  end

  function r:_activate(name)
    if self:_invoke(name, "enable") then
      self.active[name] = true
      self.errors[name] = nil
      self.lastTick[name] = nil
    end
  end

  function r:_deactivate(name, reason)
    if self.active[name] then
      self.active[name] = false
      local fn = self.features[name].disable
      if fn then pcall(fn, self.state, self.settings[name]) end
    end
    if reason then self.errors[name] = reason end
  end

  function r:_wants(name)
    local s = self.settings[name]
    return self.state.inGame and not self.tripped[name] and type(s) == "table" and s.enabled == true
  end

  function r:_sync()
    for _, name in ipairs(self.order) do
      local want = self:_wants(name)
      if want and not self.active[name] then
        self:_activate(name)
      elseif not want and self.active[name] then
        self:_deactivate(name)
      end
    end
  end

  function r:apply(featuresCfg)
    if type(featuresCfg) ~= "table" then return end
    for _, name in ipairs(self.order) do
      local cfg = featuresCfg[name]
      if type(cfg) == "table" then
        self.settings[name] = cfg
        self.tripped[name] = nil
        if self.active[name] and cfg.enabled == true then self:_invoke(name, "configure") end
      end
    end
    self:_sync()
  end

  function r:setInGame(inGame)
    if not inGame then
      for _, name in ipairs(self.order) do self:_deactivate(name) end
      -- 이전 맵의 오브젝트 캐시·자원 표시가 남지 않도록 비운다. 기능이 참조를 쥐고 있을 수 있어 테이블은 유지한다.
      for key in pairs(self.state) do self.state[key] = nil end
    end
    self.state.inGame = inGame
    self:_sync()
  end

  function r:tick(now)
    -- enable 이 실패한 기능(예: 맵 로드 직후 대상 미생성)을 매 tick 재시도한다. 계속 실패하면 safe 임계치에서 trip 된다.
    self:_sync()
    for _, name in ipairs(self.order) do
      local f = self.features[name]
      if self.active[name] and f.tick then
        local s = self.settings[name]
        local interval = (type(s) == "table" and tonumber(s.intervalSec)) or f.intervalSec or M.DEFAULT_INTERVAL
        local last = self.lastTick[name]
        if last == nil or now - last >= interval then
          self.lastTick[name] = now
          self:_invoke(name, "tick")
        end
      end
    end
  end

  function r:status(now, appliedSeq, bridgeError)
    local features = {}
    for _, name in ipairs(self.order) do
      features[name] = { active = self.active[name], lastError = self.errors[name] }
    end
    local st = self.state
    return {
      version = 1,
      heartbeat = now,
      inGame = st.inGame,
      appliedSeq = appliedSeq,
      bridgeError = bridgeError,
      features = next(features) and features or nil,
      resourceIds = (type(st.resourceIds) == "table" and #st.resourceIds > 0) and st.resourceIds or nil,
      resources = (type(st.resources) == "table" and next(st.resources)) and st.resources or nil,
      population = st.population,
      regions = (type(st.regions) == "table" and #st.regions > 0) and st.regions or nil,
    }
  end

  return r
end

return M
