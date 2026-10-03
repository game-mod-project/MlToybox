#include "test.h"
#include "control.h"

using namespace mlt;

TEST(control_combines_enabled_and_flags) {
    auto c = parseControl(R"({"version":1,"seq":7,"features":{
        "build":{"enabled":true,"instantBuild":true,"ignorePlacement":false},
        "military":{"enabled":false,"ignorePopulation":true}}})");
    CHECK(c.has_value());
    CHECK(c->seq == 7);
    CHECK(c->instantBuild == true);
    CHECK(c->ignorePlacement == false);
    CHECK(c->ignorePopulation == false);   // military.enabled=false
}

TEST(control_missing_sections_default_off) {
    auto c = parseControl(R"({"version":1,"seq":1,"features":{}})");
    CHECK(c.has_value() && !c->instantBuild && !c->ignorePlacement && !c->ignorePopulation);
    CHECK(!c->noRegionLimit && !c->noMaterials);
}

TEST(control_reads_the_build_options_the_placement_hook_scopes_to_the_player) {
    auto c = parseControl(R"({"version":1,"seq":2,"features":{
        "build":{"enabled":true,"noRegionLimit":true,"noMaterials":false}}})");
    CHECK(c.has_value() && c->noRegionLimit && !c->noMaterials);
    c = parseControl(R"({"version":1,"seq":3,"features":{
        "build":{"enabled":false,"noRegionLimit":true,"noMaterials":true}}})");
    CHECK(c.has_value() && !c->noRegionLimit && !c->noMaterials);   // build.enabled=false
}

TEST(control_rejects_bad_version_or_shape) {
    CHECK(!parseControl(R"({"version":2,"seq":1,"features":{}})").has_value());
    CHECK(!parseControl(R"({"version":1,"features":{}})").has_value());
    CHECK(!parseControl(R"({"version":1,"seq":1})").has_value());
    CHECK(!parseControl("{broken").has_value());
}
