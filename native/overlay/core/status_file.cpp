#include "status_file.h"
#include "control_doc.h"

namespace mlt::ov {

const char* overlayStateName(OverlayState s) {
    switch (s) {
        case OverlayState::Starting: return "starting";
        case OverlayState::Waiting: return "waiting";
        case OverlayState::Ready: return "ready";
        case OverlayState::Disabled: return "disabled";
    }
    return "starting";
}

std::string renderOverlayStatus(const OverlayStatus& s) {
    Json root = Json::object();
    root["heartbeat"] = s.heartbeat;
    root["state"] = overlayStateName(s.state);
    if (s.reason.empty()) root["reason"] = nullptr;
    else root["reason"] = s.reason;
    root["visible"] = s.visible;
    root["frames"] = s.frames;
    root["font"] = s.font;
    return root.dump();
}

}
