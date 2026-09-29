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
    public BuildControl Build { get; set; } = new();
    public UpgradeControl Upgrade { get; set; } = new();
    public MilitaryControl Military { get; set; } = new();
    public PopulationControl Population { get; set; } = new();
}

public sealed class PopulationControl
{
    public bool Enabled { get; set; }
    public int Multiplier { get; set; } = 2;
    public int TargetFamilies { get; set; }
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
