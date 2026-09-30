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
  anchor_prefers_manor_then_first_constructed_building = function()
    local function actor(x, constructed)
      local a = F.object({ IsConstructed = function() return constructed end })
      a.K2_GetActorLocation = function() return { X = x, Y = 0, Z = 0 } end
      return a
    end
    local pawn = F.object()
    local region = F.object({ ownerPawn = pawn, manor = actor(1, true) })
    region.GetBuildings = function() return { F.wrap(actor(2, false)), F.wrap(actor(3, true)) } end
    install({ MyPawnCPP_BP3_C = { pawn }, BP_Region_C = { region } })
    T.eq(game.anchorLocation().X, 1, "manor")
    region.manor = F.invalid()
    T.eq(game.anchorLocation().X, 3, "first constructed")
    region.GetBuildings = function() return {} end
    T.eq(game.anchorLocation(), nil, "none")
  end,
  anchor_and_region_list_can_target_a_region = function()
    local function actor(x)
      local a = F.object({ IsConstructed = function() return true end })
      a.K2_GetActorLocation = function() return { X = x, Y = 0, Z = 0 } end
      return a
    end
    local function named(tag, name, x)
      local r = F.object({ manor = actor(x) })
      r.regionUniqueTag = { ToString = function() return tag end }
      r.regionName = { ToString = function() return name end }
      r.GetBuildings = function() return {} end
      return r
    end
    local pawn = F.object()
    local gold, nus = named("gold", "Mandlach", 10), named("nus", "Haderwand", 20)
    gold.ownerPawn = pawn; nus.ownerPawn = pawn
    install({ MyPawnCPP_BP3_C = { pawn }, BP_Region_C = { gold, nus } })
    T.eq(game.anchorLocation().X, 10, "no key -> first region")
    T.eq(game.anchorLocation("nus").X, 20, "selected region")
    T.eq(game.anchorLocation("zzz"), nil, "unknown region")
    local list = game.regionList()
    T.eq(#list, 2, "two regions"); T.eq(list[2].key, "nus", "key"); T.eq(list[2].name, "Haderwand", "name")
  end,
  unwrap_handles_wrapped_and_plain = function()    T.eq(game.unwrap(F.wrap(5)), 5, "wrapped"); T.eq(game.unwrap(7), 7, "plain")
  end,
})
