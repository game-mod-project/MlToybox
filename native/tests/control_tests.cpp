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
}

TEST(control_rejects_bad_version_or_shape) {
    CHECK(!parseControl(R"({"version":2,"seq":1,"features":{}})").has_value());
    CHECK(!parseControl(R"({"version":1,"features":{}})").has_value());
    CHECK(!parseControl(R"({"version":1,"seq":1})").has_value());
    CHECK(!parseControl("{broken").has_value());
}
