using System.Text.Json;
using MLToybox.Panel.Core;
using Xunit;

namespace MLToybox.Tests;

public class SpawnRegionTests
{
    private static readonly DateTimeOffset Now = DateTimeOffset.FromUnixTimeSeconds(1_800_000_000);

    private static BridgeClient NewClient()
    {
        var dir = Path.Combine(Directory.CreateTempSubdirectory("mltb-spawnreg-").FullName, "bridge");
        Directory.CreateDirectory(dir);
        return new BridgeClient(dir, () => Now);
    }

    [Fact]
    public void SpawnAndReform_CarryRegionOnlyWhenChosen()
    {
        var c = NewClient();
        var doc = new ControlDocument();
        doc.Commands.Add(ControlCommand.SpawnSquads("spearMilitia", 2, Now, "nus"));
        doc.Commands.Add(ControlCommand.ReformSquads(Now, "gold"));
        doc.Commands.Add(ControlCommand.SpawnSquads("spearMilitia", 1, Now));
        c.SaveControl(doc);
        using var json = JsonDocument.Parse(File.ReadAllText(c.ControlPath));
        var cmds = json.RootElement.GetProperty("commands");
        Assert.Equal("nus", cmds[0].GetProperty("region").GetString());
        Assert.Equal("gold", cmds[1].GetProperty("region").GetString());
        Assert.False(cmds[2].TryGetProperty("region", out _));
    }

    [Fact]
    public void Status_ParsesPlayerRegions()
    {
        var c = NewClient();
        File.WriteAllText(c.StatusPath,
            """{"version":1,"heartbeat":1800000000,"inGame":true,"playerRegions":[{"key":"gold","name":"Mandlach"},{"key":"nus","name":"Haderwand"}]}""");
        var s = c.ReadStatus()!;
        Assert.Equal(2, s.PlayerRegions!.Count);
        Assert.Equal("Haderwand", s.PlayerRegions[1].Name);
    }
}
