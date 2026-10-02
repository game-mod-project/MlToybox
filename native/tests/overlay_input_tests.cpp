#include "test.h"
#include "overlay/render/input.h"
#include "overlay/ui/app.h"
#include <cstdio>
#include <cstring>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <windows.h>
#include <imm.h>

using namespace mlt::ov;

// 창 프로시저 후킹을 실제 창에 걸고 메시지를 보내 본다(화면에 보이지 않는 창, 그래픽 장치 없음).
namespace {
int g_reached[0x400];   // 원래 창 프로시저(게임 쪽)까지 간 메시지 수

LRESULT CALLBACK GameProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg < 0x400) ++g_reached[msg];
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// 창 하나, ImGui 컨텍스트 하나, 후킹. 테스트가 끝나면(실패해도) 모두 치운다
struct Fixture {
    WNDCLASSEXW wc{};
    HWND hwnd = nullptr;
    App& a = app();

    Fixture() {
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = GameProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"mltoybox_overlay_input_test";
        RegisterClassExW(&wc);
        hwnd = CreateWindowExW(0, wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 200, 200, nullptr, nullptr, wc.hInstance, nullptr);
        ImGui::CreateContext();
        ImGui_ImplWin32_Init(hwnd);
        installWndProc(hwnd);
        a.state = OverlayState::Ready;
        a.visible = true;
        a.wantMouse = false;
        a.wantKeyboard = false;
        std::memset(g_reached, 0, sizeof(g_reached));
    }
    ~Fixture() {
        a.visible = false;
        a.wantMouse = false;
        a.wantKeyboard = false;
        a.state = OverlayState::Starting;
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        DestroyWindow(hwnd);
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
    }
    void send(UINT msg, WPARAM wParam = 0, LPARAM lParam = 0) { SendMessageW(hwnd, msg, wParam, lParam); }
};
}

// ImGui 의 win32 백엔드는 마우스 버튼을 뗄 때 ReleaseCapture 를 부른다. 그러면 Windows 가 같은 스레드에 WM_CAPTURECHANGED 를 보내
// 우리 창 프로시저가 자기 안에서 다시 불린다. ImGui 잠금이 재진입을 견디지 못하면 그 안에서 예외가 나고,
// 창 프로시저 안의 예외는 프로세스를 0xC000041D 로 끝낸다(2026-10-01 사전 검증에서 게임이 이렇게 튕겼다).
TEST(overlay_input_survives_reentrant_window_messages) {
    Fixture f;
    CHECK(f.hwnd != nullptr);
    f.send(WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(10, 10));
    CHECK(GetCapture() == f.hwnd);                                // 백엔드가 마우스를 잡았다
    f.send(WM_LBUTTONUP, 0, MAKELPARAM(10, 10));                  // ReleaseCapture -> WM_CAPTURECHANGED -> 창 프로시저 재진입
    CHECK(GetCapture() == nullptr);
}

TEST(overlay_input_toggle_key_opens_and_closes_and_ignores_key_repeat) {
    Fixture f;
    f.send(WM_KEYDOWN, VK_INSERT, 0x00000001);
    CHECK(!f.a.visible.load());
    f.send(WM_KEYDOWN, VK_INSERT, 0x40000001);                    // 반복(이전에도 눌려 있었음)
    CHECK(!f.a.visible.load());
    f.send(WM_KEYUP, VK_INSERT, 0xC0000001);
    f.send(WM_KEYDOWN, VK_INSERT, 0x00000001);
    CHECK(f.a.visible.load());
    CHECK(g_reached[WM_KEYDOWN] == 0 && g_reached[WM_KEYUP] == 0);   // 토글 키는 게임에 넘기지 않는다

    f.a.state = OverlayState::Disabled;                           // 오버레이가 꺼진 상태에서는 토글 키도 가로채지 않는다
    f.send(WM_KEYDOWN, VK_INSERT, 0x00000001);
    CHECK(f.a.visible.load());
    CHECK(g_reached[WM_KEYDOWN] == 1);
}

// R4: 창이 원하는 입력만 게임에 넘기지 않는다. 닫혀 있으면 모두 넘긴다
TEST(overlay_input_swallows_only_what_the_overlay_wants) {
    Fixture f;
    f.send(WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(10, 10));       // 마우스가 창 밖: 게임이 받는다
    f.send(WM_LBUTTONUP, 0, MAKELPARAM(10, 10));
    CHECK(g_reached[WM_LBUTTONDOWN] == 1 && g_reached[WM_LBUTTONUP] == 1);
    f.a.wantMouse = true;                                         // 마우스가 창 위
    f.send(WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(10, 10));
    f.send(WM_LBUTTONUP, 0, MAKELPARAM(10, 10));
    f.send(WM_MOUSEWHEEL, MAKEWPARAM(0, 120), MAKELPARAM(10, 10));
    CHECK(g_reached[WM_LBUTTONDOWN] == 1 && g_reached[WM_LBUTTONUP] == 1 && g_reached[WM_MOUSEWHEEL] == 0);

    f.send(WM_KEYDOWN, 'A', 0x00000001);                          // 입력 칸에 커서가 없다: 게임이 받는다
    f.send(WM_CHAR, 'a', 0x00000001);
    CHECK(g_reached[WM_KEYDOWN] == 1 && g_reached[WM_CHAR] == 1);
    f.a.wantKeyboard = true;                                      // 입력 칸에 커서가 있다
    f.send(WM_KEYDOWN, 'A', 0x00000001);
    f.send(WM_CHAR, 'a', 0x00000001);
    f.send(WM_KEYUP, 'A', 0xC0000001);
    CHECK(g_reached[WM_KEYDOWN] == 1 && g_reached[WM_CHAR] == 1 && g_reached[WM_KEYUP] == 0);

    f.a.visible = false;                                          // 닫혀 있으면 직전 프레임의 값이 남아 있어도 넘긴다
    f.send(WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(10, 10));
    f.send(WM_KEYDOWN, 'A', 0x00000001);
    CHECK(g_reached[WM_LBUTTONDOWN] == 2 && g_reached[WM_KEYDOWN] == 2);
}

// 버튼을 누른 채 창을 닫으면 백엔드가 잡은 마우스를 놓아야 한다(안 그러면 다시 열 때 창이 마우스에 붙어 끌린다)
TEST(overlay_input_releases_held_buttons_when_the_window_closes) {
    Fixture f;
    f.send(WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(10, 10));
    CHECK(GetCapture() == f.hwnd);
    f.send(WM_KEYDOWN, VK_INSERT, 0x00000001);                    // 누른 채로 닫는다
    CHECK(!f.a.visible.load());
    CHECK(GetCapture() == nullptr);

    f.send(WM_KEYUP, VK_INSERT, 0xC0000001);
    f.send(WM_KEYDOWN, VK_INSERT, 0x00000001);                    // 다시 연다
    f.send(WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(10, 10));
    CHECK(GetCapture() == f.hwnd);
    f.a.visible = false;                                          // 닫기 버튼이나 다른 스레드가 닫은 경우: 다음 메시지에서 정리한다
    f.send(WM_NULL);
    CHECK(GetCapture() == nullptr);
}

// 게임 창은 IME 가 꺼져 있다(2026-10-02 실측). 오버레이는 그 상태를 건드리지 않는다.
// 한글은 오버레이의 조합기(core/hangul)가 만든다. 창의 IME 까지 켜면 같은 글쇠가 두 번 조합된다
TEST(overlay_input_never_touches_the_games_ime) {
    if (!GetSystemMetrics(SM_IMMENABLED)) {
        std::printf("SKIP overlay_input ime: IMM is not enabled on this system\n");
        return;
    }
    Fixture f;
    auto imeOn = [&] {
        const HIMC imc = ImmGetContext(f.hwnd);
        if (imc) ImmReleaseContext(f.hwnd, imc);
        return imc != nullptr;
    };
    ImmAssociateContext(f.hwnd, nullptr);          // 게임이 꺼 둔 상태
    f.a.wantText = true;                           // 글자 칸에 커서가 있다
    f.send(WM_NULL);
    CHECK(!imeOn());
    f.send(WM_KEYDOWN, VK_INSERT, 0x00000001);     // 닫을 때도
    CHECK(!f.a.visible.load() && !imeOn());
    f.a.wantText = false;
}

// 한/영 글쇠: 글자 칸에 커서가 있으면 오버레이의 조합기를 켜고 끈다. 그 밖에는 게임의 것이다
TEST(overlay_input_hangul_key_belongs_to_the_overlay_only_while_a_text_field_has_the_cursor) {
    Fixture f;
    const unsigned before = f.a.hangulKeys.load();
    f.send(WM_KEYDOWN, VK_HANGUL, 0x00000001);
    f.send(WM_KEYUP, VK_HANGUL, 0xC0000001);
    CHECK(f.a.hangulKeys.load() == before);
    CHECK(g_reached[WM_KEYDOWN] == 1 && g_reached[WM_KEYUP] == 1);

    f.a.wantText = true;
    f.send(WM_KEYDOWN, VK_HANGUL, 0x00000001);
    f.send(WM_KEYDOWN, VK_HANGUL, 0x40000001);     // 누르고 있는 동안의 반복은 세지 않는다
    f.send(WM_KEYUP, VK_HANGUL, 0xC0000001);
    CHECK(f.a.hangulKeys.load() == before + 1);
    CHECK(g_reached[WM_KEYDOWN] == 1 && g_reached[WM_KEYUP] == 1);   // 게임에 넘기지 않는다

    f.a.visible = false;                           // 닫혀 있으면 게임의 것
    f.send(WM_KEYDOWN, VK_HANGUL, 0x00000001);
    CHECK(f.a.hangulKeys.load() == before + 1 && g_reached[WM_KEYDOWN] == 2);
    f.a.wantText = false;
}
