using System.Text.Json;
using MLToybox.Panel.Core;
using Xunit;

namespace MLToybox.Tests;

public class RetinueTests
{
    private static readonly DateTimeOffset Now = DateTimeOffset.FromUnixTimeSeconds(1_800_000_000);

    [Fact]
    public void CustomizeRetinue_CommandCarriesTheSquadAndRegion()
    {
        var c = ControlCommand.CustomizeRetinue(63, Now, "nus");
        Assert.Equal("customizeRetinue", c.Type);
        Assert.Equal(63, c.Value);
        Assert.Equal("nus", c.Region);
        Assert.Equal(Now.ToUnixTimeSeconds(), c.IssuedAt);
        Assert.False(string.IsNullOrEmpty(c.Id));

        var json = JsonSerializer.Serialize(ControlCommand.CustomizeRetinue(64, Now), BridgeJson.Options);
        using var doc = JsonDocument.Parse(json);
        Assert.Equal(64, doc.RootElement.GetProperty("value").GetInt32());
        Assert.False(doc.RootElement.TryGetProperty("region", out _));
    }

    [Fact]
    public void Status_ReadsRetinueSquads()
    {
        const string json = """
            {"version":1,"inGame":true,"retinue":{"squads":[{"id":63,"unit":"retinue_tier1","count":36,"kind":"spawned"},{"id":64,"unit":"retinue_tier3","count":12,"kind":"mercenary"}],"editing":63}}
            """;
        var status = JsonSerializer.Deserialize<StatusDocument>(json, BridgeJson.Options)!;
        var r = status.Retinue!;
        Assert.Equal(2, r.Squads!.Count);
        Assert.Equal(63, r.Squads[0].Id);
        Assert.Equal("retinue_tier1", r.Squads[0].Unit);
        Assert.Equal(36, r.Squads[0].Count);
        Assert.Equal("spawned", r.Squads[0].Kind);
        Assert.Equal("mercenary", r.Squads[1].Kind);
        Assert.Equal(63, r.Editing);

        var none = JsonSerializer.Deserialize<StatusDocument>("""{"version":1,"inGame":true}""", BridgeJson.Options)!;
        Assert.Null(none.Retinue);
    }

    [Fact]
    public void RetinueSquad_LabelNamesTheUnitCountAndOrigin()
    {
        Assert.Equal("#63 친위대 - 1단계 ×36 (생성)", RetinueSquad.Label(new RetinueSquad { Id = 63, Unit = "retinue_tier1", Count = 36, Kind = "spawned" }));
        Assert.Equal("#64 친위대 - 3단계 ×12 (용병)", RetinueSquad.Label(new RetinueSquad { Id = 64, Unit = "retinue_tier3", Count = 12, Kind = "mercenary" }));
        Assert.Equal("#65 odd ×1 (odd)", RetinueSquad.Label(new RetinueSquad { Id = 65, Unit = "odd", Count = 1, Kind = "odd" }));
    }
}
