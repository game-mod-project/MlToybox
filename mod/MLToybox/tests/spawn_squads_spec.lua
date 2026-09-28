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
})
