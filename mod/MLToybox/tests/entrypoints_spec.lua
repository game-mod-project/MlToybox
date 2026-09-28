local T = require("t")

-- UE4SS 전역이 필요해 실행할 수 없는 진입점은 구문(컴파일)만 검사한다.
local function scriptsPath(rel)
  return SCRIPTS_DIR .. "/" .. rel
end

T.run({
  main_compiles = function()
    local fn, err = loadfile(scriptsPath("main.lua"))
    T.truthy(fn, "main.lua: " .. tostring(err))
  end,
  config_loads_with_menu_pattern = function()
    local cfg = dofile(scriptsPath("config.lua"))
    T.eq(cfg.menuGameModePattern, "MenuGameMode_ML_C", "menu pattern")
    T.eq(type(cfg.featureModules), "table", "featureModules")
  end,
})
