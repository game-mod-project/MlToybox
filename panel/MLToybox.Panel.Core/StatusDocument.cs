namespace MLToybox.Panel.Core;

public sealed class StatusDocument
{
    public int Version { get; set; }
    public long Heartbeat { get; set; }
    public bool InGame { get; set; }
    public long? AppliedSeq { get; set; }
    public string? BridgeError { get; set; }
    public Dictionary<string, FeatureStatus>? Features { get; set; }
    public List<string>? ResourceIds { get; set; }
    public Dictionary<string, double>? Resources { get; set; }
    public NativeStatus? Native { get; set; }
    public Dictionary<string, CommandResult>? Commands { get; set; }
    public SpawnStatus? Spawn { get; set; }
}

public sealed class NativeStatus
{
    public bool Loaded { get; set; }
    public string? Error { get; set; }
    public long? Heartbeat { get; set; }
    public bool Stale { get; set; }
    public Dictionary<string, NativeFeature>? Features { get; set; }
}

public sealed class NativeFeature
{
    public bool Installed { get; set; }
    public bool Active { get; set; }
    public string? LastError { get; set; }
}

public sealed class FeatureStatus
{
    public bool Active { get; set; }
    public string? LastError { get; set; }
}
