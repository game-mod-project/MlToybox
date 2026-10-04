#include "input.h"
#include "guard.h"
#include "overlay/ui/app.h"
#include <atomic>
#include <bitset>
#include <cstdio>
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
std::atomic<WNDPROC> g_original{nullptr};
std::atomic<HWND> g_hooked{nullptr};
std::atomic<BOOL(WINAPI*)(POINT*)> g_cursorSource{nullptr};   // 테스트가 바꿔 끼운다. 없으면 GetCursorPos

// --- 아래는 창 스레드만 쓴다
bool g_wasVisible = false;
// 게임이 눌린 것으로 알고 있는 키: 누름을 게임에 넘겼고 뗌은 아직 넘기지 않았다
std::bitset<256> g_gameKeysDown;

bool isKeyDown(UINT msg) { return msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN; }
bool isKeyUp(UINT msg) { return msg == WM_KEYUP || msg == WM_SYSKEYUP; }

// 메시지를 게임(원래 창 프로시저)에 넘긴다. 게임이 본 키의 누름과 뗌을 적어 둔다
LRESULT passToGame(WNDPROC original, HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (wParam < g_gameKeysDown.size()) {
        if (isKeyDown(msg)) g_gameKeysDown.set(wParam);
        else if (isKeyUp(msg)) g_gameKeysDown.reset(wParam);
    }
    return CallWindowProcW(original, hwnd, msg, wParam, lParam);
}

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
        if (!ImGui::GetCurrentContext()) return;
        ImGui_ImplWin32_WndProcHandler(m->hwnd, m->msg, m->wParam, m->lParam);
    } catch (const std::exception& e) {
        disableOverlay(std::string("input exception ") + e.what());
    } catch (...) {
        disableOverlay("input exception (unknown)");
    }
}

// 창이 닫힐 때: 버튼이나 키를 누른 채였다면 ImGui 와 백엔드에 눌린 상태가 남는다(다시 열면 창이 마우스에 붙어 끌린다).
// 버튼을 뗀 것으로 알려 백엔드가 잡은 마우스를 놓게 하고, ImGui 의 입력 상태를 비운다
void resetImGui(void* p) {
    auto* m = static_cast<InputArgs*>(p);
    try {
        TrackedLock lock(imguiMutex(), m->locked);
        if (!ImGui::GetCurrentContext()) return;
        for (UINT up : { WM_LBUTTONUP, WM_RBUTTONUP, WM_MBUTTONUP }) ImGui_ImplWin32_WndProcHandler(m->hwnd, up, 0, 0);
        for (int side : { XBUTTON1, XBUTTON2 }) ImGui_ImplWin32_WndProcHandler(m->hwnd, WM_XBUTTONUP, MAKEWPARAM(0, side), 0);
        ImGuiIO& io = ImGui::GetIO();
        io.ClearInputKeys();
        io.ClearInputMouse();
    } catch (const std::exception& e) {
        disableOverlay(std::string("input exception ") + e.what());
    } catch (...) {
        disableOverlay("input exception (unknown)");
    }
}

// 창이 열릴 때: 닫으면서 ImGui 의 마우스 위치를 비웠고, 닫혀 있는 동안의 마우스 이동은 ImGui 에 넣지 않았다.
// 지금 커서가 게임 창 안에 있으면 그 위치를 알려 준다. 그러지 않으면 마우스를 움직이기 전의 첫 클릭은
// 창 위에서 눌러도 ImGui 가 몰라서 게임으로 간다
void primeMouse(void* p) {
    auto* m = static_cast<InputArgs*>(p);
    try {
        POINT at{};
        const auto source = g_cursorSource.load();
        if (!(source ? source(&at) : GetCursorPos(&at))) return;
        RECT client{};
        if (!ScreenToClient(m->hwnd, &at) || !GetClientRect(m->hwnd, &client) || !PtInRect(&client, at)) return;
        TrackedLock lock(imguiMutex(), m->locked);
        if (!ImGui::GetCurrentContext()) return;
        ImGui_ImplWin32_WndProcHandler(m->hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(at.x, at.y));
    } catch (const std::exception& e) {
        disableOverlay(std::string("input exception ") + e.what());
    } catch (...) {
        disableOverlay("input exception (unknown)");
    }
}

void guarded(void (*fn)(void*), InputArgs& args) {
    unsigned long code = 0;
    if (!runGuarded(fn, &args, &code)) {
        if (args.locked) imguiMutex().unlock();
        disableOverlay("input exception " + hexCode(code));
    }
}

// 창이 열려 있다가 닫혔으면(토글 키, 닫기 버튼, 꾸미기 열기) 입력 상태를 비우고, 닫혀 있다가 열렸으면
// 지금의 마우스 위치를 알려 준다. 창 스레드에서만 부른다
void noteVisibility(HWND hwnd, App& a) {
    const bool visible = a.visible.load();
    if (g_wasVisible != visible) {
        InputArgs args{ hwnd, WM_NULL, 0, 0 };
        guarded(visible ? primeMouse : resetImGui, args);
    }
    g_wasVisible = visible;
}

// 개발·검증용(overlay.json 의 inputLog): 창이 열려 있는 동안의 글쇠·문자·IME 메시지를 적어 둔다.
// 파일에는 작업 스레드가 쓴다(창 스레드는 파일을 만지지 않는다)
bool loggedMessage(UINT msg) {
    return (msg >= WM_KEYFIRST && msg <= WM_KEYLAST) || (msg >= WM_IME_STARTCOMPOSITION && msg <= WM_IME_KEYLAST)
        || (msg >= WM_IME_SETCONTEXT && msg <= WM_IME_KEYUP);
}

void logInput(App& a, UINT msg, WPARAM wParam, LPARAM lParam) {
    char line[160];
    std::snprintf(line, sizeof(line), "%llu msg=0x%X w=0x%llX l=0x%llX text=%d keyboard=%d", static_cast<unsigned long long>(GetTickCount64()),
        msg, static_cast<unsigned long long>(wParam), static_cast<unsigned long long>(lParam), a.wantText.load() ? 1 : 0,
        a.wantKeyboard.load() ? 1 : 0);
    std::lock_guard<std::mutex> lock(a.mutex);
    if (a.inputLines.size() < 2000) a.inputLines.emplace_back(line);
}

LRESULT CALLBACK HookedWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    App& a = app();
    const WNDPROC original = g_original.load();
    // 오버레이가 그릴 수 없는 상태면 토글 키를 포함해 아무것도 가로채지 않는다
    if (a.state.load() != OverlayState::Ready) return passToGame(original, hwnd, msg, wParam, lParam);

    const bool keyDown = isKeyDown(msg);
    const bool keyUp = isKeyUp(msg);
    const bool toggleKey = static_cast<int>(wParam) == a.toggleVk.load();
    if (toggleKey && keyDown) {
        if (!(lParam & (1LL << 30))) a.visible = !a.visible.load();   // 누르고 있는 동안의 반복 입력은 무시
        noteVisibility(hwnd, a);
        return 0;
    }
    if (toggleKey && keyUp) return 0;
    noteVisibility(hwnd, a);

    if (a.visible.load()) {
        if (a.inputLog.load() && loggedMessage(msg)) logInput(a, msg, wParam, lParam);

        // 한/영 글쇠. 게임 창의 IME 는 건드리지 않는다(게임이 꺼 둔 채로 둔다). 한글은 오버레이의 조합기(core/hangul)가 만들고,
        // 글자 칸에 커서가 있는 동안 이 글쇠가 그 조합기를 켜고 끈다. 누른 횟수만 세어 두면 화면 쪽(ui/widgets)이 읽는다
        if (wParam == VK_HANGUL && (keyDown || keyUp) && a.wantText.load()) {
            if (keyDown && !(lParam & (1LL << 30))) ++a.hangulKeys;
            return 0;
        }

        // Win32 의 캡처·키 상태 함수는 창을 가진 스레드에서 불러야 한다. 그래서 입력은 이 스레드에서 ImGui 에 넣는다.
        // 다른 스레드가 SendMessage 로 보낸 메시지는 넘기지 않는다: 보낸 쪽이 ImGui 잠금을 쥐고 기다리고 있으면 서로 멈춘다.
        // 마우스·키 입력은 큐로 오는 메시지라 여기에 걸리지 않는다
        InputArgs args{ hwnd, msg, wParam, lParam };
        if (!InSendMessage()) guarded(feedImGui, args);

        const bool mouse = msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST;
        const bool key = msg >= WM_KEYFIRST && msg <= WM_KEYLAST;
        if ((mouse && a.wantMouse.load()) || (key && a.wantKeyboard.load())) {
            // 창이 먹은 입력은 게임에 넘기지 않는다. 다만 게임이 눌린 것으로 알고 있는 키의 뗌은 넘긴다:
            // 삼키면 게임에서 그 키가 눌린 채로 남는다(W 를 누른 채 입력 칸을 누르면 카메라가 계속 움직인다)
            const bool releaseForGame = keyUp && wParam < g_gameKeysDown.size() && g_gameKeysDown.test(wParam);
            if (!releaseForGame) return 0;
        }
    }
    return passToGame(original, hwnd, msg, wParam, lParam);
}
}

bool installWndProc(HWND hwnd) {
    const HWND hooked = g_hooked.load();
    if (hooked == hwnd) return true;
    if (hooked && IsWindow(hooked)) return true;   // 이미 다른(살아 있는) 창에 걸려 있다
    const LONG_PTR current = GetWindowLongPtrW(hwnd, GWLP_WNDPROC);
    if (!current) return false;
    // 바꾸기 전에 넣는다. 바꾼 직후 창 스레드에 온 메시지가 이어 부를 곳이 있어야 한다
    g_original = reinterpret_cast<WNDPROC>(current);
    SetLastError(0);
    const LONG_PTR replaced = SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(HookedWndProc));
    if (!replaced && GetLastError() != 0) return false;
    // 읽은 뒤 바꾸기 전에 다른 프로그램이 프로시저를 바꿨다면, 우리가 실제로 갈아 끼운 것은 그쪽 것이다. 그것을 이어 부른다
    if (replaced != current) g_original = reinterpret_cast<WNDPROC>(replaced);
    g_hooked = hwnd;
    g_gameKeysDown.reset();   // 새 창이다. 앞 창에서 본 키는 잊는다
    return true;
}

void setCursorSource(BOOL(WINAPI* source)(POINT*)) {
    g_cursorSource = source;
}

void wakeWindowThread() {
    if (const HWND hwnd = g_hooked.load()) PostMessageW(hwnd, WM_NULL, 0, 0);
}

}
