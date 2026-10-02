-- 용병 고용 창 목록 계획. 게임 객체를 만지지 않는 순수 계산이다 (spec 2026-09-30 §4.1)
local M = { MAX_SLOTS = 3, MAX_SQUADS = 10, NAME_MAX = 40, LOCK_COST = 10000000 }

local function trim(s)
  s = s:gsub("^%s+", "")
  s = s:gsub("%s+$", "")
  return s
end

local function reasonFor(name, c, ctx, seen)
  if name == "" then return "name is empty" end
  local len = utf8.len(name)
  if not len or len > M.NAME_MAX then return "name is longer than " .. M.NAME_MAX .. " characters" end
  local key = name:lower()
  if ctx.vanillaNames[key] then return "name is used by a game company" end
  if seen[key] then return "duplicate name" end
  if type(c.units) ~= "table" or #c.units < 1 or #c.units > M.MAX_SQUADS then
    return "units must list 1.." .. M.MAX_SQUADS .. " squads"
  end
  for _, u in ipairs(c.units) do
    if type(u) ~= "string" or not ctx.unitExists(u) then return "unknown unit: " .. tostring(u) end
  end
  local cost = c.cost
  if type(cost) ~= "number" or cost < 0 or cost ~= math.floor(cost) then return "cost must be a non-negative integer" end
  if #ctx.regionKeys == 0 then return "no player region" end
  return nil
end

-- 사용 중(enabled)인 정의만 본다. enabled 키가 없으면 사용으로 본다(패널과 오버레이가 그렇게 읽고 3개 제한에도 센다).
-- 통과한 것은 최대 MAX_SLOTS 개, 나머지는 이유와 함께 skipped 로 돌려준다
function M.validate(companies, ctx)
  local valid, skipped, seen = {}, {}, {}
  if type(companies) ~= "table" then return valid, skipped end
  for _, c in ipairs(companies) do
    if type(c) == "table" and c.enabled ~= false then
      local name = type(c.name) == "string" and trim(c.name) or ""
      local reason = reasonFor(name, c, ctx, seen)
      if not reason and #valid >= M.MAX_SLOTS then reason = "more than " .. M.MAX_SLOTS .. " enabled companies" end
      if reason then
        skipped[#skipped + 1] = { name = name, reason = reason }
      else
        seen[name:lower()] = true
        local region = ctx.regionKeys[1]
        for _, key in ipairs(ctx.regionKeys) do
          if key == c.region then region = key end
        end
        local units = {}
        for i, u in ipairs(c.units) do units[i] = u end
        -- 깃발: 용병 표 용병단의 이름. 모르는 이름이면 칸에 있던 깃발을 그대로 둔다(nil)
        local banner = nil
        if type(c.banner) == "string" and ctx.vanillaNames[trim(c.banner):lower()] then banner = trim(c.banner):lower() end
        valid[#valid + 1] = { name = name, units = units, cost = math.floor(c.cost), region = region, banner = banner }
      end
    end
  end
  return valid, skipped
end

local function sameUnits(a, b)
  if #a ~= #b then return false end
  for i = 1, #a do
    if tostring(a[i]):lower() ~= tostring(b[i]):lower() then return false end
  end
  return true
end

-- 목록의 칸 e 가 있어야 할 항목 d 와 같은가
local function same(e, d)
  if e.name ~= d.name or e.cost ~= d.cost then return false end
  if d.kind == "custom" then
    if not sameUnits(e.units or {}, d.company.units) or e.region ~= d.company.region then return false end
    -- 깃발을 골랐으면, 칸의 깃발·색을 쓰는 표 용병단(e.banner 목록) 가운데 고른 이름이 있어야 한다
    if d.company.banner == nil then return true end
    for _, name in ipairs(e.banner or {}) do
      if name == d.company.banner then return true end
    end
    return false
  end
  return true
end

function M.build(input)
  local candidates, byName = {}, {}
  for _, r in ipairs(input.rows) do
    if not r.quest and not input.hiredNames[r.name] then
      candidates[#candidates + 1] = r
      byName[r.name] = r
    end
  end

  local desired, used = {}, {}
  local locked = input.lockFromAi and not input.screenOpen
  for _, c in ipairs(input.customs) do
    desired[#desired + 1] = { kind = "custom", name = c.name, company = c, cost = locked and M.LOCK_COST or c.cost }
    used[c.name] = true
  end
  -- 이미 떠 있는 순정 카드는 유지한다
  for _, e in ipairs(input.current) do
    local r = byName[e.name]
    if r and not used[e.name] and #desired < M.MAX_SLOTS then
      used[e.name] = true
      desired[#desired + 1] = { kind = "vanilla", name = r.name, rowName = r.rowName, cost = r.cost, keep = true }
    end
  end
  local free = {}
  for _, r in ipairs(candidates) do
    if not used[r.name] then free[#free + 1] = r end
  end
  local need = math.min(M.MAX_SLOTS - #desired, #free)
  if need > 0 then
    for _, r in ipairs(input.pick(free, need)) do
      desired[#desired + 1] = { kind = "vanilla", name = r.name, rowName = r.rowName, cost = r.cost, keep = false }
    end
  end

  local result = { desired = desired, action = "rebuild", renames = math.max(0, #desired - #candidates) }
  if #input.current ~= #desired then return result end

  -- 칸 수가 같다. i번 칸에 desired[i] 가 오게 한다(커스텀이 앞 칸을 차지한다, spec §2.1)
  local dirty = false
  for i, e in ipairs(input.current) do
    local d = desired[i]
    if not same(e, d) then
      dirty = true
      -- 다른 순정 용병단으로 바꾸려면 표 행 내용을 칸에 복사해야 한다. 그게 안 되는 환경이면 다시 뽑는다
      if d.kind == "vanilla" and e.name ~= d.name and not input.canCopyRows then return result end
    end
  end
  result.slots = desired
  result.action = dirty and "inplace" or "none"
  return result
end

return M
