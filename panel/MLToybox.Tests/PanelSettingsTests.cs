using MLToybox.Panel.Core;
using Xunit;

namespace MLToybox.Tests;

public class PanelSettingsTests
{
    [Fact]
    public void SaveThenLoad_RoundTrips()
    {
        var path = Path.Combine(Directory.CreateTempSubdirectory("mltb-set-").FullName, "sub", "panel.json");
        new PanelSettings { GameDir = @"E:\Game" }.Save(path);
        Assert.Equal(@"E:\Game", PanelSettings.Load(path).GameDir);
    }

    [Fact]
    public void Load_MissingOrCorrupt_ReturnsEmpty()
    {
        var dir = Directory.CreateTempSubdirectory("mltb-set-").FullName;
        Assert.Null(PanelSettings.Load(Path.Combine(dir, "none.json")).GameDir);
        var bad = Path.Combine(dir, "bad.json");
        File.WriteAllText(bad, "{x");
        Assert.Null(PanelSettings.Load(bad).GameDir);
    }
}
