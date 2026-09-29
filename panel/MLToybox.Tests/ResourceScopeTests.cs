using MLToybox.Panel.Core;
using Xunit;

namespace MLToybox.Tests;

public class ResourceScopeTests
{
    private static StatusDocument Status() => new()
    {
        ResourceIds = new() { "RegionalWealth", "Influence", "Treasury", "Timber" },
        Resources = new() { ["Timber"] = 740, ["Treasury"] = 9000 },
        Regions = new()
        {
            new RegionResources { Key = "hof", Name = "Klainau", Values = new() { ["Timber"] = 700 } },
            new RegionResources { Key = "sel", Name = "Furdau", Values = new() { ["Timber"] = 40 } },
        },
    };

    [Fact]
    public void Options_CommonFirstThenRegions()
    {
        var o = ResourceScope.Options(Status());
        Assert.Equal(3, o.Count);
        Assert.Null(o[0].Key);
        Assert.Equal("hof", o[1].Key);
        Assert.Contains("Klainau", o[1].Label);
    }

    [Fact]
    public void Current_CommonIsTotalRegionIsOwnStock()
    {
        var s = Status();
        Assert.Equal(740, ResourceScope.Current(s, null)!["Timber"]);
        Assert.Equal(40, ResourceScope.Current(s, "sel")!["Timber"]);
        Assert.Null(ResourceScope.Current(s, "gone"));
    }

    [Fact]
    public void Targets_RegionStoreCreatedOnDemandAndSeparateFromCommon()
    {
        var r = new ResourcesControl { Targets = new() { ["Timber"] = 500 } };
        var hof = ResourceScope.Targets(r, "hof");
        hof["Timber"] = 2000;
        Assert.Equal(2000, r.RegionTargets["hof"]["Timber"]);
        Assert.Equal(500, ResourceScope.Targets(r, null)["Timber"]);
    }

    [Fact]
    public void Store_EmptyRegionOverridesAreDropped()
    {
        var r = new ResourcesControl();
        ResourceScope.Store(r, "hof", new Dictionary<string, int> { ["Timber"] = 10 });
        ResourceScope.Store(r, "hof", new Dictionary<string, int>());
        Assert.False(r.RegionTargets.ContainsKey("hof"));
        ResourceScope.Store(r, null, new Dictionary<string, int> { ["Timber"] = 7 });
        Assert.Equal(7, r.Targets["Timber"]);
    }

    [Fact]
    public void Ids_RegionScopeHidesLordWideValues()
    {
        var ids = ResourceScope.Ids(Status(), "hof");
        Assert.DoesNotContain("Treasury", ids!);
        Assert.DoesNotContain("Influence", ids!);
        Assert.Contains("RegionalWealth", ids!);
        Assert.Contains("Treasury", ResourceScope.Ids(Status(), null)!);
    }

    [Fact]
    public void Build_ExcludesIdsFromTargetsToo()
    {
        var rows = ResourceRows.Build(new[] { "Timber" }, null, new Dictionary<string, int> { ["Treasury"] = 5 }, new HashSet<string> { "Treasury" });
        Assert.Single(rows);
        Assert.Equal("Timber", rows[0].Id);
    }
}
