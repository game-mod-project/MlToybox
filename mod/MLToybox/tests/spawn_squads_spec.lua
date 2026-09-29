local T = require("t")
local F = require("fakes")
local game = require("core.game")
local datatable = require("core.datatable")
local spawn = require("features.spawn_squads")

local function setup()
  local calls = {}
  local engine = F.object({})
  engine.spawnArmy = function(_, pos, names, pawn, company, days)
    calls[#calls + 1] = { pos = pos, names = names, pawn = pawn, company = company, days = days }
    local ids = {}
    for i = 1, #names do ids[i] = F.wrap(100 + i) end
    return ids
  end
  local pawn = F.object({})
  game.pawn = function() return pawn end
  game.engine = function() return engine end
  game.fname = function(s) return "FName:" .. s end
  game.anchorLocation = function() return { X = 10, Y = 20, Z = 30 } end
  datatable.find = function(p)
    if p == datatable.PATHS.unitTemplates then return F.datatable({ spearMilitia = {}, bowMilitia = {} }) end
  end
  return calls, pawn
end

local IN_GAME = { inGame = true }

local function fname(s) return { ToString = function() return s end } end
local function squad(id, owner, opts)
  opts = opts or {}
  return {
    ID = id, ownerPawn = owner, squadType = opts.type or 0, companyID = opts.company or -1,
    unitType = fname(opts.unit or "retinue_tier1"),
    unitArr = F.array(opts.units or {}), assignedRecruits = F.array(opts.recruits or {}),
  }
end

-- 해제된 생성 분대(유령): 플레이어 소유, 종류 None, 용병단 없음, 병사·모집병 0
local function reformSetup(list)
  local calls, pawn = setup()
  local removed = {}
  pawn.removeSquad = function(_, id) removed[#removed + 1] = id end
  local engine = game.engine()
  engine.squads = list(pawn)
  local spawnArmy = engine.spawnArmy
  engine.spawnArmy = function(self, pos, names, owner, company, days)   -- 새 분대 ID 는 배열 끝(마지막 ID + 1)부터
    spawnArmy(self, pos, names, owner, company, days)
    local last = engine.squads[#engine.squads]
    local ids = {}
    for i = 1, #names do ids[i] = F.wrap((last and last.ID or -1) + i) end
    return ids
  end
  spawn.resetReform()
  return calls, pawn, removed, engine
end

T.run({
  spawns_requested_count_near_anchor = function()
    local calls, pawn = setup()
    local r = spawn.spawn({ unit = "spearMilitia", count = 3 }, IN_GAME)
    T.eq(r.ok, true, "ok"); T.eq(#r.squads, 3, "three ids"); T.eq(r.squads[1], 101, "unwrapped id")
    T.eq(#calls, 1, "single spawnArmy call"); T.eq(#calls[1].names, 3, "three names")
    T.eq(calls[1].names[1], "FName:spearMilitia", "fname")
    T.eq(calls[1].pawn, pawn, "owner"); T.eq(calls[1].company, -1, "militia company"); T.eq(calls[1].days, 0, "immediate")
    T.eq(calls[1].pos.X, 10 + spawn.OFFSET_X, "offset x"); T.eq(calls[1].pos.Y, 20, "y"); T.eq(calls[1].pos.Z, 30, "z")
  end,
  rejects_when_not_in_game = function()
    local calls = setup()
    local r = spawn.spawn({ unit = "spearMilitia", count = 1 }, { inGame = false })
    T.eq(r.ok, false, "fail"); T.eq(#calls, 0, "no spawn")
  end,
  rejects_bad_count = function()
    local calls = setup()
    T.eq(spawn.spawn({ unit = "spearMilitia", count = 0 }, IN_GAME).ok, false, "zero")
    T.eq(spawn.spawn({ unit = "spearMilitia", count = spawn.MAX_COUNT + 1 }, IN_GAME).ok, false, "too many")
    T.eq(spawn.spawn({ unit = "spearMilitia", count = "x" }, IN_GAME).ok, false, "nan")
    T.eq(#calls, 0, "no spawn")
  end,
  rejects_unknown_unit = function()
    local calls = setup()
    local r = spawn.spawn({ unit = "dragon", count = 1 }, IN_GAME)
    T.eq(r.ok, false, "fail"); T.truthy(r.error:find("unknown unit", 1, true), "reason"); T.eq(#calls, 0, "no spawn")
  end,
  rejects_when_objects_missing = function()
    setup()
    game.anchorLocation = function() return nil end
    T.eq(spawn.spawn({ unit = "spearMilitia", count = 1 }, IN_GAME).ok, false, "no anchor")
    setup()
    game.engine = function() return nil end
    T.eq(spawn.spawn({ unit = "spearMilitia", count = 1 }, IN_GAME).ok, false, "no engine")
  end,
  ghosts_only_disbanded_spawned_squads_of_player = function()
    local other = F.object({})
    local _, pawn = reformSetup(function(p)
      return {
        squad(40, p, { unit = "retinue_tier1" }),                                  -- 유령
        squad(41, p, { units = { 1 }, recruits = { 1 } }),                         -- 살아 있는 생성 분대
        squad(42, p, { type = 3, recruits = { 1 } }),                              -- 해제된 순정 친위대
        squad(43, p, { type = 2, company = 0 }),                                   -- 빈 용병 분대
        squad(44, other),                                                          -- 다른 영주
        squad(45, p, { unit = "mercenary_infantry" }),                             -- 유령
      }
    end)
    local g = spawn.ghosts(pawn, game.engine())
    T.eq(#g, 2, "two ghosts")
    T.eq(g[1].id, 45, "highest id first"); T.eq(g[1].unit, "mercenary_infantry", "unit")
    T.eq(g[2].id, 40, "second"); T.eq(g[2].unit, "retinue_tier1", "unit 2")
  end,
  ghosts_include_spawned_squads_marked_mercenary_without_company = function()
    -- 진단 실험 중 squadType 을 용병(2)으로 바꾼 채 저장된 생성 분대. 순정 용병은 항상 company >= 0
    local _, pawn = reformSetup(function(p) return { squad(41, p, { type = 2, company = -1, unit = "mercenary_infantry" }) } end)
    local g = spawn.ghosts(pawn, game.engine())
    T.eq(#g, 1, "included"); T.eq(g[1].id, 41, "id")
  end,
  reform_respawns_ghost_unit_types_and_queues_removal = function()
    local calls, _, removed = reformSetup(function(p)
      return { squad(40, p, { unit = "retinue_tier1" }), squad(45, p, { unit = "mercenary_infantry" }) }
    end)
    local r = spawn.reform({}, IN_GAME)
    T.eq(r.ok, true, "ok"); T.eq(r.reformed, 2, "count")
    T.eq(#calls, 1, "one spawnArmy"); T.eq(#calls[1].names, 2, "two names")
    T.eq(calls[1].names[1], "FName:mercenary_infantry", "type kept"); T.eq(calls[1].names[2], "FName:retinue_tier1", "type kept 2")
    T.eq(#removed, 0, "removal deferred to ticks")
    T.eq(spawn.pendingRemovals(), 2, "pending")
  end,
  reform_fails_without_ghosts = function()
    local calls = reformSetup(function(p) return { squad(41, p, { units = { 1 }, recruits = { 1 } }) } end)
    local r = spawn.reform({}, IN_GAME)
    T.eq(r.ok, false, "fail"); T.eq(#calls, 0, "no spawn")
    T.eq(spawn.reform({}, { inGame = false }).ok, false, "not in game")
  end,
  tick_removes_one_ghost_per_tick_highest_id_first = function()
    -- removeSquad 는 뒤쪽 분대 ID 를 당기므로 한 틱에 하나씩, 높은 ID 부터 지운다
    local _, _, removed, engine = reformSetup(function(p)
      return { squad(40, p, { unit = "retinue_tier1" }), squad(45, p, { unit = "mercenary_infantry" }) }
    end)
    spawn.reform({}, IN_GAME)
    local pawn = game.pawn()
    engine.squads[#engine.squads + 1] = squad(46, pawn, { unit = "mercenary_infantry", units = { 1 }, recruits = { 1 } })
    engine.squads[#engine.squads + 1] = squad(47, pawn, { unit = "retinue_tier1", units = { 1 }, recruits = { 1 } })
    spawn.tick(true)
    T.eq(#removed, 1, "one per tick"); T.eq(removed[1], 45, "highest first")
    table.remove(engine.squads, 2)                                                  -- 게임이 45 를 정리
    spawn.tick(true)
    T.eq(#removed, 2, "second tick"); T.eq(removed[2], 40, "then 40")
    table.remove(engine.squads, 1)
    spawn.tick(true)
    T.eq(#removed, 2, "nothing left"); T.eq(spawn.pendingRemovals(), 0, "done")
  end,
  tick_never_removes_new_squads_or_later_ghosts_of_other_types = function()
    local _, _, removed, engine = reformSetup(function(p) return { squad(40, p, { unit = "retinue_tier1" }) } end)
    spawn.reform({}, IN_GAME)
    local pawn = game.pawn()
    engine.squads[#engine.squads + 1] = squad(41, pawn, { unit = "retinue_tier1" })  -- 새 분대(아직 병사 0)
    engine.squads[1] = squad(40, pawn, { unit = "militia" })                         -- 같은 자리에 다른 병종 유령
    spawn.tick(true)
    T.eq(#removed, 0, "no match below boundary")
  end,
  tick_waits_until_game_processes_previous_removal = function()
    local _, _, removed, engine = reformSetup(function(p)
      return { squad(40, p, { unit = "retinue_tier1" }), squad(45, p, { unit = "retinue_tier1" }) }
    end)
    spawn.reform({}, IN_GAME)
    spawn.tick(true)
    T.eq(removed[1], 45, "first")
    spawn.tick(true)                                                                -- 아직 배열이 줄지 않음
    T.eq(#removed, 1, "waits"); T.eq(spawn.pendingRemovals(), 1, "still pending")
    table.remove(engine.squads, 2)
    spawn.tick(true)
    T.eq(removed[2], 40, "continues after shrink")
  end,
  second_reform_after_first_completes_still_removes_ghost = function()
    -- 첫 재구성의 마지막 제거 대기가 남아 있으면 새 생성으로 배열이 늘어 영영 대기하던 버그
    local _, pawn, removed, engine = reformSetup(function(p) return { squad(40, p, { unit = "retinue_tier1" }) } end)
    spawn.reform({}, IN_GAME)
    engine.squads[#engine.squads + 1] = squad(41, pawn, { unit = "retinue_tier1", units = { 1 }, recruits = { 1 } })
    spawn.tick(true)
    T.eq(removed[1], 40, "first removed")
    table.remove(engine.squads, 1)                                                  -- 게임이 정리, 새 분대는 40 으로
    engine.squads[1] = squad(40, pawn, { unit = "retinue_tier1" })                   -- 사용자가 그 분대를 다시 해제(틱 없이 바로)
    spawn.reform({}, IN_GAME)
    engine.squads[#engine.squads + 1] = squad(41, pawn, { unit = "retinue_tier1", units = { 1 }, recruits = { 1 } })
    spawn.tick(true)
    T.eq(removed[2], 40, "second reform removes its ghost"); T.eq(spawn.pendingRemovals(), 0, "done")
  end,
  tick_clears_pending_when_leaving_game = function()
    reformSetup(function(p) return { squad(40, p, { unit = "retinue_tier1" }) } end)
    spawn.reform({}, IN_GAME)
    spawn.tick(false)
    T.eq(spawn.pendingRemovals(), 0, "cleared")
  end,
  reform_status_counts_ghosts_by_unit = function()
    reformSetup(function(p)
      return { squad(40, p, { unit = "retinue_tier1" }), squad(41, p, { unit = "retinue_tier1" }), squad(42, p, { unit = "militia" }) }
    end)
    local s = spawn.status(true)
    T.eq(s.disbanded, 3, "total"); T.eq(s.byUnit.retinue_tier1, 2, "by unit"); T.eq(s.byUnit.militia, 1, "by unit 2")
    T.eq(spawn.status(false).disbanded, 0, "not in game")
  end,
})
