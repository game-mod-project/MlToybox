namespace MLToybox.Panel.Core;

// 모드가 한 번만 실행하는 일회성 명령. issuedAt 이 오래되면(모드 쪽 60초) 무시된다.
public sealed class ControlCommand
{
    public string Id { get; set; } = "";
    public string Type { get; set; } = "";
    public string? Unit { get; set; }
    public int Count { get; set; }
    public long IssuedAt { get; set; }

    public static ControlCommand SpawnSquads(string unit, int count, DateTimeOffset now) => new()
    {
        Id = Guid.NewGuid().ToString("N"),
        Type = "spawnSquads",
        Unit = unit,
        Count = count,
        IssuedAt = now.ToUnixTimeSeconds(),
    };

    // 해제된 생성 분대(0/N 빈 카드)를 같은 병종으로 다시 생성하고 빈 카드를 정리한다
    public static ControlCommand ReformSquads(DateTimeOffset now) => new()
    {
        Id = Guid.NewGuid().ToString("N"),
        Type = "reformSquads",
        IssuedAt = now.ToUnixTimeSeconds(),
    };

    public static ControlCommand AddFamilies(int count, DateTimeOffset now) => new()
    {
        Id = Guid.NewGuid().ToString("N"),
        Type = "addFamilies",
        Count = count,
        IssuedAt = now.ToUnixTimeSeconds(),
    };
}

public sealed class CommandResult
{
    public bool Ok { get; set; }
    public string? Error { get; set; }
    public List<int>? Squads { get; set; }
    public int? Reformed { get; set; }
    public int? Requested { get; set; }
    public int? Added { get; set; }
}

public sealed class SpawnStatus
{
    public int Disbanded { get; set; }
    public int Pending { get; set; }
    public Dictionary<string, int>? ByUnit { get; set; }
}

public sealed class PopulationStatus
{
    public int Families { get; set; }
    public int Population { get; set; }
    public int Homeless { get; set; }
    public int FreeSlots { get; set; }
}

public sealed record UnitOption(string Id, string Label);

// 분대 생성 가능 병종 (DT_UnitTemplates 중 실제 플레이용 행, findings "Plan 3 네이티브 — 주민 수를 넘는 징집")
public static class UnitCatalog
{
    public static readonly IReadOnlyList<UnitOption> Units = new UnitOption[]
    {
        new("militia", "민병대 - 농민"),
        new("spearMilitia", "민병대 - 창"),
        new("militiaPole", "민병대 - 장창"),
        new("militiaFoot", "민병대 - 보병"),
        new("bowMilitia", "민병대 - 활"),
        new("crossbowMilitia", "민병대 - 석궁"),
        new("retinue_tier1", "친위대 - 1단계"),
        new("retinue_tier3", "친위대 - 3단계"),
        new("mercenary_spearmen", "용병 - 창병"),
        new("mercenary_infantry", "용병 - 보병"),
        new("Mercenary_Archers", "용병 - 궁수"),
        new("mercenary_heavy_archers", "용병 - 중궁수"),
        new("mercenary_crossbowmen", "용병 - 석궁병"),
    };
}
