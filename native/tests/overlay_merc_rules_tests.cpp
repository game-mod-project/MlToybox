#include "test.h"
#include "overlay/core/merc_rules.h"
#include <functional>

using namespace mlt::ov;

// 패널의 MercenaryTests 와 같은 사례(이름 길이만 글자 수로 센다)
static MercCompany company(const std::string& name = "토이박스 용병단") {
    return { name, { "mercenary_infantry", "mercenary_infantry", "mercenary_crossbowmen" }, 3000, "gold", std::nullopt, true };
}

static std::string repeat(const std::string& s, int n) {
    std::string out;
    for (int i = 0; i < n; ++i) out += s;
    return out;
}

TEST(overlay_merc_rules_accept_a_good_company) {
    CHECK(!validateCompany(company(), {}, -1).has_value());
}

TEST(overlay_merc_rules_reject_bad_companies) {
    const std::vector<MercCompany> others = { company("기존 용병단") };
    auto reason = [&](const std::function<void(MercCompany&)>& change) {
        MercCompany c = company();
        change(c);
        return validateCompany(c, others, -1);
    };
    CHECK(reason([](MercCompany& c) { c.name = "   "; }) == "이름을 입력하세요.");
    CHECK(reason([](MercCompany& c) { c.name = repeat("가", 41); }) == "이름은 40자 이하여야 합니다.");
    CHECK(!reason([](MercCompany& c) { c.name = repeat("가", 40); }));            // 한글 40자는 120바이트다. 글자 수로 센다
    CHECK(reason([](MercCompany& c) { c.name = "Greencaps"; }) == "게임의 용병단 이름과 겹칩니다.");
    CHECK(reason([](MercCompany& c) { c.name = " 기존 용병단 "; }) == "같은 이름의 용병단이 이미 있습니다.");
    CHECK(reason([](MercCompany& c) { c.units.clear(); }) == "분대는 1~10개여야 합니다.");
    CHECK(reason([](MercCompany& c) { c.units.assign(11, "mercenary_infantry"); }).has_value());
    CHECK(!reason([](MercCompany& c) { c.units.assign(10, "mercenary_infantry"); }));
    CHECK(reason([](MercCompany& c) { c.units.push_back("dragon"); }) == "쓸 수 없는 병종입니다: dragon");
    CHECK(reason([](MercCompany& c) { c.cost = -1; }) == "고용비는 0 이상이어야 합니다.");
    CHECK(!reason([](MercCompany& c) { c.cost = 0; }));
}

TEST(overlay_merc_rules_editing_a_company_does_not_collide_with_itself) {
    const std::vector<MercCompany> all = { company("A"), company("B") };
    CHECK(!validateCompany(all[1], all, 1).has_value());                         // 자기 자신과는 겹치지 않는다
    MercCompany renamed = all[1];
    renamed.name = "a";                                                          // 다른 용병단과는 대소문자가 달라도 겹친다
    CHECK(validateCompany(renamed, all, 1) == "같은 이름의 용병단이 이미 있습니다.");
}

TEST(overlay_merc_rules_summary_groups_units_in_first_seen_order) {
    CHECK(unitSummary(company().units) == "용병 - 보병 × 2, 용병 - 석궁병 × 1");
    CHECK(unitSummary({ "dragon" }) == "dragon × 1");
    CHECK(unitSummary({}) == "");
    CHECK(unitSummary({ "militia", "retinue_tier1", "militia" }) == "민병대 - 농민 × 2, 친위대 - 1단계 × 1");
}

// R7: 등록 수에는 제한이 없고 "사용"은 최대 3개
TEST(overlay_merc_rules_at_most_three_enabled) {
    std::vector<MercCompany> all = { company("1"), company("2"), company("3"), company("4") };
    all[3].enabled = false;
    CHECK(!canEnableCompany(all, 3));            // 네 번째를 켤 수 없다
    CHECK(!canEnableCompany(all, -1));           // 새 용병단도 켠 채로는 못 넣는다
    CHECK(canEnableCompany(all, 0));             // 이미 켜진 것은 자기 자신을 빼고 센다
    all[1].enabled = false;
    CHECK(canEnableCompany(all, 3));
    CHECK(canEnableCompany({}, -1));
}

TEST(overlay_merc_rules_vanilla_names_and_banners) {
    CHECK(vanillaMercNames().size() == 11);
    CHECK(isVanillaMercName("huntsmen") && isVanillaMercName("HILDEBOLTS_ARMY") && !isVanillaMercName("토이박스"));
    for (size_t i = 1; i < vanillaMercNames().size(); ++i) CHECK(vanillaMercNames()[i - 1] < vanillaMercNames()[i]);   // 이름순
    CHECK(normalizeBanner("Greencaps") == "greencaps");
    CHECK(normalizeBanner(" brotherhood_of_the_forest ") == "brotherhood_of_the_forest");
    CHECK(!normalizeBanner("dragon") && !normalizeBanner("") && !normalizeBanner(std::nullopt));
    const std::vector<ScopeOption> banners = bannerOptions();
    CHECK(banners.size() == 12 && !banners[0].key && banners[0].label == "칸의 것 그대로" && banners[1].key == "battle_brothers");
}

TEST(overlay_merc_rules_region_options_keep_saved_keys_that_are_not_listed) {
    const std::vector<RegionInfo> regions = { { "gold", "Mandlach" } };
    const std::vector<ScopeOption> options = mercRegionOptions(regions, { "nus", "gold", std::nullopt, "nus" });
    CHECK(options.size() == 3);
    CHECK(!options[0].key && options[0].label == "내 첫 영지");
    CHECK(options[1].key == "gold" && options[1].label == "Mandlach (gold)");
    CHECK(options[2].key == "nus" && options[2].label == "nus");                 // 이름을 모르는 영지는 키로 보여 준다
    // 게임이 꺼져 있어 영지 목록이 없어도 저장된 도착 영지는 선택지로 남는다
    const std::vector<ScopeOption> offline = mercRegionOptions({}, { "nus" });
    CHECK(offline.size() == 2 && offline[1].key == "nus");
}
