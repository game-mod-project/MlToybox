#include "control_doc.h"
#include <algorithm>
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

static std::optional<int> asInt(const Json& v) {
    auto n = asInteger(v);
    if (!n || *n < -2147483648LL || *n > 2147483647LL) return std::nullopt;
    return static_cast<int>(*n);
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
    return asInt(*it);
}

int intOr(const Json* obj, const char* key, int def) {
    auto v = optInt(obj, key);
    return v ? *v : def;
}

std::optional<std::string> optString(const Json* obj, const char* key) {
    if (!obj || !obj->is_object()) return std::nullopt;
    auto it = obj->find(key);
    if (it == obj->end() || !it->is_string()) return std::nullopt;
    return it->get<std::string>();
}

const Json* objectAt(const Json* obj, const char* key) {
    if (!obj || !obj->is_object()) return nullptr;
    auto it = obj->find(key);
    return (it != obj->end() && it->is_object()) ? &*it : nullptr;
}

const Json* arrayAt(const Json* obj, const char* key) {
    if (!obj || !obj->is_object()) return nullptr;
    auto it = obj->find(key);
    return (it != obj->end() && it->is_array()) ? &*it : nullptr;
}

// { "이름": 정수 } 꼴의 객체를 읽는다. 정수가 아니거나 음수인 값은 건너뛴다
static std::map<std::string, int> readIntMap(const Json* obj) {
    std::map<std::string, int> out;
    if (!obj || !obj->is_object()) return out;
    for (auto it = obj->begin(); it != obj->end(); ++it) {
        auto n = asInt(it.value());
        if (n && *n >= 0) out[it.key()] = *n;
    }
    return out;
}

static Json writeIntMap(const std::map<std::string, int>& map) {
    Json out = Json::object();
    for (const auto& [key, value] : map) out[key] = value;
    return out;
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

static void setOptional(Json& obj, const char* key, const std::optional<std::string>& value) {
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

MilitarySettings ControlDoc::military() const {
    const Json* f = findFeature("military");
    const MilitarySettings d;
    MilitarySettings v;
    v.enabled = boolOr(f, "enabled", d.enabled);
    v.ignoreEquipment = boolOr(f, "ignoreEquipment", d.ignoreEquipment);
    v.ignorePopulation = boolOr(f, "ignorePopulation", d.ignorePopulation);
    v.zeroUpkeep = boolOr(f, "zeroUpkeep", d.zeroUpkeep);
    v.unlimitedSquads = boolOr(f, "unlimitedSquads", d.unlimitedSquads);
    return v;
}

void ControlDoc::setMilitary(const MilitarySettings& v) {
    Json& f = feature("military");
    f["enabled"] = v.enabled;
    f["ignoreEquipment"] = v.ignoreEquipment;
    f["ignorePopulation"] = v.ignorePopulation;
    f["zeroUpkeep"] = v.zeroUpkeep;
    f["unlimitedSquads"] = v.unlimitedSquads;
}

PopulationSettings ControlDoc::population() const {
    const Json* f = findFeature("population");
    PopulationSettings v;
    v.enabled = boolOr(f, "enabled", false);
    v.multiplier = intOr(f, "multiplier", 2);
    v.monthlyFamilies = intOr(f, "monthlyFamilies", 0);
    v.houseCapacity = intOr(f, "houseCapacity", 1);
    v.targetFamilies = intOr(f, "targetFamilies", 0);
    v.regionTargets = readIntMap(objectAt(f, "regionTargets"));
    return v;
}

void ControlDoc::setPopulation(const PopulationSettings& v) {
    Json& f = feature("population");
    f["enabled"] = v.enabled;
    f["multiplier"] = v.multiplier;
    f["monthlyFamilies"] = v.monthlyFamilies;
    f["houseCapacity"] = v.houseCapacity;
    f["targetFamilies"] = v.targetFamilies;
    f["regionTargets"] = writeIntMap(v.regionTargets);
}

ResourcesSettings ControlDoc::resources() const {
    const Json* f = findFeature("resources");
    ResourcesSettings v;
    v.enabled = boolOr(f, "enabled", false);
    v.intervalSec = intOr(f, "intervalSec", 2);
    v.targets = readIntMap(objectAt(f, "targets"));
    if (const Json* regions = objectAt(f, "regionTargets")) {
        for (auto it = regions->begin(); it != regions->end(); ++it) {
            if (it.value().is_object()) v.regionTargets[it.key()] = readIntMap(&it.value());
        }
    }
    return v;
}

void ControlDoc::setResources(const ResourcesSettings& v) {
    Json& f = feature("resources");
    f["enabled"] = v.enabled;
    f["intervalSec"] = v.intervalSec;
    f["targets"] = writeIntMap(v.targets);
    Json regions = Json::object();
    for (const auto& [key, targets] : v.regionTargets) regions[key] = writeIntMap(targets);
    f["regionTargets"] = std::move(regions);
}

MercSettings ControlDoc::mercenaries() const {
    const Json* f = findFeature("mercenaries");
    MercSettings v;
    v.enabled = boolOr(f, "enabled", false);
    v.refund = boolOr(f, "refund", true);
    v.lockFromAi = boolOr(f, "lockFromAi", true);
    if (const Json* companies = arrayAt(f, "companies")) {
        for (const Json& c : *companies) {
            if (!c.is_object()) continue;   // 손으로 고친 설정의 이상한 항목은 건너뛴다
            MercCompany company;
            company.name = optString(&c, "name").value_or("");
            if (const Json* units = arrayAt(&c, "units")) {
                for (const Json& u : *units) {
                    if (u.is_string()) company.units.push_back(u.get<std::string>());
                }
            }
            company.cost = intOr(&c, "cost", 0);
            company.region = optString(&c, "region");
            company.banner = optString(&c, "banner");
            company.enabled = boolOr(&c, "enabled", true);
            v.companies.push_back(std::move(company));
        }
    }
    return v;
}

void ControlDoc::setMercenaries(const MercSettings& v) {
    Json& f = feature("mercenaries");
    f["enabled"] = v.enabled;
    f["refund"] = v.refund;
    f["lockFromAi"] = v.lockFromAi;
    // 항목 안의 모르는 키(다음 판의 패널이나 손 편집이 넣은 것)를 남긴다: 같은 이름의 예전 항목에서 시작한다
    std::map<std::string, Json> previous;
    if (const Json* old = arrayAt(&f, "companies")) {
        for (const Json& item : *old) {
            if (!item.is_object()) continue;
            if (auto name = optString(&item, "name")) previous.emplace(*name, item);
        }
    }
    Json companies = Json::array();
    for (const MercCompany& c : v.companies) {
        const auto kept = previous.find(c.name);
        Json company = kept != previous.end() ? kept->second : Json::object();
        company["name"] = c.name;
        company["units"] = c.units;
        company["cost"] = c.cost;
        setOptional(company, "region", c.region);   // 고르지 않았으면 키를 쓰지 않는다(패널과 같다)
        setOptional(company, "banner", c.banner);
        company["enabled"] = c.enabled;
        companies.push_back(std::move(company));
    }
    f["companies"] = std::move(companies);
}

StorageSettings ControlDoc::storage() const {
    const Json* f = findFeature("storage");
    StorageSettings v;
    v.enabled = boolOr(f, "enabled", false);
    v.intervalSec = intOr(f, "intervalSec", 5);
    if (const Json* limits = objectAt(f, "limits")) {
        for (auto it = limits->begin(); it != limits->end(); ++it) {
            if (!it.value().is_object()) continue;   // 손으로 고친 설정의 이상한 항목은 건너뛴다
            const std::map<std::string, int> values = readIntMap(&it.value());
            StorageLimits entry;
            if (auto found = values.find("generic"); found != values.end()) entry.generic = found->second;
            if (auto found = values.find("large"); found != values.end()) entry.large = found->second;
            if (auto found = values.find("pantry"); found != values.end()) entry.pantry = found->second;
            if (!entry.empty()) v.limits[it.key()] = entry;
        }
    }
    return v;
}

void ControlDoc::setStorage(const StorageSettings& v) {
    Json& f = feature("storage");
    f["enabled"] = v.enabled;
    f["intervalSec"] = v.intervalSec;
    Json limits = Json::object();
    for (const auto& [id, entry] : v.limits) {
        if (entry.empty()) continue;   // 값이 하나도 없으면 그 건물은 쓰지 않는다
        Json item = Json::object();
        setOptional(item, "generic", entry.generic);
        setOptional(item, "large", entry.large);
        setOptional(item, "pantry", entry.pantry);
        limits[id] = std::move(item);
    }
    f["limits"] = std::move(limits);
}

// 손으로 고친 값은 범위 안으로 맞춰 읽는다(네이티브 DLL 도 같은 범위로 읽는다)
static MoodValue readMoodValue(const Json* obj) {
    MoodValue v;
    v.fixed = std::clamp(intOr(obj, "fixed", v.fixed), 0, 100);
    v.good = std::clamp(intOr(obj, "good", v.good), 1, 10);
    v.bad = std::clamp(intOr(obj, "bad", v.bad), 0, 100);
    return v;
}

static MoodPair readMoodPair(const Json* obj) {
    return { readMoodValue(objectAt(obj, "approval")), readMoodValue(objectAt(obj, "order")) };
}

MoodSettings ControlDoc::mood() const {
    const Json* f = findFeature("mood");
    MoodSettings v;
    v.enabled = boolOr(f, "enabled", false);
    v.common = readMoodPair(f);
    if (const Json* regions = objectAt(f, "regions")) {
        for (auto it = regions->begin(); it != regions->end(); ++it) {
            if (it.key().empty() || !it.value().is_object()) continue;   // 손으로 고친 설정의 이상한 항목은 건너뛴다
            v.regions[it.key()] = readMoodPair(&it.value());
        }
    }
    return v;
}

static void writeMoodValue(Json& obj, const char* key, const MoodValue& v) {
    if (!obj.contains(key) || !obj[key].is_object()) obj[key] = Json::object();
    Json& out = obj[key];
    out["fixed"] = v.fixed;
    out["good"] = v.good;
    out["bad"] = v.bad;
}

static void writeMoodPair(Json& obj, const MoodPair& pair) {
    writeMoodValue(obj, "approval", pair.approval);
    writeMoodValue(obj, "order", pair.order);
}

void ControlDoc::setMood(const MoodSettings& v) {
    Json& f = feature("mood");
    f["enabled"] = v.enabled;
    writeMoodPair(f, v.common);
    // 영지 항목 안의 모르는 키를 남긴다: 같은 영지의 예전 항목에서 시작한다
    const Json* previous = objectAt(&f, "regions");
    Json regions = Json::object();
    for (const auto& [key, pair] : v.regions) {
        const Json* kept = objectAt(previous, key.c_str());
        Json item = kept ? *kept : Json::object();
        writeMoodPair(item, pair);
        regions[key] = std::move(item);
    }
    f["regions"] = std::move(regions);
}

void ControlDoc::setCommands(const std::vector<Json>& commands) {
    Json list = Json::array();
    for (const auto& c : commands) list.push_back(c);
    root_["commands"] = std::move(list);
}

}
