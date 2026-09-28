using MLToybox.Panel.Core;
using Xunit;

namespace MLToybox.Tests;

public class GameLocatorTests
{
    private const string Vdf = """
        "libraryfolders"
        {
            "0"
            {
                "path"		"C:\\Program Files (x86)\\Steam"
                "apps"
                {
                    "228980"		"364999368"
                }
            }
            "1"
            {
                "path"		"E:\\SteamLibrary"
                "apps"
                {
                    "1363080"		"15360715472"
                }
            }
        }
        """;

    [Fact]
    public void FindLibraryWithApp_ReturnsUnescapedPath() =>
        Assert.Equal(@"E:\SteamLibrary", GameLocator.FindLibraryWithApp(Vdf));

    [Fact]
    public void FindLibraryWithApp_NotInstalled_ReturnsNull() =>
        Assert.Null(GameLocator.FindLibraryWithApp(Vdf.Replace("1363080", "999")));

    [Fact]
    public void FindLibraryWithApp_IgnoresSizeValueMatchingAppId() =>
        Assert.Null(GameLocator.FindLibraryWithApp(Vdf.Replace("\"1363080\"\t\t\"15360715472\"", "\"5\"\t\t\"1363080\"")));

    [Fact]
    public void BridgeDir_IsUnderUe4ssMods() =>
        Assert.Equal(@"G:\ML\ManorLords\Binaries\Win64\ue4ss\Mods\MLToybox\bridge", GameLocator.BridgeDir(@"G:\ML"));

    [Fact]
    public void Detect_RealMachine_FindsInstall()
    {
        var dir = GameLocator.Detect(GameLocator.SteamPathFromRegistry());
        Assert.NotNull(dir);
        Assert.True(GameLocator.LooksLikeGameDir(dir!));
    }
}
