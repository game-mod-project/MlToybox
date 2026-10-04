local T = require("t")

-- UE4SS 전역이 필요해 실행할 수 없는 진입점은 구문(컴파일)만 검사한다.
local function scriptsPath(rel)
  return SCRIPTS_DIR .. "/" .. rel
end

T.run({
  feature_modules_load_and_follow_contract = function()
    local cfg = dofile(SCRIPTS_DIR .. "/config.lua")
    T.eq(#cfg.featureModules, 10, "ten features")
    for _, name in ipairs(cfg.featureModules) do
      local mod = require("features." .. name)
      T.eq(mod.name, name, "name matches module " .. name)
    end
  end,
  lab_main_compiles = function()
    local fn, err = loadfile(SCRIPTS_DIR .. "/../../MLToyboxLab/Scripts/main.lua")
    T.truthy(fn, "lab main.lua: " .. tostring(err))
  end,
  main_compiles = function()
    local fn, err = loadfile(scriptsPath("main.lua"))
    T.truthy(fn, "main.lua: " .. tostring(err))
  end,
  config_loads_with_menu_pattern = function()
    local cfg = dofile(scriptsPath("config.lua"))
    T.eq(cfg.menuGameModePattern, "MenuGameMode_ML_C", "menu pattern")
    T.eq(type(cfg.featureModules), "table", "featureModules")
    T.eq(cfg.overlay, true, "overlay on by default")
  end,
})
