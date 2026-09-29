local T = require("t")
local F = require("fakes")
local game = require("core.game")
local lord = require("features.lord")

local function world(opts)
  local cheat = { changes = {} }
  cheat.ChangeTreasury = function(_, d) cheat.changes[#cheat.changes + 1] = d end
  game.pawn = function() return opts.pawn end
  game.cheat = function() return cheat end
  game.treasury = function() return opts.treasury end
  return cheat
end

local function pawn(influence, favour)
  local p = F.object({ influence = influence, kingsFavour = favour, favourChanges = {} })
  p.changeKingsFavour = function(self, d)
    self.favourChanges[#self.favourChanges + 1] = d
    self.kingsFavour = self.kingsFavour + d
  end
  return p
end

local IN_GAME = { inGame = true }

T.run({
  set_command_lowers_or_raises_each_value_exactly = function()
    local p = pawn(1600000, 1600000)
    local cheat = world({ pawn = p, treasury = 5000.0 })
    T.eq(lord.set({ key = "influence", value = 900000 }, IN_GAME).ok, true, "influence ok")
    T.eq(p.influence, 900000, "lowered")
    lord.set({ key = "kingsFavour", value = 800000 }, IN_GAME)
    T.eq(p.favourChanges[1], -800000, "favour delta via game function")
    lord.set({ key = "treasury", value = 2000 }, IN_GAME)
    T.eq(cheat.changes[1], -3000, "treasury delta from HUD reading")
  end,
  set_command_rejects_bad_input = function()
    world({ pawn = pawn(0, 0), treasury = 1.0 })
    T.eq(lord.set({ key = "gold", value = 1 }, IN_GAME).ok, false, "unknown key")
    T.eq(lord.set({ key = "influence", value = -1 }, IN_GAME).ok, false, "negative")
    T.eq(lord.set({ key = "influence", value = "x" }, IN_GAME).ok, false, "nan")
    T.eq(lord.set({ key = "influence", value = 5 }, { inGame = false }).ok, false, "not in game")
    world({ pawn = nil, treasury = nil })
    T.eq(lord.set({ key = "treasury", value = 5 }, IN_GAME).ok, false, "no hud")
  end,
  influence_raised_to_target_and_reported = function()
    local p = pawn(3, 0)
    world({ pawn = p })
    local st = {}
    lord.tick(st, { influence = 50 })
    T.eq(p.influence, 50, "raised"); T.eq(st.lord.influence, 50, "reported")
  end,
  kings_favour_raised_with_delta_call = function()
    local p = pawn(0, 2)
    world({ pawn = p })
    local st = {}
    lord.tick(st, { kingsFavour = 10 })
    T.eq(p.favourChanges[1], 8, "delta via changeKingsFavour"); T.eq(st.lord.kingsFavour, 10, "reported")
    lord.tick(st, { kingsFavour = 10 })
    T.eq(#p.favourChanges, 1, "no change at target")
  end,
  unset_values_are_only_reported = function()
    local p = pawn(3, 1)
    world({ pawn = p, treasury = 50.0 })
    local st = {}
    lord.tick(st, {})
    lord.tick(st, {})
    T.eq(p.influence, 3, "influence untouched"); T.eq(#p.favourChanges, 0, "favour untouched")
    T.eq(st.lord.influence, 3, "influence reported"); T.eq(st.lord.kingsFavour, 1, "favour reported"); T.eq(st.lord.treasury, 50, "treasury reported")
  end,
  treasury_topup_uses_delta_after_stable_reading = function()
    lord.clock = function() return 1000 end
    local cheat = world({ treasury = 300.0 })
    local st = {}
    lord.tick(st, { treasury = 1000 })
    T.eq(#cheat.changes, 0, "first reading only observed")
    lord.tick(st, { treasury = 1000 })
    T.eq(cheat.changes[1], 700, "delta once reading is stable"); T.eq(st.lord.treasury, 1000, "reported as topped")
  end,
  treasury_stale_hud_never_double_adds = function()
    local now = 2000
    lord.clock = function() return now end
    local opts = { treasury = 300.0 }
    local cheat = world(opts)
    local st = {}
    for _ = 1, 8 do lord.tick(st, { treasury = 1000 }); now = now + 2 end  -- HUD 가 16초 동안 300 그대로
    T.eq(#cheat.changes, 1, "single top-up while HUD is stale")
    opts.treasury = 1000.0                                               -- HUD 반영
    for _ = 1, 2 do lord.tick(st, { treasury = 1000 }); now = now + 2 end
    T.eq(#cheat.changes, 1, "no more once target reached")
    opts.treasury = 400.0                                                -- 이후 소비
    for _ = 1, 2 do lord.tick(st, { treasury = 1000 }); now = now + 2 end
    T.eq(#cheat.changes, 2, "tops up again after HUD moved and settled")
  end,
  treasury_unsettled_hud_is_ignored = function()
    lord.clock = function() return 3000 end
    local opts = { treasury = 0.0 }
    local cheat = world(opts)
    local st = {}
    for _, v in ipairs({ 0.0, 12000.0, 45000.0, 66000.0 }) do opts.treasury = v; lord.tick(st, { treasury = 70000 }) end
    T.eq(#cheat.changes, 0, "no top-up while HUD value keeps changing (count-up after load)")
  end,
  treasury_pending_expires = function()
    local now = 5000
    lord.clock = function() return now end
    local cheat = world({ treasury = 300.0 })
    local st = {}
    lord.tick(st, { treasury = 1000 }); lord.tick(st, { treasury = 1000 })
    now = now + lord.TREASURY_PENDING_TIMEOUT + 1
    lord.tick(st, { treasury = 1000 })
    T.eq(#cheat.changes, 2, "retries after pending timeout if HUD never moved")
  end,
  no_pawn_or_hud_is_noop = function()
    world({})
    local st = {}
    lord.tick(st, { treasury = 5, influence = 1, kingsFavour = 1 })
    T.eq(st.lord.treasury, nil, "no hud -> not reported"); T.eq(st.lord.influence, nil, "no pawn")
  end,
})
