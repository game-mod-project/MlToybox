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
    public MercenariesControl Mercenaries { get; set; } = new();
    public PopulationControl Population { get; set; } = new();
    public StorageControl Storage { get; set; } = new();
    public MoodControl Mood { get; set; } = new();
    public RegionControl Region { get; set; } = new();
}

// 영지: 가축 상인 대기, 자원 매장지의 최소 매장량, 매장지를 풍부하게. 게임 안 창의 "영지" 탭에서 고친다. 패널에는 화면이 없고, 저장할 때 설정을 그대로 남긴다
public sealed class RegionControl
{
    public bool Enabled { get; set; }
    public int IntervalSec { get; set; } = 5;
    public bool NoLivestockWait { get; set; }                                   // 가축을 주문한 뒤의 대기("상인 방문까지")를 없앤다
    public bool RichDeposits { get; set; }                                      // 내 영지의 소금·철·점토·돌 매장지를 풍부하게(무한 지하 매장지)
    public Dictionary<string, int> Targets { get; set; } = new();               // 종류 이름 → 매장지 하나의 최소 양
    public Dictionary<string, Dictionary<string, int>> RegionTargets { get; set; } = new();   // 영지 키 → 종류별 값(0 = 그 영지는 채우지 않는다)
}

// 자격·공공질서. 게임 안 창의 "자격·질서" 탭에서 고친다. 패널에는 화면이 없고, 저장할 때 설정을 그대로 남긴다
public sealed class MoodControl
{
    public bool Enabled { get; set; }
    public MoodValue Approval { get; set; } = new();   // 공통(내 영지 전체)
    public MoodValue Order { get; set; } = new();
    public Dictionary<string, MoodRegion> Regions { get; set; } = new();   // 키 = 영지 키. 있으면 그 영지는 공통 대신 이 설정을 쓴다
}

public sealed class MoodRegion
{
    public MoodValue Approval { get; set; } = new();
    public MoodValue Order { get; set; } = new();
}

// 모두 기본값이면 게임 그대로다
public sealed class MoodValue
{
    public int Fixed { get; set; }          // 고정값(1~100). 0 = 고정하지 않는다
    public int Good { get; set; } = 1;      // 오르는 요인 배율(1~10)
    public int Bad { get; set; } = 100;     // 깎이는 요인 비율(0~100 %)
}

// 건물의 저장 용량(건물 종류별 한도). 게임 안 창의 건설 탭에서 고친다. 패널에는 화면이 없고, 저장할 때 설정을 그대로 남긴다
public sealed class StorageControl
{
    public bool Enabled { get; set; }
    public int IntervalSec { get; set; } = 5;
    public Dictionary<string, StorageLimits> Limits { get; set; } = new();   // 키 = 건물 종류 번호
}

// null = 게임의 값 그대로(키를 쓰지 않음)
public sealed class StorageLimits
{
    [JsonIgnore(Condition = JsonIgnoreCondition.WhenWritingNull)] public int? Generic { get; set; }   // 일반 저장실
    [JsonIgnore(Condition = JsonIgnoreCondition.WhenWritingNull)] public int? Large { get; set; }     // 목재 저장실
    [JsonIgnore(Condition = JsonIgnoreCondition.WhenWritingNull)] public int? Pantry { get; set; }    // 식량 저장실
}

public sealed class PopulationControl
{
    public bool Enabled { get; set; }
    public int Multiplier { get; set; } = 2;   // 이민 속도 배율(게임의 월간 인구 변화에 곱한다)
    public int MonthlyFamilies { get; set; }   // 월 자연 이민 가족 수(0 = 게임 그대로. 넣으면 배율 대신 이 값이 쓰인다)
    public int HouseCapacity { get; set; } = 1;   // 집의 수용 가족 수 배율(1 = 게임 그대로)
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
    public bool NoRegionLimit { get; set; } = true;   // buildingStats.maxInRegion(지역당 개수 제한) 해제
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

// 용병 고용 창 관리. 고용비는 원래 값을 유지하고(AI 도 같은 목록에서 고용한다) 플레이어 고용만 환급한다
public sealed class MercenariesControl
{
    public bool Enabled { get; set; }
    public bool Refund { get; set; } = true;       // 내 용병단 고용비 환급 + 유지비 0
    public bool LockFromAi { get; set; } = true;   // 커스텀 용병단은 고용 창이 닫혀 있는 동안 AI 가 살 수 없는 가격
    public List<MercCompany> Companies { get; set; } = new();
}

// 커스텀 용병단 정의. Units 는 분대마다 병종 id 하나(1~10개), Region 은 내 영지 키(null = 내 첫 영지)
public sealed class MercCompany
{
    public string Name { get; set; } = "";
    public List<string> Units { get; set; } = new();
    public int Cost { get; set; }
    [JsonIgnore(Condition = JsonIgnoreCondition.WhenWritingNull)] public string? Region { get; set; }
    // 깃발: 용병 표 용병단의 이름(그 용병단의 깃발·색·문장을 쓴다). null = 카드가 들어간 칸의 것을 그대로 둔다
    [JsonIgnore(Condition = JsonIgnoreCondition.WhenWritingNull)] public string? Banner { get; set; }
    public bool Enabled { get; set; } = true;
}
