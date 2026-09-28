using MLToybox.Panel.Core;
using Xunit;

namespace MLToybox.Tests;

public class ResourceRowsTests
{
    [Fact]
    public void Build_UnionsIdsSortedWithCurrentAndTarget()
    {
        var rows = ResourceRows.Build(
            new[] { "Timber", "Stone" },
            new Dictionary<string, double> { ["Timber"] = 10 },
            new Dictionary<string, int> { ["Iron"] = 50, ["Timber"] = 500 });
        Assert.Equal(new[] { "Iron", "Stone", "Timber" }, rows.Select(r => r.Id));
        Assert.Equal(new ResourceRow("Iron", null, 50), rows[0]);
        Assert.Equal(new ResourceRow("Stone", null, null), rows[1]);
        Assert.Equal(new ResourceRow("Timber", 10, 500), rows[2]);
    }

    [Fact]
    public void Build_AllNullInputs_ReturnsEmpty() =>
        Assert.Empty(ResourceRows.Build(null, null, new Dictionary<string, int>()));
}
