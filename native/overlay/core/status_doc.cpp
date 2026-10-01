#include "status_doc.h"

namespace mlt::ov {

std::optional<std::string> optString(const Json* obj, const char* key) {
    if (!obj || !obj->is_object()) return std::nullopt;
    auto it = obj->find(key);
    if (it == obj->end() || !it->is_string()) return std::nullopt;
    return it->get<std::string>();
}

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

    s.raw = std::move(root);
    return s;
}

}
