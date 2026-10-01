#include "test.h"
#include "overlay/core/status_doc.h"
#include "runtime.h"
#include <filesystem>

using namespace mlt::ov;

TEST(overlay_status_reads_a_file_written_by_the_mod) {
    auto text = mlt::readFileUtf8(std::filesystem::path(MLT_FIXTURES_DIR) / "status_from_mod.json");
    CHECK(text.has_value());
    auto s = parseStatus(*text);
    CHECK(s.has_value());
    CHECK(s->heartbeat == 1790833555 && s->inGame && s->appliedSeq == 132 && !s->bridgeError);
    CHECK(s->features.has_value() && s->features->size() == 7);
    CHECK(s->features->at("build").active && !s->features->at("lord").active);
    CHECK(s->lord.has_value() && s->lord->treasury == 148500.0 && s->lord->influence == 27910 && s->lord->kingsFavour == 50000);
    CHECK(s->native.has_value() && s->native->loaded && !s->native->stale);
    CHECK(s->native->features.at("instant_build").installed && s->native->features.at("instant_build").active);
    CHECK(!s->commands.has_value());
    CHECK(s->raw["spawn"]["disbanded"] == 0);
}

TEST(overlay_status_reads_command_results_in_file_order) {
    auto s = parseStatus(R"({"version":1,"heartbeat":10,"inGame":true,"commands":{
        "zz01":{"ok":true,"squads":[63,64]},
        "aa02":{"ok":false,"error":"not in game"},
        "mm03":{"ok":true,"added":2,"requested":3},
        "kk04":{"ok":true,"reformed":4,"squads":[]}}})");
    CHECK(s.has_value() && s->commands.has_value() && s->commands->size() == 4);
    const auto& c = *s->commands;
    CHECK(c[0].first == "zz01" && c[0].second.ok && c[0].second.squads->size() == 2 && (*c[0].second.squads)[1] == 64);
    CHECK(c[1].first == "aa02" && !c[1].second.ok && c[1].second.error == "not in game" && !c[1].second.squads);
    CHECK(c[2].second.added == 2 && c[2].second.requested == 3);
    CHECK(c[3].second.reformed == 4 && c[3].second.squads->empty());
}

TEST(overlay_status_treats_lua_empty_tables_as_empty_objects) {
    auto s = parseStatus(R"({"version":1,"heartbeat":10,"inGame":false,"features":[],"commands":[],"native":{"loaded":false,"error":"not deployed","stale":true,"features":[]}})");
    CHECK(s.has_value());
    CHECK(s->features.has_value() && s->features->empty());
    CHECK(!s->commands.has_value());
    CHECK(s->native.has_value() && !s->native->loaded && s->native->error == "not deployed" && s->native->stale && s->native->features.empty());
    CHECK(!s->lord.has_value() && !s->appliedSeq.has_value());
}

TEST(overlay_status_feature_errors_and_missing_parts) {
    auto s = parseStatus(R"({"heartbeat":10,"inGame":true,"appliedSeq":4,"bridgeError":"parse error","features":{"build":{"active":false,"lastError":"boom"},"odd":5}})");
    CHECK(s.has_value() && s->appliedSeq == 4 && s->bridgeError == "parse error");
    CHECK(s->features->size() == 1 && s->features->at("build").lastError == "boom");
    CHECK(!s->native.has_value());
}

TEST(overlay_status_rejects_broken_text) {
    CHECK(!parseStatus("").has_value());
    CHECK(!parseStatus("{\"heartbeat\":").has_value());
    CHECK(!parseStatus("[]").has_value());
}
