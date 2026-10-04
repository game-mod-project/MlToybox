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

// 객체 안의 정수 하나. 없거나 숫자가 아니면 def, 범위를 벗어나면 가까운 끝
static int clamped(const Json* object, const char* name, int def, int lo, int hi) {
    const Json* f = object ? object->get(name) : nullptr;
    if (!f || f->type != Json::Type::Number) return def;
    if (f->n < lo) return lo;
    return f->n > hi ? hi : static_cast<int>(f->n);
}

static MoodStat moodStat(const Json* stat) {
    if (!stat || stat->type != Json::Type::Object) return {};
    return MoodStat{ clamped(stat, "fixed", 0, 0, 100), clamped(stat, "good", 1, 1, 10), clamped(stat, "bad", 100, 0, 100) };
}

static MoodSet moodSet(const Json* set) {
    if (!set || set->type != Json::Type::Object) return {};
    return MoodSet{ moodStat(set->get("approval")), moodStat(set->get("order")) };
}

static MoodControl moodControl(const Json* features) {
    MoodControl m;
    if (!enabled(features, "mood")) return m;
    const Json* section = features->get("mood");
    m.common = moodSet(section);
    const Json* regions = section->get("regions");
    if (regions && regions->type == Json::Type::Object && regions->o) {
        constexpr size_t kMaxRegions = 64, kMaxKey = 64;
        for (const auto& [key, value] : *regions->o) {
            if (m.regions.size() >= kMaxRegions) break;
            if (key.empty() || key.size() > kMaxKey) continue;
            m.regions.emplace_back(key, moodSet(&value));
        }
    }
    return m;
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
    c.noRegionLimit = flag(features, "build", "noRegionLimit");
    c.noMaterials = flag(features, "build", "noMaterials");
    c.upgradeFree = enabled(features, "upgrade");
    c.immigrationMonthly = number(features, "population", "monthlyFamilies", 0, 0, 31);
    c.immigrationMultiplier = number(features, "population", "multiplier", 1, 1, 10);
    c.houseCapacity = number(features, "population", "houseCapacity", 1, 1, 10);
    c.mood = moodControl(features);
    return c;
}

}
