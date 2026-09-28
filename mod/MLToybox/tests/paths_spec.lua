local T = require("t")
local paths = require("core.paths")

T.run({
  scripts_dir_from_source = function()
    T.eq(paths.scriptsDirFromSource("@E:\\G\\Mods\\MLToybox\\Scripts\\main.lua"), "E:\\G\\Mods\\MLToybox\\Scripts", "backslash")
    T.eq(paths.scriptsDirFromSource("@E:/G/Mods/MLToybox/Scripts/main.lua"), "E:/G/Mods/MLToybox/Scripts", "slash")
  end,
  parent_dir = function()
    T.eq(paths.parentDir("E:\\G\\Mods\\MLToybox\\Scripts"), "E:\\G\\Mods\\MLToybox", "parent")
    T.eq(paths.parentDir("nodir"), nil, "no separator")
  end,
})
