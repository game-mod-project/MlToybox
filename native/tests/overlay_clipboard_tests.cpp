#include "test.h"
#include "overlay/ui/app.h"
#include "overlay/ui/clipboard_sync.h"
#include <imgui.h>
#include <optional>
#include <string>
#include <vector>

using namespace mlt::ov;

// 복사·붙여넣기는 화면 스레드가 Windows 클립보드를 만지지 않고 한다: 화면 스레드는 App 에 적어 둔 글만 읽고 쓰고,
// 작업 스레드가 0.1초마다 시스템 클립보드와 맞춘다. 사용자의 실제 클립보드는 테스트에서 건드리지 않는다(가짜로 바꿔 끼운다).
namespace {
std::vector<std::string> g_written;      // 시스템 클립보드에 쓰려고 한 글(성공·실패 모두)
int g_writeFailures = 0;                 // 앞으로 실패할 횟수(다른 프로그램이 클립보드를 잡고 있다)
std::optional<std::string> g_system;     // 시스템 클립보드의 글. 값 없음 = 열지 못함
unsigned long g_sequence = 0;
int g_reads = 0;

const ClipboardApi kFake = {
    [](const std::string& text) {
        g_written.push_back(text);
        if (g_writeFailures > 0) {
            --g_writeFailures;
            return false;
        }
        g_system = text;
        ++g_sequence;
        return true;
    },
    []() -> std::optional<std::string> {
        ++g_reads;
        return g_system;
    },
    [] { return g_sequence; },
};

struct Scene {
    App& a = app();
    ClipboardSync sync{ kFake };

    Scene() {
        g_written.clear();
        g_writeFailures = 0;
        g_system = "";
        g_sequence = 1;
        g_reads = 0;
        a.clipboardOut.clear();
        a.clipboardIn.clear();
        a.clipboardPending = false;
        a.visible = false;
        a.wantText = false;
    }
    ~Scene() {
        a.clipboardOut.clear();
        a.clipboardIn.clear();
        a.clipboardPending = false;
        a.visible = false;
        a.wantText = false;
    }
    void copy(const std::string& text) {   // 화면에서 복사했다
        a.clipboardOut = text;
        a.clipboardPending = true;
    }
};
}

TEST(overlay_clipboard_write_is_retried_while_the_clipboard_is_busy) {
    Scene s;
    s.copy("검사대");
    g_writeFailures = 2;
    s.sync.tick(s.a);
    s.sync.tick(s.a);
    CHECK(g_written.size() == 2 && !g_system->size());        // 두 번 실패했다
    s.sync.tick(s.a);
    CHECK(g_written.size() == 3 && g_system == "검사대");     // 세 번째에 나갔다
    s.sync.tick(s.a);
    CHECK(g_written.size() == 3);                              // 나간 뒤에는 다시 쓰지 않는다
}

TEST(overlay_clipboard_a_newer_copy_replaces_the_one_still_waiting) {
    Scene s;
    s.copy("옛 글");
    g_writeFailures = 1;
    s.sync.tick(s.a);
    s.copy("새 글");
    s.sync.tick(s.a);
    CHECK(g_system == "새 글" && g_written.back() == "새 글");
}

TEST(overlay_clipboard_write_gives_up_after_two_seconds) {
    Scene s;
    s.copy("x");
    g_writeFailures = 1000;
    for (int i = 0; i < ClipboardSync::kWriteTries + 5; ++i) s.sync.tick(s.a);
    CHECK(static_cast<int>(g_written.size()) == ClipboardSync::kWriteTries);
}

TEST(overlay_clipboard_is_read_only_while_a_text_field_has_the_cursor_and_only_when_it_changed) {
    Scene s;
    g_system = "밖에서 복사한 글";
    s.sync.tick(s.a);
    CHECK(g_reads == 0 && s.a.clipboardIn.empty());            // 글자 칸에 커서가 없으면 읽지 않는다
    s.a.visible = true;
    s.a.wantText = true;
    s.sync.tick(s.a);
    CHECK(g_reads == 1 && s.a.clipboardIn == "밖에서 복사한 글");
    s.sync.tick(s.a);
    CHECK(g_reads == 1);                                       // 바뀌지 않았으면 다시 읽지 않는다
    g_system = "다른 글";
    ++g_sequence;
    s.sync.tick(s.a);
    CHECK(g_reads == 2 && s.a.clipboardIn == "다른 글");

    g_system.reset();                                          // 열지 못했다: 직전 글을 두고 다음 회차에 다시 읽는다
    ++g_sequence;
    s.sync.tick(s.a);
    CHECK(g_reads == 3 && s.a.clipboardIn == "다른 글");
    g_system = "풀린 뒤의 글";
    s.sync.tick(s.a);
    CHECK(g_reads == 4 && s.a.clipboardIn == "풀린 뒤의 글");
}

TEST(overlay_clipboard_imgui_copies_and_pastes_through_the_app) {
    Scene s;
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    installClipboardCallbacks();
    ImGui::SetClipboardText("가나다");                         // 화면에서 Ctrl+C
    CHECK(s.a.clipboardPending && s.a.clipboardOut == "가나다");
    CHECK(std::string(ImGui::GetClipboardText()) == "가나다"); // 바로 붙여넣어도 그 글이다
    s.a.clipboardIn = "밖에서 복사한 글";                      // 작업 스레드가 시스템 클립보드에서 읽어 왔다
    CHECK(std::string(ImGui::GetClipboardText()) == "밖에서 복사한 글");
    ImGui::DestroyContext();
}

// 실제 Windows 클립보드를 읽는 함수는 사용자의 클립보드를 바꾸지 않는다(쓰기는 테스트하지 않는다)
TEST(overlay_clipboard_reading_the_system_clipboard_leaves_it_alone) {
    const ClipboardApi& api = systemClipboard();
    const unsigned long before = api.sequence();
    const auto first = api.read();
    const auto second = api.read();
    CHECK(api.sequence() == before);
    if (first && second) CHECK(*first == *second);   // 열지 못했으면(다른 프로그램이 잡고 있으면) 값 없음
}
