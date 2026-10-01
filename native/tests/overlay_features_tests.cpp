#include "test.h"
#include "overlay/core/commands.h"
#include "overlay/core/control_doc.h"
#include "overlay/core/units.h"
#include "runtime.h"
#include <filesystem>

using namespace mlt::ov;

static ControlDoc panelFixture() {
    auto text = mlt::readFileUtf8(std::filesystem::path(MLT_FIXTURES_DIR) / "control_from_panel.json");
    CHECK(text.has_value());
    return ControlDoc::parse(*text);
}

TEST(overlay_features_defaults_match_the_panel) {
    ControlDoc doc;
    const MilitarySettings m = doc.military();
    CHECK(!m.enabled && m.ignoreEquipment && m.ignorePopulation && m.zeroUpkeep && m.unlimitedSquads);
    const PopulationSettings p = doc.population();
    CHECK(!p.enabled && p.multiplier == 2 && p.targetFamilies == 0 && p.regionTargets.empty());
    const ResourcesSettings r = doc.resources();
    CHECK(!r.enabled && r.intervalSec == 2 && r.targets.empty() && r.regionTargets.empty());
    const MercSettings me = doc.mercenaries();
    CHECK(!me.enabled && me.refund && me.lockFromAi && me.companies.empty());
}

TEST(overlay_features_read_a_file_written_by_the_panel) {
    const ControlDoc doc = panelFixture();
    CHECK(doc.military().enabled && doc.military().unlimitedSquads);
    const PopulationSettings p = doc.population();
    CHECK(p.enabled && p.multiplier == 1 && p.targetFamilies == 0 && p.regionTargets.empty());
    const ResourcesSettings r = doc.resources();
    CHECK(r.enabled && r.intervalSec == 2 && r.targets.size() == 56 && r.targets.at("Ale") == 500);
    CHECK(r.regionTargets.size() == 2 && r.regionTargets.at("sel").at("Ale") == 1000 && r.regionTargets.at("hof").at("Barley") == 600);
    const MercSettings me = doc.mercenaries();
    CHECK(me.enabled && me.refund && me.lockFromAi && me.companies.size() == 1);
    const MercCompany& c = me.companies[0];
    CHECK(c.name == "검사대" && c.units.size() == 4 && c.units[0] == "mercenary_infantry" && c.cost == 1000);
    CHECK(c.region == "nus" && c.banner == "battle_brothers" && c.enabled);
}

TEST(overlay_features_population_and_resources_round_trip) {
    ControlDoc doc;
    PopulationSettings p;
    p.enabled = true;
    p.multiplier = 3;
    p.targetFamilies = 12;
    p.regionTargets["sel"] = 30;
    p.regionTargets["hof"] = 0;                       // 0 = 그 영지는 끔. 값으로 남는다
    doc.setPopulation(p);
    ResourcesSettings r;
    r.enabled = true;
    r.intervalSec = 5;
    r.targets["Timber"] = 500;
    r.regionTargets["hof"]["Timber"] = 2000;
    doc.setResources(r);
    const ControlDoc back = ControlDoc::parse(doc.dump());
    const PopulationSettings p2 = back.population();
    CHECK(p2.enabled && p2.multiplier == 3 && p2.targetFamilies == 12 && p2.regionTargets.size() == 2);
    CHECK(p2.regionTargets.at("sel") == 30 && p2.regionTargets.at("hof") == 0);
    const ResourcesSettings r2 = back.resources();
    CHECK(r2.enabled && r2.intervalSec == 5 && r2.targets.at("Timber") == 500 && r2.regionTargets.at("hof").at("Timber") == 2000);
    // 영지 목표를 지우면 키도 사라진다
    r.regionTargets.erase("hof");
    doc.setResources(r);
    CHECK(Json::parse(doc.dump())["features"]["resources"]["regionTargets"].empty());
}

TEST(overlay_features_mercenary_companies_round_trip_and_omit_what_is_not_chosen) {
    ControlDoc doc;
    MercSettings m;
    m.enabled = true;
    m.lockFromAi = false;
    MercCompany chosen{ "토이박스 용병단", { "mercenary_infantry", "mercenary_infantry", "mercenary_crossbowmen" }, 3000, "gold", "greencaps", true };
    MercCompany plain{ "B", { "militia" }, 0, std::nullopt, std::nullopt, false };
    m.companies = { chosen, plain };
    doc.setMercenaries(m);
    const Json j = Json::parse(doc.dump());
    const Json& companies = j["features"]["mercenaries"]["companies"];
    CHECK(companies[0]["name"] == "토이박스 용병단" && companies[0]["units"].size() == 3 && companies[0]["units"][2] == "mercenary_crossbowmen");
    CHECK(companies[0]["cost"] == 3000 && companies[0]["region"] == "gold" && companies[0]["banner"] == "greencaps" && companies[0]["enabled"] == true);
    CHECK(!companies[1].contains("region") && !companies[1].contains("banner") && companies[1]["enabled"] == false);
    const MercSettings back = ControlDoc::parse(doc.dump()).mercenaries();
    CHECK(back.enabled && back.refund && !back.lockFromAi && back.companies.size() == 2);
    CHECK(back.companies[0] == chosen && back.companies[1] == plain);
}

TEST(overlay_features_tolerate_hand_edited_garbage) {
    const ControlDoc doc = ControlDoc::parse(R"({"version":1,"seq":2,"features":{
        "military":{"enabled":"yes","zeroUpkeep":0},
        "population":{"multiplier":"many","regionTargets":["a"]},
        "resources":{"targets":{"Timber":"lots","Stone":-5,"Iron":7.0,"Clay":7.5},"regionTargets":{"hof":5,"sel":{"Timber":9}}},
        "mercenaries":{"companies":[7,{"name":5,"units":["militia",3,null],"cost":"free","region":9},"x",{"name":"ok","units":"all"}]}}})");
    CHECK(!doc.military().enabled && doc.military().zeroUpkeep);        // 형식이 다르면 기본값
    CHECK(doc.population().multiplier == 2 && doc.population().regionTargets.empty());
    const ResourcesSettings r = doc.resources();
    CHECK(r.targets.size() == 1 && r.targets.at("Iron") == 7);          // 글자, 음수, 소수는 버린다
    CHECK(r.regionTargets.size() == 1 && r.regionTargets.at("sel").at("Timber") == 9);
    const MercSettings m = doc.mercenaries();
    CHECK(m.companies.size() == 2);                                     // 객체가 아닌 항목은 건너뛴다
    CHECK(m.companies[0].name.empty() && m.companies[0].units.size() == 1 && m.companies[0].cost == 0 && !m.companies[0].region);
    CHECK(m.companies[1].name == "ok" && m.companies[1].units.empty() && m.companies[1].enabled);
    CHECK(ControlDoc::parse(R"({"features":{"mercenaries":{"companies":{"a":1}}}})").mercenaries().companies.empty());
}

TEST(overlay_features_setters_keep_unknown_keys_of_the_feature) {
    ControlDoc doc = ControlDoc::parse(R"({"version":1,"seq":1,"features":{"military":{"enabled":false,"future":1},"mercenaries":{"future":"x"}}})");
    MilitarySettings m = doc.military();
    m.enabled = true;
    doc.setMilitary(m);
    doc.setMercenaries(doc.mercenaries());
    const Json j = Json::parse(doc.dump());
    CHECK(j["features"]["military"]["future"] == 1 && j["features"]["military"]["enabled"] == true);
    CHECK(j["features"]["mercenaries"]["future"] == "x" && j["features"]["mercenaries"]["companies"].is_array());
}

TEST(overlay_units_list_the_thirteen_playable_units) {
    CHECK(units().size() == 13);
    CHECK(units()[1].id == "spearMilitia" && units()[1].label == "민병대 - 창");
    CHECK(unitLabel("retinue_tier3") == "친위대 - 3단계" && unitLabel("Mercenary_Archers") == "용병 - 궁수");
    CHECK(unitLabel("dragon") == "dragon");                             // 모르는 병종은 id 그대로
    CHECK(isKnownUnit("mercenary_crossbowmen") && !isKnownUnit("dragon") && !isKnownUnit("mercenary_archers"));   // 대소문자를 가린다
}

TEST(overlay_scope_options_find_a_key_or_fall_back_to_the_first_line) {
    const std::vector<ScopeOption> options = { { std::nullopt, "공통" }, { "gold", regionLabel("Mandlach", "gold") }, { "nus", "nus" } };
    CHECK(options[1].label == "Mandlach (gold)");
    CHECK(indexOfKey(options, std::nullopt) == 0 && indexOfKey(options, "gold") == 1 && indexOfKey(options, "nus") == 2);
    CHECK(indexOfKey(options, "gone") == 0);                            // 사라진 영지는 첫 줄로
    CHECK(indexOfKey({}, "gold") == 0);
}

TEST(overlay_commands_carry_the_fields_the_mod_reads) {
    const Json spawn = makeSpawnSquads("spearMilitia", 3, 1790000000, "nus");
    CHECK(spawn["type"] == "spawnSquads" && spawn["unit"] == "spearMilitia" && spawn["count"] == 3 && spawn["region"] == "nus" && spawn["issuedAt"] == 1790000000);
    CHECK(spawn["id"].get<std::string>().size() == 32);
    CHECK(!makeSpawnSquads("militia", 1, 1, std::nullopt).contains("region"));   // 고르지 않았으면 키가 없다
    const Json reform = makeReformSquads(1790000001, "gold");
    CHECK(reform["type"] == "reformSquads" && reform["region"] == "gold" && !reform.contains("unit"));
    CHECK(!makeReformSquads(1, std::nullopt).contains("region"));
    const Json retinue = makeCustomizeRetinue(63, 1790000002, "nus");
    CHECK(retinue["type"] == "customizeRetinue" && retinue["value"] == 63 && retinue["region"] == "nus");
    CHECK(!makeCustomizeRetinue(64, 1, std::nullopt).contains("region"));
    const Json families = makeAddFamilies(3, 1790000003, "sel");
    CHECK(families["type"] == "addFamilies" && families["count"] == 3 && families["region"] == "sel");
    CHECK(!makeAddFamilies(2, 1, std::nullopt).contains("region"));
    CHECK(spawn["id"] != reform["id"] && retinue["id"] != families["id"]);
}
