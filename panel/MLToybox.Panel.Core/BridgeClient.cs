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

    public ControlDocument LoadControl()
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
                return doc;
            }
        }
        catch (JsonException) { }
        catch (IOException) { }
        return new ControlDocument();
    }

    public long SaveControl(ControlDocument doc)
    {
        Directory.CreateDirectory(Dir);
        doc.Version = 1;
        doc.Seq = Math.Max(LoadControl().Seq, doc.Seq) + 1;
        var tmp = ControlPath + ".tmp";
        File.WriteAllText(tmp, JsonSerializer.Serialize(doc, BridgeJson.Options));
        for (var attempt = 1; ; attempt++)
        {
            try
            {
                File.Move(tmp, ControlPath, overwrite: true);
                return doc.Seq;
            }
            catch (IOException) when (attempt < 5)
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
