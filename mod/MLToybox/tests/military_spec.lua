local T = require("t")
local F = require("fakes")
local datatable = require("core.datatable")
local game = require("core.game")
local military = require("features.military")

local function setup()
  local units = {
    militiaFoot = { requiredEquipment = F.array({ { Type = 133, amt = 1 } }), minHouseLv = 1, minMeleeTraining = 0.5, minArcheryTraining = 0.2 },
    retinue_tier1 = { requiredEquipment = F.array({}), minHouseLv = 1, minMeleeTraining = 2.0, minArcheryTraining = 0 },
  }
  local mercs = { crazy_goose = { cost = 90 } }
  datatable.find = function(p)
    if p == datatable.PATHS.unitTemplates then return F.datatable(units) end
    if p == datatable.PATHS.mercenaries then return F.datatable(mercs) end
  end
  local hired = { { 0, { cost = 50 } }, { 1, { cost = 60 } } }
  local pawn = F.object({ maxNumOfMilitiaToSpawn = 6, recruitCost = 25.0 })
  local engine = F.object({ hiredMercs = F.map(hired) })
  game.pawn = function() return pawn end
  game.engine = function() return engine end
  return units, mercs, hired, pawn
end

local ALL = { enabled = true, ignoreEquipment = true, ignorePopulation = true, zeroUpkeep = true, unlimitedSquads = true }

T.run({
  enable_patches_templates_and_merc_table = function()
    local units, mercs = setup()
    military.enable({}, ALL)
    T.eq(#units.militiaFoot.requiredEquipment, 0, "equipment"); T.eq(units.militiaFoot.minHouseLv, 0, "house")
    T.eq(units.retinue_tier1.minMeleeTraining, 0, "melee"); T.eq(units.militiaFoot.minArcheryTraining, 0, "archery")
    T.eq(mercs.crazy_goose.cost, 0, "merc table cost")
  end,
  configure_reapplies_with_new_settings = function()
    local units, mercs = setup()
    military.configure({}, { enabled = true, ignoreEquipment = true, ignorePopulation = false, zeroUpkeep = false })
    T.eq(#units.militiaFoot.requiredEquipment, 0, "equipment cleared"); T.eq(units.militiaFoot.minHouseLv, 1, "house untouched")
    T.eq(mercs.crazy_goose.cost, 90, "merc untouched")
  end,
  tick_sets_squad_cap_and_zero_upkeep = function()
    local _, _, hired, pawn = setup()
    military.tick({}, ALL)
    T.eq(pawn.maxNumOfMilitiaToSpawn, 99, "cap"); T.eq(pawn.recruitCost, 0, "recruit cost")
    T.eq(hired[1][2].cost, 0, "hired 0"); T.eq(hired[2][2].cost, 0, "hired 1")
  end,
  tick_keeps_higher_cap = function()
    local _, _, _, pawn = setup()
    pawn.maxNumOfMilitiaToSpawn = 150
    military.tick({}, ALL)
    T.eq(pawn.maxNumOfMilitiaToSpawn, 150, "not lowered")
  end,
  tick_respects_flags_off = function()
    local _, _, hired, pawn = setup()
    military.tick({}, { enabled = true })
    T.eq(pawn.maxNumOfMilitiaToSpawn, 6, "cap untouched"); T.eq(hired[1][2].cost, 50, "upkeep untouched")
  end,
  tick_without_objects_is_noop = function()
    setup()
    game.pawn = function() return nil end
    game.engine = function() return nil end
    military.tick({}, ALL)
  end,
})
