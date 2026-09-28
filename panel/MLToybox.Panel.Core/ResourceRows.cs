namespace MLToybox.Panel.Core;

public sealed record ResourceRow(string Id, double? Current, int? Target);

public static class ResourceRows
{
    public static List<ResourceRow> Build(
        IEnumerable<string>? ids,
        IReadOnlyDictionary<string, double>? current,
        IReadOnlyDictionary<string, int> targets)
    {
        var all = new SortedSet<string>(StringComparer.Ordinal);
        if (ids is not null) all.UnionWith(ids);
        if (current is not null) all.UnionWith(current.Keys);
        all.UnionWith(targets.Keys);
        return all.Select(id => new ResourceRow(
            id,
            current is not null && current.TryGetValue(id, out var c) ? c : null,
            targets.TryGetValue(id, out var t) ? t : null)).ToList();
    }
}
