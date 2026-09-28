using System.Text.Json;
using MLToybox.Panel.Core;
using Xunit;

namespace MLToybox.Tests;

public class CommandTests
{
    private static readonly DateTimeOffset Now = DateTimeOffset.FromUnixTimeSeconds(1_800_000_000);

    [Fact]
    public void SpawnCommand_SerializesProtocolFields()
    {
        var dir = Path.Combine(Directory.CreateTempSubdirectory("mltb-cmd-").FullName, "bridge");
        var c = new BridgeClient(dir, () => Now);
        var doc = new ControlDocument();
        doc.Commands.Add(ControlCommand.SpawnSquads("spearMilitia", 3, Now));
        c.SaveControl(doc);
        using var json = JsonDocument.Parse(File.ReadAllText(c.ControlPath));
        var cmd = json.RootElement.GetProperty("commands")[0];
        Assert.Equal("spawnSquads", cmd.GetProperty("type").GetString());
        Assert.Equal("spearMilitia", cmd.GetProperty("unit").GetString());
        Assert.Equal(3, cmd.GetProperty("count").GetInt32());
        Assert.Equal(1_800_000_000, cmd.GetProperty("issuedAt").GetInt64());
        Assert.False(string.IsNullOrWhiteSpace(cmd.GetProperty("id").GetString()));
    }

    [Fact]
    public void SpawnCommand_IdsAreUnique() =>
        Assert.NotEqual(ControlCommand.SpawnSquads("a", 1, Now).Id, ControlCommand.SpawnSquads("a", 1, Now).Id);

    [Fact]
    public void ReadStatus_ParsesCommandResults()
    {
        var dir = Path.Combine(Directory.CreateTempSubdirectory("mltb-cmd-").FullName, "bridge");
        Directory.CreateDirectory(dir);
        var c = new BridgeClient(dir, () => Now);
        File.WriteAllText(c.StatusPath,
            """{"version":1,"heartbeat":1800000000,"inGame":true,"commands":{"x1":{"ok":true,"squads":[43,44]},"x2":{"ok":false,"error":"unknown unit: dragon"}}}""");
        var s = c.ReadStatus()!;
        Assert.True(s.Commands!["x1"].Ok);
        Assert.Equal(new[] { 43, 44 }, s.Commands["x1"].Squads);
        Assert.Equal("unknown unit: dragon", s.Commands["x2"].Error);
    }

    [Fact]
    public void UnitCatalog_ListsPlayableUnitsOnly()
    {
        var ids = UnitCatalog.Units.Select(u => u.Id).ToList();
        Assert.Equal(13, ids.Count);
        Assert.Contains("spearMilitia", ids);
        Assert.Contains("retinue_tier3", ids);
        Assert.Contains("mercenary_crossbowmen", ids);
        Assert.DoesNotContain(ids, id => id.StartsWith('-') || id.Contains("test", StringComparison.OrdinalIgnoreCase) || id.EndsWith("_old"));
        Assert.All(UnitCatalog.Units, u => Assert.False(string.IsNullOrWhiteSpace(u.Label)));
    }
}
