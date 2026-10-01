#include "test.h"
#include "overlay/render/input.h"
#include "overlay/ui/app.h"
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <windows.h>

using namespace mlt::ov;

// 창 프로시저 후킹을 실제 창에 걸고 메시지를 보내 본다(화면에 보이지 않는 창, 그래픽 장치 없음).
// ImGui 의 win32 백엔드는 마우스 버튼을 뗄 때 ReleaseCapture 를 부른다. 그러면 Windows 가 같은 스레드에 WM_CAPTURECHANGED 를 보내
// 우리 창 프로시저가 자기 안에서 다시 불린다. ImGui 잠금이 재진입을 견디지 못하면 그 안에서 예외가 나고,
// 창 프로시저 안의 예외는 프로세스를 0xC000041D 로 끝낸다(2026-10-01 사전 검증에서 게임이 이렇게 튕겼다).
TEST(overlay_input_survives_reentrant_window_messages) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"mltoybox_overlay_input_test";
    RegisterClassExW(&wc);
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 200, 200, nullptr, nullptr, wc.hInstance, nullptr);
    CHECK(hwnd != nullptr);
    ImGui::CreateContext();
    CHECK(ImGui_ImplWin32_Init(hwnd));
    CHECK(installWndProc(hwnd));

    App& a = app();
    a.state = OverlayState::Ready;
    a.visible = true;
    SendMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(10, 10));
    CHECK(GetCapture() == hwnd);                                  // 백엔드가 마우스를 잡았다
    SendMessageW(hwnd, WM_LBUTTONUP, 0, MAKELPARAM(10, 10));      // ReleaseCapture -> WM_CAPTURECHANGED -> 창 프로시저 재진입
    CHECK(GetCapture() == nullptr);

    // 토글 키는 창을 여닫고, 누르고 있는 동안의 반복 입력은 무시한다
    SendMessageW(hwnd, WM_KEYDOWN, VK_INSERT, 0x00000001);
    CHECK(!a.visible.load());
    SendMessageW(hwnd, WM_KEYDOWN, VK_INSERT, 0x40000001);        // 반복(이전에도 눌려 있었음)
    CHECK(!a.visible.load());
    SendMessageW(hwnd, WM_KEYUP, VK_INSERT, 0xC0000001);
    SendMessageW(hwnd, WM_KEYDOWN, VK_INSERT, 0x00000001);
    CHECK(a.visible.load());

    // 오버레이가 꺼진 상태(Disabled)에서는 토글 키도 가로채지 않는다
    a.state = OverlayState::Disabled;
    SendMessageW(hwnd, WM_KEYDOWN, VK_INSERT, 0x00000001);
    CHECK(a.visible.load());

    a.visible = false;
    a.state = OverlayState::Starting;
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
}
