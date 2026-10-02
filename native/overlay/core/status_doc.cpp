#include "status_doc.h"

namespace mlt::ov {

std::optional<double> optNumber(const Json* obj, const char* key) {
    if (!obj || !obj->is_object()) return std::nullopt;
    auto it = obj->find(key);
    if (it == obj->end() || !it->is_number()) return std::nullopt;
    return it->get<double>();
}

static std::optional<long long> optInteger(const Json* obj, const char* key) {
    auto n = optNumber(obj, key);
    if (!n) return std::nullopt;
    return static_cast<long long>(*n);
}

static std::string stringOr(const Json* obj, const char* key) {
    return optString(obj, key).value_or("");
}

// { "이름": 수 } 꼴. 객체 자리에 온 배열(Lua 의 빈 표)은 빈 것으로 본다
static std::map<std::string, double> readNumberMap(const Json& v) {
    std::map<std::string, double> out;
    if (!v.is_object()) return out;
    for (auto it = v.begin(); it != v.end(); ++it) {
        if (it.value().is_number()) out[it.key()] = it.value().get<double>();
    }
    return out;
}

static CommandResult readCommand(const Json& r) {
    CommandResult c;
    c.ok = boolOr(&r, "ok", false);
    c.error = optString(&r, "error");
    c.reformed = optInt(&r, "reformed");
    c.requested = optInt(&r, "requested");
    c.added = optInt(&r, "added");
    auto it = r.find("squads");
    if (it != r.end() && it->is_array()) {
        std::vector<int> ids;
        for (const auto& v : *it) {
            if (v.is_number()) ids.push_back(static_cast<int>(v.get<double>()));
        }
        c.squads = std::move(ids);
    }
    return c;
}

static std::vector<RegionInfo> readRegionInfos(const Json& list) {
    std::vector<RegionInfo> out;
    for (const Json& r : list) {
        if (r.is_object()) out.push_back({ stringOr(&r, "key"), stringOr(&r, "name") });
    }
    return out;
}

static SpawnStatus readSpawn(const Json& v) {
    SpawnStatus s;
    s.disbanded = intOr(&v, "disbanded", 0);
    s.pending = intOr(&v, "pending", 0);
    if (const Json* byUnit = objectAt(&v, "byUnit")) {
        for (auto it = byUnit->begin(); it != byUnit->end(); ++it) {
            if (it.value().is_number()) s.byUnit.emplace_back(it.key(), static_cast<int>(it.value().get<double>()));
        }
    }
    return s;
}

static RetinueStatus readRetinue(const Json& v) {
    RetinueStatus r;
    if (const Json* squads = arrayAt(&v, "squads")) {
        for (const Json& s : *squads) {
            if (!s.is_object()) continue;
            RetinueSquad squad;
            squad.id = intOr(&s, "id", 0);
            squad.unit = stringOr(&s, "unit");
            squad.count = intOr(&s, "count", 0);
            squad.kind = stringOr(&s, "kind");
            r.squads.push_back(std::move(squad));
        }
    }
    r.editing = optInt(&v, "editing");
    return r;
}

static PopulationStatus readPopulation(const Json& v) {
    PopulationStatus p;
    p.families = intOr(&v, "families", 0);
    p.population = intOr(&v, "population", 0);
    p.homeless = intOr(&v, "homeless", 0);
    p.freeSlots = intOr(&v, "freeSlots", 0);
    p.natural = intOr(&v, "natural", 0);
    p.multiplied = intOr(&v, "multiplied", 0);
    p.unassigned = intOr(&v, "unassigned", 0);
    if (const Json* regions = arrayAt(&v, "regions")) {
        for (const Json& r : *regions) {
            if (!r.is_object()) continue;
            PopulationRegion region;
            region.key = stringOr(&r, "key");
            region.name = stringOr(&r, "name");
            region.families = intOr(&r, "families", 0);
            region.population = intOr(&r, "population", 0);
            region.homeless = intOr(&r, "homeless", 0);
            region.freeSlots = intOr(&r, "freeSlots", 0);
            region.unassigned = intOr(&r, "unassigned", 0);
            p.regions.push_back(std::move(region));
        }
    }
    return p;
}

static MercenaryStatus readMercenaries(const Json& v) {
    MercenaryStatus m;
    if (const Json* slots = arrayAt(&v, "slots")) {
        for (const Json& s : *slots) {
            if (s.is_object()) m.slots.push_back({ stringOr(&s, "name"), intOr(&s, "cost", 0), boolOr(&s, "custom", false) });
        }
    }
    m.hiredMine = intOr(&v, "hiredMine", 0);
    m.hiredAi = intOr(&v, "hiredAi", 0);
    m.refunded = intOr(&v, "refunded", 0);
    if (const Json* skipped = arrayAt(&v, "skipped")) {
        for (const Json& s : *skipped) {
            if (s.is_object()) m.skipped.push_back({ stringOr(&s, "name"), stringOr(&s, "reason") });
        }
    }
    m.note = optString(&v, "note");
    return m;
}

std::optional<StatusDoc> parseStatus(std::string_view text) {
    Json root = Json::parse(text.begin(), text.end(), nullptr, false);
    if (root.is_discarded() || !root.is_object()) return std::nullopt;
    StatusDoc s;
    s.heartbeat = optInteger(&root, "heartbeat").value_or(0);
    s.inGame = boolOr(&root, "inGame", false);
    s.appliedSeq = optInteger(&root, "appliedSeq");
    s.bridgeError = optString(&root, "bridgeError");

    if (auto it = root.find("features"); it != root.end() && (it->is_object() || it->is_array())) {
        std::map<std::string, FeatureStatus> features;
        if (it->is_object()) {
            for (auto f = it->begin(); f != it->end(); ++f) {
                if (!f.value().is_object()) continue;
                FeatureStatus fs;
                fs.active = boolOr(&f.value(), "active", false);
                fs.lastError = optString(&f.value(), "lastError");
                features[f.key()] = std::move(fs);
            }
        }
        s.features = std::move(features);
    }

    if (const Json* commands = objectAt(&root, "commands")) {
        std::vector<std::pair<std::string, CommandResult>> list;
        for (auto c = commands->begin(); c != commands->end(); ++c) {
            if (c.value().is_object()) list.emplace_back(c.key(), readCommand(c.value()));
        }
        s.commands = std::move(list);
    }

    if (const Json* native = objectAt(&root, "native")) {
        NativeStatus n;
        n.loaded = boolOr(native, "loaded", false);
        n.error = optString(native, "error");
        n.stale = boolOr(native, "stale", false);
        if (const Json* features = objectAt(native, "features")) {
            for (auto f = features->begin(); f != features->end(); ++f) {
                if (!f.value().is_object()) continue;
                NativeFeature nf;
                nf.installed = boolOr(&f.value(), "installed", false);
                nf.active = boolOr(&f.value(), "active", false);
                nf.lastError = optString(&f.value(), "lastError");
                n.features[f.key()] = std::move(nf);
            }
        }
        s.native = std::move(n);
    }

    if (const Json* lord = objectAt(&root, "lord")) {
        LordStatus l;
        l.treasury = optNumber(lord, "treasury");
        l.influence = optInt(lord, "influence");
        l.kingsFavour = optInt(lord, "kingsFavour");
        s.lord = l;
    }

    if (const Json* ids = arrayAt(&root, "resourceIds")) {
        std::vector<std::string> list;
        for (const Json& id : *ids) {
            if (id.is_string()) list.push_back(id.get<std::string>());
        }
        s.resourceIds = std::move(list);
    }
    if (auto it = root.find("resources"); it != root.end() && (it->is_object() || it->is_array())) s.resources = readNumberMap(*it);
    if (const Json* regions = arrayAt(&root, "regions")) {
        std::vector<RegionResources> list;
        for (const Json& r : *regions) {
            if (!r.is_object()) continue;
            RegionResources region;
            region.key = stringOr(&r, "key");
            region.name = stringOr(&r, "name");
            if (auto values = r.find("values"); values != r.end()) region.values = readNumberMap(*values);
            list.push_back(std::move(region));
        }
        s.regions = std::move(list);
    }
    if (const Json* regions = arrayAt(&root, "playerRegions")) s.playerRegions = readRegionInfos(*regions);
    if (const Json* spawn = objectAt(&root, "spawn")) s.spawn = readSpawn(*spawn);
    if (const Json* retinue = objectAt(&root, "retinue")) s.retinue = readRetinue(*retinue);
    if (const Json* population = objectAt(&root, "population")) s.population = readPopulation(*population);
    if (const Json* mercenaries = objectAt(&root, "mercenaries")) s.mercenaries = readMercenaries(*mercenaries);

    s.raw = std::move(root);
    return s;
}

}
