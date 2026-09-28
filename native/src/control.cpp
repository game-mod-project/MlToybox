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
    return c;
}

}
