local T = require("t")
local F = require("fakes")
local game = require("core.game")

local function install(byClass)
  game.find.first = function(c) local l = byClass[c]; return l and l[1] end
  game.find.all = function(c) return byClass[c] or {} end
end

T.run({
  player_regions_filters_by_owner = function()
    local pawn, other = F.object(), F.object()
    local mine, theirs, ownerless = F.object({ ownerPawn = pawn }), F.object({ ownerPawn = other }), F.object({ ownerPawn = F.invalid() })
    install({ MyPawnCPP_BP3_C = { pawn }, BP_Region_C = { theirs, mine, ownerless } })
    local rs = game.playerRegions()
    T.eq(#rs, 1, "one region"); T.eq(rs[1], mine, "mine")
  end,
  player_regions_empty_without_pawn = function()
    install({ BP_Region_C = { F.object({ ownerPawn = F.object() }) } })
    T.eq(#game.playerRegions(), 0, "no pawn -> none")
  end,
  invalid_objects_are_nil = function()
    install({ MyPawnCPP_BP3_C = { F.invalid() }, MLCheatManager_C = {}, MyRTSMultiEngineCPP_BP_C = { F.object() } })
    T.eq(game.pawn(), nil, "invalid pawn"); T.eq(game.cheat(), nil, "no cheat"); T.truthy(game.engine(), "engine")
  end,
  treasury_reads_first_valid_hud = function()
    local dead = F.object({ TreasuryNumeric = F.invalid() })
    local live = F.object({ TreasuryNumeric = F.object({ CurrentNumericValue = 66116.0 }) })
    install({ W_HUD_LordPanel_V2_C = { dead, live } })
    T.eq(game.treasury(), 66116.0, "treasury")
    install({})
    T.eq(game.treasury(), nil, "no hud")
  end,
  unwrap_handles_wrapped_and_plain = function()
    T.eq(game.unwrap(F.wrap(5)), 5, "wrapped"); T.eq(game.unwrap(7), 7, "plain")
  end,
})
