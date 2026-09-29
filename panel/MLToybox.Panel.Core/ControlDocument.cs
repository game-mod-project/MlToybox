using System.Text.Json.Serialization;

namespace MLToybox.Panel.Core;

public sealed class ControlDocument
{
    public int Version { get; set; } = 1;
    public long Seq { get; set; }
    public FeaturesControl Features { get; set; } = new();
    public List<ControlCommand> Commands { get; set; } = new();
}

public sealed class FeaturesControl
{
    public ResourcesControl Resources { get; set; } = new();
    public LordControl Lord { get; set; } = new();
    public BuildControl Build { get; set; } = new();
    public UpgradeControl Upgrade { get; set; } = new();
    public MilitaryControl Military { get; set; } = new();
    public PopulationControl Population { get; set; } = new();
}

public sealed class PopulationControl
{
    public bool Enabled { get; set; }
    public int Multiplier { get; set; } = 2;
    public int TargetFamilies { get; set; }   // 영지마다 최소 가족 수(0 = 끔)
    public Dictionary<string, int> RegionTargets { get; set; } = new();   // 영지별 값(0 = 그 영지 끔)
}

// 영주 전체 값. null = 관리 안 함(키를 쓰지 않음), 값이 있으면 그 이상으로 유지
public sealed class LordControl
{
    public bool Enabled { get; set; }
    public int IntervalSec { get; set; } = 2;
    [JsonIgnore(Condition = JsonIgnoreCondition.WhenWritingNull)] public int? Treasury { get; set; }
    [JsonIgnore(Condition = JsonIgnoreCondition.WhenWritingNull)] public int? Influence { get; set; }
    [JsonIgnore(Condition = JsonIgnoreCondition.WhenWritingNull)] public int? KingsFavour { get; set; }

    // 예전 버전은 국고·영향력을 자원 목표(targets.Treasury/Influence)에 넣었다. 영주 설정이 비어 있을 때만 옮긴다
    public static void MigrateFrom(FeaturesControl features, bool hadLordSection)
    {
        var targets = features.Resources.Targets;
        if (!hadLordSection)
        {
            if (targets.TryGetValue("Treasury", out var t)) features.Lord.Treasury = t;
            if (targets.TryGetValue("Influence", out var i)) features.Lord.Influence = i;
            if (features.Lord.Treasury is not null || features.Lord.Influence is not null) features.Lord.Enabled = features.Resources.Enabled;
        }
        targets.Remove("Treasury");
        targets.Remove("Influence");
    }
}

public sealed class ResourcesControl
{
    public bool Enabled { get; set; }
    public int IntervalSec { get; set; } = 2;
    public Dictionary<string, int> Targets { get; set; } = new();
    // 영지별 목표(키 = regionUniqueTag). 없는 자원은 공통 목표를 따른다
    public Dictionary<string, Dictionary<string, int>> RegionTargets { get; set; } = new();
}

public sealed class BuildControl
{
    public bool Enabled { get; set; }
    public bool IgnorePlacement { get; set; } = true;
    public bool InstantBuild { get; set; } = true;
    public bool InstantRepair { get; set; } = true;
    public bool NoMaterials { get; set; } = true;
}

public sealed class UpgradeControl
{
    public bool Enabled { get; set; }
}

public sealed class MilitaryControl
{
    public bool Enabled { get; set; }
    public bool IgnoreEquipment { get; set; } = true;
    public bool IgnorePopulation { get; set; } = true;
    public bool ZeroUpkeep { get; set; } = true;
    public bool UnlimitedSquads { get; set; } = true;
}
