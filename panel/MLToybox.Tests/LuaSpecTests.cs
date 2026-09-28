using NLua;
using NLua.Exceptions;
using Xunit;

namespace MLToybox.Tests;

public class LuaSpecTests
{
    public static IEnumerable<object[]> Specs() =>
        Directory.GetFiles(RepoPaths.LuaTests, "*_spec.lua")
            .Select(p => new object[] { Path.GetFileName(p) });

    [Theory]
    [MemberData(nameof(Specs))]
    public void Spec(string file)
    {
        var tmp = Directory.CreateTempSubdirectory("mltb-lua-").FullName;
        using var lua = new Lua();
        lua.State.Encoding = System.Text.Encoding.UTF8;
        lua["TEST_TMP"] = tmp;
        lua["SCRIPTS_DIR"] = RepoPaths.Scripts;
        lua.DoString($"package.path = [[{RepoPaths.Scripts}\\?.lua;{RepoPaths.LuaTests}\\?.lua;]] .. package.path");
        lua.DoString("require('core.log').sink = function() end", "silence");
        try
        {
            lua.DoFile(Path.Combine(RepoPaths.LuaTests, file));
        }
        catch (LuaScriptException ex)
        {
            Assert.Fail($"{file}: {ex.Message}");
        }
    }
}
