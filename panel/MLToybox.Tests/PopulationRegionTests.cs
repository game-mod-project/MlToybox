using System.Text.Json;
using MLToybox.Panel.Core;
using Xunit;

namespace MLToybox.Tests;

public class PopulationRegionTests
{
    private static BridgeClient NewClient()
    {
        var dir = Path.Combine(Directory.CreateTempSubdirectory("mltb-pop-").FullName, "bridge");
        Directory.CreateDirectory(dir);
        return new BridgeClient(dir, () => DateTimeOffset.FromUnixTimeSeconds(1_800_000_000));
    }

    [Fact]
    public void AddFamilies_WithRegion_SerializesRegionKey_WithoutRegion_OmitsIt()
    {
        var c = NewClient();
        var doc = new ControlDocument();
        doc.Commands.Add(ControlCommand.AddFamilies(3, DateTimeOffset.FromUnixTimeSeconds(1_800_000_000), "sel"));
        doc.Commands.Add(ControlCommand.AddFamilies(2, DateTimeOffset.FromUnixTimeSeconds(1_800_000_000)));
        c.SaveControl(doc);
        using var json = JsonDocument.Parse(File.ReadAllText(c.ControlPath));
        var cmds = json.RootElement.GetProperty("commands");
        Assert.Equal("sel", cmds[0].GetProperty("region").GetString());
        Assert.Equal(3, cmds[0].GetProperty("count").GetInt32());
        Assert.False(cmds[1].TryGetProperty("region", out _));
    }

    [Fact]
    public void Control_RegionTargetsRoundTrip()
    {
        var c = NewClient();
        var doc = new ControlDocument();
        doc.Features.Population.TargetFamilies = 12;
        doc.Features.Population.RegionTargets["sel"] = 30;
        c.SaveControl(doc);
        var back = c.LoadControl();
        Assert.Equal(12, back.Features.Population.TargetFamilies);
        Assert.Equal(30, back.Features.Population.RegionTargets["sel"]);
    }

    [Fact]
    public void Status_ParsesPerRegionPopulation()
    {
        var c = NewClient();
        File.WriteAllText(c.StatusPath,
            """{"version":1,"heartbeat":1800000000,"inGame":true,"population":{"families":14,"population":42,"homeless":0,"freeSlots":3,"unassigned":2,"natural":1,"multiplied":2,"regions":[{"key":"hof","name":"Klainau","families":10,"population":30,"homeless":0,"freeSlots":3,"unassigned":2}]}}""");
        var p = c.ReadStatus()!.Population!;
        Assert.Equal(2, p.Unassigned);
        Assert.Single(p.Regions!);
        Assert.Equal("Klainau", p.Regions![0].Name);
        Assert.Equal(3, p.Regions[0].FreeSlots);
    }
}
