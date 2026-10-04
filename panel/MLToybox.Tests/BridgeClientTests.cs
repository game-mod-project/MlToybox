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
        Assert.True(root.GetProperty("features").GetProperty("build").GetProperty("noMaterials").GetBoolean());
        Assert.True(root.GetProperty("features").GetProperty("military").GetProperty("unlimitedSquads").GetBoolean());
    }

    // 저장 용량은 게임 안 창에서만 고친다. 패널에는 화면이 없지만, 패널이 저장해도 그 설정이 사라지지 않아야 한다
    [Fact]
    public void SaveControl_KeepsTheStorageLimitsSetInTheOverlay()
    {
        var c = NewClient(out var dir);
        Directory.CreateDirectory(dir);
        File.WriteAllText(c.ControlPath, "{\"version\":1,\"seq\":3,\"features\":{\"storage\":{\"enabled\":true,\"intervalSec\":5,"
            + "\"limits\":{\"99\":{\"generic\":5000},\"69\":{\"generic\":3000,\"pantry\":6000}}}},\"commands\":[]}");
        var doc = c.LoadControl();
        Assert.True(doc.Features.Storage.Enabled);
        Assert.Equal(5000, doc.Features.Storage.Limits["99"].Generic);
        doc.Features.Build.Enabled = true;
        c.SaveControl(doc);
        using var json = JsonDocument.Parse(File.ReadAllText(c.ControlPath));
        var storage = json.RootElement.GetProperty("features").GetProperty("storage");
        Assert.True(storage.GetProperty("enabled").GetBoolean());
        Assert.Equal(5, storage.GetProperty("intervalSec").GetInt32());
        Assert.Equal(5000, storage.GetProperty("limits").GetProperty("99").GetProperty("generic").GetInt32());
        Assert.False(storage.GetProperty("limits").GetProperty("99").TryGetProperty("pantry", out _));   // 값이 없는 분류는 쓰지 않는다
        Assert.Equal(6000, storage.GetProperty("limits").GetProperty("69").GetProperty("pantry").GetInt32());
        // 설정이 없던 파일에는 꺼진 기본값이 쓰인다
        var fresh = new ControlDocument();
        Assert.False(fresh.Features.Storage.Enabled);
        Assert.Equal(5, fresh.Features.Storage.IntervalSec);
        Assert.Empty(fresh.Features.Storage.Limits);
    }

    // 자격·공공질서도 게임 안 창에서만 고친다. 패널이 저장해도 공통 설정과 영지별 설정이 그대로 남아야 한다
    [Fact]
    public void SaveControl_KeepsTheMoodSettingsSetInTheOverlay()
    {
        var c = NewClient(out var dir);
        Directory.CreateDirectory(dir);
        File.WriteAllText(c.ControlPath, "{\"version\":1,\"seq\":3,\"features\":{\"mood\":{\"enabled\":true,"
            + "\"approval\":{\"fixed\":0,\"good\":3,\"bad\":25},\"order\":{\"fixed\":90,\"good\":1,\"bad\":100},"
            + "\"regions\":{\"eich\":{\"approval\":{\"fixed\":100,\"good\":1,\"bad\":100},\"order\":{\"fixed\":0,\"good\":1,\"bad\":0}}}}},\"commands\":[]}");
        var doc = c.LoadControl();
        Assert.True(doc.Features.Mood.Enabled);
        Assert.Equal(3, doc.Features.Mood.Approval.Good);
        doc.Features.Build.Enabled = true;
        c.SaveControl(doc);
        using var json = JsonDocument.Parse(File.ReadAllText(c.ControlPath));
        var mood = json.RootElement.GetProperty("features").GetProperty("mood");
        Assert.True(mood.GetProperty("enabled").GetBoolean());
        Assert.Equal(3, mood.GetProperty("approval").GetProperty("good").GetInt32());
        Assert.Equal(25, mood.GetProperty("approval").GetProperty("bad").GetInt32());
        Assert.Equal(90, mood.GetProperty("order").GetProperty("fixed").GetInt32());
        var eich = mood.GetProperty("regions").GetProperty("eich");
        Assert.Equal(100, eich.GetProperty("approval").GetProperty("fixed").GetInt32());
        Assert.Equal(0, eich.GetProperty("order").GetProperty("bad").GetInt32());
        // 설정이 없던 파일에는 "게임 그대로"인 기본값이 쓰인다
        var fresh = new ControlDocument();
        Assert.False(fresh.Features.Mood.Enabled);
        Assert.Equal(0, fresh.Features.Mood.Approval.Fixed);
        Assert.Equal(1, fresh.Features.Mood.Approval.Good);
        Assert.Equal(100, fresh.Features.Mood.Order.Bad);
        Assert.Empty(fresh.Features.Mood.Regions);
    }

    // 영지(가축 상인 대기, 매장량)도 게임 안 창에서만 고친다. 패널이 저장해도 그대로 남아야 한다
    [Fact]
    public void SaveControl_KeepsTheRegionSettingsSetInTheOverlay()
    {
        var c = NewClient(out var dir);
        Directory.CreateDirectory(dir);
        File.WriteAllText(c.ControlPath, "{\"version\":1,\"seq\":3,\"features\":{\"region\":{\"enabled\":true,\"intervalSec\":5,\"noLivestockWait\":true,"
            + "\"targets\":{\"Iron\":1000,\"Fish\":400},\"regionTargets\":{\"imm\":{\"Iron\":3000},\"eich\":{\"Clay\":0}}}},\"commands\":[]}");
        var doc = c.LoadControl();
        Assert.True(doc.Features.Region.Enabled);
        Assert.True(doc.Features.Region.NoLivestockWait);
        doc.Features.Build.Enabled = true;
        c.SaveControl(doc);
        using var json = JsonDocument.Parse(File.ReadAllText(c.ControlPath));
        var region = json.RootElement.GetProperty("features").GetProperty("region");
        Assert.True(region.GetProperty("enabled").GetBoolean());
        Assert.True(region.GetProperty("noLivestockWait").GetBoolean());
        Assert.Equal(5, region.GetProperty("intervalSec").GetInt32());
        Assert.Equal(1000, region.GetProperty("targets").GetProperty("Iron").GetInt32());
        Assert.Equal(400, region.GetProperty("targets").GetProperty("Fish").GetInt32());
        Assert.Equal(3000, region.GetProperty("regionTargets").GetProperty("imm").GetProperty("Iron").GetInt32());
        Assert.Equal(0, region.GetProperty("regionTargets").GetProperty("eich").GetProperty("Clay").GetInt32());
        // 설정이 없던 파일에는 꺼진 기본값이 쓰인다
        var fresh = new ControlDocument();
        Assert.False(fresh.Features.Region.Enabled);
        Assert.False(fresh.Features.Region.NoLivestockWait);
        Assert.Equal(5, fresh.Features.Region.IntervalSec);
        Assert.Empty(fresh.Features.Region.Targets);
        Assert.Empty(fresh.Features.Region.RegionTargets);
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
    public void LoadControlChecked_TellsAnUnreadableFileFromAMissingOne()
    {
        var c = NewClient(out var dir);
        Assert.False(c.LoadControlChecked().Unreadable);   // 파일이 없다: 첫 실행
        Directory.CreateDirectory(dir);
        // 정수 자리에 소수를 넣은 손 편집. 문법은 맞지만 패널은 읽지 못한다
        File.WriteAllText(c.ControlPath, "{\"version\":1,\"seq\":4,\"features\":{\"resources\":{\"targets\":{\"Timber\":100.5}}}}");
        var loaded = c.LoadControlChecked();
        Assert.True(loaded.Unreadable);
        Assert.Equal(0, loaded.Document.Seq);
        File.WriteAllText(c.ControlPath, "{\"version\":1,\"seq\":4,\"features\":{}}");
        Assert.False(c.LoadControlChecked().Unreadable);
    }

    // 읽지 못한 control.json 으로 패널을 켜면 기본값으로 뜬다. 그대로 "적용"하면 기본값이 파일을 덮으므로,
    // 덮기 전에 control.json.bak 으로 사본을 남긴다(게임 안 창과 같다)
    [Fact]
    public void SaveControl_KeepsACopyOfAnUnreadableFile()
    {
        var c = NewClient(out var dir);
        Directory.CreateDirectory(dir);
        File.WriteAllText(c.ControlPath, "{broken");
        Assert.Equal(1, c.SaveControl(c.LoadControl()));
        Assert.Equal("{broken", File.ReadAllText(c.BackupPath));
        File.Delete(c.BackupPath);
        Assert.Equal(2, c.SaveControl(c.LoadControl()));    // 읽을 수 있는 파일은 사본을 만들지 않는다
        Assert.False(File.Exists(c.BackupPath));
    }

    // 모드(Lua 의 io.open)와 네이티브 DLL 은 control.json 을 삭제 공유 없이 연다. 그 순간과 겹치면 덮어쓰기가 실패하고,
    // 잠깐 뒤에 다시 하면 된다. 실패가 어떤 예외로 오든(공유 위반, 접근 거부) 다시 해야 한다
    [Fact]
    public void SaveControl_RetriesWhileAnotherProgramHoldsTheFile()
    {
        var c = NewClient(out _);
        Assert.Equal(1, c.SaveControl(new ControlDocument()));
        var held = new FileStream(c.ControlPath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite);   // 삭제 공유 없음
        var release = Task.Run(async () => { await Task.Delay(60); held.Dispose(); });
        Assert.Equal(2, c.SaveControl(c.LoadControl()));
        release.Wait();
        Assert.Equal(2, c.LoadControl().Seq);
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
    public void ReadStatus_ParsesNativeSection()
    {
        var c = NewClient(out var dir);
        Directory.CreateDirectory(dir);
        File.WriteAllText(c.StatusPath,
            """{"version":1,"heartbeat":1800000000,"inGame":true,"native":{"loaded":true,"stale":false,"heartbeat":1800000000,"features":{"instant_build":{"installed":true,"active":true}}}}""");
        var s = c.ReadStatus()!;
        Assert.True(s.Native!.Loaded);
        Assert.False(s.Native.Stale);
        Assert.True(s.Native.Features!["instant_build"].Active);
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
