namespace MLToybox.Panel.Core;

// 커스텀 용병단 정의의 패널 쪽 검증. 모드도 같은 규칙으로 다시 검증한다(mod/MLToybox/Scripts/features/merc_plan.lua)
public static class MercCompanyRules
{
    public const int MaxSquads = 10;
    public const int NameMax = 40;
    public const int MaxEnabled = 3;

    // 용병 표(DT_MercenaryCompanies)의 Name 11개. findings "용병 고용 — 목록 보충과 커스텀 용병단"
    public static readonly IReadOnlySet<string> VanillaNames = new HashSet<string>(StringComparer.OrdinalIgnoreCase)
    {
        "brotherhood_of_the_forest", "crazy_goose", "brigands", "brigands_small", "wayward_sons", "greencaps",
        "vultures", "battle_brothers", "hildebolts_army", "hildebolts_army_large", "huntsmen",
    };

    // 용병 고용 경로로 생성되지 않는 병종 id. findings "용병 실측 M1~M5" 의 M2 에서 FAIL 인 것을 넣는다
    public static readonly IReadOnlySet<string> ExcludedUnits = new HashSet<string>();

    public static IReadOnlyList<UnitOption> Units => UnitCatalog.Units.Where(u => !ExcludedUnits.Contains(u.Id)).ToList();

    // 문제가 없으면 null, 있으면 사용자에게 보여 줄 이유. others 에 c 자신이 들어 있어도 된다
    public static string? Validate(MercCompany c, IEnumerable<MercCompany> others)
    {
        var name = c.Name.Trim();
        if (name.Length == 0) return "이름을 입력하세요.";
        if (name.Length > NameMax) return $"이름은 {NameMax}자 이하여야 합니다.";
        if (VanillaNames.Contains(name)) return "게임의 용병단 이름과 겹칩니다.";
        if (others.Any(o => !ReferenceEquals(o, c) && string.Equals(o.Name.Trim(), name, StringComparison.OrdinalIgnoreCase)))
            return "같은 이름의 용병단이 이미 있습니다.";
        if (c.Units.Count < 1 || c.Units.Count > MaxSquads) return $"분대는 1~{MaxSquads}개여야 합니다.";
        var known = Units.Select(u => u.Id).ToHashSet();
        var unknown = c.Units.FirstOrDefault(u => !known.Contains(u));
        if (unknown is not null) return $"쓸 수 없는 병종입니다: {unknown}";
        if (c.Cost < 0) return "고용비는 0 이상이어야 합니다.";
        return null;
    }

    // "용병 - 보병 × 2, 용병 - 석궁병 × 1" (처음 나온 순서). 모르는 병종 id 는 그대로 보여 준다
    public static string Summary(IEnumerable<string> units)
    {
        var labels = UnitCatalog.Units.ToDictionary(u => u.Id, u => u.Label);
        var order = new List<string>();
        var counts = new Dictionary<string, int>();
        foreach (var u in units)
        {
            if (!counts.ContainsKey(u))
            {
                counts[u] = 0;
                order.Add(u);
            }
            counts[u]++;
        }
        return string.Join(", ", order.Select(u => $"{labels.GetValueOrDefault(u, u)} × {counts[u]}"));
    }

    // target 을 사용으로 바꿔도 되는가(target 을 뺀 사용 수가 MaxEnabled 미만). target 이 null 이면 새 용병단
    public static bool CanEnable(IEnumerable<MercCompany> all, MercCompany? target) =>
        all.Count(c => c.Enabled && !ReferenceEquals(c, target)) < MaxEnabled;
}
