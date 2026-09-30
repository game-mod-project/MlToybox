local game = require("core.game")
local datatable = require("core.datatable")
local safe = require("core.safe")
local plan = require("features.merc_plan")
local list = require("features.merc_list")

-- 용병 고용 창 관리: 빈 칸 자동 보충, 커스텀 용병단 상시 등록, 플레이어 고용비 환급, AI 잠금 (spec 2026-09-30-mercenary-companies-design)
local M = { name = "mercenaries", intervalSec = 1, REBUILD_MIN_INTERVAL = 3, MISMATCH_RETRY = 10 }
M.clock = os.time

-- 새 순정 칸 고르기: 후보에서 중복 없이 count 개
M.pick = function(candidates, count)
  local pool, out = {}, {}
  for i, c in ipairs(candidates) do pool[i] = c end
  for _ = 1, count do out[#out + 1] = table.remove(pool, math.random(#pool)) end
  return out
end

local function fresh()
  return { baselineDone = false, refunded = 0, nextRebuild = 0, note = nil }
end

function M.enable(state)
  state.merc = fresh()
  state.mercenaries = nil
end

function M.disable(state)
  state.mercenaries = nil
end

-- 용병단 ID -> { mine = 플레이어 분대가 하나라도 있는가 }. 분대가 없는 용병단은 표에 없다
local function owners(engine, pawn)
  local out, addr = {}, pawn:GetAddress()
  local squads = engine.squads
  for i = 1, #squads do
    local squad = squads[i]
    local id = squad.companyID
    if id >= 0 then
      local o = out[id] or { mine = false }
      local owner = squad.ownerPawn
      if safe.valid(owner) and owner:GetAddress() == addr then o.mine = true end
      out[id] = o
    end
  end
  return out
end

-- 내 용병단의 고용비를 돌려주고 유지비를 0으로 만든다. 내 용병단 수와 AI 용병단 수를 돌려준다.
-- 환급을 켠 뒤 첫 점검은 환급 없이 0 으로만 만든다(켜기 전에 고용한 용병단). 그 뒤에 비용이 남은 내 용병단은 새 고용이다.
-- 치트 매니저가 없으면 비용을 그대로 둔다(0 으로 만들면 환급할 금액을 잃는다).
local function settle(st, settings, engine, pawn)
  local own = owners(engine, pawn)
  local mine, ai = 0, 0
  local refund = settings.refund == true
  local cheat = refund and game.cheat() or nil
  engine.hiredMercs:ForEach(function(k, v)
    local o = own[game.unwrap(k)]
    if not o then return end
    if not o.mine then
      ai = ai + 1
      return
    end
    mine = mine + 1
    local company = game.unwrap(v)
    if not refund or company.cost <= 0 then return end
    if not st.baselineDone then
      company.cost = 0
    elseif cheat then
      cheat:ChangeTreasury(company.cost)
      st.refunded = st.refunded + company.cost
      company.cost = 0
    end
  end)
  st.baselineDone = refund
  return mine, ai
end

function M.tick(state, settings)
  settings = settings or {}
  local pawn, engine = game.pawn(), game.engine()
  if not pawn or not engine then return end
  local st = state.merc
  if not st then
    st = fresh()
    state.merc = st
  end

  local mine, ai = settle(st, settings, engine, pawn)

  local rows = list.rows()
  local vanillaNames, regionKeys = {}, {}
  for _, r in ipairs(rows) do vanillaNames[r.name:lower()] = true end
  for _, r in ipairs(game.regionList()) do regionKeys[#regionKeys + 1] = r.key end
  local units = datatable.object("unitTemplates")
  local customs, skipped = plan.validate(settings.companies, {
    vanillaNames = vanillaNames,
    regionKeys = regionKeys,
    unitExists = function(u) return units:FindRow(u) ~= nil end,
  })

  -- 고용 확인 창이 떠 있는 동안 목록을 바꾸면 확인 중인 카드가 다른 용병단이 된다
  local screen = list.screenState()
  if not screen.confirming then
    local result = plan.build({
      rows = rows, hiredNames = list.hiredNames(engine), current = list.read(engine), customs = customs,
      screenOpen = screen.open, lockFromAi = settings.lockFromAi == true, canCopyRows = list.CAN_COPY_ROWS, pick = M.pick,
    })
    local now = M.clock()
    if result.action == "inplace" then
      list.applyInPlace(engine, result.slots)
      list.refreshScreen()
    elseif result.action == "rebuild" and now >= st.nextRebuild then
      local made = list.rebuild(engine, result.desired, result.renames)
      if made == #result.desired then
        st.note = nil
        st.nextRebuild = now + M.REBUILD_MIN_INTERVAL
      else
        st.note = string.format("rebuild produced %d of %d slots", made, #result.desired)
        st.nextRebuild = now + M.MISMATCH_RETRY
      end
      list.refreshScreen()
    end
  end

  local customNames, slots = {}, {}
  for _, c in ipairs(customs) do customNames[c.name] = true end
  for i, e in ipairs(list.read(engine)) do
    slots[i] = { name = e.name, cost = e.cost, custom = customNames[e.name] == true }
  end
  state.mercenaries = { slots = slots, hiredMine = mine, hiredAi = ai, refunded = st.refunded, skipped = skipped, note = st.note }
end

return M
