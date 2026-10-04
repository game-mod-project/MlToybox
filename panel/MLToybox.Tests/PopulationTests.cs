using System.Text.Json;
using MLToybox.Panel.Core;
using Xunit;

namespace MLToybox.Tests;

public class PopulationTests
{
    private static readonly DateTimeOffset Now = DateTimeOffset.FromUnixTimeSeconds(1_800_000_000);

    private static BridgeClient NewClient()
    {
        var dir = Path.Combine(Directory.CreateTempSubdirectory("mltb-pop-").FullName, "bridge");
        return new BridgeClient(dir, () => Now);
    }

    [Fact]
    public void PopulationControl_SerializesWithDefaults()
    {
        var c = NewClient();
        var doc = new ControlDocument();
        doc.Features.Population.Enabled = true;
        c.SaveControl(doc);
        using var json = JsonDocument.Parse(File.ReadAllText(c.ControlPath));
        var p = json.RootElement.GetProperty("features").GetProperty("population");
        Assert.True(p.GetProperty("enabled").GetBoolean());
        Assert.Equal(2, p.GetProperty("multiplier").GetInt32());
        Assert.Equal(0, p.GetProperty("monthlyFamilies").GetInt32());   // 월 자연 이민 가족 수: 0 = 게임 그대로
        Assert.Equal(0, p.GetProperty("targetFamilies").GetInt32());
    }

    // 오버레이나 손으로 넣은 월 가족 수를 패널이 읽고, 저장해도 남긴다
    [Fact]
    public void PopulationControl_KeepsMonthlyFamilies()
    {
        var c = NewClient();
        Directory.CreateDirectory(c.Dir);
        File.WriteAllText(c.ControlPath, """{"version":1,"seq":3,"features":{"population":{"enabled":true,"multiplier":4,"monthlyFamilies":6}}}""");
        var doc = c.LoadControl();
        c.SaveControl(doc);
        using var json = JsonDocument.Parse(File.ReadAllText(c.ControlPath));
        var p = json.RootElement.GetProperty("features").GetProperty("population");
        Assert.Equal(6, p.GetProperty("monthlyFamilies").GetInt32());
        Assert.Equal(4, p.GetProperty("multiplier").GetInt32());
    }

    [Fact]
    public void AddFamiliesCommand_Serializes()
    {
        var cmd = ControlCommand.AddFamilies(5, Now);
        Assert.Equal("addFamilies", cmd.Type);
        Assert.Equal(5, cmd.Count);
        Assert.Equal(1_800_000_000, cmd.IssuedAt);
        Assert.False(string.IsNullOrEmpty(cmd.Id));
    }

    [Fact]
    public void ReadStatus_ParsesPopulationAndAddedCount()
    {
        var c = NewClient();
        Directory.CreateDirectory(c.Dir);
        File.WriteAllText(c.StatusPath,
            """{"version":1,"heartbeat":1800000000,"inGame":true,"population":{"families":30,"population":95,"homeless":0,"freeSlots":7},"commands":{"x":{"ok":true,"requested":3,"added":2}}}""");
        var s = c.ReadStatus()!;
        Assert.Equal(30, s.Population!.Families);
        Assert.Equal(95, s.Population.Population);
        Assert.Equal(7, s.Population.FreeSlots);
        Assert.Equal(2, s.Commands!["x"].Added);
        Assert.Equal(3, s.Commands["x"].Requested);
    }
}
