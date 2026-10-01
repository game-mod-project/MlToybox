#include "input.h"
#include "guard.h"
#include "overlay/ui/app.h"
#include <atomic>
#include <exception>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imm.h>
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

// --- 아래는 창 스레드만 쓴다
bool g_wasVisible = false;
bool g_imeOn = false;       // 우리가 IME 를 켜 둔 상태인가
bool g_imeWasOff = false;   // 켜기 전에 게임이 IME 를 꺼 두었는가(그랬다면 끝날 때 다시 끈다)
int g_imeX = -1;
int g_imeY = -1;

struct InputArgs {
    HWND hwnd;
    UINT msg;
    WPARAM wParam;
    LPARAM lParam;
    LRESULT result = 0;    // ImGui 백엔드가 돌려준 값
    bool fed = false;      // 백엔드까지 갔는가
    bool locked = false;   // 구조적 예외가 났을 때 풀어야 하는 잠금(guard.h 의 TrackedLock)
};

// 창 프로시저 밖으로 예외가 나가면 Windows 가 게임을 끝낸다(0xC000041D). C++ 예외는 여기서 잡는다
void feedImGui(void* p) {
    auto* m = static_cast<InputArgs*>(p);
    try {
        TrackedLock lock(imguiMutex(), m->locked);
        if (!ImGui::GetCurrentContext()) return;
        m->result = ImGui_ImplWin32_WndProcHandler(m->hwnd, m->msg, m->wParam, m->lParam);
        m->fed = true;
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
        ImGuiIO& io = ImGui::GetIO();
        io.ClearInputKeys();
        io.ClearInputMouse();
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

// 창이 열려 있다가 닫혔으면(토글 키, 닫기 버튼, 꾸미기 열기) 입력 상태를 비운다. 창 스레드에서만 부른다
void noteVisibility(HWND hwnd, App& a) {
    const bool visible = a.visible.load();
    if (g_wasVisible && !visible) {
        InputArgs args{ hwnd, WM_NULL, 0, 0 };
        guarded(resetImGui, args);
    }
    g_wasVisible = visible;
}

// 글자 입력 칸에 커서가 있는 동안만 IME(한글 입력기)를 켠다. 창 스레드에서만 부른다.
// 게임이 창의 IME 를 꺼 두었으면(언리얼 엔진은 자기 입력 칸에 커서가 없을 때 그렇게 한다. 이 게임에서 재 보지는 않았다)
// 그대로는 오버레이의 이름 칸에 한글을 칠 수 없다. 꺼져 있을 때만 켜고, 끝나면 꺼 둔 상태로 되돌린다.
// IME 함수는 창을 가진 스레드에서 불러야 한다. ImGui 의 기본 IME 처리는 프레임을 그리는 스레드에서 부르므로 쓰지 않는다
void syncIme(HWND hwnd, App& a) {
    const bool want = a.visible.load() && a.wantText.load();
    if (want != g_imeOn) {
        g_imeOn = want;   // IME 함수가 이 창에 메시지를 보내 여기로 다시 들어올 수 있다. 먼저 적어 둔다
        g_imeX = -1;
        g_imeY = -1;
        if (want) {
            const HIMC current = ImmGetContext(hwnd);
            g_imeWasOff = current == nullptr;
            if (current) ImmReleaseContext(hwnd, current);
            if (g_imeWasOff) ImmAssociateContextEx(hwnd, nullptr, IACE_DEFAULT);
        } else if (g_imeWasOff) {
            ImmAssociateContext(hwnd, nullptr);   // 게임이 꺼 두었던 상태로 되돌린다
        }
    }
    if (!want) return;
    // 조합 중인 글자를 입력 커서 자리에 띄운다
    const int x = a.imeX.load();
    const int y = a.imeY.load();
    if (x == g_imeX && y == g_imeY) return;
    g_imeX = x;
    g_imeY = y;
    if (const HIMC imc = ImmGetContext(hwnd)) {
        COMPOSITIONFORM composition{};
        composition.dwStyle = CFS_FORCE_POSITION;
        composition.ptCurrentPos = { x, y };
        ImmSetCompositionWindow(imc, &composition);
        CANDIDATEFORM candidate{};
        candidate.dwStyle = CFS_CANDIDATEPOS;
        candidate.ptCurrentPos = { x, y };
        ImmSetCandidateWindow(imc, &candidate);
        ImmReleaseContext(hwnd, imc);
    }
}

// 조합의 시작·진행·끝과, 조합이 끝나 나온 글자
bool isCompositionMessage(UINT msg) {
    return msg == WM_IME_STARTCOMPOSITION || msg == WM_IME_COMPOSITION || msg == WM_IME_ENDCOMPOSITION || msg == WM_IME_CHAR;
}

LRESULT CALLBACK HookedWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    App& a = app();
    const WNDPROC original = g_original.load();
    // 오버레이가 그릴 수 없는 상태면 토글 키를 포함해 아무것도 가로채지 않는다
    if (a.state.load() != OverlayState::Ready) return CallWindowProcW(original, hwnd, msg, wParam, lParam);

    const bool toggleKey = static_cast<int>(wParam) == a.toggleVk.load();
    if (toggleKey && (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN)) {
        if (!(lParam & (1LL << 30))) a.visible = !a.visible.load();   // 누르고 있는 동안의 반복 입력은 무시
        noteVisibility(hwnd, a);
        syncIme(hwnd, a);
        return 0;
    }
    if (toggleKey && (msg == WM_KEYUP || msg == WM_SYSKEYUP)) return 0;
    noteVisibility(hwnd, a);
    syncIme(hwnd, a);

    if (a.visible.load()) {
        // Win32 의 캡처·키 상태 함수는 창을 가진 스레드에서 불러야 한다. 그래서 입력은 이 스레드에서 ImGui 에 넣는다.
        // 다른 스레드가 SendMessage 로 보낸 메시지는 넘기지 않는다: 보낸 쪽이 ImGui 잠금을 쥐고 기다리고 있으면 서로 멈춘다.
        // 마우스·키 입력은 큐로 오는 메시지라 여기에 걸리지 않는다
        InputArgs args{ hwnd, msg, wParam, lParam };
        if (!InSendMessage()) guarded(feedImGui, args);

        // 글자 입력 칸에 커서가 있을 때의 한글 조합은 게임에 넘기지 않는다
        // (게임도 같은 조합을 처리해 글자가 두 번 들어가거나 게임의 입력기가 끼어든다).
        // 백엔드는 WM_IME_COMPOSITION 을 기본 처리까지 해 준다. 나머지는 여기서 기본 처리한다: 조합이 끝난 글자가 WM_CHAR 로 다시 온다
        if (a.wantText.load() && isCompositionMessage(msg)) {
            if (msg == WM_IME_COMPOSITION && args.fed) return args.result;
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }

        const bool mouse = msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST;
        const bool key = msg >= WM_KEYFIRST && msg <= WM_KEYLAST;
        if ((mouse && a.wantMouse.load()) || (key && a.wantKeyboard.load())) return 0;   // 창이 먹은 입력은 게임에 넘기지 않는다
    }
    return CallWindowProcW(original, hwnd, msg, wParam, lParam);
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
    if (!SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(HookedWndProc)) && GetLastError() != 0) return false;
    g_hooked = hwnd;
    return true;
}

void wakeWindowThread() {
    if (const HWND hwnd = g_hooked.load()) PostMessageW(hwnd, WM_NULL, 0, 0);
}

}
