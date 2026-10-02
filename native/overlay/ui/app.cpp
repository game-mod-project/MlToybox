#include "app.h"

namespace mlt::ov {

App& app() {
    static App instance;
    return instance;
}

void disableOverlay(const std::string& reason) {
    App& a = app();
    if (a.state.exchange(OverlayState::Disabled) == OverlayState::Disabled) return;   // 첫 이유를 남긴다
    a.visible = false;
    a.wantMouse = false;
    a.wantKeyboard = false;
    a.wantText = false;
    std::lock_guard<std::mutex> lock(a.mutex);
    a.reason = reason;
}

void syncSettingsAtoms(App& a) {
    a.toggleVk = toggleKeyCode(a.settings.toggleKey);
    a.scale = a.settings.scale;
    a.inputLog = a.settings.inputLog;
}

}
