#include "status.h"
#include "json.h"

namespace mlt {

std::string renderStatus(long long heartbeat, long long appliedSeq, const std::vector<FeatureState>& features, std::string_view extra) {
    std::string out = "{\"version\":1,\"heartbeat\":" + std::to_string(heartbeat) + ",\"appliedSeq\":" + std::to_string(appliedSeq);
    if (!features.empty()) {
        out += ",\"features\":{";
        for (size_t i = 0; i < features.size(); ++i) {
            const auto& f = features[i];
            if (i) out += ",";
            out += "\"" + escapeJson(f.name) + "\":{\"installed\":" + (f.installed ? "true" : "false") + ",\"active\":" + (f.active ? "true" : "false");
            if (!f.error.empty()) out += ",\"lastError\":\"" + escapeJson(f.error) + "\"";
            out += "}";
        }
        out += "}";
    }
    if (!extra.empty()) {
        out += ",";
        out += extra;
    }
    out += "}";
    return out;
}

}
