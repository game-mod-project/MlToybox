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
    public RetinueStatus? Retinue { get; set; }
    public PopulationStatus? Population { get; set; }
    public List<RegionResources>? Regions { get; set; }
    public LordStatus? Lord { get; set; }
    public List<RegionInfo>? PlayerRegions { get; set; }
    public MercenaryStatus? Mercenaries { get; set; }
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

public sealed class RegionResources
{
    public string Key { get; set; } = "";
    public string Name { get; set; } = "";
    public Dictionary<string, double>? Values { get; set; }
}

public sealed class LordStatus
{
    public double? Treasury { get; set; }
    public int? Influence { get; set; }
    public int? KingsFavour { get; set; }
}

public sealed class RegionInfo
{
    public string Key { get; set; } = "";
    public string Name { get; set; } = "";
}

public sealed class MercenaryStatus
{
    public List<MercSlot>? Slots { get; set; }
    public int HiredMine { get; set; }
    public int HiredAi { get; set; }
    public int Refunded { get; set; }   // 맵을 불러온 뒤의 환급 합계
    public List<MercSkipped>? Skipped { get; set; }
    public string? Note { get; set; }
}

public sealed class MercSlot
{
    public string Name { get; set; } = "";
    public int Cost { get; set; }
    public bool Custom { get; set; }
}

public sealed class MercSkipped
{
    public string Name { get; set; } = "";
    public string Reason { get; set; } = "";
}
