#include "settings.h"
#include "control_doc.h"
#include "status_doc.h"
#include <algorithm>
#include <cmath>
#include <utility>

namespace mlt::ov {

namespace {
// 가상 키 코드(VK_*). windows.h 없이 쓰도록 값으로 적는다
const std::vector<std::pair<std::string, int>> kKeys = {
    { "Insert", 0x2D }, { "Home", 0x24 }, { "End", 0x23 }, { "F7", 0x76 }, { "F8", 0x77 }, { "F9", 0x78 },
};
}

const std::vector<std::string>& toggleKeyNames() {
    static const std::vector<std::string> names = [] {
        std::vector<std::string> out;
        for (const auto& k : kKeys) out.push_back(k.first);
        return out;
    }();
    return names;
}

int toggleKeyCode(const std::string& name) {
    for (const auto& k : kKeys) {
        if (k.first == name) return k.second;
    }
    return kKeys.front().second;
}

static bool knownKey(const std::string& name) {
    for (const auto& k : kKeys) {
        if (k.first == name) return true;
    }
    return false;
}

OverlaySettings parseSettings(std::string_view text) {
    OverlaySettings s;
    Json root = Json::parse(text.begin(), text.end(), nullptr, false);
    if (root.is_discarded() || !root.is_object()) return s;
    if (auto key = optString(&root, "toggleKey"); key && knownKey(*key)) s.toggleKey = *key;
    if (auto scale = optNumber(&root, "scale"); scale && std::isfinite(*scale)) {
        s.scale = std::clamp(static_cast<float>(*scale), kScaleMin, kScaleMax);
    }
    if (const Json* window = objectAt(&root, "window")) {
        s.x = std::clamp(intOr(window, "x", s.x), -10000, 10000);
        s.y = std::clamp(intOr(window, "y", s.y), -10000, 10000);
        s.w = std::clamp(intOr(window, "w", s.w), 320, 4000);
        s.h = std::clamp(intOr(window, "h", s.h), 240, 4000);
    }
    s.startOpen = boolOr(&root, "startOpen", false);
    if (auto tab = optString(&root, "devTab")) s.devTab = *tab;
    s.inputLog = boolOr(&root, "inputLog", false);
    if (const Json* columns = arrayAt(&root, "resourceColumns"); columns && columns->size() == kResourceColumnCount) {
        for (const Json& v : *columns) {
            if (!v.is_number() || !(v.get<double>() > 0.0) || !std::isfinite(v.get<double>())) break;
            s.resourceColumns.push_back(static_cast<float>(v.get<double>()));
        }
        if (s.resourceColumns.size() != kResourceColumnCount) s.resourceColumns.clear();
    }
    return s;
}

std::string dumpSettings(const OverlaySettings& s) {
    Json root = Json::object();
    root["toggleKey"] = s.toggleKey;
    root["scale"] = std::round(s.scale * 100.0f) / 100.0f;
    Json window = Json::object();
    window["x"] = s.x;
    window["y"] = s.y;
    window["w"] = s.w;
    window["h"] = s.h;
    root["window"] = std::move(window);
    root["startOpen"] = s.startOpen;
    if (s.devTab.empty()) root["devTab"] = nullptr;
    else root["devTab"] = s.devTab;
    if (s.inputLog) root["inputLog"] = true;   // 켰을 때만 적는다
    if (s.resourceColumns.size() == kResourceColumnCount) {   // 사용자가 열 너비를 바꿨을 때만 적는다
        Json columns = Json::array();
        for (float share : s.resourceColumns) columns.push_back(std::round(static_cast<double>(share) * 10.0) / 10.0);
        root["resourceColumns"] = std::move(columns);
    }
    return root.dump(2);
}

}
