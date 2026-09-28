using System.Text.Json;
using MLToybox.Panel.Core;
using Xunit;

namespace MLToybox.Tests;

public class BridgeClientTests
{
    private static readonly DateTimeOffset Now = DateTimeOffset.FromUnixTimeSeconds(1_800_000_000);
    private static BridgeClient NewClient(out string dir)
    {
        dir = Path.Combine(Directory.CreateTempSubdirectory("mltb-bridge-").FullName, "bridge");
        return new BridgeClient(dir, () => Now);
    }

    [Fact]
    public void SaveControl_CreatesDirAndIncrementsSeq()
    {
        var c = NewClient(out _);
        var doc = c.LoadControl();
        Assert.Equal(1, c.SaveControl(doc));
        Assert.Equal(2, c.SaveControl(c.LoadControl()));
        Assert.False(File.Exists(c.ControlPath + ".tmp"));
    }

    [Fact]
    public void SaveControl_WritesCamelCaseProtocol()
    {
        var c = NewClient(out _);
        var doc = new ControlDocument();
        doc.Features.Resources.Enabled = true;
        doc.Features.Resources.Targets["Timber"] = 500;
        c.SaveControl(doc);
        using var json = JsonDocument.Parse(File.ReadAllText(c.ControlPath));
        var root = json.RootElement;
        Assert.Equal(1, root.GetProperty("version").GetInt32());
        Assert.Equal(1, root.GetProperty("seq").GetInt64());
        var res = root.GetProperty("features").GetProperty("resources");
        Assert.True(res.GetProperty("enabled").GetBoolean());
        Assert.Equal(2, res.GetProperty("intervalSec").GetInt32());
        Assert.Equal(500, res.GetProperty("targets").GetProperty("Timber").GetInt32());
        Assert.True(root.GetProperty("features").GetProperty("military").GetProperty("unlimitedSquads").GetBoolean());
    }

    [Fact]
    public void LoadControl_CorruptFile_ReturnsDefault()
    {
        var c = NewClient(out var dir);
        Directory.CreateDirectory(dir);
        File.WriteAllText(c.ControlPath, "{broken");
        var doc = c.LoadControl();
        Assert.Equal(0, doc.Seq);
        Assert.False(doc.Features.Build.Enabled);
    }

    [Fact]
    public void ReadStatus_MissingOrCorrupt_ReturnsNull()
    {
        var c = NewClient(out var dir);
        Assert.Null(c.ReadStatus());
        Directory.CreateDirectory(dir);
        File.WriteAllText(c.StatusPath, "{broken");
        Assert.Null(c.ReadStatus());
    }

    [Fact]
    public void ReadStatus_ParsesLuaOutput()
    {
        var c = NewClient(out var dir);
        Directory.CreateDirectory(dir);
        File.WriteAllText(c.StatusPath,
            """{"version":1,"heartbeat":1800000000,"inGame":true,"appliedSeq":3,"features":{"build":{"active":true}},"resourceIds":["Timber"],"resources":{"Timber":12.0}}""");
        var s = c.ReadStatus()!;
        Assert.True(s.InGame);
        Assert.Equal(3, s.AppliedSeq);
        Assert.True(s.Features!["build"].Active);
        Assert.Null(s.Features["build"].LastError);
        Assert.Equal(12.0, s.Resources!["Timber"]);
    }

    [Fact]
    public void Evaluate_NullStatus_IsDisconnected() =>
        Assert.Equal(BridgeState.Disconnected, NewClient(out _).Evaluate(null, 0));

    [Fact]
    public void Evaluate_StaleHeartbeat_IsDisconnected() =>
        Assert.Equal(BridgeState.Disconnected,
            NewClient(out _).Evaluate(new StatusDocument { Heartbeat = Now.ToUnixTimeSeconds() - 6, InGame = true }, 0));

    [Fact]
    public void Evaluate_States()
    {
        var c = NewClient(out _);
        var hb = Now.ToUnixTimeSeconds() - 1;
        Assert.Equal(BridgeState.MainMenu, c.Evaluate(new StatusDocument { Heartbeat = hb, InGame = false }, 0));
        Assert.Equal(BridgeState.Pending, c.Evaluate(new StatusDocument { Heartbeat = hb, InGame = true, AppliedSeq = 1 }, 2));
        Assert.Equal(BridgeState.Pending, c.Evaluate(new StatusDocument { Heartbeat = hb, InGame = true, AppliedSeq = null }, 1));
        Assert.Equal(BridgeState.Applied, c.Evaluate(new StatusDocument { Heartbeat = hb, InGame = true, AppliedSeq = 2 }, 2));
    }
}
