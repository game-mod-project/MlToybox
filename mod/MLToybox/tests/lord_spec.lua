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
  game.refreshLordHud = function() end
  lord.forget()   -- 앞 경우에서 모드가 바꿔 둔 국고를 기억하고 있지 않게 한다
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
  set_influence_or_favour_refreshes_lord_hud = function()
    -- 영향력·총애는 값만 바꾸면 화면(W_HUD_LordPanel_V2)이 갱신되지 않는다(실측). 설정 후 updatePlayerStats 를 부른다
    local p = pawn(20410, 50000)
    world({ pawn = p, treasury = 1.0 })
    local refreshed = 0
    game.refreshLordHud = function() refreshed = refreshed + 1 end
    lord.set({ key = "influence", value = 33333 }, IN_GAME)
    T.eq(refreshed, 1, "after influence")
    lord.set({ key = "kingsFavour", value = 10 }, IN_GAME)
    T.eq(refreshed, 2, "after favour")
    local st = {}
    lord.tick(st, { influence = 50000 })
    T.eq(refreshed, 3, "after tick raised influence")
    lord.tick(st, { influence = 50000 })
    T.eq(refreshed, 3, "no refresh when nothing changed")
  end,
  read_reports_current_values_without_managing = function()
    -- 기능이 꺼져 있어도 패널 '현재' 값은 갱신돼야 한다(main 이 매 루프 read 로 보고)
    local p = pawn(5129, 5000)
    local cheat = world({ pawn = p, treasury = 4955.0 })
    local v = lord.read()
    T.eq(v.influence, 5129, "influence"); T.eq(v.kingsFavour, 5000, "favour"); T.eq(v.treasury, 4955, "treasury")
    T.eq(#cheat.changes, 0, "no side effects"); T.eq(#p.favourChanges, 0, "no side effects 2")
    world({})
    T.eq(next(lord.read()), nil, "nothing readable -> empty")
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
  -- '지금 설정'을 연달아 눌러도 그 값이 된다. HUD 숫자는 몇 초 늦게 따라오므로, 모드가 방금 바꾼 것을 기억해 두고 계산한다
  set_treasury_again_before_the_hud_catches_up_lands_on_the_value = function()
    lord.clock = function() return 7000 end
    local cheat = world({ treasury = 0.0 })
    lord.set({ key = "treasury", value = 1000 }, IN_GAME)
    lord.set({ key = "treasury", value = 1000 }, IN_GAME)        -- HUD 는 아직 0
    T.eq(#cheat.changes, 1, "the second command has nothing to change"); T.eq(cheat.changes[1], 1000, "one delta")
    lord.set({ key = "treasury", value = 400 }, IN_GAME)         -- HUD 는 여전히 0 이지만 실제는 1000
    T.eq(cheat.changes[2], -600, "lowered from what the treasury really is")
    T.eq(lord.read().treasury, 400, "the current value is reported the same way")
  end,
  set_treasury_trusts_the_hud_once_it_caught_up_or_the_wait_ran_out = function()
    local now = 8000
    lord.clock = function() return now end
    local opts = { treasury = 0.0 }
    local cheat = world(opts)
    lord.set({ key = "treasury", value = 1000 }, IN_GAME)
    opts.treasury = 1000.0                                       -- HUD 가 따라왔다
    T.eq(lord.read().treasury, 1000, "caught up")
    opts.treasury = 900.0                                        -- 게임이 100 을 썼다
    lord.set({ key = "treasury", value = 1000 }, IN_GAME)
    T.eq(cheat.changes[2], 100, "the HUD is the truth again")
    now = now + lord.TREASURY_PENDING_TIMEOUT + 1                -- HUD 가 끝내 따라오지 않았다(900 그대로)
    lord.set({ key = "treasury", value = 1000 }, IN_GAME)
    T.eq(cheat.changes[3], 100, "after the wait the HUD reading is used")
  end,
  set_treasury_right_after_the_keeper_topped_up_counts_the_top_up = function()
    lord.clock = function() return 9000 end
    local cheat = world({ treasury = 300.0 })
    local st = {}
    lord.tick(st, { treasury = 1000 }); lord.tick(st, { treasury = 1000 })   -- 700 을 채웠고 HUD 는 아직 300
    lord.set({ key = "treasury", value = 500 }, IN_GAME)
    T.eq(cheat.changes[2], -500, "from 1000, not from the stale 300")
    lord.tick(st, { treasury = 1000 })
    T.eq(#cheat.changes, 2, "the keeper waits for the HUD too instead of topping up from the stale reading")
  end,
  -- 맵을 떠나면 잊는다(main.lua 가 부른다). 다음 맵의 국고는 다른 값이다
  forget_drops_what_the_mod_remembered_about_the_treasury = function()
    lord.clock = function() return 9500 end
    local cheat = world({ treasury = 0.0 })
    lord.set({ key = "treasury", value = 1000 }, IN_GAME)
    lord.forget()
    lord.set({ key = "treasury", value = 1000 }, IN_GAME)
    T.eq(cheat.changes[2], 1000, "computed from the HUD of the new map")
  end,
  influence_raised_to_target_and_reported = function()
    local p = pawn(3, 0)
    world({ pawn = p })
    local st = {}
    lord.tick(st, { influence = 50 })
    T.eq(p.influence, 50, "raised"); T.eq(lord.read().influence, 50, "reported")
  end,
  kings_favour_raised_with_delta_call = function()
    local p = pawn(0, 2)
    world({ pawn = p })
    local st = {}
    lord.tick(st, { kingsFavour = 10 })
    T.eq(p.favourChanges[1], 8, "delta via changeKingsFavour"); T.eq(lord.read().kingsFavour, 10, "reported")
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
    local v = lord.read()
    T.eq(v.influence, 3, "influence reported"); T.eq(v.kingsFavour, 1, "favour reported"); T.eq(v.treasury, 50, "treasury reported")
  end,
  treasury_topup_uses_delta_after_stable_reading = function()
    lord.clock = function() return 1000 end
    local cheat = world({ treasury = 300.0 })
    local st = {}
    lord.tick(st, { treasury = 1000 })
    T.eq(#cheat.changes, 0, "first reading only observed")
    lord.tick(st, { treasury = 1000 })
    T.eq(cheat.changes[1], 700, "delta once reading is stable"); T.eq(lord.read().treasury, 1000, "reported as topped")
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
    local v = lord.read()
    T.eq(v.treasury, nil, "no hud -> not reported"); T.eq(v.influence, nil, "no pawn")
  end,
})
