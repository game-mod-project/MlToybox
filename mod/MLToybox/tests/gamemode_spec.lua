local T = require("t")
local gamemode = require("core.gamemode")

local MENU = "MenuGameMode_ML_C /Game/NotStronghold/Maps/MainMenu.MainMenu:PersistentLevel.MenuGameMode_ML_C_2147482357"
local GAME = "SomeGameMode_C /Game/NotStronghold/Maps/Map1.Map1:PersistentLevel.SomeGameMode_C_1"

T.run({
  menu_pattern_excludes_main_menu = function()
    local cfg = { menuGameModePattern = "MenuGameMode_ML_C" }
    T.eq(gamemode.isGameplay(MENU, cfg), false, "menu")
    T.eq(gamemode.isGameplay(GAME, cfg), true, "game")
  end,
  gameplay_pattern_requires_match = function()
    local cfg = { gameStateClassPattern = "SomeGameMode_C" }
    T.eq(gamemode.isGameplay(GAME, cfg), true, "match")
    T.eq(gamemode.isGameplay(MENU, cfg), false, "no match")
  end,
  menu_pattern_wins_over_gameplay_pattern = function()
    local cfg = { menuGameModePattern = "MenuGameMode", gameStateClassPattern = "GameMode" }
    T.eq(gamemode.isGameplay(MENU, cfg), false, "menu excluded first")
  end,
  no_patterns_means_everything_is_gameplay = function()
    T.eq(gamemode.isGameplay(MENU, {}), true, "no config")
  end,
  patterns_are_plain_text = function()
    T.eq(gamemode.isGameplay("A.B_C", { menuGameModePattern = "A.B" }), false, "dot literal")
    T.eq(gamemode.isGameplay("AxB_C", { menuGameModePattern = "A.B" }), true, "dot not wildcard")
  end,
})
