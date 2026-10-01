#pragma once
#include <string>

// bridge/overlay_status.json: 오버레이가 살아 있는지와, 그리기를 포기했다면 그 이유. Lua 가 읽어 status.json 의 overlay 로 합친다
namespace mlt::ov {

enum class OverlayState { Starting, Waiting, Ready, Disabled };

const char* overlayStateName(OverlayState s);

struct OverlayStatus {
    long long heartbeat = 0;
    OverlayState state = OverlayState::Starting;
    std::string reason;      // Disabled 일 때의 이유. 비어 있으면 null 로 쓴다
    bool visible = false;
    long long frames = 0;
    std::string font;        // "malgun" 또는 "default"
};

std::string renderOverlayStatus(const OverlayStatus& s);
}
