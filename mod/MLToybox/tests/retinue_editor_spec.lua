local T = require("t")
local F = require("fakes")
local game = require("core.game")
local retinue = require("features.retinue_editor")

local function fname(s) return { ToString = function() return s end } end

local function squad(id, owner, opts)
  opts = opts or {}
  local units = {}
  for i = 1, opts.count or 3 do units[i] = F.object({}) end
  return { ID = id, ownerPawn = owner, squadType = opts.type or 0, companyID = opts.company or -1, unitType = fname(opts.unit or "retinue_tier1"), unitArr = F.array(units) }
end

-- 가짜 게임: 내 영지 두 개(gold, nus), 편집 화면 위젯, 분대 목록
local function setup(opts)
  opts = opts or {}
  local pawn, other = F.object({}), F.object({})
  local regions = {
    gold = F.object({ manor = F.object({}), retinueSquadID = 33 }),
    nus = F.object({ manor = F.object({}), retinueSquadID = 22 }),
  }
  local squads = {
    squad(22, pawn, { type = 3 }),                       -- 진짜 수행원 분대
    squad(63, pawn, { count = 36 }),                     -- 모드가 만든 수행원 분대
    squad(64, pawn, { type = 2, company = 5, unit = "retinue_tier3" }),   -- 커스텀 용병으로 고용한 수행원 분대
    squad(70, pawn, { unit = "militiaFoot" }),           -- 수행원 병종이 아님
    squad(80, other, {}),                                -- 남의 분대
  }
  local engine = F.object({ squads = F.array(squads) })
  local editor = { visible = false, opens = {} }
  editor.widget = F.object({
    IsVisible = function() return editor.visible end,
    Open = function(_, manor)
      if opts.openFails then error("open exploded") end
      editor.opens[#editor.opens + 1] = manor
      editor.visible = true
    end,
  })
  game.pawn = function() return pawn end
  game.engine = function() return engine end
  game.playerRegions = function() return { regions.gold, regions.nus } end
  game.regionByKey = function(key) return regions[key] end
  game.retinueEditor = function() if opts.noEditor then return nil end return editor.widget end
  retinue.reset()
  return { pawn = pawn, regions = regions, editor = editor, engine = engine }
end

local IN_GAME = { inGame = true }

T.run({
  open_points_the_region_at_the_squad_and_opens_the_editor = function()
    local s = setup()
    local r = retinue.open({ value = 63, region = "nus" }, IN_GAME)
    T.eq(r.ok, true, "ok: " .. tostring(r.error)); T.eq(r.squad, 63, "squad")
    T.eq(s.regions.nus.retinueSquadID, 63, "region points at the mod squad"); T.eq(s.regions.gold.retinueSquadID, 33, "other region untouched")
    T.eq(#s.editor.opens, 1, "opened once"); T.eq(s.editor.opens[1], s.regions.nus.manor, "with that region's manor")
    T.eq(retinue.status(true).editing, 63, "reported as editing")
  end,
  open_uses_the_first_region_when_none_is_given = function()
    local s = setup()
    T.eq(retinue.open({ value = 64 }, IN_GAME).ok, true, "ok")
    T.eq(s.regions.gold.retinueSquadID, 64, "first region redirected"); T.eq(s.editor.opens[1], s.regions.gold.manor, "first manor")
  end,
  open_rejects_squads_that_are_not_mod_made_retinue = function()
    local s = setup()
    for _, case in ipairs({ { 22, "real retinue" }, { 70, "not a retinue unit" }, { 80, "someone else's" }, { 999, "missing" }, { "x", "not a number" } }) do
      local r = retinue.open({ value = case[1], region = "nus" }, IN_GAME)
      T.eq(r.ok, false, case[2] .. " rejected"); T.eq(s.regions.nus.retinueSquadID, 22, case[2] .. ": region untouched")
    end
    T.eq(#s.editor.opens, 0, "never opened")
  end,
  open_needs_the_game_and_a_manor = function()
    local s = setup()
    T.eq(retinue.open({ value = 63 }, { inGame = false }).ok, false, "not in game")
    s.regions.nus.manor = F.invalid()
    local r = retinue.open({ value = 63, region = "nus" }, IN_GAME)
    T.eq(r.ok, false, "no manor"); T.truthy(r.error:find("manor", 1, true), "reason: " .. tostring(r.error))
    T.eq(retinue.open({ value = 63, region = "zzz" }, IN_GAME).ok, false, "unknown region")
    setup({ noEditor = true })
    T.eq(retinue.open({ value = 63 }, IN_GAME).ok, false, "no editor widget")
  end,
  open_refuses_while_the_editor_is_open = function()
    local s = setup()
    T.eq(retinue.open({ value = 63, region = "nus" }, IN_GAME).ok, true, "first")
    local r = retinue.open({ value = 64, region = "nus" }, IN_GAME)
    T.eq(r.ok, false, "second refused"); T.eq(s.regions.nus.retinueSquadID, 63, "still the first squad")
    s.editor.visible = true
    retinue.reset()
    T.eq(retinue.open({ value = 64, region = "nus" }, IN_GAME).ok, false, "the game's own editor is open")
  end,
  open_restores_the_region_when_the_editor_throws = function()
    local s = setup({ openFails = true })
    local r = retinue.open({ value = 63, region = "nus" }, IN_GAME)
    T.eq(r.ok, false, "failed"); T.truthy(r.error:find("open exploded", 1, true), "reason kept"); T.eq(s.regions.nus.retinueSquadID, 22, "restored")
    T.eq(retinue.status(true).editing, nil, "nothing being edited")
  end,
  tick_restores_the_region_once_the_editor_closes = function()
    local s = setup()
    retinue.open({ value = 63, region = "nus" }, IN_GAME)
    retinue.tick(true)
    T.eq(s.regions.nus.retinueSquadID, 63, "still open -> still redirected")
    s.editor.visible = false
    retinue.tick(true)
    T.eq(s.regions.nus.retinueSquadID, 22, "closed -> restored"); T.eq(retinue.status(true).editing, nil, "session over")
    retinue.tick(true)
    T.eq(s.regions.nus.retinueSquadID, 22, "idempotent")
  end,
  tick_restores_when_leaving_the_map = function()
    local s = setup()
    retinue.open({ value = 63, region = "nus" }, IN_GAME)
    retinue.tick(false)
    T.eq(s.regions.nus.retinueSquadID, 22, "restored on the way out"); T.eq(retinue.status(false).editing, nil, "cleared")
  end,
  status_lists_mod_made_retinue_squads = function()
    setup()
    local st = retinue.status(true)
    T.eq(#st.squads, 2, "two eligible squads")
    T.eq(st.squads[1].id, 63, "id"); T.eq(st.squads[1].unit, "retinue_tier1", "unit"); T.eq(st.squads[1].count, 36, "count"); T.eq(st.squads[1].kind, "spawned", "kind")
    T.eq(st.squads[2].id, 64, "id 2"); T.eq(st.squads[2].kind, "mercenary", "hired custom company"); T.eq(st.squads[2].unit, "retinue_tier3", "unit 2")
    T.eq(st.editing, nil, "not editing")
    T.eq(#retinue.status(false).squads, 0, "outside the map")
  end,
  status_survives_missing_game_objects = function()
    setup()
    game.engine = function() return nil end
    local st = retinue.status(true)
    T.eq(#st.squads, 0, "empty"); T.eq(st.editing, nil, "nothing")
    retinue.tick(true)
  end,
})
