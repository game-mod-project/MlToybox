namespace MLToybox.Panel.Core;

public sealed record ScopeOption(string? Key, string Label)
{
    public override string ToString() => Label;
}

// [자원] 탭 범위: 공통(Key=null, 모든 영지 합계와 공통 목표) 또는 영지 하나(그 영지 재고와 영지별 목표)
public static class ResourceScope
{
    // 금고·영향력은 영주 전체 값이라 영지 보기에서는 숨긴다
    public static readonly IReadOnlySet<string> LordWide = new HashSet<string> { "Treasury", "Influence" };

    public static List<ScopeOption> Options(StatusDocument? status)
    {
        var list = new List<ScopeOption> { new(null, "공통 (모든 내 영지, 현재=합계)") };
        foreach (var r in status?.Regions ?? new List<RegionResources>())
            list.Add(new(r.Key, $"{r.Name} ({r.Key})"));
        return list;
    }

    public static Dictionary<string, double>? Current(StatusDocument? status, string? key) =>
        key is null ? status?.Resources : status?.Regions?.FirstOrDefault(r => r.Key == key)?.Values;

    public static List<string>? Ids(StatusDocument? status, string? key) =>
        key is null ? status?.ResourceIds : status?.ResourceIds?.Where(id => !LordWide.Contains(id)).ToList();

    public static Dictionary<string, int> Targets(ResourcesControl control, string? key)
    {
        if (key is null) return control.Targets;
        if (!control.RegionTargets.TryGetValue(key, out var t))
        {
            t = new Dictionary<string, int>();
            control.RegionTargets[key] = t;
        }
        return t;
    }

    public static void Store(ResourcesControl control, string? key, Dictionary<string, int> targets)
    {
        if (key is null) control.Targets = targets;
        else if (targets.Count == 0) control.RegionTargets.Remove(key);
        else control.RegionTargets[key] = targets;
    }
}
