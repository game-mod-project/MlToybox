#include "clipboard_sync.h"
#include "overlay/render/clipboard.h"
#include <imgui.h>
#include <mutex>
#include <utility>

namespace mlt::ov {

const ClipboardApi& systemClipboard() {
    static const ClipboardApi api = { writeClipboardText, readClipboardText, clipboardSequence };
    return api;
}

void installClipboardCallbacks() {
    // 둘 다 프레임 안에서(화면 스레드가 ImGui 잠금과 App::mutex 를 쥔 채) 불린다. 그래서 여기서는 잠그지 않는다
    ImGuiPlatformIO& platform = ImGui::GetPlatformIO();
    platform.Platform_SetClipboardTextFn = [](ImGuiContext*, const char* text) {
        App& a = app();
        a.clipboardOut = text ? text : "";
        a.clipboardPending = true;
        a.clipboardIn = a.clipboardOut;   // 시스템 클립보드에 나가기 전에 붙여넣어도 방금 복사한 글이 나온다
    };
    platform.Platform_GetClipboardTextFn = [](ImGuiContext*) -> const char* {
        static std::string held;          // 화면 스레드만 쓴다. ImGui 가 읽는 동안 살아 있어야 한다
        held = app().clipboardIn;
        return held.c_str();
    };
}

void ClipboardSync::tick(App& a) {
    {
        std::lock_guard<std::mutex> lock(a.mutex);
        if (a.clipboardPending) {         // 새로 복사한 글이 아직 나가지 못한 글을 대신한다
            a.clipboardPending = false;
            out_ = a.clipboardOut;
            triesLeft_ = kWriteTries;
        }
    }
    if (triesLeft_ > 0) {
        if (api_.write(out_)) triesLeft_ = 0;
        else --triesLeft_;                // 다른 프로그램이 클립보드를 잡고 있다. 다음 회차에 다시 쓴다
    }

    // 붙여넣기에 쓸 글. 글자 칸에 커서가 있는 동안에만, 시스템 클립보드가 바뀌었을 때만 읽는다
    if (!a.visible.load() || !a.wantText.load()) return;
    if (triesLeft_ > 0) return;           // 방금 복사한 글이 아직 나가지 않았다. 시스템의 옛 글로 덮지 않는다
    const unsigned long sequence = api_.sequence();
    if (seen_ && sequence == sequence_) return;
    auto text = api_.read();
    if (!text) return;                    // 열지 못했다. 다음 회차에 다시 읽는다
    seen_ = true;
    sequence_ = sequence;
    std::lock_guard<std::mutex> lock(a.mutex);
    a.clipboardIn = std::move(*text);
}

}
