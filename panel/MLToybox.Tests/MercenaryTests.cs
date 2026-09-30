using System.Text.Json;
using MLToybox.Panel.Core;
using Xunit;

namespace MLToybox.Tests;

public class MercenaryTests
{
    private static readonly DateTimeOffset Now = DateTimeOffset.FromUnixTimeSeconds(1_800_000_000);

    private static BridgeClient NewClient()
    {
        var dir = Path.Combine(Directory.CreateTempSubdirectory("mltb-merc-").FullName, "bridge");
        Directory.CreateDirectory(dir);
        return new BridgeClient(dir, () => Now);
    }

    private static MercCompany Company(string name = "토이박스 용병단") => new()
    {
        Name = name,
        Units = new List<string> { "mercenary_infantry", "mercenary_infantry", "mercenary_crossbowmen" },
        Cost = 3000,
        Region = "gold",
    };

    [Fact]
    public void SaveControl_WritesMercenariesProtocol()
    {
        var c = NewClient();
        var doc = new ControlDocument();
        doc.Features.Mercenaries.Enabled = true;
        doc.Features.Mercenaries.Companies.Add(Company());
        c.SaveControl(doc);
        using var json = JsonDocument.Parse(File.ReadAllText(c.ControlPath));
        var m = json.RootElement.GetProperty("features").GetProperty("mercenaries");
        Assert.True(m.GetProperty("enabled").GetBoolean());
        Assert.True(m.GetProperty("refund").GetBoolean());
        Assert.True(m.GetProperty("lockFromAi").GetBoolean());
        var company = m.GetProperty("companies")[0];
        Assert.Equal("토이박스 용병단", company.GetProperty("name").GetString());
        Assert.Equal(3, company.GetProperty("units").GetArrayLength());
        Assert.Equal("mercenary_crossbowmen", company.GetProperty("units")[2].GetString());
        Assert.Equal(3000, company.GetProperty("cost").GetInt32());
        Assert.Equal("gold", company.GetProperty("region").GetString());
        Assert.True(company.GetProperty("enabled").GetBoolean());
    }

    [Fact]
    public void SaveControl_OmitsRegionWhenNotChosen()
    {
        var c = NewClient();
        var doc = new ControlDocument();
        var company = Company();
        company.Region = null;
        doc.Features.Mercenaries.Companies.Add(company);
        c.SaveControl(doc);
        using var json = JsonDocument.Parse(File.ReadAllText(c.ControlPath));
        var saved = json.RootElement.GetProperty("features").GetProperty("mercenaries").GetProperty("companies")[0];
        Assert.False(saved.TryGetProperty("region", out _));
    }

    [Fact]
    public void LoadControl_WithoutMercenaries_UsesDefaults()
    {
        var c = NewClient();
        File.WriteAllText(c.ControlPath, """{"version":1,"seq":3,"features":{"military":{"enabled":true}}}""");
        var m = c.LoadControl().Features.Mercenaries;
        Assert.False(m.Enabled);
        Assert.True(m.Refund);
        Assert.True(m.LockFromAi);
        Assert.Empty(m.Companies);
    }

    [Fact]
    public void LoadControl_RoundTripsCompanies()
    {
        var c = NewClient();
        var doc = new ControlDocument();
        doc.Features.Mercenaries.Companies.Add(Company());
        c.SaveControl(doc);
        var loaded = c.LoadControl().Features.Mercenaries.Companies.Single();
        Assert.Equal("토이박스 용병단", loaded.Name);
        Assert.Equal(new[] { "mercenary_infantry", "mercenary_infantry", "mercenary_crossbowmen" }, loaded.Units);
        Assert.Equal(3000, loaded.Cost);
        Assert.Equal("gold", loaded.Region);
        Assert.True(loaded.Enabled);
    }

    [Fact]
    public void ReadStatus_ParsesMercenaries()
    {
        var c = NewClient();
        File.WriteAllText(c.StatusPath,
            """{"version":1,"heartbeat":1800000000,"inGame":true,"mercenaries":{"slots":[{"name":"토이박스 용병단","cost":10000000,"custom":true},{"name":"wayward_sons","cost":90,"custom":false}],"hiredMine":2,"hiredAi":3,"refunded":6000,"skipped":[{"name":"궁수대","reason":"unknown unit: foo"}],"note":"rebuild produced 1 of 3 slots"}}""");
        var m = c.ReadStatus()!.Mercenaries!;
        Assert.Equal(2, m.Slots!.Count);
        Assert.Equal("토이박스 용병단", m.Slots[0].Name);
        Assert.Equal(10_000_000, m.Slots[0].Cost);
        Assert.True(m.Slots[0].Custom);
        Assert.False(m.Slots[1].Custom);
        Assert.Equal(2, m.HiredMine);
        Assert.Equal(3, m.HiredAi);
        Assert.Equal(6000, m.Refunded);
        var skipped = m.Skipped!.Single();
        Assert.Equal("궁수대", skipped.Name);
        Assert.Equal("unknown unit: foo", skipped.Reason);
        Assert.Equal("rebuild produced 1 of 3 slots", m.Note);
    }

    [Fact]
    public void ReadStatus_ParsesEmptyMercenaryLists()
    {
        // 모드의 JSON 인코더는 빈 표를 [] 로 쓴다
        var c = NewClient();
        File.WriteAllText(c.StatusPath,
            """{"version":1,"heartbeat":1800000000,"inGame":true,"mercenaries":{"slots":[],"hiredMine":0,"hiredAi":0,"refunded":0,"skipped":[]}}""");
        var m = c.ReadStatus()!.Mercenaries!;
        Assert.Empty(m.Slots!);
        Assert.Empty(m.Skipped!);
        Assert.Null(m.Note);
    }

    [Fact]
    public void Rules_AcceptAGoodCompany() =>
        Assert.Null(MercCompanyRules.Validate(Company(), new List<MercCompany>()));

    [Fact]
    public void Rules_RejectBadCompanies()
    {
        var others = new List<MercCompany> { Company("기존 용병단") };
        string? V(Action<MercCompany> change)
        {
            var c = Company();
            change(c);
            return MercCompanyRules.Validate(c, others);
        }
        Assert.NotNull(V(c => c.Name = "   "));
        Assert.NotNull(V(c => c.Name = new string('가', 41)));
        Assert.Null(V(c => c.Name = new string('가', 40)));
        Assert.NotNull(V(c => c.Name = "Greencaps"));
        Assert.NotNull(V(c => c.Name = " 기존 용병단 "));
        Assert.NotNull(V(c => c.Units.Clear()));
        Assert.NotNull(V(c => c.Units = Enumerable.Repeat("mercenary_infantry", 11).ToList()));
        Assert.Null(V(c => c.Units = Enumerable.Repeat("mercenary_infantry", 10).ToList()));
        Assert.NotNull(V(c => c.Units.Add("dragon")));
        Assert.NotNull(V(c => c.Cost = -1));
        Assert.Null(V(c => c.Cost = 0));
    }

    [Fact]
    public void Rules_EditingACompanyDoesNotCollideWithItself()
    {
        var c = Company();
        Assert.Null(MercCompanyRules.Validate(c, new List<MercCompany> { c }));
    }

    [Fact]
    public void Rules_SummaryGroupsUnitsInFirstSeenOrder()
    {
        Assert.Equal("용병 - 보병 × 2, 용병 - 석궁병 × 1", MercCompanyRules.Summary(Company().Units));
        Assert.Equal("dragon × 1", MercCompanyRules.Summary(new[] { "dragon" }));
        Assert.Equal("", MercCompanyRules.Summary(Array.Empty<string>()));
    }

    [Fact]
    public void Rules_AtMostThreeEnabled()
    {
        var all = new List<MercCompany> { Company("1"), Company("2"), Company("3"), Company("4") };
        all[3].Enabled = false;
        Assert.False(MercCompanyRules.CanEnable(all, all[3]));
        Assert.False(MercCompanyRules.CanEnable(all, null));
        Assert.True(MercCompanyRules.CanEnable(all, all[0]));
        all[1].Enabled = false;
        Assert.True(MercCompanyRules.CanEnable(all, all[3]));
    }

    [Fact]
    public void Rules_VanillaNamesMatchTheGameTable()
    {
        Assert.Equal(11, MercCompanyRules.VanillaNames.Count);
        Assert.Contains("huntsmen", MercCompanyRules.VanillaNames);
        Assert.Contains("HILDEBOLTS_ARMY", MercCompanyRules.VanillaNames);
    }

    [Fact]
    public void Rules_UnitsLeaveOutTheOnesThatCannotBeHired()
    {
        var ids = MercCompanyRules.Units.Select(u => u.Id).ToList();
        Assert.Equal(UnitCatalog.Units.Count - MercCompanyRules.ExcludedUnits.Count, ids.Count);
        Assert.DoesNotContain(ids, id => MercCompanyRules.ExcludedUnits.Contains(id));
        Assert.Contains("mercenary_infantry", ids);
    }

    [Fact]
    public void Rules_RegionOptionsKeepSavedKeysThatAreNotInTheRegionList()
    {
        var regions = new[] { new RegionInfo { Key = "gold", Name = "Mandlach" } };
        var options = MercCompanyRules.RegionOptions(regions, new string?[] { "nus", "gold", null, "nus" });
        Assert.Equal(new string?[] { null, "gold", "nus" }, options.Select(o => o.Key));
        Assert.Equal(MercCompanyRules.FirstRegionLabel, options[0].Label);
        Assert.Equal("Mandlach (gold)", options[1].Label);
        Assert.Equal("nus", options[2].Label);   // 이름을 모르는 영지는 키로 보여 준다

        // 게임이 꺼져 있어 영지 목록이 없어도, 저장된 도착 영지는 선택지로 남는다
        var offline = MercCompanyRules.RegionOptions(Array.Empty<RegionInfo>(), new string?[] { "nus" });
        Assert.Equal(new string?[] { null, "nus" }, offline.Select(o => o.Key));
    }
}
