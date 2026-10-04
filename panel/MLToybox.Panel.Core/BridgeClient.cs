using System.Text.Json;

namespace MLToybox.Panel.Core;

public enum BridgeState { Disconnected, MainMenu, Pending, Applied }

public sealed class BridgeClient
{
    public static readonly TimeSpan HeartbeatTimeout = TimeSpan.FromSeconds(5);
    private readonly Func<DateTimeOffset> _now;

    public BridgeClient(string dir, Func<DateTimeOffset>? now = null)
    {
        Dir = dir;
        _now = now ?? (() => DateTimeOffset.UtcNow);
    }

    public string Dir { get; }
    public string ControlPath => Path.Combine(Dir, "control.json");
    public string StatusPath => Path.Combine(Dir, "status.json");
    public string BackupPath => ControlPath + ".bak";

    // 파일이 없거나 읽지 못하면 기본 문서
    public ControlDocument LoadControl() => LoadControlChecked().Document;

    // 위와 같되, 파일은 있는데 해석하지 못한 경우(손으로 고치다 틀렸다)를 Unreadable 로 알려 준다
    public (ControlDocument Document, bool Unreadable) LoadControlChecked()
    {
        try
        {
            if (File.Exists(ControlPath))
            {
                var text = File.ReadAllText(ControlPath);
                var doc = JsonSerializer.Deserialize<ControlDocument>(text, BridgeJson.Options) ?? new();
                using var raw = JsonDocument.Parse(text);
                var hadLord = raw.RootElement.TryGetProperty("features", out var fs) && fs.ValueKind == JsonValueKind.Object && fs.TryGetProperty("lord", out _);
                LordControl.MigrateFrom(doc.Features, hadLord);
                return (doc, false);
            }
        }
        catch (JsonException) { return (new ControlDocument(), true); }
        catch (IOException) { }
        return (new ControlDocument(), false);
    }

    public long SaveControl(ControlDocument doc)
    {
        Directory.CreateDirectory(Dir);
        // 읽을 수 없는 파일은 사본을 남긴 뒤에만 덮는다(게임 안 창과 같다). 사본을 못 남기면 예외가 나가고 저장하지 않는다
        var existing = LoadControlChecked();
        if (existing.Unreadable) File.Copy(ControlPath, BackupPath, overwrite: true);
        doc.Version = 1;
        doc.Seq = Math.Max(existing.Document.Seq, doc.Seq) + 1;
        var tmp = ControlPath + ".tmp";
        File.WriteAllText(tmp, JsonSerializer.Serialize(doc, BridgeJson.Options));
        for (var attempt = 1; ; attempt++)
        {
            try
            {
                File.Move(tmp, ControlPath, overwrite: true);
                return doc.Seq;
            }
            // 모드가 파일을 읽는 순간과 겹치면 실패한다. 다른 프로그램이 삭제 공유 없이 열고 있으면 Windows 는 "접근 거부"로 알린다(실측)
            catch (Exception ex) when (ex is IOException or UnauthorizedAccessException && attempt < 5)
            {
                Thread.Sleep(50);
            }
        }
    }

    public StatusDocument? ReadStatus()
    {
        try
        {
            using var fs = new FileStream(StatusPath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete);
            return JsonSerializer.Deserialize<StatusDocument>(fs, BridgeJson.Options);
        }
        catch (IOException) { return null; }
        catch (UnauthorizedAccessException) { return null; }
        catch (JsonException) { return null; }
    }

    public BridgeState Evaluate(StatusDocument? status, long lastSentSeq)
    {
        if (status is null) return BridgeState.Disconnected;
        var age = _now() - DateTimeOffset.FromUnixTimeSeconds(status.Heartbeat);
        if (age > HeartbeatTimeout) return BridgeState.Disconnected;
        if (!status.InGame) return BridgeState.MainMenu;
        return (status.AppliedSeq ?? -1) >= lastSentSeq ? BridgeState.Applied : BridgeState.Pending;
    }
}
