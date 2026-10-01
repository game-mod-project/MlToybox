#include "input.h"
#include "guard.h"
#include "overlay/ui/app.h"
#include <exception>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <string>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace mlt::ov {

std::recursive_mutex& imguiMutex() {
    static std::recursive_mutex m;
    return m;
}

namespace {
WNDPROC g_original = nullptr;

struct InputArgs {
    HWND hwnd;
    UINT msg;
    WPARAM wParam;
    LPARAM lParam;
    bool locked = false;   // 구조적 예외가 났을 때 풀어야 하는 잠금(guard.h 의 TrackedLock)
};

// 창 프로시저 밖으로 예외가 나가면 Windows 가 게임을 끝낸다(0xC000041D). C++ 예외는 여기서 잡는다
void feedImGui(void* p) {
    auto* m = static_cast<InputArgs*>(p);
    try {
        TrackedLock lock(imguiMutex(), m->locked);
        if (ImGui::GetCurrentContext()) ImGui_ImplWin32_WndProcHandler(m->hwnd, m->msg, m->wParam, m->lParam);
    } catch (const std::exception& e) {
        disableOverlay(std::string("input exception ") + e.what());
    } catch (...) {
        disableOverlay("input exception (unknown)");
    }
}

LRESULT CALLBACK HookedWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    App& a = app();
    // 오버레이가 그릴 수 없는 상태면 토글 키를 포함해 아무것도 가로채지 않는다
    if (a.state.load() != OverlayState::Ready) return CallWindowProcW(g_original, hwnd, msg, wParam, lParam);

    const bool toggleKey = static_cast<int>(wParam) == a.toggleVk.load();
    if (toggleKey && (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN)) {
        if (!(lParam & (1LL << 30))) a.visible = !a.visible.load();   // 누르고 있는 동안의 반복 입력은 무시
        return 0;
    }
    if (toggleKey && (msg == WM_KEYUP || msg == WM_SYSKEYUP)) return 0;

    if (a.visible.load()) {
        // Win32 의 캡처·키 상태 함수는 창을 가진 스레드에서 불러야 한다. 그래서 입력은 이 스레드에서 ImGui 에 넣는다.
        // 다른 스레드가 SendMessage 로 보낸 메시지는 넘기지 않는다: 보낸 쪽이 ImGui 잠금을 쥐고 기다리고 있으면 서로 멈춘다.
        // 마우스·키 입력은 큐로 오는 메시지라 여기에 걸리지 않는다
        if (!InSendMessage()) {
            InputArgs args{ hwnd, msg, wParam, lParam };
            unsigned long code = 0;
            if (!runGuarded(feedImGui, &args, &code)) {
                if (args.locked) imguiMutex().unlock();
                disableOverlay("input exception " + hexCode(code));
            }
        }
        const bool mouse = msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST;
        const bool key = msg >= WM_KEYFIRST && msg <= WM_KEYLAST;
        if ((mouse && a.wantMouse.load()) || (key && a.wantKeyboard.load())) return 0;   // 창이 먹은 입력은 게임에 넘기지 않는다
    }
    return CallWindowProcW(g_original, hwnd, msg, wParam, lParam);
}
}

bool installWndProc(HWND hwnd) {
    if (g_original) return true;
    SetLastError(0);
    LONG_PTR previous = SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(HookedWndProc));
    if (!previous) return false;
    g_original = reinterpret_cast<WNDPROC>(previous);
    return true;
}

}
