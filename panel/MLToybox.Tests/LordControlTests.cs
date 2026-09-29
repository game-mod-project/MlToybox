using System.Text.Json;
using MLToybox.Panel.Core;
using Xunit;

namespace MLToybox.Tests;

public class LordControlTests
{
    private static BridgeClient NewClient()
    {
        var dir = Path.Combine(Directory.CreateTempSubdirectory("mltb-lord-").FullName, "bridge");
        Directory.CreateDirectory(dir);
        return new BridgeClient(dir, () => DateTimeOffset.FromUnixTimeSeconds(1_800_000_000));
    }

    [Fact]
    public void LoadControl_MovesOldTreasuryAndInfluenceTargetsToLord()
    {
        var c = NewClient();
        File.WriteAllText(c.ControlPath,
            """{"version":1,"seq":3,"features":{"resources":{"enabled":true,"intervalSec":2,"targets":{"Timber":500,"Treasury":70000,"Influence":600}}}}""");
        var doc = c.LoadControl();
        Assert.False(doc.Features.Resources.Targets.ContainsKey("Treasury"));
        Assert.False(doc.Features.Resources.Targets.ContainsKey("Influence"));
        Assert.Equal(500, doc.Features.Resources.Targets["Timber"]);
        Assert.True(doc.Features.Lord.Enabled);
        Assert.Equal(70000, doc.Features.Lord.Treasury);
        Assert.Equal(600, doc.Features.Lord.Influence);
        Assert.Null(doc.Features.Lord.KingsFavour);
    }

    [Fact]
    public void LoadControl_ExistingLordSettingsWin()
    {
        var c = NewClient();
        File.WriteAllText(c.ControlPath,
            """{"version":1,"seq":3,"features":{"resources":{"enabled":true,"targets":{"Treasury":1}},"lord":{"enabled":false,"treasury":9000,"kingsFavour":5}}}""");
        var doc = c.LoadControl();
        Assert.False(doc.Features.Lord.Enabled);
        Assert.Equal(9000, doc.Features.Lord.Treasury);
        Assert.Equal(5, doc.Features.Lord.KingsFavour);
        Assert.False(doc.Features.Resources.Targets.ContainsKey("Treasury"));
    }

    [Fact]
    public void SaveControl_WritesLordSectionAndOmitsUnsetValues()
    {
        var c = NewClient();
        var doc = new ControlDocument();
        doc.Features.Lord.Enabled = true;
        doc.Features.Lord.KingsFavour = 10;
        c.SaveControl(doc);
        using var json = JsonDocument.Parse(File.ReadAllText(c.ControlPath));
        var lord = json.RootElement.GetProperty("features").GetProperty("lord");
        Assert.True(lord.GetProperty("enabled").GetBoolean());
        Assert.Equal(10, lord.GetProperty("kingsFavour").GetInt32());
        Assert.False(lord.TryGetProperty("treasury", out _));   // 관리 안 함 = 키 없음
    }

    [Fact]
    public void ReadStatus_ParsesLordValues()
    {
        var c = NewClient();
        File.WriteAllText(c.StatusPath, """{"version":1,"heartbeat":1800000000,"inGame":true,"lord":{"treasury":1234.0,"influence":600,"kingsFavour":3}}""");
        var s = c.ReadStatus()!;
        Assert.Equal(1234, s.Lord!.Treasury);
        Assert.Equal(3, s.Lord.KingsFavour);
    }
}
