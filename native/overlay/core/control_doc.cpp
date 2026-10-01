#include "control_doc.h"
#include <cmath>

namespace mlt::ov {

static std::optional<long long> asInteger(const Json& v) {
    if (v.is_number_integer()) return v.get<long long>();
    if (v.is_number_float()) {
        double d = v.get<double>();
        if (std::floor(d) == d && std::fabs(d) < 9e15) return static_cast<long long>(d);
    }
    return std::nullopt;
}

bool boolOr(const Json* obj, const char* key, bool def) {
    if (!obj || !obj->is_object()) return def;
    auto it = obj->find(key);
    return (it != obj->end() && it->is_boolean()) ? it->get<bool>() : def;
}

std::optional<int> optInt(const Json* obj, const char* key) {
    if (!obj || !obj->is_object()) return std::nullopt;
    auto it = obj->find(key);
    if (it == obj->end()) return std::nullopt;
    auto n = asInteger(*it);
    if (!n || *n < -2147483648LL || *n > 2147483647LL) return std::nullopt;
    return static_cast<int>(*n);
}

int intOr(const Json* obj, const char* key, int def) {
    auto v = optInt(obj, key);
    return v ? *v : def;
}

const Json* objectAt(const Json* obj, const char* key) {
    if (!obj || !obj->is_object()) return nullptr;
    auto it = obj->find(key);
    return (it != obj->end() && it->is_object()) ? &*it : nullptr;
}

ControlDoc::ControlDoc() {
    root_ = Json::object();
    root_["version"] = 1;
    root_["seq"] = 0;
    root_["features"] = Json::object();
    root_["commands"] = Json::array();
}

ControlDoc ControlDoc::parse(std::string_view text) {
    return tryParse(text).value_or(ControlDoc());
}

std::optional<ControlDoc> ControlDoc::tryParse(std::string_view text) {
    ControlDoc doc;
    Json parsed = Json::parse(text.begin(), text.end(), nullptr, false);
    if (parsed.is_discarded() || !parsed.is_object()) return std::nullopt;
    doc.root_ = std::move(parsed);
    Json& root = doc.root_;
    if (!root.contains("features") || !root["features"].is_object()) root["features"] = Json::object();
    if (!root.contains("commands") || !root["commands"].is_array()) root["commands"] = Json::array();

    // 옛 버전은 국고·영향력을 자원 목표(targets.Treasury/Influence)에 넣었다.
    // 영주 설정이 없을 때만 옮기고, 자원 목표에서는 항상 뺀다 (패널의 LordControl.MigrateFrom)
    // (기능 객체를 새로 넣으면 다른 기능을 가리키던 참조가 무효가 되므로, 값을 먼저 읽고 지운 뒤에 영주 설정을 쓴다)
    std::optional<LordSettings> migrated;
    {
        Json& features = root["features"];
        const bool hadLord = features.contains("lord");
        if (features.contains("resources") && features["resources"].is_object()) {
            Json& resources = features["resources"];
            if (resources.contains("targets") && resources["targets"].is_object()) {
                Json& targets = resources["targets"];
                if (!hadLord) {
                    LordSettings lord;
                    if (auto t = optInt(&targets, "Treasury")) lord.treasury = *t;
                    if (auto i = optInt(&targets, "Influence")) lord.influence = *i;
                    if (lord.treasury || lord.influence) {
                        lord.enabled = boolOr(&resources, "enabled", false);
                        migrated = lord;
                    }
                }
                targets.erase("Treasury");
                targets.erase("Influence");
            }
        }
    }
    if (migrated) doc.setLord(*migrated);
    return doc;
}

std::string ControlDoc::dump() const {
    // 잘못된 UTF-8 이 섞여 있어도 예외를 던지지 않고 대체 문자(U+FFFD)로 쓴다
    return root_.dump(2, ' ', false, Json::error_handler_t::replace);
}

long long ControlDoc::seq() const {
    auto it = root_.find("seq");
    if (it == root_.end()) return 0;
    auto n = asInteger(*it);
    return n ? *n : 0;
}

void ControlDoc::setSeq(long long seq) {
    root_["version"] = 1;
    root_["seq"] = seq;
}

Json& ControlDoc::feature(const char* name) {
    Json& features = root_["features"];
    if (!features.is_object()) features = Json::object();
    if (!features.contains(name) || !features[name].is_object()) features[name] = Json::object();
    return features[name];
}

const Json* ControlDoc::findFeature(const char* name) const {
    auto it = root_.find("features");
    if (it == root_.end()) return nullptr;
    return objectAt(&*it, name);
}

BuildSettings ControlDoc::build() const {
    const Json* f = findFeature("build");
    const BuildSettings d;
    BuildSettings v;
    v.enabled = boolOr(f, "enabled", d.enabled);
    v.ignorePlacement = boolOr(f, "ignorePlacement", d.ignorePlacement);
    v.instantBuild = boolOr(f, "instantBuild", d.instantBuild);
    v.instantRepair = boolOr(f, "instantRepair", d.instantRepair);
    v.noMaterials = boolOr(f, "noMaterials", d.noMaterials);
    v.noRegionLimit = boolOr(f, "noRegionLimit", d.noRegionLimit);
    return v;
}

void ControlDoc::setBuild(const BuildSettings& v) {
    Json& f = feature("build");
    f["enabled"] = v.enabled;
    f["ignorePlacement"] = v.ignorePlacement;
    f["instantBuild"] = v.instantBuild;
    f["instantRepair"] = v.instantRepair;
    f["noMaterials"] = v.noMaterials;
    f["noRegionLimit"] = v.noRegionLimit;
}

UpgradeSettings ControlDoc::upgrade() const {
    UpgradeSettings v;
    v.enabled = boolOr(findFeature("upgrade"), "enabled", false);
    return v;
}

void ControlDoc::setUpgrade(const UpgradeSettings& v) {
    feature("upgrade")["enabled"] = v.enabled;
}

LordSettings ControlDoc::lord() const {
    const Json* f = findFeature("lord");
    LordSettings v;
    v.enabled = boolOr(f, "enabled", false);
    v.intervalSec = intOr(f, "intervalSec", 2);
    v.treasury = optInt(f, "treasury");
    v.influence = optInt(f, "influence");
    v.kingsFavour = optInt(f, "kingsFavour");
    return v;
}

static void setOptional(Json& obj, const char* key, const std::optional<int>& value) {
    if (value) obj[key] = *value;
    else obj.erase(key);
}

void ControlDoc::setLord(const LordSettings& v) {
    Json& f = feature("lord");
    f["enabled"] = v.enabled;
    f["intervalSec"] = v.intervalSec;
    setOptional(f, "treasury", v.treasury);
    setOptional(f, "influence", v.influence);
    setOptional(f, "kingsFavour", v.kingsFavour);
}

void ControlDoc::setCommands(const std::vector<Json>& commands) {
    Json list = Json::array();
    for (const auto& c : commands) list.push_back(c);
    root_["commands"] = std::move(list);
}

}
