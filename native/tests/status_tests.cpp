#include "test.h"
#include "json.h"
#include "status.h"

using namespace mlt;

TEST(status_renders_features_and_omits_empty_error) {
    std::vector<FeatureState> fs{ { "instant_build", true, true, "" }, { "placement", false, false, "pattern not found" } };
    auto j = parseJson(renderStatus(123, 7, fs));
    CHECK(j.has_value());
    CHECK(j->get("version")->asNumber(0) == 1);
    CHECK(j->get("heartbeat")->asNumber(0) == 123);
    CHECK(j->get("appliedSeq")->asNumber(0) == 7);
    CHECK(j->get("features")->get("instant_build")->get("active")->asBool(false));
    CHECK(j->get("features")->get("instant_build")->get("lastError") == nullptr);
    CHECK(j->get("features")->get("placement")->get("lastError")->s == "pattern not found");
}

TEST(status_without_features_omits_map) {
    auto j = parseJson(renderStatus(1, -1, {}));
    CHECK(j.has_value() && j->get("features") == nullptr);
}
