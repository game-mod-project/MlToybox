#include "test.h"
#include "overlay/core/control_doc.h"
#include "runtime.h"
#include <filesystem>

using namespace mlt::ov;

static std::string fixture(const char* name) {
    auto text = mlt::readFileUtf8(std::filesystem::path(MLT_FIXTURES_DIR) / name);
    CHECK(text.has_value());
    return *text;
}

TEST(overlay_control_empty_doc_has_panel_defaults) {
    ControlDoc doc;
    CHECK(doc.seq() == 0);
    auto b = doc.build();
    CHECK(!b.enabled && b.ignorePlacement && b.instantBuild && b.instantRepair && b.noMaterials && b.noRegionLimit);
    CHECK(!doc.upgrade().enabled);
    auto l = doc.lord();
    CHECK(!l.enabled && l.intervalSec == 2 && !l.treasury && !l.influence && !l.kingsFavour);
    auto j = Json::parse(doc.dump());
    CHECK(j["version"] == 1 && j["seq"] == 0 && j["features"].is_object() && j["commands"].is_array());
}

TEST(overlay_control_reads_a_file_written_by_the_panel) {
    auto doc = ControlDoc::parse(fixture("control_from_panel.json"));
    CHECK(doc.seq() == 132);
    CHECK(doc.build().enabled && doc.build().noRegionLimit);
    CHECK(doc.upgrade().enabled);
    auto l = doc.lord();
    CHECK(!l.enabled && l.intervalSec == 2 && l.treasury == 150000 && l.influence == 20000 && l.kingsFavour == 50000);
    // 아직 구조체가 없는 기능도 그대로 들고 있다
    CHECK(doc.raw()["features"]["military"]["unlimitedSquads"] == true);
    CHECK(doc.raw()["features"]["mercenaries"]["companies"][0]["name"] == "검사대");
}

TEST(overlay_control_writes_keep_unknown_keys_and_other_features) {
    auto doc = ControlDoc::parse(R"({"version":1,"seq":5,"extra":"keep","features":{"build":{"enabled":false,"future":7},"military":{"enabled":true}},"commands":[]})");
    auto b = doc.build();
    b.enabled = true;
    b.instantRepair = false;
    doc.setBuild(b);
    auto j = Json::parse(doc.dump());
    CHECK(j["extra"] == "keep");
    CHECK(j["features"]["build"]["future"] == 7);
    CHECK(j["features"]["build"]["enabled"] == true);
    CHECK(j["features"]["build"]["instantRepair"] == false);
    CHECK(j["features"]["build"]["instantBuild"] == true);       // 빠져 있던 키는 기본값으로 채워 쓴다
    CHECK(j["features"]["military"]["enabled"] == true);
    CHECK(j["seq"] == 5);
}

TEST(overlay_control_lord_keys_are_written_only_when_managed) {
    ControlDoc doc;
    LordSettings l;
    l.enabled = true;
    l.treasury = 150000;
    doc.setLord(l);
    auto j = Json::parse(doc.dump());
    CHECK(j["features"]["lord"]["treasury"] == 150000);
    CHECK(!j["features"]["lord"].contains("influence"));
    CHECK(!j["features"]["lord"].contains("kingsFavour"));
    l.treasury.reset();
    l.kingsFavour = 0;
    doc.setLord(l);
    j = Json::parse(doc.dump());
    CHECK(!j["features"]["lord"].contains("treasury"));
    CHECK(j["features"]["lord"]["kingsFavour"] == 0);
    CHECK(doc.lord().kingsFavour == 0 && !doc.lord().treasury);
}

TEST(overlay_control_moves_old_treasury_and_influence_targets_to_lord) {
    auto doc = ControlDoc::parse(R"({"version":1,"seq":3,"features":{"resources":{"enabled":true,"targets":{"Treasury":90000,"Influence":500,"Timber":50}}}})");
    auto l = doc.lord();
    CHECK(l.enabled && l.treasury == 90000 && l.influence == 500 && !l.kingsFavour);
    const Json& targets = doc.raw()["features"]["resources"]["targets"];
    CHECK(!targets.contains("Treasury") && !targets.contains("Influence") && targets["Timber"] == 50);
}

TEST(overlay_control_existing_lord_section_wins_over_old_targets) {
    auto doc = ControlDoc::parse(R"({"version":1,"seq":3,"features":{"lord":{"enabled":false},"resources":{"enabled":true,"targets":{"Treasury":90000}}}})");
    CHECK(!doc.lord().enabled && !doc.lord().treasury);
    CHECK(!doc.raw()["features"]["resources"]["targets"].contains("Treasury"));
}

TEST(overlay_control_broken_or_odd_input_falls_back_to_defaults) {
    CHECK(ControlDoc::parse("").seq() == 0);
    CHECK(ControlDoc::parse("{").seq() == 0);
    CHECK(ControlDoc::parse("[1,2]").seq() == 0);
    auto doc = ControlDoc::parse(R"({"version":1,"seq":"x","features":[],"commands":{}})");
    CHECK(doc.seq() == 0);
    CHECK(doc.build().instantBuild);
    doc = ControlDoc::parse(R"({"version":1,"seq":2,"features":{"build":{"enabled":"yes","instantBuild":0},"lord":{"treasury":"many","intervalSec":2.0}}})");
    CHECK(!doc.build().enabled && doc.build().instantBuild);     // 형식이 다르면 기본값
    CHECK(!doc.lord().treasury && doc.lord().intervalSec == 2);
}

TEST(overlay_control_commands_are_replaced_as_a_whole) {
    ControlDoc doc;
    Json c = Json::object();
    c["id"] = "abc";
    c["type"] = "setLord";
    doc.setCommands({ c });
    CHECK(Json::parse(doc.dump())["commands"].size() == 1);
    doc.setCommands({});
    CHECK(Json::parse(doc.dump())["commands"].empty());
}

TEST(overlay_control_keeps_korean_text_as_utf8) {
    auto doc = ControlDoc::parse(fixture("control_from_panel.json"));
    std::string text = doc.dump();
    CHECK(text.find("검사대") != std::string::npos);
    CHECK(ControlDoc::parse(text).raw()["features"]["mercenaries"]["companies"][0]["name"] == "검사대");
}
