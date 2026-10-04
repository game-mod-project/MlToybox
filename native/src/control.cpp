#include "control.h"
#include "json.h"

namespace mlt {

static bool flag(const Json* features, const char* section, const char* name) {
    if (!features) return false;
    const Json* s = features->get(section);
    if (!s || !s->get("enabled") || !s->get("enabled")->asBool(false)) return false;
    const Json* f = s->get(name);
    return f && f->asBool(false);
}

static bool enabled(const Json* features, const char* section) {
    if (!features) return false;
    const Json* s = features->get(section);
    return s && s->get("enabled") && s->get("enabled")->asBool(false);
}

// 기능이 켜져 있을 때의 정수 설정값. 꺼져 있거나 숫자가 아니거나 lo 보다 작으면 def, hi 보다 크면 hi
static int number(const Json* features, const char* section, const char* name, int def, int lo, int hi) {
    if (!enabled(features, section)) return def;
    const Json* f = features->get(section)->get(name);
    if (!f || f->type != Json::Type::Number || f->n < lo) return def;
    return f->n > hi ? hi : static_cast<int>(f->n);
}

std::optional<NativeControl> parseControl(std::string_view text) {
    auto j = parseJson(text);
    if (!j || j->type != Json::Type::Object) return std::nullopt;
    const Json* version = j->get("version");
    const Json* seq = j->get("seq");
    const Json* features = j->get("features");
    if (!version || version->asNumber(0) != 1) return std::nullopt;
    if (!seq || seq->type != Json::Type::Number) return std::nullopt;
    if (!features || features->type != Json::Type::Object) return std::nullopt;
    NativeControl c;
    c.seq = static_cast<long long>(seq->n);
    c.instantBuild = flag(features, "build", "instantBuild");
    c.ignorePlacement = flag(features, "build", "ignorePlacement");
    c.ignorePopulation = flag(features, "military", "ignorePopulation");
    c.noRegionLimit = flag(features, "build", "noRegionLimit");
    c.noMaterials = flag(features, "build", "noMaterials");
    c.upgradeFree = enabled(features, "upgrade");
    c.immigrationMonthly = number(features, "population", "monthlyFamilies", 0, 0, 31);
    c.immigrationMultiplier = number(features, "population", "multiplier", 1, 1, 10);
    return c;
}

}
