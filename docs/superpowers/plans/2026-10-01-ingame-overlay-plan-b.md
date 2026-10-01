# 게임 안 오버레이 창 — 계획 B (나머지 탭, 글자 입력, 다지기) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 게임 안 창에 자원·군사·용병·인구 탭을 더해 패널의 기능을 모두 옮기고, 용병단 이름에 한글을 칠 수 있게 하며, 계획 A의 최종 리뷰에서 미뤄 둔 안전 항목을 고친다.

**Architecture:** 탭이 쓰는 설정·상태·규칙·문구는 그래픽에 의존하지 않는 `overlay_core`에 두어 단위 테스트하고, 탭 파일(`ui/tab_*.cpp`)은 그것을 ImGui로 그리기만 한다. 한글 입력기(IME)는 창을 가진 스레드(창 프로시저)에서 켜고 끄고, 복사는 작업 스레드가 Windows 클립보드에 쓴다. 화면 스레드는 Windows에 메시지를 보낼 수 있는 함수를 부르지 않는다.

**Tech Stack:** C++20(MSVC, CMake + Ninja), Dear ImGui v1.92.9b, nlohmann/json v3.12.0, MinHook, IMM32, UE4SS 3.0.1 Lua 5.4, PowerShell 7

**Spec:** `docs/superpowers/specs/2026-10-01-ingame-overlay-design.md`. 실측 근거는 `analysis/findings.md`의 "게임 안 오버레이 창" 절들(스파이크, 계획 코드 사전 검증, 계획 A 구현 검증, 코드 리뷰 뒤 고친 것).

**범위:** 스펙 8절의 4~7단계(군사·인구, 자원, 용병, 문서와 검증)와, 계획 A의 최종 리뷰에서 미룬 항목 가운데 안전과 관련된 것. 계획 A(`docs/superpowers/plans/2026-10-01-ingame-overlay-plan-a.md`)는 `develop`에 병합돼 있다(`d47885b`).

**사전 검증:** 이 문서의 코드는 레포 밖 사본에서 태스크 순서대로 적용해 빌드와 테스트를 통과시킨 것이다(끝까지 적용하면 네이티브 테스트 127개). 2026-10-01에 그 빌드를 게임에서 한 번 돌렸다(`saveGame_8`): 탭 8개가 순서대로 뜨고, 자원·군사·용병·인구 탭의 값이 `control.json`·`status.json`과 맞았으며, 세이브를 불러온 직후 "Insert: MLToybox" 안내가 떴다(그때 화면은 아직 검은 전환 화면이었다). 마우스로 하는 조작(체크, 표 편집, 용병단 등록, 한글 입력)은 확인하지 못했다. Task 9의 사용자 확인 항목이다. 자원 표의 열 너비는 그 실행 뒤에 고쳤고 게임에서 다시 보지 않았다(Task 7에서 본다). "실패 확인" 단계의 기대 출력도 같은 사본에서 확인했다.

## Global Constraints

- 패널(`panel/`) 코드는 바꾸지 않는다(R8). `mlt_core`와 `mltoybox_native`의 소스(`native/src/`)도 바꾸지 않는다.
- 브리지 프로토콜(`control.json` version 1, `status.json`)의 모양을 바꾸지 않는다(R6). 설정 키와 기본값은 패널(`panel/MLToybox.Panel.Core/ControlDocument.cs`)과 같다.
- 화면 문구는 패널과 같게 한다(`panel/MLToybox.Panel/MainForm.cs`, `MercenaryTab.cs`). 탭 순서도 패널과 같다: 자원, 영주, 건설, 업그레이드, 군사, 용병, 인구, 상태.
- 즉시 반영(스펙 2.3): 체크와 목록 선택은 바꾸는 즉시, 숫자 칸은 Enter를 누르거나 포커스가 떠날 때 저장한다. 용병단 편집은 "등록"을 누를 때만 반영한다. 일회성 버튼은 게임 안일 때만 누를 수 있다.
- 용병단(R7): 등록 수에는 제한이 없다. "사용"은 최대 3개. 이름은 앞뒤 공백을 뗀 뒤 1~40자이고 길이는 글자(코드 포인트) 수로 센다. 분대는 1~10개.
- 스레드 규칙: ImGui 함수는 `imguiMutex()`(재진입 잠금) 아래에서만 부른다. 잠금 순서는 `imguiMutex()` 다음 `App::mutex`. 화면 스레드(Present 후킹 안)에서는 파일을 만지지 않고, Windows에 메시지를 보낼 수 있는 함수(IME, 클립보드 비우기)도 부르지 않는다. IME 함수는 창 스레드에서, 클립보드 쓰기는 작업 스레드에서 부른다.
- 후킹한 함수(창 프로시저, Present, ResizeBuffers) 밖으로 예외를 내보내지 않는다.
- 탭 코드는 화면에서 고른 것(범위, 편집 중인 용병단)만 파일 안의 정적 변수로 들고, 설정은 매 프레임 문서에서 읽는다(패널 등 밖에서 바뀐 값이 바로 보인다). 문서를 바꾼 뒤에는 `markDirty(a)`만 부른다.
- 이름공간은 `mlt::ov`. 포함 경로의 뿌리는 `native/`와 `native/third_party`다.
- C++ 소스는 UTF-8(BOM 없음). Lua 파일은 Write/Edit 도구로만 고친다(셸 heredoc은 백슬래시를 깨뜨린다).
- 확인 명령: `pwsh E:/MLToybox/tools/build-native.ps1 -Test`, `dotnet test E:/MLToybox/panel/MLToybox.sln`, `pwsh E:/MLToybox/tools/tests/Tools.Tests.ps1`. 테스트 수는 `& E:\MLToybox\native\build\native_tests.exe | Select-Object -Last 1`로 본다.
- 오버레이 DLL과 네이티브 DLL은 게임 실행 중에는 잠겨 있다. 배포는 게임이 꺼져 있을 때 한다.
- PowerShell 변수는 도구 호출 사이에 남지 않는다. 게임 확인 단계의 명령 블록들은 한 번에 이어서 실행하거나, 뒤 블록 앞에 `$mod`와 `$shots`를 정의하는 세 줄(`. E:\MLToybox\tools\common.ps1`부터)을 다시 넣는다.
- 게임 작업 규칙: 사용자의 게임이나 패널이 켜져 있으면 끄지 말고 먼저 묻는다. 게임을 켜기 전에 사용자에게 "시험 중이니 게임 창을 누르지 말라"고 알린다. 시작 전에 `pwsh E:/MLToybox/tools/backup-saves.ps1`을 실행한다. 기존 세이브를 덮어쓰지 않고, 확인이 끝나면 바로 저장하지 않고 게임을 끈다. 화면은 `tools/capture-game.ps1`로 게임 창만 찍는다. 실제 마우스와 전경 창은 건드리지 않는다. 사용자의 `control.json`을 고쳤으면 반드시 되돌린다.
- git: 작업 브랜치 `feat/overlay-tabs`(`develop`에서 분기). 태스크마다 커밋한다. 끝나면 `develop`에 `--no-ff`로 병합하고 `develop`을 푸시한다. 커밋 메시지 끝에 `Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>`.
- 모든 git·파일 명령은 절대경로 또는 `git -C E:/MLToybox`를 쓴다(`cd` 뒤 상대경로 금지).
- 표기: "찾을 부분"은 파일에 정확히 한 번 나온다. 그 부분을 "바꿀 내용"으로 바꾼다. 파일 이름만 있고 코드 블록 하나가 오면 그 파일을 새로 만들거나, 이미 있으면 **파일 전체를 그 내용으로 바꾼다**.
- 스펙을 고친 곳(이 계획과 함께 커밋): 2.1 안내를 게임에 들어갈 때도 띄움, 2.5 한글 입력기와 클립보드를 다루는 스레드, 3.2 파일 목록, 5.3 깨진 `control.json`의 사본과 작업 스레드 보호.

## Review Focus

1. **용병단 "사용" 3개 제한**(R7): 네 번째를 켜려 할 때, 3개가 사용 중일 때 새로 등록할 때, 하나를 끄고 다른 것을 켤 때. 등록 자체는 몇 개든 돼야 한다. → Task 8 `overlay_merc_rules_any_number_can_be_registered_but_only_three_are_in_use`
2. **한글 이름**: 한글 40자(120바이트)가 길이 제한에 걸리면 안 되고, 오버레이가 쓴 이름을 모드가 같은 글자로 읽고 모드의 검증도 통과해야 한다. → Task 4 `overlay_merc_rules_reject_bad_companies`, Task 3 Lua 스펙 `mercenary_companies_pass_the_mods_own_validation`
3. **사라진 영지**(영지를 잃었거나 다른 세이브를 불러옴): 골라 둔 범위는 공통(첫 줄)으로 돌아가고, 저장된 영지 목표와 용병단의 도착 영지는 지워지지 않아야 한다. → Task 3 `overlay_scope_options_find_a_key_or_fall_back_to_the_first_line`, `overlay_features_read_a_file_written_by_the_panel`(지금 세이브에 없는 영지 `sel`, `hof`의 목표가 남아 있다), Task 4 `overlay_merc_rules_region_options_keep_saved_keys_that_are_not_listed`
4. **손으로 고친 설정의 이상한 값**(용병단 목록이 배열이 아님, 병종이 글자가 아님, 목표가 글자나 음수): 그 항목만 버리고 나머지는 읽어야 한다. 파일 전체를 못 읽으면 기본값으로 덮기 전에 사본을 남겨야 한다. → Task 3 `overlay_features_tolerate_hand_edited_garbage`, Task 2 `overlay_bridge_tells_an_unreadable_control_file_from_a_missing_one`
5. **창을 닫는 순간의 입력 상태**: 버튼을 누른 채, 또는 글자 칸에 커서를 둔 채(한글 입력기가 켜진 채) 닫아도 마우스가 붙들려 있거나 게임의 입력기 상태가 바뀐 채로 남으면 안 된다. → Task 1 `overlay_input_releases_held_buttons_when_the_window_closes`, Task 5 `overlay_input_turns_the_ime_on_only_while_a_text_field_has_the_cursor`

단위 테스트로 잡을 수 없는 것: 실제 마우스로 하는 조작과 한글 조합. Task 9의 사용자 확인 항목이다.

## File Structure

| 파일 | 작업 | 책임 |
|---|---|---|
| `native/overlay/core/hint.h/.cpp` | 생성 | 안내를 띄우는 때(그리기 시작, 맵에 들어갈 때) |
| `native/overlay/core/units.h/.cpp` | 생성 | 병종 13종, 선택지 한 줄(`ScopeOption`) |
| `native/overlay/core/merc_rules.h/.cpp` | 생성 | 용병단 검증, 등록, "사용" 제한, 깃발·영지 선택지 |
| `native/overlay/core/resources.h/.cpp` | 생성 | 자원 탭의 범위와 표의 줄 |
| `native/overlay/core/view.h/.cpp` | 생성 | 탭에 보일 문구와 선택지(재구성, 수행원, 인구, 용병 상태) |
| `native/overlay/core/control_doc.h/.cpp` | 수정 | 군사·인구·자원·용병 설정, `tryParse`, 예외 없는 `dump` |
| `native/overlay/core/status_doc.h/.cpp` | 수정 | 탭이 읽는 상태 항목 |
| `native/overlay/core/commands.h/.cpp` | 수정 | 분대 생성, 재구성, 꾸미기, 가족 추가 명령 |
| `native/overlay/core/bridge.h/.cpp` | 수정 | 삭제 공유로 읽기, 깨진 `control.json` 가리기와 사본 |
| `native/overlay/render/input.h/.cpp` | 수정 | 창이 닫힐 때 입력 비우기, 창 다시 걸기, IME 켜고 끄기 |
| `native/overlay/render/clipboard.h/.cpp` | 생성 | Windows 클립보드에 쓰기(작업 스레드) |
| `native/overlay/render/dx12_hook.cpp`, `imgui_layer.cpp` | 수정 | 리뷰에서 미룬 항목, IME·클립보드 콜백 |
| `native/overlay/ui/widgets.h/.cpp` | 수정 | 목록 선택, 빈칸을 허용하는 숫자 칸, 글자 칸 |
| `native/overlay/ui/tab_military.cpp`, `tab_population.cpp`, `tab_resources.cpp`, `tab_mercenaries.cpp` | 생성 | 탭 |
| `native/overlay/ui/tabs.h`, `window.cpp`, `app.h`, `app.cpp`, `tab_status.cpp`, `native/overlay/worker.cpp` | 수정 | 탭 등록, 안내, 나눠 쓰는 상태, 작업 스레드 보호 |
| `native/tests/overlay_*_tests.cpp` | 생성·수정 | 위 모듈의 테스트 |
| `mod/MLToybox/tests/overlay_fixture_spec.lua`, `fixtures/control_from_overlay.json` | 수정 | 오버레이가 쓴 설정을 모드가 읽는지 |
| `README.md`, `CLAUDE.md`, `docs/CHANGELOG.md`, `analysis/findings.md` | 수정 | 문서 |

---

### Task 1: 렌더·입력 다지기와 안내를 띄우는 때

계획 A의 최종 리뷰에서 미룬 항목 가운데 화면 출력과 입력에 관한 것을 고친다.
- 창을 닫을 때 ImGui와 백엔드에 남는 눌린 버튼·키를 비운다(버튼을 누른 채 닫으면 백엔드가 잡은 마우스가 풀리지 않는다).
- 걸어 둔 창이 없어졌으면 새 창에 창 프로시저를 다시 건다. 바꾸기 전에 이어 부를 프로시저를 먼저 적어 둔다.
- 같은 스레드에서 직접 큐가 둘 실행됐으면 가장 최근 것을 고른다. 꺼진 뒤에는 큐를 기록하지 않는다. 초기화 뒤 상태는 꺼진 경우가 아니면 반드시 `ready`가 된다.
- 예외 뒤 잠금 표시를 스레드마다 둔다. `DXGI_PRESENT_TEST` 호출에는 그리지 않는다.
- 안내("Insert: MLToybox")를 맵에 들어갈 때도 8초 동안 띄운다. 그리기 시작할 때의 안내는 게임의 검은 시작 화면에 떠서 보기 어렵다(계획 A 구현 검증).

**Files:**
- Create: `native/overlay/core/hint.h`, `native/overlay/core/hint.cpp`, `native/tests/overlay_hint_tests.cpp`
- Modify: `native/overlay/render/input.h`, `native/overlay/render/input.cpp`(파일 전체), `native/overlay/render/dx12_hook.cpp`, `native/overlay/ui/window.cpp`
- Modify: `native/tests/overlay_input_tests.cpp`(파일 전체), `native/tests/overlay_render_tests.cpp`, `native/CMakeLists.txt`

**Interfaces:**
- Consumes: 계획 A의 `App`, `imguiMutex()`, `TrackedLock`, `runGuarded`, `hexCode`, `disableOverlay`, `Bridge::evaluate`
- Produces:
  - `overlay/core/hint.h`: `class HintTimer { static constexpr unsigned long long kShowMs = 8000; void onReady(unsigned long long nowMs); void update(bool inGame, unsigned long long nowMs); bool active(unsigned long long nowMs) const; }`
  - `overlay/render/input.h`: `void wakeWindowThread()`(창 스레드에 빈 메시지를 보낸다). `installWndProc`는 걸어 둔 창이 없어졌으면 새 창에 다시 건다
  - 테스트 도우미(`overlay_input_tests.cpp`): 창·ImGui 컨텍스트·후킹을 만들고 치우는 `Fixture`, 게임 쪽까지 간 메시지 수 `g_reached[msg]`

- [ ] **Step 1: 브랜치 준비**

이 계획과 스펙 수정이 있는 `docs/overlay-plan-b`를 `develop`에 병합하고 작업 브랜치를 만든다.

```bash
git -C E:/MLToybox checkout develop
git -C E:/MLToybox merge --no-ff docs/overlay-plan-b -m "Merge branch 'docs/overlay-plan-b' into develop" -m "Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
git -C E:/MLToybox push origin develop
git -C E:/MLToybox checkout -b feat/overlay-tabs
```

- [ ] **Step 2: 실패하는 테스트 작성**

<!-- file: native/tests/overlay_hint_tests.cpp -->
```cpp
#include "test.h"
#include "overlay/core/hint.h"

using namespace mlt::ov;

TEST(overlay_hint_shows_for_eight_seconds_after_ready) {
    HintTimer h;
    CHECK(!h.active(0) && !h.active(5000));        // 준비되기 전에는 띄우지 않는다
    h.onReady(1000);
    CHECK(h.active(1000) && h.active(8999));
    CHECK(!h.active(9000));
}

TEST(overlay_hint_shows_again_each_time_the_player_enters_a_map) {
    HintTimer h;
    h.onReady(0);
    h.update(false, 20000);                        // 메인 메뉴
    CHECK(!h.active(20000));
    h.update(true, 30000);                         // 맵에 들어갔다
    CHECK(h.active(30000) && h.active(37999) && !h.active(38000));
    h.update(true, 36000);                         // 맵 안에 계속 있는 동안에는 늘리지 않는다
    CHECK(!h.active(38000));
    h.update(false, 50000);                        // 메뉴로 나갔다가
    CHECK(!h.active(50000));
    h.update(true, 60000);                         // 다시 들어가면 또 띄운다
    CHECK(h.active(67999) && !h.active(68000));
}
```

`overlay_input_tests.cpp`는 파일 전체를 바꾼다. 창마다 후킹을 걸고 치우는 `Fixture`를 두고, 게임에 넘긴 메시지를 세어 삼킴 규칙(R4)도 시험한다.

<!-- file: native/tests/overlay_input_tests.cpp -->
```cpp
#include "test.h"
#include "overlay/render/input.h"
#include "overlay/ui/app.h"
#include <cstring>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <windows.h>

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
```

<!-- edit: native/tests/overlay_render_tests.cpp -->
`native/tests/overlay_render_tests.cpp` — 찾을 부분:

```cpp
    CHECK(a.frames.load() > before);                                                        // 안내를 그렸다
```

바꿀 내용:

```cpp
    CHECK(a.frames.load() > before);                                                        // 안내를 그렸다
    before = a.frames.load();
    CHECK(SUCCEEDED(swap->Present(0, DXGI_PRESENT_TEST)));                                  // 화면에 내지 않는 확인용 호출
    CHECK(a.frames.load() == before);                                                       // 거기에는 그리지 않는다
```

<!-- edit: native/CMakeLists.txt -->
`native/CMakeLists.txt` (1/2) — 찾을 부분:

```cmake
    overlay/core/frame_gate.cpp)
```

바꿀 내용:

```cmake
    overlay/core/frame_gate.cpp
    overlay/core/hint.cpp)
```

<!-- edit: native/CMakeLists.txt -->
`native/CMakeLists.txt` (2/2) — 찾을 부분:

```cmake
    tests/overlay_frame_gate_tests.cpp
```

바꿀 내용:

```cmake
    tests/overlay_frame_gate_tests.cpp
    tests/overlay_hint_tests.cpp
```

- [ ] **Step 3: 빌드해서 실패 확인**

Run: `pwsh E:/MLToybox/tools/build-native.ps1 -Test`
Expected: `Cannot find source file: overlay/core/hint.cpp`

- [ ] **Step 4: 안내 시점 구현**

<!-- file: native/overlay/core/hint.h -->
```cpp
#pragma once

// "Insert: MLToybox" 안내를 띄우는 때를 정한다.
// 오버레이가 그리기 시작한 직후와, 게임(맵)에 들어간 직후에 각각 8초 동안 띄운다.
// 그리기 시작은 게임의 검은 시작 화면일 때라(실측) 그것만으로는 사용자가 보기 어렵다.
namespace mlt::ov {

class HintTimer {
public:
    static constexpr unsigned long long kShowMs = 8000;

    // 오버레이가 그릴 준비를 마쳤다
    void onReady(unsigned long long nowMs);
    // 모드가 알린 게임 상태(맵 안인가). 프레임마다 불러도 된다. 밖에서 안으로 바뀔 때만 다시 띄운다
    void update(bool inGame, unsigned long long nowMs);
    bool active(unsigned long long nowMs) const;

private:
    unsigned long long until_ = 0;
    bool wasInGame_ = false;
};
}
```

<!-- file: native/overlay/core/hint.cpp -->
```cpp
#include "hint.h"

namespace mlt::ov {

void HintTimer::onReady(unsigned long long nowMs) {
    until_ = nowMs + kShowMs;
}

void HintTimer::update(bool inGame, unsigned long long nowMs) {
    if (inGame && !wasInGame_) until_ = nowMs + kShowMs;
    wasInGame_ = inGame;
}

bool HintTimer::active(unsigned long long nowMs) const {
    return nowMs < until_;
}

}
```

- [ ] **Step 5: 빌드하고 입력·렌더 테스트가 실패하는 것을 확인**

Run: `pwsh E:/MLToybox/tools/build-native.ps1 -Test`
Expected: 빌드는 되고 테스트 4개가 실패한다(`89 tests, 4 failed`).
- `overlay_input_toggle_key_opens_and_closes_and_ignores_key_repeat`, `overlay_input_swallows_only_what_the_overlay_wants`: 두 번째 창부터는 후킹이 걸리지 않는다
- `overlay_input_releases_held_buttons_when_the_window_closes`: 닫아도 마우스가 잡힌 채다
- `overlay_render_draws_and_never_blocks_the_games_resize`: `DXGI_PRESENT_TEST` 호출에도 그린다

- [ ] **Step 6: 입력 구현**

`input.h`와 `input.cpp`는 파일 전체를 바꾼다.

<!-- file: native/overlay/render/input.h -->
```cpp
#pragma once
#include <mutex>
#include <windows.h>

namespace mlt::ov {
// ImGui 는 스레드 안전하지 않다. 화면 스레드(프레임)와 창 스레드(입력)가 이 잠금 아래에서만 ImGui 를 부른다.
// 재진입 잠금이어야 한다: 창 프로시저는 Win32 호출(ReleaseCapture 등) 안에서 같은 스레드로 다시 불린다.
std::recursive_mutex& imguiMutex();
// 게임 창의 창 프로시저를 바꿔 토글 키와 입력을 가로챈다. 이전 프로시저는 이어 부른다.
// 이미 걸어 둔 창이 살아 있으면 아무것도 하지 않는다. 그 창이 없어졌으면 새 창에 다시 건다
bool installWndProc(HWND hwnd);
// 창 스레드가 다음 메시지를 기다리지 않고 한 번 돌게 한다(빈 메시지를 보낸다). 어느 스레드에서 불러도 된다
void wakeWindowThread();
}
```

<!-- file: native/overlay/render/input.cpp -->
```cpp
#include "input.h"
#include "guard.h"
#include "overlay/ui/app.h"
#include <atomic>
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
bool g_wasVisible = false;   // 창 스레드만 쓴다

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

LRESULT CALLBACK HookedWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    App& a = app();
    const WNDPROC original = g_original.load();
    // 오버레이가 그릴 수 없는 상태면 토글 키를 포함해 아무것도 가로채지 않는다
    if (a.state.load() != OverlayState::Ready) return CallWindowProcW(original, hwnd, msg, wParam, lParam);

    const bool toggleKey = static_cast<int>(wParam) == a.toggleVk.load();
    if (toggleKey && (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN)) {
        if (!(lParam & (1LL << 30))) a.visible = !a.visible.load();   // 누르고 있는 동안의 반복 입력은 무시
        noteVisibility(hwnd, a);
        return 0;
    }
    if (toggleKey && (msg == WM_KEYUP || msg == WM_SYSKEYUP)) return 0;
    noteVisibility(hwnd, a);

    if (a.visible.load()) {
        // Win32 의 캡처·키 상태 함수는 창을 가진 스레드에서 불러야 한다. 그래서 입력은 이 스레드에서 ImGui 에 넣는다.
        // 다른 스레드가 SendMessage 로 보낸 메시지는 넘기지 않는다: 보낸 쪽이 ImGui 잠금을 쥐고 기다리고 있으면 서로 멈춘다.
        // 마우스·키 입력은 큐로 오는 메시지라 여기에 걸리지 않는다
        if (!InSendMessage()) {
            InputArgs args{ hwnd, msg, wParam, lParam };
            guarded(feedImGui, args);
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
```

- [ ] **Step 7: 화면 출력 후킹과 안내 고치기**

<!-- edit: native/overlay/render/dx12_hook.cpp -->
`native/overlay/render/dx12_hook.cpp` (1/10) — 찾을 부분:

```cpp
struct SeenQueue {
    ID3D12CommandQueue* queue;
    DWORD thread;
};
```

바꿀 내용:

```cpp
struct SeenQueue {
    ID3D12CommandQueue* queue;
    DWORD thread;
    unsigned long long order;   // 몇 번째 실행이었는가(클수록 최근)
};
```

<!-- edit: native/overlay/render/dx12_hook.cpp -->
`native/overlay/render/dx12_hook.cpp` (2/10) — 찾을 부분:

```cpp
int g_seenCount = 0;
```

바꿀 내용:

```cpp
int g_seenCount = 0;
unsigned long long g_seenOrder = 0;
```

<!-- edit: native/overlay/render/dx12_hook.cpp -->
`native/overlay/render/dx12_hook.cpp` (3/10) — 찾을 부분:

```cpp
bool g_holdsImgui = false;
bool g_holdsApp = false;
```

바꿀 내용:

```cpp
// Present 와 ResizeBuffers 가 다른 스레드에서 불려도 남의 잠금을 풀지 않게 스레드마다 둔다
thread_local bool g_holdsImgui = false;
thread_local bool g_holdsApp = false;
```

<!-- edit: native/overlay/render/dx12_hook.cpp -->
`native/overlay/render/dx12_hook.cpp` (4/10) — 찾을 부분:

```cpp
    for (int i = 0; i < g_seenCount; ++i) {
        if (g_seen[i].thread == tid) return g_seen[i].queue;
    }
    return nullptr;
```

바꿀 내용:

```cpp
    // 이 스레드에서 실행된 큐가 둘 이상이면 가장 최근에 실행된 것
    const SeenQueue* latest = nullptr;
    for (int i = 0; i < g_seenCount; ++i) {
        if (g_seen[i].thread == tid && (!latest || g_seen[i].order > latest->order)) latest = &g_seen[i];
    }
    return latest ? latest->queue : nullptr;
```

<!-- edit: native/overlay/render/dx12_hook.cpp -->
`native/overlay/render/dx12_hook.cpp` (5/10) — 찾을 부분:

```cpp
    OverlayState expected = OverlayState::Waiting;
    if (!app().state.compare_exchange_strong(expected, OverlayState::Ready)) {
        expected = OverlayState::Starting;
        app().state.compare_exchange_strong(expected, OverlayState::Ready);
    }
```

바꿀 내용:

```cpp
    // 꺼진 상태가 아니면 Ready 로 둔다(작업 스레드가 Starting 을 Waiting 으로 바꾸는 것과 겹쳐도 Ready 로 끝난다)
    OverlayState current = app().state.load();
    while (current != OverlayState::Disabled && !app().state.compare_exchange_weak(current, OverlayState::Ready)) {
    }
```

<!-- edit: native/overlay/render/dx12_hook.cpp -->
`native/overlay/render/dx12_hook.cpp` (6/10) — 찾을 부분:

```cpp
    App& a = app();
    {
        TrackedLock imguiLock(imguiMutex(), g_holdsImgui);
        TrackedLock appLock(a.mutex, g_holdsApp);
        ImGui::GetStyle().FontScaleMain = a.scale.load();
```

바꿀 내용:

```cpp
    App& a = app();
    const bool openAtStart = a.visible.load();
    {
        TrackedLock imguiLock(imguiMutex(), g_holdsImgui);
        TrackedLock appLock(a.mutex, g_holdsApp);
        ImGui::GetStyle().FontScaleMain = a.scale.load();
```

<!-- edit: native/overlay/render/dx12_hook.cpp -->
`native/overlay/render/dx12_hook.cpp` (7/10) — 찾을 부분:

```cpp
        a.wantKeyboard = visible && (io.WantCaptureKeyboard || io.WantTextInput);
    }
```

바꿀 내용:

```cpp
        a.wantKeyboard = visible && (io.WantCaptureKeyboard || io.WantTextInput);
        if (openAtStart && !visible) wakeWindowThread();   // 화면에서 닫았다. 창 스레드가 눌린 채 남은 입력을 정리하게 한다
    }
```

<!-- edit: native/overlay/render/dx12_hook.cpp -->
`native/overlay/render/dx12_hook.cpp` (8/10) — 찾을 부분:

```cpp
    if (app().state.load() != OverlayState::Disabled) {
        unsigned long code = 0;
        if (!runGuarded(frameThunk, swap, &code)) afterCrash(code);
```

바꿀 내용:

```cpp
    // DXGI_PRESENT_TEST 는 화면에 내지 않는 확인용 호출이다. 그리지 않는다
    if (!(flags & DXGI_PRESENT_TEST) && app().state.load() != OverlayState::Disabled) {
        unsigned long code = 0;
        if (!runGuarded(frameThunk, swap, &code)) afterCrash(code);
```

<!-- edit: native/overlay/render/dx12_hook.cpp -->
`native/overlay/render/dx12_hook.cpp` (9/10) — 찾을 부분:

```cpp
    if (g_recordQueues.load() && queue->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT) {
```

바꿀 내용:

```cpp
    // 큐를 고른 뒤나 오버레이가 꺼진 뒤에는 기록하지 않는다(게임의 모든 실행이 이 후킹을 지나간다)
    if (g_recordQueues.load() && app().state.load() != OverlayState::Disabled && queue->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT) {
```

<!-- edit: native/overlay/render/dx12_hook.cpp -->
`native/overlay/render/dx12_hook.cpp` (10/10) — 찾을 부분:

```cpp
        if (slot >= 0) g_seen[slot].thread = tid;
```

바꿀 내용:

```cpp
        if (slot >= 0) {
            g_seen[slot].thread = tid;
            g_seen[slot].order = ++g_seenOrder;
        }
```

<!-- edit: native/overlay/ui/window.cpp -->
`native/overlay/ui/window.cpp` (1/5) — 찾을 부분:

```cpp
#include "window.h"
#include "runtime.h"
```

바꿀 내용:

```cpp
#include "window.h"
#include "overlay/core/hint.h"
#include "runtime.h"
```

<!-- edit: native/overlay/ui/window.cpp -->
`native/overlay/ui/window.cpp` (2/5) — 찾을 부분:

```cpp
constexpr ULONGLONG kHintMs = 8000;
ULONGLONG g_readyTick = 0;
```

바꿀 내용:

```cpp
HintTimer g_hint;   // 안내를 띄울 때(core/hint)
```

<!-- edit: native/overlay/ui/window.cpp -->
`native/overlay/ui/window.cpp` (3/5) — 찾을 부분:

```cpp
    return g_readyTick != 0 && GetTickCount64() - g_readyTick < kHintMs;
```

바꿀 내용:

```cpp
    return g_hint.active(GetTickCount64());
```

<!-- edit: native/overlay/ui/window.cpp -->
`native/overlay/ui/window.cpp` (4/5) — 찾을 부분:

```cpp
    g_readyTick = GetTickCount64();
```

바꿀 내용:

```cpp
    g_hint.onReady(GetTickCount64());
```

<!-- edit: native/overlay/ui/window.cpp -->
`native/overlay/ui/window.cpp` (5/5) — 찾을 부분:

```cpp
bool overlayWantsFrame(App& a) {
    return a.visible.load() || hintActive();
```

바꿀 내용:

```cpp
bool overlayWantsFrame(App& a) {
    // 게임(맵)에 들어간 순간에도 안내를 띄운다. 그리기 시작할 때의 안내는 검은 시작 화면에 떠서 보기 어렵다
    const BridgeState state = Bridge::evaluate(a.status.get(), a.lastSentSeq, nowEpochSeconds());
    g_hint.update(state == BridgeState::Applied || state == BridgeState::Pending, GetTickCount64());
    return a.visible.load() || hintActive();
```

- [ ] **Step 8: 빌드와 테스트 통과 확인**

Run: `pwsh E:/MLToybox/tools/build-native.ps1 -Test`
Expected: `100% tests passed, 0 tests failed out of 1`. `native/overlay/` 소스에서 경고가 없다.

Run: `& E:\MLToybox\native\build\native_tests.exe | Select-Object -Last 1`
Expected: `89 tests, 0 failed`

- [ ] **Step 9: 게임 확인**

화면 출력과 입력 경로를 고쳤으므로 게임에서 한 번 돌린다. 게임과 패널이 꺼져 있는지 보고(켜져 있으면 묻는다), 사용자에게 게임 창을 누르지 말라고 알린 뒤 진행한다. 닫힌 채로 시작해 세이브를 불러온 직후의 안내를 찍는다.

```powershell
Get-Process -Name 'ManorLords-Win64-Shipping', 'MLToybox.Panel' -ErrorAction SilentlyContinue | Select-Object Name, Id
pwsh E:\MLToybox\tools\backup-saves.ps1
pwsh E:\MLToybox\tools\deploy.ps1 -Mod MLToybox
pwsh E:\MLToybox\tools\deploy.ps1 -Mod MLToyboxLab
. E:\MLToybox\tools\common.ps1
$mod = Join-Path (Get-MLModsDir) 'MLToybox'
$shots = New-Item -ItemType Directory -Force (Join-Path $env:TEMP 'mltb-overlay-shots')
Copy-Item "$mod\bridge\control.json" "$shots\control.before.json" -Force
if (-not (Test-Path "$shots\overlay.before.json")) { Copy-Item "$mod\bridge\overlay.json" "$shots\overlay.before.json" }   # 사용자가 쓰던 창 위치·키·배율. Task 9 에서 되돌린다(이 단계를 다시 돌려도 덮지 않는다)
@{ toggleKey = 'Insert'; scale = 1.0; window = @{ x = 1250; y = 300; w = 640; h = 720 }; startOpen = $false; devTab = $null } | ConvertTo-Json | Set-Content "$mod\bridge\overlay.json" -Encoding utf8NoBOM
& E:\MLToybox\tools\lab-load.ps1 -Slot saveGame_8 -Start
& E:\MLToybox\tools\capture-game.ps1 -Out "$shots\hint-1.png" -WaitMs 1500
& E:\MLToybox\tools\capture-game.ps1 -Out "$shots\hint-2.png" -WaitMs 3000
(Get-Content "$mod\bridge\status.json" -Raw | ConvertFrom-Json).overlay
```

Expected: `overlay`가 `loaded = True`, `state = ready`. `hint-1.png`나 `hint-2.png`의 왼쪽 위에 "Insert: MLToybox"가 있다(맵에 들어간 뒤 8초 동안).

토글, 마우스 버튼 메시지, 해상도 변경을 본다.

```powershell
& E:\MLToybox\tools\capture-game.ps1 -Key Insert; Start-Sleep -Seconds 2; Get-Content "$mod\bridge\overlay_status.json"
1..5 | ForEach-Object { & E:\MLToybox\tools\capture-game.ps1 -Click 1500, 600; Start-Sleep -Milliseconds 400 }
& E:\MLToybox\tools\lab.ps1 -File E:\MLToybox\tools\lab\resize.lua -Vars @{ W = 1280; H = 720; MODE = 2 }
Start-Sleep -Seconds 4
& E:\MLToybox\tools\capture-game.ps1 -Out "$shots\resized.png"
& E:\MLToybox\tools\lab.ps1 -File E:\MLToybox\tools\lab\resize.lua -Vars @{ W = 1920; H = 1080; MODE = 1 }
Start-Sleep -Seconds 4
Get-Content "$mod\bridge\overlay_status.json"
Get-Process -Name 'ManorLords-Win64-Shipping' | Select-Object Name, Id
```

Expected: 토글 뒤 `"visible":true`. 버튼 메시지 뒤에도 게임 프로세스가 살아 있고 마지막 상태가 `"state":"ready"`이며 `frames`가 늘어 있다. `resized.png`에서 창이 1280×720 화면 안에 다 들어와 있다.

저장하지 않고 끈다. `control.json`이 바뀌지 않았는지 본다.

```powershell
Stop-Process -Name 'ManorLords-Win64-Shipping' -Force
Start-Sleep -Seconds 4
Get-Process -Name 'CrashReportClient' -ErrorAction SilentlyContinue | Stop-Process -Force
(Get-FileHash "$mod\bridge\control.json").Hash -eq (Get-FileHash "$shots\control.before.json").Hash
```

Expected: `True`

- [ ] **Step 10: 커밋**

```bash
git -C E:/MLToybox add native/overlay native/tests native/CMakeLists.txt
git -C E:/MLToybox commit -m "fix(overlay): input reset on close, re-hook, queue choice, hint on entering a map" -m "Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 2: 파일과 작업 스레드 다지기

- 손으로 고치다 문법을 틀린 `control.json`으로 시작하면 오버레이는 기본값으로 뜨고, 첫 변경이 파일을 기본값으로 덮는다. 덮기 전에 `control.json.bak`으로 사본을 남긴다.
- 저장(`dump`)이 잘못된 UTF-8에서 예외를 던지지 않게 한다. 작업 스레드의 한 회차가 예외로 끊겨도 스레드는 계속 돈다.
- 파일은 삭제 공유로 읽는다(모드가 `status.json`을 지우고 다시 쓰는 것을 막지 않는다). `overlay.json` 저장이 실패하면 다음 회차에 다시 쓴다.

**Files:**
- Modify: `native/overlay/core/bridge.h`, `native/overlay/core/bridge.cpp`(파일 전체), `native/overlay/core/control_doc.h`, `native/overlay/core/control_doc.cpp`
- Modify: `native/overlay/worker.cpp`, `native/overlay/ui/app.h`, `native/overlay/ui/tab_status.cpp`
- Modify: `native/tests/overlay_bridge_tests.cpp`, `native/tests/overlay_control_doc_tests.cpp`

**Interfaces:**
- Consumes: 계획 A의 `Bridge`, `ControlDoc`, `App`
- Produces:
  - `overlay/core/control_doc.h`: `static std::optional<ControlDoc> ControlDoc::tryParse(std::string_view)`
  - `overlay/core/bridge.h`: `std::optional<std::string> readFileShared(const std::filesystem::path&)`, `struct LoadedControl { ControlDoc doc; bool unreadable; }`, `LoadedControl Bridge::loadControlChecked() const`, `bool Bridge::backupControl() const`, `Bridge::controlBackupPath()`
  - `overlay/ui/app.h`: `std::atomic<long long> App::workerErrors`

- [ ] **Step 1: 실패하는 테스트 작성**

<!-- edit: native/tests/overlay_bridge_tests.cpp -->
`native/tests/overlay_bridge_tests.cpp` — 찾을 부분:

```cpp
// Lua 스펙(overlay_fixture_spec.lua)이 읽는 견본. 코어의 출력이 바뀌면 이 테스트가 실패한다.
```

바꿀 내용:

```cpp
// 저장 실패의 실제 방아쇠: 다른 프로그램이 control.json 을 잡고 있어 이름 바꾸기가 안 된다
TEST(overlay_bridge_failed_save_reports_and_leaves_the_seq_alone) {
    Bridge b(freshDir("locked"));
    ControlDoc doc;
    CHECK(b.saveControl(doc) == 1);
    HANDLE held = CreateFileW(b.controlPath().c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);   // 공유 없이 연다
    CHECK(held != INVALID_HANDLE_VALUE);
    CHECK(!b.saveControl(doc).has_value());
    CHECK(doc.seq() == 1);                 // 실패하면 문서의 seq 를 올리지 않는다
    CloseHandle(held);
    CHECK(b.saveControl(doc) == 2);        // 풀리면 다음 번호로 저장된다
    CHECK(b.peekSeq() == 2);
}

// 손으로 고치다 문법을 틀린 control.json 으로 시작하면 오버레이는 기본값으로 뜬다.
// 그 상태에서 무엇이든 바꾸면 파일을 기본값으로 덮으므로, 덮기 전에 사본을 남길 수 있어야 한다
TEST(overlay_bridge_tells_an_unreadable_control_file_from_a_missing_one) {
    Bridge b(freshDir("broken"));
    CHECK(!b.loadControlChecked().unreadable);                       // 파일이 없다: 첫 실행
    CHECK(!b.backupControl());                                       // 남길 것이 없다
    writeText(b.controlPath(), R"({"version":1,"seq":9,"features":{"build":{"enabled":true}})");   // 닫는 괄호가 없다
    const LoadedControl loaded = b.loadControlChecked();
    CHECK(loaded.unreadable && loaded.doc.seq() == 0 && !loaded.doc.build().enabled);
    CHECK(b.backupControl());
    auto kept = readFileShared(b.controlBackupPath());
    CHECK(kept.has_value() && kept->find("\"seq\":9") != std::string::npos);
    writeText(b.controlPath(), R"({"version":1,"seq":9,"features":{"build":{"enabled":true}}})");
    const LoadedControl good = b.loadControlChecked();
    CHECK(!good.unreadable && good.doc.seq() == 9 && good.doc.build().enabled);
}

TEST(overlay_bridge_shared_read_returns_the_bytes_or_nothing) {
    Bridge b(freshDir("shared"));
    CHECK(!readFileShared(b.statusPath()).has_value());
    writeText(b.statusPath(), "{\"name\":\"검사대\"}");
    CHECK(readFileShared(b.statusPath()) == "{\"name\":\"검사대\"}");
    writeText(b.statusPath(), "");
    CHECK(readFileShared(b.statusPath()) == "");
}

// Lua 스펙(overlay_fixture_spec.lua)이 읽는 견본. 코어의 출력이 바뀌면 이 테스트가 실패한다.
```

<!-- edit: native/tests/overlay_control_doc_tests.cpp -->
`native/tests/overlay_control_doc_tests.cpp` — 찾을 부분:

```cpp
TEST(overlay_control_commands_are_replaced_as_a_whole) {
```

바꿀 내용:

```cpp
TEST(overlay_control_try_parse_tells_broken_text_apart) {
    CHECK(!ControlDoc::tryParse("").has_value());
    CHECK(!ControlDoc::tryParse("{").has_value());
    CHECK(!ControlDoc::tryParse("[1,2]").has_value());
    CHECK(ControlDoc::tryParse("{}").has_value());
    CHECK(ControlDoc::tryParse(R"({"version":1,"seq":4})")->seq() == 4);
}

// 저장할 때 예외가 나면 작업 스레드의 그 회차가 날아가고 저장이 되지 않는다. 잘린 글자는 대체 문자로 쓴다
TEST(overlay_control_dump_replaces_bad_utf8_instead_of_throwing) {
    ControlDoc doc;
    doc.feature("mercenaries")["companies"] = Json::array({ Json::object({ { "name", std::string("\xEA\xB2") } }) });   // 한글 한 글자의 앞 두 바이트
    const std::string text = doc.dump();
    CHECK(text.find("\xEF\xBF\xBD") != std::string::npos);   // U+FFFD
    CHECK(ControlDoc::tryParse(text).has_value());
}

TEST(overlay_control_commands_are_replaced_as_a_whole) {
```

- [ ] **Step 2: 빌드해서 실패 확인**

Run: `pwsh E:/MLToybox/tools/build-native.ps1 -Test`
Expected: 컴파일 오류. `tryParse`, `loadControlChecked`, `readFileShared`가 없다.

- [ ] **Step 3: 구현**

`bridge.h`와 `bridge.cpp`는 파일 전체를 바꾼다.

<!-- file: native/overlay/core/bridge.h -->
```cpp
#pragma once
#include "control_doc.h"
#include "status_doc.h"
#include <filesystem>
#include <optional>
#include <string>

// bridge 폴더의 control.json / status.json 읽고 쓰기 (패널의 BridgeClient 와 같은 동작)
namespace mlt::ov {

enum class BridgeState { Disconnected, MainMenu, Pending, Applied };

// 다른 프로그램이 그 파일을 지우거나 이름을 바꾸는 것을 막지 않고 읽는다(패널도 이렇게 읽는다).
// 모드는 status.json 을 "지우고 이름 바꾸기"로 쓴다. 삭제 공유 없이 열고 있으면 그 순간 모드의 지우기가 실패한다.
// (읽는 동안 파일을 잡는 시간은 아주 짧다. 이 성질은 단위 테스트로 재현하지 못해 테스트가 없다)
std::optional<std::string> readFileShared(const std::filesystem::path& path);

struct LoadedControl {
    ControlDoc doc;
    bool unreadable = false;   // 파일은 있는데 해석하지 못했다(문서는 기본값)
};

class Bridge {
public:
    static constexpr long long kHeartbeatTimeoutSec = 5;

    explicit Bridge(std::filesystem::path dir) : dir_(std::move(dir)) {}

    std::filesystem::path controlPath() const { return dir_ / L"control.json"; }
    std::filesystem::path controlBackupPath() const { return dir_ / L"control.json.bak"; }
    std::filesystem::path statusPath() const { return dir_ / L"status.json"; }
    std::filesystem::path settingsPath() const { return dir_ / L"overlay.json"; }
    std::filesystem::path overlayStatusPath() const { return dir_ / L"overlay_status.json"; }

    // 파일이 없거나 깨졌으면 빈 문서
    ControlDoc loadControl() const;
    // 위와 같되, 파일이 있는데 해석하지 못한 경우를 알려 준다
    LoadedControl loadControlChecked() const;
    // control.json 을 control.json.bak 으로 복사한다(있던 사본은 덮는다). 파일이 없거나 복사하지 못하면 false
    bool backupControl() const;
    // 파일에 적힌 seq. 파일이 없거나 깨졌으면 값 없음
    std::optional<long long> peekSeq() const;
    // seq = max(파일의 seq, 문서의 seq) + 1 로 저장하고 그 seq 를 돌려준다. 문서의 seq 도 바꾼다. 실패하면 값 없음(문서는 그대로)
    std::optional<long long> saveControl(ControlDoc& doc) const;
    std::optional<StatusDoc> readStatus() const;

    static BridgeState evaluate(const StatusDoc* status, long long lastSentSeq, long long nowEpochSeconds);

private:
    std::filesystem::path dir_;
};
}
```

<!-- file: native/overlay/core/bridge.cpp -->
```cpp
#include "bridge.h"
#include "runtime.h"
#include <algorithm>
#include <chrono>
#include <system_error>
#include <thread>
#include <windows.h>

namespace mlt::ov {

std::optional<std::string> readFileShared(const std::filesystem::path& path) {
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return std::nullopt;
    std::string out;
    char buf[16384];
    for (;;) {
        DWORD got = 0;
        if (!ReadFile(file, buf, sizeof(buf), &got, nullptr)) {
            CloseHandle(file);
            return std::nullopt;
        }
        if (got == 0) break;
        out.append(buf, got);
    }
    CloseHandle(file);
    return out;
}

ControlDoc Bridge::loadControl() const {
    return loadControlChecked().doc;
}

LoadedControl Bridge::loadControlChecked() const {
    LoadedControl loaded;
    auto text = readFileShared(controlPath());
    if (!text) return loaded;
    if (auto doc = ControlDoc::tryParse(*text)) loaded.doc = std::move(*doc);
    else loaded.unreadable = true;
    return loaded;
}

bool Bridge::backupControl() const {
    std::error_code ec;
    return std::filesystem::copy_file(controlPath(), controlBackupPath(), std::filesystem::copy_options::overwrite_existing, ec);
}

std::optional<long long> Bridge::peekSeq() const {
    auto text = readFileShared(controlPath());
    if (!text) return std::nullopt;
    Json root = Json::parse(text->begin(), text->end(), nullptr, false);
    if (root.is_discarded() || !root.is_object()) return std::nullopt;
    auto it = root.find("seq");
    if (it == root.end() || !it->is_number()) return std::nullopt;
    return static_cast<long long>(it->get<double>());
}

std::optional<long long> Bridge::saveControl(ControlDoc& doc) const {
    std::error_code ec;
    std::filesystem::create_directories(dir_, ec);
    const long long previous = doc.seq();
    const long long next = std::max(peekSeq().value_or(0), previous) + 1;
    doc.setSeq(next);
    const std::string text = doc.dump();
    // 모드가 파일을 읽는 순간과 겹치면 이름 바꾸기가 실패할 수 있다. 잠깐 쉬고 다시 한다
    for (int attempt = 1; attempt <= 5; ++attempt) {
        if (writeFileAtomic(controlPath(), text)) return next;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    doc.setSeq(previous);
    return std::nullopt;
}

std::optional<StatusDoc> Bridge::readStatus() const {
    auto text = readFileShared(statusPath());
    if (!text) return std::nullopt;
    return parseStatus(*text);
}

BridgeState Bridge::evaluate(const StatusDoc* status, long long lastSentSeq, long long nowEpochSeconds) {
    if (!status) return BridgeState::Disconnected;
    if (nowEpochSeconds - status->heartbeat > kHeartbeatTimeoutSec) return BridgeState::Disconnected;
    if (!status->inGame) return BridgeState::MainMenu;
    return status->appliedSeq.value_or(-1) >= lastSentSeq ? BridgeState::Applied : BridgeState::Pending;
}

}
```

<!-- edit: native/overlay/core/control_doc.h -->
`native/overlay/core/control_doc.h` — 찾을 부분:

```cpp
    static ControlDoc parse(std::string_view text);
```

바꿀 내용:

```cpp
    static ControlDoc parse(std::string_view text);
    // 해석하지 못하면 값 없음(파일이 깨졌는지 가릴 때 쓴다)
    static std::optional<ControlDoc> tryParse(std::string_view text);
```

<!-- edit: native/overlay/core/control_doc.cpp -->
`native/overlay/core/control_doc.cpp` (1/2) — 찾을 부분:

```cpp
ControlDoc ControlDoc::parse(std::string_view text) {
    ControlDoc doc;
    Json parsed = Json::parse(text.begin(), text.end(), nullptr, false);
    if (parsed.is_discarded() || !parsed.is_object()) return doc;
```

바꿀 내용:

```cpp
ControlDoc ControlDoc::parse(std::string_view text) {
    return tryParse(text).value_or(ControlDoc());
}

std::optional<ControlDoc> ControlDoc::tryParse(std::string_view text) {
    ControlDoc doc;
    Json parsed = Json::parse(text.begin(), text.end(), nullptr, false);
    if (parsed.is_discarded() || !parsed.is_object()) return std::nullopt;
```

<!-- edit: native/overlay/core/control_doc.cpp -->
`native/overlay/core/control_doc.cpp` (2/2) — 찾을 부분:

```cpp
    return root_.dump(2);
```

바꿀 내용:

```cpp
    // 잘못된 UTF-8 이 섞여 있어도 예외를 던지지 않고 대체 문자(U+FFFD)로 쓴다
    return root_.dump(2, ' ', false, Json::error_handler_t::replace);
```

<!-- edit: native/overlay/worker.cpp -->
`native/overlay/worker.cpp` (1/4) — 찾을 부분:

```cpp
    if (!writeFileAtomic(bridge.settingsPath(), dumpSettings(copy))) return;
```

바꿀 내용:

```cpp
    if (!writeFileAtomic(bridge.settingsPath(), dumpSettings(copy))) {
        std::lock_guard<std::mutex> lock(a.mutex);
        a.settingsDirty = true;   // 다음 회차에 다시 쓴다
        return;
    }
```

<!-- edit: native/overlay/worker.cpp -->
`native/overlay/worker.cpp` (2/4) — 찾을 부분:

```cpp
    auto text = readFileUtf8(bridge.settingsPath());
    if (!text) return;
```

바꿀 내용:

```cpp
    auto text = readFileShared(bridge.settingsPath());
    if (!text) return;
```

<!-- edit: native/overlay/worker.cpp -->
`native/overlay/worker.cpp` (3/4) — 찾을 부분:

```cpp
        a.settings = parseSettings(readFileUtf8(bridge.settingsPath()).value_or(""));
        syncSettingsAtoms(a);
        a.visible = a.settings.startOpen;
        a.control = bridge.loadControl();
        a.lastSentSeq = a.control.seq();
```

바꿀 내용:

```cpp
        a.settings = parseSettings(readFileShared(bridge.settingsPath()).value_or(""));
        syncSettingsAtoms(a);
        a.visible = a.settings.startOpen;
        // control.json 이 있는데 읽지 못했으면(손으로 고치다 문법을 틀린 경우) 기본값으로 덮기 전에 사본을 남긴다
        LoadedControl loaded = bridge.loadControlChecked();
        if (loaded.unreadable) bridge.backupControl();
        a.control = std::move(loaded.doc);
        a.lastSentSeq = a.control.seq();
```

<!-- edit: native/overlay/worker.cpp -->
`native/overlay/worker.cpp` (4/4) — 찾을 부분:

```cpp
    for (unsigned tick = 0;; ++tick) {
        if (tick % 2 == 0 && tick >= saveRetryAt) {                // 0.2초마다. 실패했으면 1초 뒤에 다시 한다
            if (!saveControlIfDirty(a, bridge)) saveRetryAt = tick + 10;
        }
        if (tick % 10 == 0) {                                      // 1초마다
            readStatus(a, bridge);
            reloadControlIfChangedOutside(a, bridge);
            saveSettingsIfDirty(a, bridge, settingsWritten);
            reloadSettingsIfChangedOutside(a, bridge, settingsWritten);
            writeOverlayStatus(a, bridge);
        }
        Sleep(100);
    }
```

바꿀 내용:

```cpp
    for (unsigned tick = 0;; ++tick) {
        // 이 스레드가 예외로 끝나면 저장이 조용히 멈춘다. 그 회차만 건너뛰고 계속한다
        try {
            if (tick % 2 == 0 && tick >= saveRetryAt) {            // 0.2초마다. 실패했으면 1초 뒤에 다시 한다
                if (!saveControlIfDirty(a, bridge)) saveRetryAt = tick + 10;
            }
            if (tick % 10 == 0) {                                  // 1초마다
                readStatus(a, bridge);
                reloadControlIfChangedOutside(a, bridge);
                saveSettingsIfDirty(a, bridge, settingsWritten);
                reloadSettingsIfChangedOutside(a, bridge, settingsWritten);
                writeOverlayStatus(a, bridge);
            }
        } catch (...) {
            ++a.workerErrors;
        }
        Sleep(100);
    }
```

<!-- edit: native/overlay/ui/app.h -->
`native/overlay/ui/app.h` — 찾을 부분:

```cpp
    std::atomic<bool> applyWindowRect{true};   // 설정의 창 위치·크기를 다음 프레임에 적용한다
```

바꿀 내용:

```cpp
    std::atomic<bool> applyWindowRect{true};   // 설정의 창 위치·크기를 다음 프레임에 적용한다
    std::atomic<long long> workerErrors{0};    // 작업 스레드가 예외로 건너뛴 회차 수(0 이 아니면 상태 탭에 보인다)
```

<!-- edit: native/overlay/ui/tab_status.cpp -->
`native/overlay/ui/tab_status.cpp` — 찾을 부분:

```cpp
    ImGui::Text("글꼴: %s", a.koreanFont.load() ? "맑은 고딕" : "기본 글꼴(한글이 표시되지 않습니다)");
```

바꿀 내용:

```cpp
    ImGui::Text("글꼴: %s", a.koreanFont.load() ? "맑은 고딕" : "기본 글꼴(한글이 표시되지 않습니다)");
    if (const long long errors = a.workerErrors.load()) ImGui::Text("작업 스레드 오류: %lld회 (저장이 늦어질 수 있습니다)", errors);
```

삭제 공유로 읽는 성질(읽는 동안 모드가 파일을 지울 수 있다)은 단위 테스트로 재현하지 못했다. `readFileShared`가 여는 방식(`FILE_SHARE_DELETE`)을 읽어서 확인한다.

- [ ] **Step 4: 빌드와 테스트 통과 확인**

Run: `pwsh E:/MLToybox/tools/build-native.ps1 -Test`
Expected: `100% tests passed, 0 tests failed out of 1`

Run: `& E:\MLToybox\native\build\native_tests.exe | Select-Object -Last 1`
Expected: `94 tests, 0 failed`

- [ ] **Step 5: 커밋**

```bash
git -C E:/MLToybox add native/overlay native/tests
git -C E:/MLToybox commit -m "fix(overlay): keep a copy of an unreadable control.json, non-throwing dump, shared reads, worker loop guard" -m "Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 3: 문서 모델 — 군사·인구·자원·용병 설정, 탭이 읽는 상태, 명령

**Files:**
- Create: `native/overlay/core/units.h`, `native/overlay/core/units.cpp`
- Modify(파일 전체): `native/overlay/core/control_doc.h/.cpp`, `native/overlay/core/status_doc.h/.cpp`, `native/overlay/core/commands.h/.cpp`
- Create: `native/tests/overlay_features_tests.cpp`, `native/tests/overlay_status_tabs_tests.cpp`
- Modify: `native/tests/overlay_bridge_tests.cpp`, `native/CMakeLists.txt`
- Modify(파일 전체): `mod/MLToybox/tests/overlay_fixture_spec.lua`; 다시 만든다: `mod/MLToybox/tests/fixtures/control_from_overlay.json`

**Interfaces:**
- Consumes: 계획 A의 `Json`, `ControlDoc`, `StatusDoc`; 견본 `native/tests/fixtures/control_from_panel.json`, `status_from_mod.json`
- Produces (이름공간 `mlt::ov`):
  - `overlay/core/control_doc.h`: `struct MilitarySettings { bool enabled, ignoreEquipment, ignorePopulation, zeroUpkeep, unlimitedSquads; }`, `struct PopulationSettings { bool enabled; int multiplier, targetFamilies; std::map<std::string, int> regionTargets; }`, `struct ResourcesSettings { bool enabled; int intervalSec; std::map<std::string, int> targets; std::map<std::string, std::map<std::string, int>> regionTargets; }`, `struct MercCompany { std::string name; std::vector<std::string> units; int cost; std::optional<std::string> region, banner; bool enabled; }`, `struct MercSettings { bool enabled, refund, lockFromAi; std::vector<MercCompany> companies; }`, `ControlDoc::military()/setMilitary()`, `population()/setPopulation()`, `resources()/setResources()`, `mercenaries()/setMercenaries()`, `optString`(여기로 옮김), `arrayAt`
  - `overlay/core/status_doc.h`: `RegionInfo { key, name }`, `RegionResources { key, name, values }`, `SpawnStatus { disbanded, pending, byUnit }`, `RetinueSquad { id, unit, count, kind }`, `RetinueStatus { squads, editing }`, `PopulationRegion`, `PopulationStatus`, `MercSlot`, `MercSkipped`, `MercenaryStatus`; `StatusDoc`에 `resourceIds`, `resources`, `regions`, `playerRegions`, `spawn`, `retinue`, `population`, `mercenaries`(모두 `std::optional`)
  - `overlay/core/commands.h`: `makeSpawnSquads(unit, count, now, region)`, `makeReformSquads(now, region)`, `makeCustomizeRetinue(squadId, now, region)`, `makeAddFamilies(count, now, region)`. `region`은 `const std::optional<std::string>&`
  - `overlay/core/units.h`: `struct UnitOption { id, label }`, `const std::vector<UnitOption>& units()`, `std::string unitLabel(std::string_view)`, `bool isKnownUnit(std::string_view)`, `struct ScopeOption { std::optional<std::string> key; std::string label; }`, `std::string regionLabel(name, key)`, `int indexOfKey(const std::vector<ScopeOption>&, const std::optional<std::string>&)`

- [ ] **Step 1: 실패하는 테스트 작성**

<!-- file: native/tests/overlay_features_tests.cpp -->
```cpp
#include "test.h"
#include "overlay/core/commands.h"
#include "overlay/core/control_doc.h"
#include "overlay/core/units.h"
#include "runtime.h"
#include <filesystem>

using namespace mlt::ov;

static ControlDoc panelFixture() {
    auto text = mlt::readFileUtf8(std::filesystem::path(MLT_FIXTURES_DIR) / "control_from_panel.json");
    CHECK(text.has_value());
    return ControlDoc::parse(*text);
}

TEST(overlay_features_defaults_match_the_panel) {
    ControlDoc doc;
    const MilitarySettings m = doc.military();
    CHECK(!m.enabled && m.ignoreEquipment && m.ignorePopulation && m.zeroUpkeep && m.unlimitedSquads);
    const PopulationSettings p = doc.population();
    CHECK(!p.enabled && p.multiplier == 2 && p.targetFamilies == 0 && p.regionTargets.empty());
    const ResourcesSettings r = doc.resources();
    CHECK(!r.enabled && r.intervalSec == 2 && r.targets.empty() && r.regionTargets.empty());
    const MercSettings me = doc.mercenaries();
    CHECK(!me.enabled && me.refund && me.lockFromAi && me.companies.empty());
}

TEST(overlay_features_read_a_file_written_by_the_panel) {
    const ControlDoc doc = panelFixture();
    CHECK(doc.military().enabled && doc.military().unlimitedSquads);
    const PopulationSettings p = doc.population();
    CHECK(p.enabled && p.multiplier == 1 && p.targetFamilies == 0 && p.regionTargets.empty());
    const ResourcesSettings r = doc.resources();
    CHECK(r.enabled && r.intervalSec == 2 && r.targets.size() == 56 && r.targets.at("Ale") == 500);
    CHECK(r.regionTargets.size() == 2 && r.regionTargets.at("sel").at("Ale") == 1000 && r.regionTargets.at("hof").at("Barley") == 600);
    const MercSettings me = doc.mercenaries();
    CHECK(me.enabled && me.refund && me.lockFromAi && me.companies.size() == 1);
    const MercCompany& c = me.companies[0];
    CHECK(c.name == "검사대" && c.units.size() == 4 && c.units[0] == "mercenary_infantry" && c.cost == 1000);
    CHECK(c.region == "nus" && c.banner == "battle_brothers" && c.enabled);
}

TEST(overlay_features_population_and_resources_round_trip) {
    ControlDoc doc;
    PopulationSettings p;
    p.enabled = true;
    p.multiplier = 3;
    p.targetFamilies = 12;
    p.regionTargets["sel"] = 30;
    p.regionTargets["hof"] = 0;                       // 0 = 그 영지는 끔. 값으로 남는다
    doc.setPopulation(p);
    ResourcesSettings r;
    r.enabled = true;
    r.intervalSec = 5;
    r.targets["Timber"] = 500;
    r.regionTargets["hof"]["Timber"] = 2000;
    doc.setResources(r);
    const ControlDoc back = ControlDoc::parse(doc.dump());
    const PopulationSettings p2 = back.population();
    CHECK(p2.enabled && p2.multiplier == 3 && p2.targetFamilies == 12 && p2.regionTargets.size() == 2);
    CHECK(p2.regionTargets.at("sel") == 30 && p2.regionTargets.at("hof") == 0);
    const ResourcesSettings r2 = back.resources();
    CHECK(r2.enabled && r2.intervalSec == 5 && r2.targets.at("Timber") == 500 && r2.regionTargets.at("hof").at("Timber") == 2000);
    // 영지 목표를 지우면 키도 사라진다
    r.regionTargets.erase("hof");
    doc.setResources(r);
    CHECK(Json::parse(doc.dump())["features"]["resources"]["regionTargets"].empty());
}

TEST(overlay_features_mercenary_companies_round_trip_and_omit_what_is_not_chosen) {
    ControlDoc doc;
    MercSettings m;
    m.enabled = true;
    m.lockFromAi = false;
    MercCompany chosen{ "토이박스 용병단", { "mercenary_infantry", "mercenary_infantry", "mercenary_crossbowmen" }, 3000, "gold", "greencaps", true };
    MercCompany plain{ "B", { "militia" }, 0, std::nullopt, std::nullopt, false };
    m.companies = { chosen, plain };
    doc.setMercenaries(m);
    const Json j = Json::parse(doc.dump());
    const Json& companies = j["features"]["mercenaries"]["companies"];
    CHECK(companies[0]["name"] == "토이박스 용병단" && companies[0]["units"].size() == 3 && companies[0]["units"][2] == "mercenary_crossbowmen");
    CHECK(companies[0]["cost"] == 3000 && companies[0]["region"] == "gold" && companies[0]["banner"] == "greencaps" && companies[0]["enabled"] == true);
    CHECK(!companies[1].contains("region") && !companies[1].contains("banner") && companies[1]["enabled"] == false);
    const MercSettings back = ControlDoc::parse(doc.dump()).mercenaries();
    CHECK(back.enabled && back.refund && !back.lockFromAi && back.companies.size() == 2);
    CHECK(back.companies[0] == chosen && back.companies[1] == plain);
}

TEST(overlay_features_tolerate_hand_edited_garbage) {
    const ControlDoc doc = ControlDoc::parse(R"({"version":1,"seq":2,"features":{
        "military":{"enabled":"yes","zeroUpkeep":0},
        "population":{"multiplier":"many","regionTargets":["a"]},
        "resources":{"targets":{"Timber":"lots","Stone":-5,"Iron":7.0,"Clay":7.5},"regionTargets":{"hof":5,"sel":{"Timber":9}}},
        "mercenaries":{"companies":[7,{"name":5,"units":["militia",3,null],"cost":"free","region":9},"x",{"name":"ok","units":"all"}]}}})");
    CHECK(!doc.military().enabled && doc.military().zeroUpkeep);        // 형식이 다르면 기본값
    CHECK(doc.population().multiplier == 2 && doc.population().regionTargets.empty());
    const ResourcesSettings r = doc.resources();
    CHECK(r.targets.size() == 1 && r.targets.at("Iron") == 7);          // 글자, 음수, 소수는 버린다
    CHECK(r.regionTargets.size() == 1 && r.regionTargets.at("sel").at("Timber") == 9);
    const MercSettings m = doc.mercenaries();
    CHECK(m.companies.size() == 2);                                     // 객체가 아닌 항목은 건너뛴다
    CHECK(m.companies[0].name.empty() && m.companies[0].units.size() == 1 && m.companies[0].cost == 0 && !m.companies[0].region);
    CHECK(m.companies[1].name == "ok" && m.companies[1].units.empty() && m.companies[1].enabled);
    CHECK(ControlDoc::parse(R"({"features":{"mercenaries":{"companies":{"a":1}}}})").mercenaries().companies.empty());
}

TEST(overlay_features_setters_keep_unknown_keys_of_the_feature) {
    ControlDoc doc = ControlDoc::parse(R"({"version":1,"seq":1,"features":{"military":{"enabled":false,"future":1},"mercenaries":{"future":"x"}}})");
    MilitarySettings m = doc.military();
    m.enabled = true;
    doc.setMilitary(m);
    doc.setMercenaries(doc.mercenaries());
    const Json j = Json::parse(doc.dump());
    CHECK(j["features"]["military"]["future"] == 1 && j["features"]["military"]["enabled"] == true);
    CHECK(j["features"]["mercenaries"]["future"] == "x" && j["features"]["mercenaries"]["companies"].is_array());
}

TEST(overlay_units_list_the_thirteen_playable_units) {
    CHECK(units().size() == 13);
    CHECK(units()[1].id == "spearMilitia" && units()[1].label == "민병대 - 창");
    CHECK(unitLabel("retinue_tier3") == "친위대 - 3단계" && unitLabel("Mercenary_Archers") == "용병 - 궁수");
    CHECK(unitLabel("dragon") == "dragon");                             // 모르는 병종은 id 그대로
    CHECK(isKnownUnit("mercenary_crossbowmen") && !isKnownUnit("dragon") && !isKnownUnit("mercenary_archers"));   // 대소문자를 가린다
}

TEST(overlay_scope_options_find_a_key_or_fall_back_to_the_first_line) {
    const std::vector<ScopeOption> options = { { std::nullopt, "공통" }, { "gold", regionLabel("Mandlach", "gold") }, { "nus", "nus" } };
    CHECK(options[1].label == "Mandlach (gold)");
    CHECK(indexOfKey(options, std::nullopt) == 0 && indexOfKey(options, "gold") == 1 && indexOfKey(options, "nus") == 2);
    CHECK(indexOfKey(options, "gone") == 0);                            // 사라진 영지는 첫 줄로
    CHECK(indexOfKey({}, "gold") == 0);
}

TEST(overlay_commands_carry_the_fields_the_mod_reads) {
    const Json spawn = makeSpawnSquads("spearMilitia", 3, 1790000000, "nus");
    CHECK(spawn["type"] == "spawnSquads" && spawn["unit"] == "spearMilitia" && spawn["count"] == 3 && spawn["region"] == "nus" && spawn["issuedAt"] == 1790000000);
    CHECK(spawn["id"].get<std::string>().size() == 32);
    CHECK(!makeSpawnSquads("militia", 1, 1, std::nullopt).contains("region"));   // 고르지 않았으면 키가 없다
    const Json reform = makeReformSquads(1790000001, "gold");
    CHECK(reform["type"] == "reformSquads" && reform["region"] == "gold" && !reform.contains("unit"));
    CHECK(!makeReformSquads(1, std::nullopt).contains("region"));
    const Json retinue = makeCustomizeRetinue(63, 1790000002, "nus");
    CHECK(retinue["type"] == "customizeRetinue" && retinue["value"] == 63 && retinue["region"] == "nus");
    CHECK(!makeCustomizeRetinue(64, 1, std::nullopt).contains("region"));
    const Json families = makeAddFamilies(3, 1790000003, "sel");
    CHECK(families["type"] == "addFamilies" && families["count"] == 3 && families["region"] == "sel");
    CHECK(!makeAddFamilies(2, 1, std::nullopt).contains("region"));
    CHECK(spawn["id"] != reform["id"] && retinue["id"] != families["id"]);
}
```

<!-- file: native/tests/overlay_status_tabs_tests.cpp -->
```cpp
#include "test.h"
#include "overlay/core/status_doc.h"
#include "runtime.h"
#include <filesystem>

using namespace mlt::ov;

// 탭이 읽는 상태 항목(자원, 영지, 병력, 수행원, 인구, 용병)
TEST(overlay_status_reads_the_tab_data_from_a_real_file) {
    auto text = mlt::readFileUtf8(std::filesystem::path(MLT_FIXTURES_DIR) / "status_from_mod.json");
    CHECK(text.has_value());
    auto s = parseStatus(*text);
    CHECK(s.has_value());
    CHECK(s->resourceIds.has_value() && s->resourceIds->size() == 56 && (*s->resourceIds)[1] == "Timber");
    CHECK(s->resources.has_value() && s->resources->at("Timber") == 1849.0);
    CHECK(s->regions.has_value() && s->regions->size() == 3);
    CHECK((*s->regions)[0].key == "gold" && (*s->regions)[0].name == "Mandlach" && (*s->regions)[0].values.at("Timber") == 722.0);
    CHECK(s->playerRegions.has_value() && s->playerRegions->size() == 3 && (*s->playerRegions)[2].key == "nus" && (*s->playerRegions)[2].name == "Haderwand");
    CHECK(s->spawn.has_value() && s->spawn->disbanded == 0 && s->spawn->pending == 0 && s->spawn->byUnit.empty());   // byUnit 은 [] 로 적혀 있다
    CHECK(s->retinue.has_value() && s->retinue->squads.empty() && !s->retinue->editing);
    const PopulationStatus& p = *s->population;
    CHECK(p.families == 157 && p.population == 486 && p.homeless == 0 && p.freeSlots == 58 && p.unassigned == 67 && p.natural == 2 && p.multiplied == 0);
    CHECK(p.regions.size() == 3 && p.regions[1].key == "Lei" && p.regions[1].families == 5 && p.regions[1].freeSlots == 15);
    const MercenaryStatus& m = *s->mercenaries;
    CHECK(m.slots.size() == 3 && m.slots[0].name == "검사대" && m.slots[0].cost == 10000000 && m.slots[0].custom && !m.slots[1].custom);
    CHECK(m.hiredMine == 1 && m.hiredAi == 1 && m.refunded == 0 && m.skipped.empty() && !m.note);
}

TEST(overlay_status_reads_spawn_retinue_and_mercenary_details) {
    auto s = parseStatus(R"({"version":1,"heartbeat":10,"inGame":true,
        "spawn":{"disbanded":3,"pending":1,"byUnit":{"retinue_tier1":2,"militia":1}},
        "retinue":{"squads":[{"id":63,"unit":"retinue_tier1","count":36,"kind":"spawned"},{"id":64,"unit":"retinue_tier3","count":12,"kind":"mercenary"}],"editing":63},
        "mercenaries":{"slots":[{"name":"토이박스 용병단","cost":10000000,"custom":true}],"hiredMine":2,"hiredAi":3,"refunded":6000,
                       "skipped":[{"name":"궁수대","reason":"unknown unit: foo"}],"note":"rebuild produced 1 of 3 slots"}})");
    CHECK(s.has_value());
    CHECK(s->spawn->disbanded == 3 && s->spawn->pending == 1 && s->spawn->byUnit.size() == 2);
    CHECK(s->spawn->byUnit[0].first == "retinue_tier1" && s->spawn->byUnit[0].second == 2);   // 파일에 적힌 순서
    CHECK(s->retinue->squads.size() == 2 && s->retinue->squads[1].id == 64 && s->retinue->squads[1].kind == "mercenary" && s->retinue->editing == 63);
    CHECK(s->mercenaries->refunded == 6000 && s->mercenaries->skipped.size() == 1 && s->mercenaries->skipped[0].reason == "unknown unit: foo");
    CHECK(s->mercenaries->note == "rebuild produced 1 of 3 slots");
}

TEST(overlay_status_tab_data_is_absent_or_empty_when_the_mod_sends_nothing) {
    auto s = parseStatus(R"({"version":1,"heartbeat":10,"inGame":false})");
    CHECK(s.has_value());
    CHECK(!s->resourceIds && !s->resources && !s->regions && !s->playerRegions && !s->spawn && !s->retinue && !s->population && !s->mercenaries);
    // 모드의 JSON 인코더는 빈 표를 [] 로 쓴다
    s = parseStatus(R"({"version":1,"heartbeat":10,"inGame":true,"resources":[],"resourceIds":[],"regions":[],"playerRegions":[],
        "spawn":{"disbanded":0,"byUnit":[],"pending":0},"retinue":{"squads":[]},"population":{"families":1,"regions":[]},
        "mercenaries":{"slots":[],"hiredMine":0,"hiredAi":0,"refunded":0,"skipped":[]}})");
    CHECK(s.has_value());
    CHECK(s->resources.has_value() && s->resources->empty() && s->resourceIds->empty() && s->regions->empty() && s->playerRegions->empty());
    CHECK(s->spawn->byUnit.empty() && s->retinue->squads.empty() && s->population->families == 1 && s->population->regions.empty());
    CHECK(s->mercenaries->slots.empty() && s->mercenaries->skipped.empty());
    // 형식이 다른 항목은 건너뛴다
    s = parseStatus(R"({"heartbeat":10,"regions":[5,{"key":"a","values":[]}],"playerRegions":"x","retinue":{"squads":[1,{"id":"x"}]}})");
    CHECK(s.has_value() && s->regions->size() == 1 && (*s->regions)[0].values.empty() && !s->playerRegions);
    CHECK(s->retinue->squads.size() == 1 && s->retinue->squads[0].id == 0);
}
```

Lua 스펙이 읽는 견본에 새 기능을 넣는다.

<!-- edit: native/tests/overlay_bridge_tests.cpp -->
`native/tests/overlay_bridge_tests.cpp` — 찾을 부분:

```cpp
    doc.setLord(lord);
    Json command = makeSetLord("influence", 20000, 1790000000);
    command["id"] = "0123456789abcdef0123456789abcdef";
    doc.setCommands({ command });
```

바꿀 내용:

```cpp
    doc.setLord(lord);
    MilitarySettings military;
    military.enabled = true;
    military.zeroUpkeep = false;
    doc.setMilitary(military);
    PopulationSettings population;
    population.enabled = true;
    population.multiplier = 3;
    population.targetFamilies = 12;
    population.regionTargets["nus"] = 30;
    doc.setPopulation(population);
    ResourcesSettings resources;
    resources.enabled = true;
    resources.targets["Timber"] = 500;
    resources.regionTargets["gold"]["Timber"] = 2000;
    doc.setResources(resources);
    MercSettings mercenaries;
    mercenaries.enabled = true;
    mercenaries.companies = {
        { "토이박스 용병단", { "mercenary_infantry", "mercenary_crossbowmen" }, 3000, "gold", "greencaps", true },
        { "예비대", { "militia" }, 0, std::nullopt, std::nullopt, false },
    };
    doc.setMercenaries(mercenaries);
    Json command = makeSetLord("influence", 20000, 1790000000);
    command["id"] = "0123456789abcdef0123456789abcdef";
    Json spawn = makeSpawnSquads("spearMilitia", 2, 1790000000, "nus");
    spawn["id"] = "fedcba9876543210fedcba9876543210";
    doc.setCommands({ command, spawn });
```

<!-- edit: native/CMakeLists.txt -->
`native/CMakeLists.txt` (1/2) — 찾을 부분:

```cmake
    overlay/core/hint.cpp)
```

바꿀 내용:

```cmake
    overlay/core/hint.cpp
    overlay/core/units.cpp)
```

<!-- edit: native/CMakeLists.txt -->
`native/CMakeLists.txt` (2/2) — 찾을 부분:

```cmake
    tests/overlay_hint_tests.cpp
```

바꿀 내용:

```cmake
    tests/overlay_hint_tests.cpp
    tests/overlay_features_tests.cpp
    tests/overlay_status_tabs_tests.cpp
```

`overlay_fixture_spec.lua`는 파일 전체를 바꾼다. 오버레이가 쓴 용병단이 모드의 검증(`merc_plan.validate`)을 통과하는지도 본다.

<!-- file: mod/MLToybox/tests/overlay_fixture_spec.lua -->
```lua
local T = require("t")
local bridge = require("core.bridge")
local fileio = require("core.fileio")
local plan = require("features.merc_plan")

-- 오버레이 코어(C++)가 쓴 control.json 견본을 모드가 읽을 수 있는가.
-- 견본은 네이티브 테스트(overlay_fixture_for_the_lua_spec_matches_core_output)가 코어의 출력과 같은지 지킨다.
local function load()
  local text = fileio.read(SCRIPTS_DIR .. "\\..\\tests\\fixtures\\control_from_overlay.json")
  T.truthy(text, "fixture exists")
  local control, err = bridge.parseControl(text)
  T.truthy(control, "parses: " .. tostring(err))
  return control
end

T.run({
  control_written_by_the_overlay_core_is_valid_for_the_mod = function()
    local control = load()
    T.eq(control.version, 1, "version"); T.eq(control.seq, 7, "seq")
    local f = control.features
    T.eq(f.build.enabled, true, "build.enabled"); T.eq(f.build.instantRepair, false, "build.instantRepair"); T.eq(f.build.instantBuild, true, "build.instantBuild")
    T.eq(f.upgrade.enabled, true, "upgrade.enabled")
    T.eq(f.lord.enabled, true, "lord.enabled"); T.eq(f.lord.intervalSec, 2, "lord.intervalSec")
    T.eq(f.lord.treasury, 150000, "lord.treasury"); T.eq(f.lord.influence, nil, "unmanaged key is absent"); T.eq(f.lord.kingsFavour, 0, "zero is a value")
  end,
  military_population_and_resources_keep_their_shape = function()
    local f = load().features
    T.eq(f.military.enabled, true, "military.enabled"); T.eq(f.military.zeroUpkeep, false, "military.zeroUpkeep"); T.eq(f.military.unlimitedSquads, true, "military.unlimitedSquads")
    T.eq(f.population.enabled, true, "population.enabled"); T.eq(f.population.multiplier, 3, "population.multiplier")
    T.eq(f.population.targetFamilies, 12, "population.targetFamilies"); T.eq(f.population.regionTargets.nus, 30, "population.regionTargets")
    T.eq(f.resources.enabled, true, "resources.enabled"); T.eq(f.resources.intervalSec, 2, "resources.intervalSec")
    T.eq(f.resources.targets.Timber, 500, "resources.targets"); T.eq(f.resources.regionTargets.gold.Timber, 2000, "resources.regionTargets")
  end,
  mercenary_companies_pass_the_mods_own_validation = function()
    local m = load().features.mercenaries
    T.eq(m.enabled, true, "enabled"); T.eq(m.refund, true, "refund"); T.eq(m.lockFromAi, true, "lockFromAi")
    T.eq(#m.companies, 2, "two companies")
    T.eq(m.companies[1].name, "토이박스 용병단", "korean name survives"); T.eq(m.companies[2].region, nil, "unset region is absent"); T.eq(m.companies[2].enabled, false, "disabled")
    local ctx = { vanillaNames = { greencaps = true }, unitExists = function() return true end, regionKeys = { "nus", "gold" } }
    local valid, skipped = plan.validate(m.companies, ctx)
    T.eq(#skipped, 0, "nothing skipped")
    T.eq(#valid, 1, "only the enabled company is used")
    T.eq(valid[1].name, "토이박스 용병단", "name"); T.eq(#valid[1].units, 2, "units"); T.eq(valid[1].cost, 3000, "cost")
    T.eq(valid[1].region, "gold", "region"); T.eq(valid[1].banner, "greencaps", "banner")
  end,
  commands_keep_the_fields_the_handlers_read = function()
    local commands = load().commands
    T.eq(#commands, 2, "two commands")
    local c = commands[1]
    T.eq(c.type, "setLord", "type"); T.eq(c.key, "influence", "key"); T.eq(c.value, 20000, "value")
    T.eq(c.id, "0123456789abcdef0123456789abcdef", "id"); T.eq(c.issuedAt, 1790000000, "issuedAt")
    local s = commands[2]
    T.eq(s.type, "spawnSquads", "type"); T.eq(s.unit, "spearMilitia", "unit"); T.eq(s.count, 2, "count"); T.eq(s.region, "nus", "region")
  end,
})
```

- [ ] **Step 2: 빌드해서 실패 확인**

Run: `pwsh E:/MLToybox/tools/build-native.ps1 -Test`
Expected: `Cannot find source file: overlay/core/units.cpp`

- [ ] **Step 3: 구현**

아래 파일들은 파일 전체를 바꾼다.

<!-- file: native/overlay/core/control_doc.h -->
```cpp
#pragma once
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// control.json 문서. JSON 을 그대로 들고 있어 모르는 키를 보존한다. 아는 기능은 구조체로 읽고 쓴다.
// 기본값은 패널(Panel.Core/ControlDocument.cs)과 같다.
namespace mlt::ov {
using Json = nlohmann::ordered_json;

struct BuildSettings {
    bool enabled = false;
    bool ignorePlacement = true;
    bool instantBuild = true;
    bool instantRepair = true;
    bool noMaterials = true;
    bool noRegionLimit = true;
};

struct UpgradeSettings {
    bool enabled = false;
};

// 값이 없는 항목은 관리하지 않는다(키를 쓰지 않는다)
struct LordSettings {
    bool enabled = false;
    int intervalSec = 2;
    std::optional<int> treasury;
    std::optional<int> influence;
    std::optional<int> kingsFavour;
};

struct MilitarySettings {
    bool enabled = false;
    bool ignoreEquipment = true;
    bool ignorePopulation = true;
    bool zeroUpkeep = true;
    bool unlimitedSquads = true;
};

struct PopulationSettings {
    bool enabled = false;
    int multiplier = 2;
    int targetFamilies = 0;                      // 영지마다 최소 가족 수(0 = 끔)
    std::map<std::string, int> regionTargets;    // 영지별 값(0 = 그 영지 끔). 키가 없으면 공통 값을 따른다
};

struct ResourcesSettings {
    bool enabled = false;
    int intervalSec = 2;
    std::map<std::string, int> targets;                               // 공통 목표. 키가 없는 자원은 관리하지 않는다
    std::map<std::string, std::map<std::string, int>> regionTargets;  // 영지별 목표(키 = 영지 키). 없는 자원은 공통 목표를 따른다
};

// 커스텀 용병단 정의. units 는 분대마다 병종 id 하나(1~10개), region 은 내 영지 키(없으면 내 첫 영지),
// banner 는 깃발을 빌릴 순정 용병단 이름(없으면 카드가 들어간 칸의 것 그대로)
struct MercCompany {
    std::string name;
    std::vector<std::string> units;
    int cost = 0;
    std::optional<std::string> region;
    std::optional<std::string> banner;
    bool enabled = true;

    bool operator==(const MercCompany&) const = default;
};

struct MercSettings {
    bool enabled = false;
    bool refund = true;       // 내 용병단 고용비 환급 + 유지비 0
    bool lockFromAi = true;   // 커스텀 용병단은 고용 창이 닫혀 있는 동안 AI 가 살 수 없는 가격
    std::vector<MercCompany> companies;
};

class ControlDoc {
public:
    ControlDoc();
    // 깨졌거나 객체가 아니면 빈 문서. 옛 설정(자원 목표에 든 국고·영향력)을 영주 설정으로 옮긴다
    static ControlDoc parse(std::string_view text);
    // 해석하지 못하면 값 없음(파일이 깨졌는지 가릴 때 쓴다)
    static std::optional<ControlDoc> tryParse(std::string_view text);
    std::string dump() const;

    long long seq() const;
    void setSeq(long long seq);

    BuildSettings build() const;
    void setBuild(const BuildSettings& v);
    UpgradeSettings upgrade() const;
    void setUpgrade(const UpgradeSettings& v);
    LordSettings lord() const;
    void setLord(const LordSettings& v);
    MilitarySettings military() const;
    void setMilitary(const MilitarySettings& v);
    PopulationSettings population() const;
    void setPopulation(const PopulationSettings& v);
    ResourcesSettings resources() const;
    void setResources(const ResourcesSettings& v);
    MercSettings mercenaries() const;
    void setMercenaries(const MercSettings& v);

    void setCommands(const std::vector<Json>& commands);

    Json& raw() { return root_; }
    const Json& raw() const { return root_; }
    Json& feature(const char* name);                   // 없으면 만든다
    const Json* findFeature(const char* name) const;   // 없거나 객체가 아니면 nullptr

private:
    Json root_;
};

// 읽기 도우미: 키가 없거나 형식이 다르면 기본값
bool boolOr(const Json* obj, const char* key, bool def);
int intOr(const Json* obj, const char* key, int def);
std::optional<int> optInt(const Json* obj, const char* key);
std::optional<std::string> optString(const Json* obj, const char* key);
const Json* objectAt(const Json* obj, const char* key);
const Json* arrayAt(const Json* obj, const char* key);
}
```

<!-- file: native/overlay/core/control_doc.cpp -->
```cpp
#include "control_doc.h"
#include <cmath>

namespace mlt::ov {

static std::optional<long long> asInteger(const Json& v) {
    if (v.is_number_integer()) return v.get<long long>();
    if (v.is_number_float()) {
        double d = v.get<double>();
        if (std::floor(d) == d && std::fabs(d) < 9e15) return static_cast<long long>(d);
    }
    return std::nullopt;
}

static std::optional<int> asInt(const Json& v) {
    auto n = asInteger(v);
    if (!n || *n < -2147483648LL || *n > 2147483647LL) return std::nullopt;
    return static_cast<int>(*n);
}

bool boolOr(const Json* obj, const char* key, bool def) {
    if (!obj || !obj->is_object()) return def;
    auto it = obj->find(key);
    return (it != obj->end() && it->is_boolean()) ? it->get<bool>() : def;
}

std::optional<int> optInt(const Json* obj, const char* key) {
    if (!obj || !obj->is_object()) return std::nullopt;
    auto it = obj->find(key);
    if (it == obj->end()) return std::nullopt;
    return asInt(*it);
}

int intOr(const Json* obj, const char* key, int def) {
    auto v = optInt(obj, key);
    return v ? *v : def;
}

std::optional<std::string> optString(const Json* obj, const char* key) {
    if (!obj || !obj->is_object()) return std::nullopt;
    auto it = obj->find(key);
    if (it == obj->end() || !it->is_string()) return std::nullopt;
    return it->get<std::string>();
}

const Json* objectAt(const Json* obj, const char* key) {
    if (!obj || !obj->is_object()) return nullptr;
    auto it = obj->find(key);
    return (it != obj->end() && it->is_object()) ? &*it : nullptr;
}

const Json* arrayAt(const Json* obj, const char* key) {
    if (!obj || !obj->is_object()) return nullptr;
    auto it = obj->find(key);
    return (it != obj->end() && it->is_array()) ? &*it : nullptr;
}

// { "이름": 정수 } 꼴의 객체를 읽는다. 정수가 아니거나 음수인 값은 건너뛴다
static std::map<std::string, int> readIntMap(const Json* obj) {
    std::map<std::string, int> out;
    if (!obj || !obj->is_object()) return out;
    for (auto it = obj->begin(); it != obj->end(); ++it) {
        auto n = asInt(it.value());
        if (n && *n >= 0) out[it.key()] = *n;
    }
    return out;
}

static Json writeIntMap(const std::map<std::string, int>& map) {
    Json out = Json::object();
    for (const auto& [key, value] : map) out[key] = value;
    return out;
}

ControlDoc::ControlDoc() {
    root_ = Json::object();
    root_["version"] = 1;
    root_["seq"] = 0;
    root_["features"] = Json::object();
    root_["commands"] = Json::array();
}

ControlDoc ControlDoc::parse(std::string_view text) {
    return tryParse(text).value_or(ControlDoc());
}

std::optional<ControlDoc> ControlDoc::tryParse(std::string_view text) {
    ControlDoc doc;
    Json parsed = Json::parse(text.begin(), text.end(), nullptr, false);
    if (parsed.is_discarded() || !parsed.is_object()) return std::nullopt;
    doc.root_ = std::move(parsed);
    Json& root = doc.root_;
    if (!root.contains("features") || !root["features"].is_object()) root["features"] = Json::object();
    if (!root.contains("commands") || !root["commands"].is_array()) root["commands"] = Json::array();

    // 옛 버전은 국고·영향력을 자원 목표(targets.Treasury/Influence)에 넣었다.
    // 영주 설정이 없을 때만 옮기고, 자원 목표에서는 항상 뺀다 (패널의 LordControl.MigrateFrom)
    // (기능 객체를 새로 넣으면 다른 기능을 가리키던 참조가 무효가 되므로, 값을 먼저 읽고 지운 뒤에 영주 설정을 쓴다)
    std::optional<LordSettings> migrated;
    {
        Json& features = root["features"];
        const bool hadLord = features.contains("lord");
        if (features.contains("resources") && features["resources"].is_object()) {
            Json& resources = features["resources"];
            if (resources.contains("targets") && resources["targets"].is_object()) {
                Json& targets = resources["targets"];
                if (!hadLord) {
                    LordSettings lord;
                    if (auto t = optInt(&targets, "Treasury")) lord.treasury = *t;
                    if (auto i = optInt(&targets, "Influence")) lord.influence = *i;
                    if (lord.treasury || lord.influence) {
                        lord.enabled = boolOr(&resources, "enabled", false);
                        migrated = lord;
                    }
                }
                targets.erase("Treasury");
                targets.erase("Influence");
            }
        }
    }
    if (migrated) doc.setLord(*migrated);
    return doc;
}

std::string ControlDoc::dump() const {
    // 잘못된 UTF-8 이 섞여 있어도 예외를 던지지 않고 대체 문자(U+FFFD)로 쓴다
    return root_.dump(2, ' ', false, Json::error_handler_t::replace);
}

long long ControlDoc::seq() const {
    auto it = root_.find("seq");
    if (it == root_.end()) return 0;
    auto n = asInteger(*it);
    return n ? *n : 0;
}

void ControlDoc::setSeq(long long seq) {
    root_["version"] = 1;
    root_["seq"] = seq;
}

Json& ControlDoc::feature(const char* name) {
    Json& features = root_["features"];
    if (!features.is_object()) features = Json::object();
    if (!features.contains(name) || !features[name].is_object()) features[name] = Json::object();
    return features[name];
}

const Json* ControlDoc::findFeature(const char* name) const {
    auto it = root_.find("features");
    if (it == root_.end()) return nullptr;
    return objectAt(&*it, name);
}

BuildSettings ControlDoc::build() const {
    const Json* f = findFeature("build");
    const BuildSettings d;
    BuildSettings v;
    v.enabled = boolOr(f, "enabled", d.enabled);
    v.ignorePlacement = boolOr(f, "ignorePlacement", d.ignorePlacement);
    v.instantBuild = boolOr(f, "instantBuild", d.instantBuild);
    v.instantRepair = boolOr(f, "instantRepair", d.instantRepair);
    v.noMaterials = boolOr(f, "noMaterials", d.noMaterials);
    v.noRegionLimit = boolOr(f, "noRegionLimit", d.noRegionLimit);
    return v;
}

void ControlDoc::setBuild(const BuildSettings& v) {
    Json& f = feature("build");
    f["enabled"] = v.enabled;
    f["ignorePlacement"] = v.ignorePlacement;
    f["instantBuild"] = v.instantBuild;
    f["instantRepair"] = v.instantRepair;
    f["noMaterials"] = v.noMaterials;
    f["noRegionLimit"] = v.noRegionLimit;
}

UpgradeSettings ControlDoc::upgrade() const {
    UpgradeSettings v;
    v.enabled = boolOr(findFeature("upgrade"), "enabled", false);
    return v;
}

void ControlDoc::setUpgrade(const UpgradeSettings& v) {
    feature("upgrade")["enabled"] = v.enabled;
}

LordSettings ControlDoc::lord() const {
    const Json* f = findFeature("lord");
    LordSettings v;
    v.enabled = boolOr(f, "enabled", false);
    v.intervalSec = intOr(f, "intervalSec", 2);
    v.treasury = optInt(f, "treasury");
    v.influence = optInt(f, "influence");
    v.kingsFavour = optInt(f, "kingsFavour");
    return v;
}

static void setOptional(Json& obj, const char* key, const std::optional<int>& value) {
    if (value) obj[key] = *value;
    else obj.erase(key);
}

static void setOptional(Json& obj, const char* key, const std::optional<std::string>& value) {
    if (value) obj[key] = *value;
    else obj.erase(key);
}

void ControlDoc::setLord(const LordSettings& v) {
    Json& f = feature("lord");
    f["enabled"] = v.enabled;
    f["intervalSec"] = v.intervalSec;
    setOptional(f, "treasury", v.treasury);
    setOptional(f, "influence", v.influence);
    setOptional(f, "kingsFavour", v.kingsFavour);
}

MilitarySettings ControlDoc::military() const {
    const Json* f = findFeature("military");
    const MilitarySettings d;
    MilitarySettings v;
    v.enabled = boolOr(f, "enabled", d.enabled);
    v.ignoreEquipment = boolOr(f, "ignoreEquipment", d.ignoreEquipment);
    v.ignorePopulation = boolOr(f, "ignorePopulation", d.ignorePopulation);
    v.zeroUpkeep = boolOr(f, "zeroUpkeep", d.zeroUpkeep);
    v.unlimitedSquads = boolOr(f, "unlimitedSquads", d.unlimitedSquads);
    return v;
}

void ControlDoc::setMilitary(const MilitarySettings& v) {
    Json& f = feature("military");
    f["enabled"] = v.enabled;
    f["ignoreEquipment"] = v.ignoreEquipment;
    f["ignorePopulation"] = v.ignorePopulation;
    f["zeroUpkeep"] = v.zeroUpkeep;
    f["unlimitedSquads"] = v.unlimitedSquads;
}

PopulationSettings ControlDoc::population() const {
    const Json* f = findFeature("population");
    PopulationSettings v;
    v.enabled = boolOr(f, "enabled", false);
    v.multiplier = intOr(f, "multiplier", 2);
    v.targetFamilies = intOr(f, "targetFamilies", 0);
    v.regionTargets = readIntMap(objectAt(f, "regionTargets"));
    return v;
}

void ControlDoc::setPopulation(const PopulationSettings& v) {
    Json& f = feature("population");
    f["enabled"] = v.enabled;
    f["multiplier"] = v.multiplier;
    f["targetFamilies"] = v.targetFamilies;
    f["regionTargets"] = writeIntMap(v.regionTargets);
}

ResourcesSettings ControlDoc::resources() const {
    const Json* f = findFeature("resources");
    ResourcesSettings v;
    v.enabled = boolOr(f, "enabled", false);
    v.intervalSec = intOr(f, "intervalSec", 2);
    v.targets = readIntMap(objectAt(f, "targets"));
    if (const Json* regions = objectAt(f, "regionTargets")) {
        for (auto it = regions->begin(); it != regions->end(); ++it) {
            if (it.value().is_object()) v.regionTargets[it.key()] = readIntMap(&it.value());
        }
    }
    return v;
}

void ControlDoc::setResources(const ResourcesSettings& v) {
    Json& f = feature("resources");
    f["enabled"] = v.enabled;
    f["intervalSec"] = v.intervalSec;
    f["targets"] = writeIntMap(v.targets);
    Json regions = Json::object();
    for (const auto& [key, targets] : v.regionTargets) regions[key] = writeIntMap(targets);
    f["regionTargets"] = std::move(regions);
}

MercSettings ControlDoc::mercenaries() const {
    const Json* f = findFeature("mercenaries");
    MercSettings v;
    v.enabled = boolOr(f, "enabled", false);
    v.refund = boolOr(f, "refund", true);
    v.lockFromAi = boolOr(f, "lockFromAi", true);
    if (const Json* companies = arrayAt(f, "companies")) {
        for (const Json& c : *companies) {
            if (!c.is_object()) continue;   // 손으로 고친 설정의 이상한 항목은 건너뛴다
            MercCompany company;
            company.name = optString(&c, "name").value_or("");
            if (const Json* units = arrayAt(&c, "units")) {
                for (const Json& u : *units) {
                    if (u.is_string()) company.units.push_back(u.get<std::string>());
                }
            }
            company.cost = intOr(&c, "cost", 0);
            company.region = optString(&c, "region");
            company.banner = optString(&c, "banner");
            company.enabled = boolOr(&c, "enabled", true);
            v.companies.push_back(std::move(company));
        }
    }
    return v;
}

void ControlDoc::setMercenaries(const MercSettings& v) {
    Json& f = feature("mercenaries");
    f["enabled"] = v.enabled;
    f["refund"] = v.refund;
    f["lockFromAi"] = v.lockFromAi;
    Json companies = Json::array();
    for (const MercCompany& c : v.companies) {
        Json company = Json::object();
        company["name"] = c.name;
        company["units"] = c.units;
        company["cost"] = c.cost;
        setOptional(company, "region", c.region);   // 고르지 않았으면 키를 쓰지 않는다(패널과 같다)
        setOptional(company, "banner", c.banner);
        company["enabled"] = c.enabled;
        companies.push_back(std::move(company));
    }
    f["companies"] = std::move(companies);
}

void ControlDoc::setCommands(const std::vector<Json>& commands) {
    Json list = Json::array();
    for (const auto& c : commands) list.push_back(c);
    root_["commands"] = std::move(list);
}

}
```

<!-- file: native/overlay/core/status_doc.h -->
```cpp
#pragma once
#include "control_doc.h"
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// status.json 문서(모드가 1초마다 쓴다). 읽기 전용.
// Lua 의 JSON 인코더는 빈 표를 {} 가 아니라 [] 로 쓴다. 객체 자리에 온 배열은 빈 객체로 본다.
namespace mlt::ov {

struct FeatureStatus {
    bool active = false;
    std::optional<std::string> lastError;
};

struct NativeFeature {
    bool installed = false;
    bool active = false;
    std::optional<std::string> lastError;
};

struct NativeStatus {
    bool loaded = false;
    std::optional<std::string> error;
    bool stale = false;
    std::map<std::string, NativeFeature> features;
};

struct CommandResult {
    bool ok = false;
    std::optional<std::string> error;
    std::optional<std::vector<int>> squads;
    std::optional<int> reformed;
    std::optional<int> requested;
    std::optional<int> added;
};

struct LordStatus {
    std::optional<double> treasury;
    std::optional<int> influence;
    std::optional<int> kingsFavour;
};

struct RegionInfo {
    std::string key;    // 영지 키(regionUniqueTag)
    std::string name;
};

struct RegionResources {
    std::string key;
    std::string name;
    std::map<std::string, double> values;   // 그 영지의 재고
};

// 해제돼 빈 카드가 된 생성 분대
struct SpawnStatus {
    int disbanded = 0;
    int pending = 0;                                   // 빈 카드 정리 중
    std::vector<std::pair<std::string, int>> byUnit;   // 병종 id 와 수. 파일에 적힌 순서
};

struct RetinueSquad {
    int id = 0;
    std::string unit;
    int count = 0;
    std::string kind;   // spawned(병력 생성) | mercenary(커스텀 용병 고용)
};

// 모드가 만든 수행원 분대와, 지금 꾸미기 화면이 열려 있는 분대
struct RetinueStatus {
    std::vector<RetinueSquad> squads;
    std::optional<int> editing;
};

struct PopulationRegion {
    std::string key;
    std::string name;
    int families = 0;
    int population = 0;
    int homeless = 0;
    int freeSlots = 0;
    int unassigned = 0;
};

struct PopulationStatus {
    int families = 0;
    int population = 0;
    int homeless = 0;
    int freeSlots = 0;
    int natural = 0;
    int multiplied = 0;
    int unassigned = 0;
    std::vector<PopulationRegion> regions;
};

struct MercSlot {
    std::string name;
    int cost = 0;
    bool custom = false;
};

struct MercSkipped {
    std::string name;
    std::string reason;
};

struct MercenaryStatus {
    std::vector<MercSlot> slots;
    int hiredMine = 0;
    int hiredAi = 0;
    int refunded = 0;   // 맵을 불러온 뒤의 환급 합계
    std::vector<MercSkipped> skipped;
    std::optional<std::string> note;
};

struct StatusDoc {
    long long heartbeat = 0;
    bool inGame = false;
    std::optional<long long> appliedSeq;
    std::optional<std::string> bridgeError;
    std::optional<std::map<std::string, FeatureStatus>> features;                 // 이름순
    std::optional<std::vector<std::pair<std::string, CommandResult>>> commands;   // 파일에 적힌 순서
    std::optional<NativeStatus> native;
    std::optional<LordStatus> lord;
    std::optional<std::vector<std::string>> resourceIds;      // 관리할 수 있는 자원 이름
    std::optional<std::map<std::string, double>> resources;   // 모든 내 영지 합계
    std::optional<std::vector<RegionResources>> regions;      // 영지별 재고
    std::optional<std::vector<RegionInfo>> playerRegions;     // 내 영지
    std::optional<SpawnStatus> spawn;
    std::optional<RetinueStatus> retinue;
    std::optional<PopulationStatus> population;
    std::optional<MercenaryStatus> mercenaries;
    Json raw;
};

// 깨졌거나 객체가 아니면 값 없음
std::optional<StatusDoc> parseStatus(std::string_view text);

// 읽기 도우미
std::optional<double> optNumber(const Json* obj, const char* key);
}
```

<!-- file: native/overlay/core/status_doc.cpp -->
```cpp
#include "status_doc.h"

namespace mlt::ov {

std::optional<double> optNumber(const Json* obj, const char* key) {
    if (!obj || !obj->is_object()) return std::nullopt;
    auto it = obj->find(key);
    if (it == obj->end() || !it->is_number()) return std::nullopt;
    return it->get<double>();
}

static std::optional<long long> optInteger(const Json* obj, const char* key) {
    auto n = optNumber(obj, key);
    if (!n) return std::nullopt;
    return static_cast<long long>(*n);
}

static std::string stringOr(const Json* obj, const char* key) {
    return optString(obj, key).value_or("");
}

// { "이름": 수 } 꼴. 객체 자리에 온 배열(Lua 의 빈 표)은 빈 것으로 본다
static std::map<std::string, double> readNumberMap(const Json& v) {
    std::map<std::string, double> out;
    if (!v.is_object()) return out;
    for (auto it = v.begin(); it != v.end(); ++it) {
        if (it.value().is_number()) out[it.key()] = it.value().get<double>();
    }
    return out;
}

static CommandResult readCommand(const Json& r) {
    CommandResult c;
    c.ok = boolOr(&r, "ok", false);
    c.error = optString(&r, "error");
    c.reformed = optInt(&r, "reformed");
    c.requested = optInt(&r, "requested");
    c.added = optInt(&r, "added");
    auto it = r.find("squads");
    if (it != r.end() && it->is_array()) {
        std::vector<int> ids;
        for (const auto& v : *it) {
            if (v.is_number()) ids.push_back(static_cast<int>(v.get<double>()));
        }
        c.squads = std::move(ids);
    }
    return c;
}

static std::vector<RegionInfo> readRegionInfos(const Json& list) {
    std::vector<RegionInfo> out;
    for (const Json& r : list) {
        if (r.is_object()) out.push_back({ stringOr(&r, "key"), stringOr(&r, "name") });
    }
    return out;
}

static SpawnStatus readSpawn(const Json& v) {
    SpawnStatus s;
    s.disbanded = intOr(&v, "disbanded", 0);
    s.pending = intOr(&v, "pending", 0);
    if (const Json* byUnit = objectAt(&v, "byUnit")) {
        for (auto it = byUnit->begin(); it != byUnit->end(); ++it) {
            if (it.value().is_number()) s.byUnit.emplace_back(it.key(), static_cast<int>(it.value().get<double>()));
        }
    }
    return s;
}

static RetinueStatus readRetinue(const Json& v) {
    RetinueStatus r;
    if (const Json* squads = arrayAt(&v, "squads")) {
        for (const Json& s : *squads) {
            if (!s.is_object()) continue;
            RetinueSquad squad;
            squad.id = intOr(&s, "id", 0);
            squad.unit = stringOr(&s, "unit");
            squad.count = intOr(&s, "count", 0);
            squad.kind = stringOr(&s, "kind");
            r.squads.push_back(std::move(squad));
        }
    }
    r.editing = optInt(&v, "editing");
    return r;
}

static PopulationStatus readPopulation(const Json& v) {
    PopulationStatus p;
    p.families = intOr(&v, "families", 0);
    p.population = intOr(&v, "population", 0);
    p.homeless = intOr(&v, "homeless", 0);
    p.freeSlots = intOr(&v, "freeSlots", 0);
    p.natural = intOr(&v, "natural", 0);
    p.multiplied = intOr(&v, "multiplied", 0);
    p.unassigned = intOr(&v, "unassigned", 0);
    if (const Json* regions = arrayAt(&v, "regions")) {
        for (const Json& r : *regions) {
            if (!r.is_object()) continue;
            PopulationRegion region;
            region.key = stringOr(&r, "key");
            region.name = stringOr(&r, "name");
            region.families = intOr(&r, "families", 0);
            region.population = intOr(&r, "population", 0);
            region.homeless = intOr(&r, "homeless", 0);
            region.freeSlots = intOr(&r, "freeSlots", 0);
            region.unassigned = intOr(&r, "unassigned", 0);
            p.regions.push_back(std::move(region));
        }
    }
    return p;
}

static MercenaryStatus readMercenaries(const Json& v) {
    MercenaryStatus m;
    if (const Json* slots = arrayAt(&v, "slots")) {
        for (const Json& s : *slots) {
            if (s.is_object()) m.slots.push_back({ stringOr(&s, "name"), intOr(&s, "cost", 0), boolOr(&s, "custom", false) });
        }
    }
    m.hiredMine = intOr(&v, "hiredMine", 0);
    m.hiredAi = intOr(&v, "hiredAi", 0);
    m.refunded = intOr(&v, "refunded", 0);
    if (const Json* skipped = arrayAt(&v, "skipped")) {
        for (const Json& s : *skipped) {
            if (s.is_object()) m.skipped.push_back({ stringOr(&s, "name"), stringOr(&s, "reason") });
        }
    }
    m.note = optString(&v, "note");
    return m;
}

std::optional<StatusDoc> parseStatus(std::string_view text) {
    Json root = Json::parse(text.begin(), text.end(), nullptr, false);
    if (root.is_discarded() || !root.is_object()) return std::nullopt;
    StatusDoc s;
    s.heartbeat = optInteger(&root, "heartbeat").value_or(0);
    s.inGame = boolOr(&root, "inGame", false);
    s.appliedSeq = optInteger(&root, "appliedSeq");
    s.bridgeError = optString(&root, "bridgeError");

    if (auto it = root.find("features"); it != root.end() && (it->is_object() || it->is_array())) {
        std::map<std::string, FeatureStatus> features;
        if (it->is_object()) {
            for (auto f = it->begin(); f != it->end(); ++f) {
                if (!f.value().is_object()) continue;
                FeatureStatus fs;
                fs.active = boolOr(&f.value(), "active", false);
                fs.lastError = optString(&f.value(), "lastError");
                features[f.key()] = std::move(fs);
            }
        }
        s.features = std::move(features);
    }

    if (const Json* commands = objectAt(&root, "commands")) {
        std::vector<std::pair<std::string, CommandResult>> list;
        for (auto c = commands->begin(); c != commands->end(); ++c) {
            if (c.value().is_object()) list.emplace_back(c.key(), readCommand(c.value()));
        }
        s.commands = std::move(list);
    }

    if (const Json* native = objectAt(&root, "native")) {
        NativeStatus n;
        n.loaded = boolOr(native, "loaded", false);
        n.error = optString(native, "error");
        n.stale = boolOr(native, "stale", false);
        if (const Json* features = objectAt(native, "features")) {
            for (auto f = features->begin(); f != features->end(); ++f) {
                if (!f.value().is_object()) continue;
                NativeFeature nf;
                nf.installed = boolOr(&f.value(), "installed", false);
                nf.active = boolOr(&f.value(), "active", false);
                nf.lastError = optString(&f.value(), "lastError");
                n.features[f.key()] = std::move(nf);
            }
        }
        s.native = std::move(n);
    }

    if (const Json* lord = objectAt(&root, "lord")) {
        LordStatus l;
        l.treasury = optNumber(lord, "treasury");
        l.influence = optInt(lord, "influence");
        l.kingsFavour = optInt(lord, "kingsFavour");
        s.lord = l;
    }

    if (const Json* ids = arrayAt(&root, "resourceIds")) {
        std::vector<std::string> list;
        for (const Json& id : *ids) {
            if (id.is_string()) list.push_back(id.get<std::string>());
        }
        s.resourceIds = std::move(list);
    }
    if (auto it = root.find("resources"); it != root.end() && (it->is_object() || it->is_array())) s.resources = readNumberMap(*it);
    if (const Json* regions = arrayAt(&root, "regions")) {
        std::vector<RegionResources> list;
        for (const Json& r : *regions) {
            if (!r.is_object()) continue;
            RegionResources region;
            region.key = stringOr(&r, "key");
            region.name = stringOr(&r, "name");
            if (auto values = r.find("values"); values != r.end()) region.values = readNumberMap(*values);
            list.push_back(std::move(region));
        }
        s.regions = std::move(list);
    }
    if (const Json* regions = arrayAt(&root, "playerRegions")) s.playerRegions = readRegionInfos(*regions);
    if (const Json* spawn = objectAt(&root, "spawn")) s.spawn = readSpawn(*spawn);
    if (const Json* retinue = objectAt(&root, "retinue")) s.retinue = readRetinue(*retinue);
    if (const Json* population = objectAt(&root, "population")) s.population = readPopulation(*population);
    if (const Json* mercenaries = objectAt(&root, "mercenaries")) s.mercenaries = readMercenaries(*mercenaries);

    s.raw = std::move(root);
    return s;
}

}
```

<!-- file: native/overlay/core/commands.h -->
```cpp
#pragma once
#include "control_doc.h"
#include <optional>
#include <string>

// 모드가 한 번만 실행하는 일회성 명령(control.json 의 commands). issuedAt 이 60초 넘게 지나면 모드가 버린다.
// region 은 영지 키다. 값이 없으면 키를 쓰지 않는다(패널과 같다. 모드가 정한 기본 영지를 쓴다)
namespace mlt::ov {
// 32자리 16진수
std::string newCommandId();
// 영주 값(treasury / influence / kingsFavour)을 그 값으로 한 번 맞춘다
Json makeSetLord(const std::string& key, int value, long long nowEpochSeconds);
// 병종 unit 의 분대를 count 개 만든다. region 이 없으면 내 첫 영지
Json makeSpawnSquads(const std::string& unit, int count, long long nowEpochSeconds, const std::optional<std::string>& region);
// 해제돼 빈 카드가 된 생성 분대를 같은 병종으로 다시 만든다
Json makeReformSquads(long long nowEpochSeconds, const std::optional<std::string>& region);
// 모드가 만든 수행원 분대(squadId)에 게임의 꾸미기 화면을 연다. region 은 화면을 열 영주 저택의 영지
Json makeCustomizeRetinue(int squadId, long long nowEpochSeconds, const std::optional<std::string>& region);
// 가족을 count 만큼 들인다. region 이 없으면 빈 자리가 많은 영지부터
Json makeAddFamilies(int count, long long nowEpochSeconds, const std::optional<std::string>& region);
}
```

<!-- file: native/overlay/core/commands.cpp -->
```cpp
#include "commands.h"
#include <random>

namespace mlt::ov {

std::string newCommandId() {
    static std::mt19937_64 rng{std::random_device{}()};
    static const char* hex = "0123456789abcdef";
    std::string id;
    id.reserve(32);
    for (int part = 0; part < 2; ++part) {
        unsigned long long v = rng();
        for (int i = 0; i < 16; ++i) {
            id.push_back(hex[v & 0xF]);
            v >>= 4;
        }
    }
    return id;
}

static Json base(const char* type, long long nowEpochSeconds) {
    Json c = Json::object();
    c["id"] = newCommandId();
    c["type"] = type;
    c["issuedAt"] = nowEpochSeconds;
    return c;
}

static void setRegion(Json& c, const std::optional<std::string>& region) {
    if (region) c["region"] = *region;
}

Json makeSetLord(const std::string& key, int value, long long nowEpochSeconds) {
    Json c = base("setLord", nowEpochSeconds);
    c["key"] = key;
    c["value"] = value;
    return c;
}

Json makeSpawnSquads(const std::string& unit, int count, long long nowEpochSeconds, const std::optional<std::string>& region) {
    Json c = base("spawnSquads", nowEpochSeconds);
    c["unit"] = unit;
    c["count"] = count;
    setRegion(c, region);
    return c;
}

Json makeReformSquads(long long nowEpochSeconds, const std::optional<std::string>& region) {
    Json c = base("reformSquads", nowEpochSeconds);
    setRegion(c, region);
    return c;
}

Json makeCustomizeRetinue(int squadId, long long nowEpochSeconds, const std::optional<std::string>& region) {
    Json c = base("customizeRetinue", nowEpochSeconds);
    c["value"] = squadId;
    setRegion(c, region);
    return c;
}

Json makeAddFamilies(int count, long long nowEpochSeconds, const std::optional<std::string>& region) {
    Json c = base("addFamilies", nowEpochSeconds);
    c["count"] = count;
    setRegion(c, region);
    return c;
}

}
```

<!-- file: native/overlay/core/units.h -->
```cpp
#pragma once
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// 분대로 만들 수 있는 병종(패널의 UnitCatalog 와 같은 13종)과, 여러 탭이 쓰는 선택지 한 줄
namespace mlt::ov {

struct UnitOption {
    std::string id;      // DT_UnitTemplates 의 행 이름
    std::string label;
};

const std::vector<UnitOption>& units();
// 병종의 한글 이름. 모르는 id 는 id 를 그대로 돌려준다
std::string unitLabel(std::string_view id);
bool isKnownUnit(std::string_view id);

// 선택지 한 줄. key 가 없으면 "공통", "내 첫 영지"처럼 특정 대상이 아닌 줄이다
struct ScopeOption {
    std::optional<std::string> key;
    std::string label;
};

// "Mandlach (gold)"
std::string regionLabel(const std::string& name, const std::string& key);
// key 가 있는 줄의 번호. 없으면 0(첫 줄)
int indexOfKey(const std::vector<ScopeOption>& options, const std::optional<std::string>& key);
}
```

<!-- file: native/overlay/core/units.cpp -->
```cpp
#include "units.h"

namespace mlt::ov {

const std::vector<UnitOption>& units() {
    static const std::vector<UnitOption> list = {
        { "militia", "민병대 - 농민" },
        { "spearMilitia", "민병대 - 창" },
        { "militiaPole", "민병대 - 장창" },
        { "militiaFoot", "민병대 - 보병" },
        { "bowMilitia", "민병대 - 활" },
        { "crossbowMilitia", "민병대 - 석궁" },
        { "retinue_tier1", "친위대 - 1단계" },
        { "retinue_tier3", "친위대 - 3단계" },
        { "mercenary_spearmen", "용병 - 창병" },
        { "mercenary_infantry", "용병 - 보병" },
        { "Mercenary_Archers", "용병 - 궁수" },
        { "mercenary_heavy_archers", "용병 - 중궁수" },
        { "mercenary_crossbowmen", "용병 - 석궁병" },
    };
    return list;
}

std::string unitLabel(std::string_view id) {
    for (const UnitOption& u : units()) {
        if (u.id == id) return u.label;
    }
    return std::string(id);
}

bool isKnownUnit(std::string_view id) {
    for (const UnitOption& u : units()) {
        if (u.id == id) return true;
    }
    return false;
}

std::string regionLabel(const std::string& name, const std::string& key) {
    return name + " (" + key + ")";
}

int indexOfKey(const std::vector<ScopeOption>& options, const std::optional<std::string>& key) {
    for (size_t i = 0; i < options.size(); ++i) {
        if (options[i].key == key) return static_cast<int>(i);
    }
    return 0;
}

}
```

- [ ] **Step 4: 빌드하고 견본이 옛것이라 실패하는 것을 확인**

Run: `pwsh E:/MLToybox/tools/build-native.ps1 -Test`
Expected: 빌드는 되고 테스트 하나가 실패한다. `FAIL overlay_fixture_for_the_lua_spec_matches_core_output`(견본에 새 기능이 없다)

- [ ] **Step 5: 견본을 코어의 출력으로 다시 만든다**

```powershell
$env:MLT_WRITE_FIXTURES = '1'
& E:\MLToybox\native\build\native_tests.exe | Select-Object -Last 1
Remove-Item Env:MLT_WRITE_FIXTURES
& E:\MLToybox\native\build\native_tests.exe | Select-Object -Last 1
```

Expected: 두 번 모두 `106 tests, 0 failed`. `mod/MLToybox/tests/fixtures/control_from_overlay.json`의 내용은 다음과 같다.

```json
{
  "version": 1,
  "seq": 7,
  "features": {
    "build": {
      "enabled": true,
      "ignorePlacement": true,
      "instantBuild": true,
      "instantRepair": false,
      "noMaterials": true,
      "noRegionLimit": true
    },
    "upgrade": {
      "enabled": true
    },
    "lord": {
      "enabled": true,
      "intervalSec": 2,
      "treasury": 150000,
      "kingsFavour": 0
    },
    "military": {
      "enabled": true,
      "ignoreEquipment": true,
      "ignorePopulation": true,
      "zeroUpkeep": false,
      "unlimitedSquads": true
    },
    "population": {
      "enabled": true,
      "multiplier": 3,
      "targetFamilies": 12,
      "regionTargets": {
        "nus": 30
      }
    },
    "resources": {
      "enabled": true,
      "intervalSec": 2,
      "targets": {
        "Timber": 500
      },
      "regionTargets": {
        "gold": {
          "Timber": 2000
        }
      }
    },
    "mercenaries": {
      "enabled": true,
      "refund": true,
      "lockFromAi": true,
      "companies": [
        {
          "name": "토이박스 용병단",
          "units": [
            "mercenary_infantry",
            "mercenary_crossbowmen"
          ],
          "cost": 3000,
          "region": "gold",
          "banner": "greencaps",
          "enabled": true
        },
        {
          "name": "예비대",
          "units": [
            "militia"
          ],
          "cost": 0,
          "enabled": false
        }
      ]
    }
  },
  "commands": [
    {
      "id": "0123456789abcdef0123456789abcdef",
      "type": "setLord",
      "issuedAt": 1790000000,
      "key": "influence",
      "value": 20000
    },
    {
      "id": "fedcba9876543210fedcba9876543210",
      "type": "spawnSquads",
      "issuedAt": 1790000000,
      "unit": "spearMilitia",
      "count": 2,
      "region": "nus"
    }
  ]
}
```

- [ ] **Step 6: Lua 스펙 통과 확인**

Run: `dotnet test E:/MLToybox/panel/MLToybox.sln --filter "FullyQualifiedName~LuaSpec"`
Expected: 모두 통과

- [ ] **Step 7: 커밋**

```bash
git -C E:/MLToybox add native/overlay native/tests native/CMakeLists.txt mod/MLToybox/tests
git -C E:/MLToybox commit -m "feat(overlay): document model for military, population, resources and mercenaries; tab status data; commands" -m "Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 4: 규칙과 화면 문구 — 용병단 검증, 자원 표, 상태 문구

화면에 보일 문구와 선택지를 만드는 일을 ImGui 밖에 두어 단위 테스트한다. 문구는 패널과 같다.

**Files:**
- Create: `native/overlay/core/merc_rules.h/.cpp`, `native/overlay/core/resources.h/.cpp`, `native/overlay/core/view.h/.cpp`
- Create: `native/tests/overlay_merc_rules_tests.cpp`, `native/tests/overlay_view_tests.cpp`
- Modify: `native/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 3의 `MercCompany`, `ResourcesSettings`, `StatusDoc`와 그 항목들, `ScopeOption`, `unitLabel`, `isKnownUnit`, `regionLabel`; 계획 A의 `trim`, `codePointCount`, `lowerAscii`, `equalsIgnoreCaseAscii`
- Produces (이름공간 `mlt::ov`):
  - `overlay/core/merc_rules.h`: `kMercMaxSquads = 10`, `kMercNameMax = 40`, `kMercMaxEnabled = 3`, `kFirstRegionLabel`, `kInheritBannerLabel`, `vanillaMercNames()`, `isVanillaMercName`, `normalizeBanner`, `bannerOptions()`, `mercRegionOptions(regions, keep)`, `std::optional<std::string> validateCompany(const MercCompany&, const std::vector<MercCompany>& all, int self)`, `std::string unitSummary(const std::vector<std::string>&)`, `bool canEnableCompany(const std::vector<MercCompany>&, int self)`
  - `overlay/core/resources.h`: `struct ResourceRow { id, current, target }`, `isLordWide`, `resourceScopeOptions(const StatusDoc*)`, `resourceCurrent(status, key)`, `resourceTargets(settings, key)`, `storeResourceTargets(settings, key, targets)`, `buildResourceRows(status, settings, key)`
  - `overlay/core/view.h`: `formatThousands`, `spawnRegionOptions(const std::vector<RegionInfo>*)`, `reformText(const SpawnStatus*)`, `canReform`, `retinueLabel`, `populationScopeOptions(const PopulationStatus*)`, `populationInfo(population, regionKey)`, `mercStatusLines(const MercenaryStatus*)`

- [ ] **Step 1: 실패하는 테스트 작성**

<!-- file: native/tests/overlay_merc_rules_tests.cpp -->
```cpp
#include "test.h"
#include "overlay/core/merc_rules.h"
#include <functional>

using namespace mlt::ov;

// 패널의 MercenaryTests 와 같은 사례(이름 길이만 글자 수로 센다)
static MercCompany company(const std::string& name = "토이박스 용병단") {
    return { name, { "mercenary_infantry", "mercenary_infantry", "mercenary_crossbowmen" }, 3000, "gold", std::nullopt, true };
}

static std::string repeat(const std::string& s, int n) {
    std::string out;
    for (int i = 0; i < n; ++i) out += s;
    return out;
}

TEST(overlay_merc_rules_accept_a_good_company) {
    CHECK(!validateCompany(company(), {}, -1).has_value());
}

TEST(overlay_merc_rules_reject_bad_companies) {
    const std::vector<MercCompany> others = { company("기존 용병단") };
    auto reason = [&](const std::function<void(MercCompany&)>& change) {
        MercCompany c = company();
        change(c);
        return validateCompany(c, others, -1);
    };
    CHECK(reason([](MercCompany& c) { c.name = "   "; }) == "이름을 입력하세요.");
    CHECK(reason([](MercCompany& c) { c.name = repeat("가", 41); }) == "이름은 40자 이하여야 합니다.");
    CHECK(!reason([](MercCompany& c) { c.name = repeat("가", 40); }));            // 한글 40자는 120바이트다. 글자 수로 센다
    CHECK(reason([](MercCompany& c) { c.name = "Greencaps"; }) == "게임의 용병단 이름과 겹칩니다.");
    CHECK(reason([](MercCompany& c) { c.name = " 기존 용병단 "; }) == "같은 이름의 용병단이 이미 있습니다.");
    CHECK(reason([](MercCompany& c) { c.units.clear(); }) == "분대는 1~10개여야 합니다.");
    CHECK(reason([](MercCompany& c) { c.units.assign(11, "mercenary_infantry"); }).has_value());
    CHECK(!reason([](MercCompany& c) { c.units.assign(10, "mercenary_infantry"); }));
    CHECK(reason([](MercCompany& c) { c.units.push_back("dragon"); }) == "쓸 수 없는 병종입니다: dragon");
    CHECK(reason([](MercCompany& c) { c.cost = -1; }) == "고용비는 0 이상이어야 합니다.");
    CHECK(!reason([](MercCompany& c) { c.cost = 0; }));
}

TEST(overlay_merc_rules_editing_a_company_does_not_collide_with_itself) {
    const std::vector<MercCompany> all = { company("A"), company("B") };
    CHECK(!validateCompany(all[1], all, 1).has_value());                         // 자기 자신과는 겹치지 않는다
    MercCompany renamed = all[1];
    renamed.name = "a";                                                          // 다른 용병단과는 대소문자가 달라도 겹친다
    CHECK(validateCompany(renamed, all, 1) == "같은 이름의 용병단이 이미 있습니다.");
}

TEST(overlay_merc_rules_summary_groups_units_in_first_seen_order) {
    CHECK(unitSummary(company().units) == "용병 - 보병 × 2, 용병 - 석궁병 × 1");
    CHECK(unitSummary({ "dragon" }) == "dragon × 1");
    CHECK(unitSummary({}) == "");
    CHECK(unitSummary({ "militia", "retinue_tier1", "militia" }) == "민병대 - 농민 × 2, 친위대 - 1단계 × 1");
}

// R7: 등록 수에는 제한이 없고 "사용"은 최대 3개
TEST(overlay_merc_rules_at_most_three_enabled) {
    std::vector<MercCompany> all = { company("1"), company("2"), company("3"), company("4") };
    all[3].enabled = false;
    CHECK(!canEnableCompany(all, 3));            // 네 번째를 켤 수 없다
    CHECK(!canEnableCompany(all, -1));           // 새 용병단도 켠 채로는 못 넣는다
    CHECK(canEnableCompany(all, 0));             // 이미 켜진 것은 자기 자신을 빼고 센다
    all[1].enabled = false;
    CHECK(canEnableCompany(all, 3));
    CHECK(canEnableCompany({}, -1));
}

TEST(overlay_merc_rules_vanilla_names_and_banners) {
    CHECK(vanillaMercNames().size() == 11);
    CHECK(isVanillaMercName("huntsmen") && isVanillaMercName("HILDEBOLTS_ARMY") && !isVanillaMercName("토이박스"));
    for (size_t i = 1; i < vanillaMercNames().size(); ++i) CHECK(vanillaMercNames()[i - 1] < vanillaMercNames()[i]);   // 이름순
    CHECK(normalizeBanner("Greencaps") == "greencaps");
    CHECK(normalizeBanner(" brotherhood_of_the_forest ") == "brotherhood_of_the_forest");
    CHECK(!normalizeBanner("dragon") && !normalizeBanner("") && !normalizeBanner(std::nullopt));
    const std::vector<ScopeOption> banners = bannerOptions();
    CHECK(banners.size() == 12 && !banners[0].key && banners[0].label == "칸의 것 그대로" && banners[1].key == "battle_brothers");
}

TEST(overlay_merc_rules_region_options_keep_saved_keys_that_are_not_listed) {
    const std::vector<RegionInfo> regions = { { "gold", "Mandlach" } };
    const std::vector<ScopeOption> options = mercRegionOptions(regions, { "nus", "gold", std::nullopt, "nus" });
    CHECK(options.size() == 3);
    CHECK(!options[0].key && options[0].label == "내 첫 영지");
    CHECK(options[1].key == "gold" && options[1].label == "Mandlach (gold)");
    CHECK(options[2].key == "nus" && options[2].label == "nus");                 // 이름을 모르는 영지는 키로 보여 준다
    // 게임이 꺼져 있어 영지 목록이 없어도 저장된 도착 영지는 선택지로 남는다
    const std::vector<ScopeOption> offline = mercRegionOptions({}, { "nus" });
    CHECK(offline.size() == 2 && offline[1].key == "nus");
}
```

<!-- file: native/tests/overlay_view_tests.cpp -->
```cpp
#include "test.h"
#include "overlay/core/resources.h"
#include "overlay/core/view.h"

using namespace mlt::ov;

static StatusDoc resourceStatus() {
    auto s = parseStatus(R"({"heartbeat":1,"inGame":true,
        "resourceIds":["RegionalWealth","Influence","Treasury","Timber"],
        "resources":{"Timber":740,"Treasury":9000},
        "regions":[{"key":"hof","name":"Klainau","values":{"Timber":700}},{"key":"sel","name":"Furdau","values":{"Timber":40}}]})");
    CHECK(s.has_value());
    return *s;
}

TEST(overlay_resources_scope_options_are_common_first_then_regions) {
    const StatusDoc s = resourceStatus();
    const std::vector<ScopeOption> options = resourceScopeOptions(&s);
    CHECK(options.size() == 3 && !options[0].key && options[0].label == "공통 (모든 내 영지, 현재=합계)");
    CHECK(options[1].key == "hof" && options[1].label == "Klainau (hof)");
    CHECK(resourceScopeOptions(nullptr).size() == 1);              // 게임 밖: 공통만
}

TEST(overlay_resources_current_is_the_total_or_the_regions_own_stock) {
    const StatusDoc s = resourceStatus();
    CHECK(resourceCurrent(&s, std::nullopt)->at("Timber") == 740.0);
    CHECK(resourceCurrent(&s, "sel")->at("Timber") == 40.0);
    CHECK(resourceCurrent(&s, "gone") == nullptr && resourceCurrent(nullptr, std::nullopt) == nullptr);
}

TEST(overlay_resources_region_targets_are_separate_and_dropped_when_empty) {
    ResourcesSettings r;
    r.targets["Timber"] = 500;
    CHECK(resourceTargets(r, "hof").empty());
    storeResourceTargets(r, "hof", { { "Timber", 2000 } });
    CHECK(r.regionTargets.at("hof").at("Timber") == 2000 && resourceTargets(r, std::nullopt).at("Timber") == 500);
    storeResourceTargets(r, "hof", {});                            // 영지 목표가 하나도 없으면 영지 키를 지운다
    CHECK(r.regionTargets.find("hof") == r.regionTargets.end());
    storeResourceTargets(r, std::nullopt, { { "Timber", 7 } });
    CHECK(r.targets.at("Timber") == 7);
}

TEST(overlay_resources_rows_join_ids_current_and_targets_sorted_by_name) {
    const StatusDoc s = resourceStatus();
    ResourcesSettings r;
    r.targets = { { "Iron", 50 }, { "Timber", 500 } };
    const std::vector<ResourceRow> rows = buildResourceRows(&s, r, std::nullopt);
    CHECK(rows.size() == 5);
    CHECK(rows[0] == (ResourceRow{ "Influence", std::nullopt, std::nullopt }));
    CHECK(rows[1] == (ResourceRow{ "Iron", std::nullopt, 50 }));       // 목표만 있고 모드가 모르는 자원도 줄로 남는다
    CHECK(rows[3] == (ResourceRow{ "Timber", 740.0, 500 }));
    CHECK(rows[4] == (ResourceRow{ "Treasury", 9000.0, std::nullopt }));
    // 영지 범위: 영주 전체 값(국고, 영향력)을 숨기고, 그 영지의 재고와 영지 목표를 쓴다
    r.regionTargets["hof"] = { { "Timber", 2000 }, { "Treasury", 5 } };
    const std::vector<ResourceRow> region = buildResourceRows(&s, r, "hof");
    CHECK(region.size() == 2 && region[0].id == "RegionalWealth" && region[1] == (ResourceRow{ "Timber", 700.0, 2000 }));
    // 게임 밖: 목표가 있는 자원만
    const std::vector<ResourceRow> offline = buildResourceRows(nullptr, r, std::nullopt);
    CHECK(offline.size() == 2 && offline[0].id == "Iron" && !offline[0].current);
    CHECK(buildResourceRows(nullptr, ResourcesSettings(), std::nullopt).empty());
}

TEST(overlay_view_thousands) {
    CHECK(formatThousands(0) == "0" && formatThousands(999) == "999" && formatThousands(1000) == "1,000");
    CHECK(formatThousands(10000000) == "10,000,000" && formatThousands(-1234567) == "-1,234,567");
}

TEST(overlay_view_spawn_regions_and_reform_text) {
    CHECK(spawnRegionOptions(nullptr).size() == 1 && !spawnRegionOptions(nullptr)[0].key && spawnRegionOptions(nullptr)[0].label == "내 첫 영지");
    const std::vector<RegionInfo> regions = { { "gold", "Mandlach" }, { "nus", "Haderwand" } };
    const std::vector<ScopeOption> options = spawnRegionOptions(&regions);
    CHECK(options.size() == 2 && options[0].key == "gold" && options[1].label == "Haderwand (nus)");
    const std::vector<RegionInfo> none;
    CHECK(spawnRegionOptions(&none).size() == 1);                  // 영지가 없으면 "내 첫 영지"

    CHECK(reformText(nullptr) == "해제된 생성 분대: -" && !canReform(nullptr));
    SpawnStatus spawn;
    CHECK(reformText(&spawn) == "해제된 생성 분대: 0개" && !canReform(&spawn));
    spawn.disbanded = 3;
    spawn.byUnit = { { "retinue_tier1", 2 }, { "militia", 1 } };
    CHECK(reformText(&spawn) == "해제된 생성 분대: 3개 (친위대 - 1단계 2, 민병대 - 농민 1)" && canReform(&spawn));
    spawn.pending = 1;
    CHECK(reformText(&spawn) == "해제된 생성 분대: 3개 (친위대 - 1단계 2, 민병대 - 농민 1) — 빈 카드 정리 중 1" && !canReform(&spawn));
}

TEST(overlay_view_retinue_label_names_the_unit_count_and_origin) {
    CHECK(retinueLabel({ 63, "retinue_tier1", 36, "spawned" }) == "#63 친위대 - 1단계 ×36 (생성)");
    CHECK(retinueLabel({ 64, "retinue_tier3", 12, "mercenary" }) == "#64 친위대 - 3단계 ×12 (용병)");
    CHECK(retinueLabel({ 65, "odd", 1, "odd" }) == "#65 odd ×1 (odd)");
}

TEST(overlay_view_population_scope_and_info) {
    CHECK(populationScopeOptions(nullptr).size() == 1 && populationScopeOptions(nullptr)[0].label == "공통 (모든 내 영지)");
    CHECK(populationInfo(nullptr, std::nullopt) == std::vector<std::string>{ "현재: - (인구 기능이 꺼져 있거나 게임 밖)" });
    PopulationStatus p;
    p.families = 14;
    p.population = 42;
    p.freeSlots = 3;
    p.unassigned = 2;
    p.natural = 1;
    p.multiplied = 2;
    p.regions.push_back({ "hof", "Klainau", 10, 30, 0, 3, 2 });
    const std::vector<ScopeOption> options = populationScopeOptions(&p);
    CHECK(options.size() == 2 && options[1].key == "hof" && options[1].label == "Klainau (hof)");
    std::vector<std::string> lines = populationInfo(&p, std::nullopt);
    CHECK(lines.size() == 2);
    CHECK(lines[0] == "현재(모든 내 영지 합계): 가족 14 · 인구 42 · 집 없는 가족 0 · 빈 자리 3 · 미배치 가족 2");
    CHECK(lines[1] == "이번 세션(전체): 자연 이민 1가족 → 배율로 추가 2가족");
    lines = populationInfo(&p, "hof");
    CHECK(lines[0] == "현재(Klainau): 가족 10 · 인구 30 · 집 없는 가족 0 · 빈 자리 3 · 미배치 가족 2");
    CHECK(populationInfo(&p, "gone")[0].find("모든 내 영지 합계") != std::string::npos);   // 사라진 영지는 합계로
}

TEST(overlay_view_mercenary_status_lines) {
    CHECK(mercStatusLines(nullptr) == std::vector<std::string>{ "고용 창: - (용병 기능이 꺼져 있거나 게임 밖)" });
    MercenaryStatus m;
    std::vector<std::string> lines = mercStatusLines(&m);
    CHECK(lines.size() == 2 && lines[0] == "고용 창: (비어 있음)" && lines[1] == "고용 중: 내 용병단 0개, AI 0개 · 맵을 불러온 뒤 환급 0");
    m.slots = { { "토이박스 용병단", 10000000, true }, { "wayward_sons", 90, false } };
    m.hiredMine = 2;
    m.hiredAi = 3;
    m.refunded = 6000;
    m.skipped = { { "궁수대", "unknown unit: foo" } };
    m.note = "rebuild produced 1 of 3 slots";
    lines = mercStatusLines(&m);
    CHECK(lines.size() == 4);
    CHECK(lines[0] == "고용 창: 토이박스 용병단(커스텀) 10,000,000, wayward_sons 90");
    CHECK(lines[1] == "고용 중: 내 용병단 2개, AI 3개 · 맵을 불러온 뒤 환급 6,000");
    CHECK(lines[2] == "띄우지 못함: 궁수대 — unknown unit: foo");
    CHECK(lines[3] == "참고: rebuild produced 1 of 3 slots");
}
```

<!-- edit: native/CMakeLists.txt -->
`native/CMakeLists.txt` (1/2) — 찾을 부분:

```cmake
    overlay/core/units.cpp)
```

바꿀 내용:

```cmake
    overlay/core/units.cpp
    overlay/core/merc_rules.cpp
    overlay/core/resources.cpp
    overlay/core/view.cpp)
```

<!-- edit: native/CMakeLists.txt -->
`native/CMakeLists.txt` (2/2) — 찾을 부분:

```cmake
    tests/overlay_status_tabs_tests.cpp
```

바꿀 내용:

```cmake
    tests/overlay_status_tabs_tests.cpp
    tests/overlay_merc_rules_tests.cpp
    tests/overlay_view_tests.cpp
```

- [ ] **Step 2: 빌드해서 실패 확인**

Run: `pwsh E:/MLToybox/tools/build-native.ps1 -Test`
Expected: `Cannot find source file: overlay/core/merc_rules.cpp`

- [ ] **Step 3: 구현**

<!-- file: native/overlay/core/merc_rules.h -->
```cpp
#pragma once
#include "control_doc.h"
#include "status_doc.h"
#include "units.h"
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// 커스텀 용병단 정의의 화면 쪽 검증과 선택지. 패널의 MercCompanyRules 와 같은 규칙이고,
// 모드도 같은 규칙으로 다시 검증한다(mod/MLToybox/Scripts/features/merc_plan.lua).
// 이름 길이는 모드처럼 글자(코드 포인트) 수로 센다.
namespace mlt::ov {

inline constexpr int kMercMaxSquads = 10;
inline constexpr int kMercNameMax = 40;
inline constexpr int kMercMaxEnabled = 3;
inline constexpr const char* kFirstRegionLabel = "내 첫 영지";
inline constexpr const char* kInheritBannerLabel = "칸의 것 그대로";

// 용병 표(DT_MercenaryCompanies)의 순정 용병단 이름 11개, 이름순. 깃발 선택지이기도 하다
const std::vector<std::string>& vanillaMercNames();
// 대소문자를 가리지 않는다
bool isVanillaMercName(std::string_view name);
// 깃발 입력을 표의 이름으로 맞춘다. 모르는 이름이나 빈 값은 없음(칸의 깃발 그대로)
std::optional<std::string> normalizeBanner(const std::optional<std::string>& banner);
// 깃발 선택지: "칸의 것 그대로" + 순정 용병단 11개
std::vector<ScopeOption> bannerOptions();
// 도착 영지 선택지: "내 첫 영지", 내 영지들, 그리고 keep 의 키 가운데 영지 목록에 없는 것.
// 게임 밖이거나 그 영지를 잃어 목록에 없는 키도 남겨야, 용병단을 고쳐 등록할 때 저장된 도착 영지가 사라지지 않는다
std::vector<ScopeOption> mercRegionOptions(const std::vector<RegionInfo>& regions, const std::vector<std::optional<std::string>>& keep);

// 문제가 없으면 값 없음, 있으면 사용자에게 보여 줄 이유. self 는 all 안에서 c 자신의 자리(새 용병단이면 -1)
std::optional<std::string> validateCompany(const MercCompany& c, const std::vector<MercCompany>& all, int self);
// "용병 - 보병 × 2, 용병 - 석궁병 × 1" (처음 나온 순서). 모르는 병종 id 는 그대로 보여 준다
std::string unitSummary(const std::vector<std::string>& units);
// self 를 "사용"으로 바꿔도 되는가(self 를 뺀 사용 수가 3 미만). 새 용병단이면 self = -1
bool canEnableCompany(const std::vector<MercCompany>& all, int self);
}
```

<!-- file: native/overlay/core/merc_rules.cpp -->
```cpp
#include "merc_rules.h"
#include "text.h"
#include <algorithm>
#include <map>

namespace mlt::ov {

const std::vector<std::string>& vanillaMercNames() {
    // findings "용병 고용 — 목록 보충과 커스텀 용병단"
    static const std::vector<std::string> names = {
        "battle_brothers", "brigands", "brigands_small", "brotherhood_of_the_forest", "crazy_goose", "greencaps",
        "hildebolts_army", "hildebolts_army_large", "huntsmen", "vultures", "wayward_sons",
    };
    return names;
}

bool isVanillaMercName(std::string_view name) {
    const std::string lower = lowerAscii(name);
    const auto& names = vanillaMercNames();
    return std::find(names.begin(), names.end(), lower) != names.end();
}

std::optional<std::string> normalizeBanner(const std::optional<std::string>& banner) {
    if (!banner) return std::nullopt;
    const std::string lower = lowerAscii(trim(*banner));
    const auto& names = vanillaMercNames();
    if (std::find(names.begin(), names.end(), lower) == names.end()) return std::nullopt;
    return lower;
}

std::vector<ScopeOption> bannerOptions() {
    std::vector<ScopeOption> options = { { std::nullopt, kInheritBannerLabel } };
    for (const std::string& name : vanillaMercNames()) options.push_back({ name, name });
    return options;
}

std::vector<ScopeOption> mercRegionOptions(const std::vector<RegionInfo>& regions, const std::vector<std::optional<std::string>>& keep) {
    std::vector<ScopeOption> options = { { std::nullopt, kFirstRegionLabel } };
    for (const RegionInfo& r : regions) options.push_back({ r.key, regionLabel(r.name, r.key) });
    for (const auto& key : keep) {
        if (!key) continue;
        const bool listed = std::any_of(options.begin(), options.end(), [&](const ScopeOption& o) { return o.key == key; });
        if (!listed) options.push_back({ *key, *key });   // 이름을 모르는 영지는 키로 보여 준다
    }
    return options;
}

std::optional<std::string> validateCompany(const MercCompany& c, const std::vector<MercCompany>& all, int self) {
    const std::string name = trim(c.name);
    if (name.empty()) return "이름을 입력하세요.";
    if (codePointCount(name) > static_cast<size_t>(kMercNameMax)) return "이름은 " + std::to_string(kMercNameMax) + "자 이하여야 합니다.";
    if (isVanillaMercName(name)) return "게임의 용병단 이름과 겹칩니다.";
    for (size_t i = 0; i < all.size(); ++i) {
        if (static_cast<int>(i) != self && equalsIgnoreCaseAscii(trim(all[i].name), name)) return "같은 이름의 용병단이 이미 있습니다.";
    }
    if (c.units.empty() || c.units.size() > static_cast<size_t>(kMercMaxSquads)) return "분대는 1~" + std::to_string(kMercMaxSquads) + "개여야 합니다.";
    for (const std::string& unit : c.units) {
        if (!isKnownUnit(unit)) return "쓸 수 없는 병종입니다: " + unit;
    }
    if (c.cost < 0) return "고용비는 0 이상이어야 합니다.";
    return std::nullopt;
}

std::string unitSummary(const std::vector<std::string>& units) {
    std::vector<std::string> order;
    std::map<std::string, int> counts;
    for (const std::string& unit : units) {
        if (counts[unit]++ == 0) order.push_back(unit);
    }
    std::string out;
    for (const std::string& unit : order) {
        if (!out.empty()) out += ", ";
        out += unitLabel(unit) + " × " + std::to_string(counts[unit]);
    }
    return out;
}

bool canEnableCompany(const std::vector<MercCompany>& all, int self) {
    int enabled = 0;
    for (size_t i = 0; i < all.size(); ++i) {
        if (static_cast<int>(i) != self && all[i].enabled) ++enabled;
    }
    return enabled < kMercMaxEnabled;
}

}
```

<!-- file: native/overlay/core/resources.h -->
```cpp
#pragma once
#include "control_doc.h"
#include "status_doc.h"
#include "units.h"
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// [자원] 탭의 범위와 표. 범위는 공통(key 없음: 모든 내 영지의 합계와 공통 목표) 또는 영지 하나(그 영지의 재고와 영지 목표).
// 패널의 ResourceScope, ResourceRows 와 같은 규칙이다.
namespace mlt::ov {

struct ResourceRow {
    std::string id;
    std::optional<double> current;   // 모드가 알려 준 현재 값. 없으면 "-"
    std::optional<int> target;       // 없으면 관리하지 않는다(영지 범위에서는 공통 목표를 따른다)

    bool operator==(const ResourceRow&) const = default;
};

// 국고·영향력은 영주 전체 값이라 영지 범위에서는 숨긴다
bool isLordWide(std::string_view id);
// "공통 (모든 내 영지, 현재=합계)"과 영지들
std::vector<ScopeOption> resourceScopeOptions(const StatusDoc* status);
// 그 범위의 현재 값. 모르면 nullptr
const std::map<std::string, double>* resourceCurrent(const StatusDoc* status, const std::optional<std::string>& key);
// 그 범위의 목표
std::map<std::string, int> resourceTargets(const ResourcesSettings& settings, const std::optional<std::string>& key);
// 그 범위의 목표를 바꾼다. 영지 목표가 하나도 없으면 그 영지 키를 지운다
void storeResourceTargets(ResourcesSettings& settings, const std::optional<std::string>& key, std::map<std::string, int> targets);
// 표의 줄: 모드가 알려 준 자원 이름, 현재 값이 있는 자원, 목표가 있는 자원을 합쳐 이름순으로
std::vector<ResourceRow> buildResourceRows(const StatusDoc* status, const ResourcesSettings& settings, const std::optional<std::string>& key);
}
```

<!-- file: native/overlay/core/resources.cpp -->
```cpp
#include "resources.h"
#include <set>
#include <utility>

namespace mlt::ov {

bool isLordWide(std::string_view id) {
    return id == "Treasury" || id == "Influence";
}

std::vector<ScopeOption> resourceScopeOptions(const StatusDoc* status) {
    std::vector<ScopeOption> options = { { std::nullopt, "공통 (모든 내 영지, 현재=합계)" } };
    if (status && status->regions) {
        for (const RegionResources& r : *status->regions) options.push_back({ r.key, regionLabel(r.name, r.key) });
    }
    return options;
}

const std::map<std::string, double>* resourceCurrent(const StatusDoc* status, const std::optional<std::string>& key) {
    if (!status) return nullptr;
    if (!key) return status->resources ? &*status->resources : nullptr;
    if (!status->regions) return nullptr;
    for (const RegionResources& r : *status->regions) {
        if (r.key == *key) return &r.values;
    }
    return nullptr;
}

std::map<std::string, int> resourceTargets(const ResourcesSettings& settings, const std::optional<std::string>& key) {
    if (!key) return settings.targets;
    auto it = settings.regionTargets.find(*key);
    return it == settings.regionTargets.end() ? std::map<std::string, int>() : it->second;
}

void storeResourceTargets(ResourcesSettings& settings, const std::optional<std::string>& key, std::map<std::string, int> targets) {
    if (!key) settings.targets = std::move(targets);
    else if (targets.empty()) settings.regionTargets.erase(*key);
    else settings.regionTargets[*key] = std::move(targets);
}

std::vector<ResourceRow> buildResourceRows(const StatusDoc* status, const ResourcesSettings& settings, const std::optional<std::string>& key) {
    const std::map<std::string, double>* current = resourceCurrent(status, key);
    const std::map<std::string, int> targets = resourceTargets(settings, key);
    std::set<std::string> ids;
    if (status && status->resourceIds) ids.insert(status->resourceIds->begin(), status->resourceIds->end());
    if (current) {
        for (const auto& entry : *current) ids.insert(entry.first);
    }
    for (const auto& entry : targets) ids.insert(entry.first);

    std::vector<ResourceRow> rows;
    for (const std::string& id : ids) {
        if (key && isLordWide(id)) continue;
        ResourceRow row;
        row.id = id;
        if (current) {
            if (auto it = current->find(id); it != current->end()) row.current = it->second;
        }
        if (auto it = targets.find(id); it != targets.end()) row.target = it->second;
        rows.push_back(std::move(row));
    }
    return rows;
}

}
```

<!-- file: native/overlay/core/view.h -->
```cpp
#pragma once
#include "status_doc.h"
#include "units.h"
#include <optional>
#include <string>
#include <vector>

// 탭에 보일 문구와 선택지를 상태에서 만든다. 문구는 패널과 같다. ImGui 에 의존하지 않아 단위 테스트한다.
// 포인터 인자가 nullptr 이면 "모드가 그 정보를 주지 않았다"(기능이 꺼져 있거나 게임 밖)는 뜻이다.
namespace mlt::ov {

// 3000 -> "3,000"
std::string formatThousands(long long n);

// [군사] 병력 생성·재구성 위치. 내 영지 목록이 없으면 "내 첫 영지" 한 줄
std::vector<ScopeOption> spawnRegionOptions(const std::vector<RegionInfo>* regions);
// "해제된 생성 분대: 3개 (친위대 - 1단계 2, 민병대 - 농민 1) — 빈 카드 정리 중 1"
std::string reformText(const SpawnStatus* spawn);
// 재구성 버튼을 누를 수 있는가: 해제된 분대가 있고 빈 카드 정리가 끝났다
bool canReform(const SpawnStatus* spawn);
// "#63 친위대 - 1단계 ×36 (생성)"
std::string retinueLabel(const RetinueSquad& squad);

// [인구] 범위: "공통 (모든 내 영지)"과 영지들
std::vector<ScopeOption> populationScopeOptions(const PopulationStatus* population);
// 현재 상태 두 줄(정보가 없으면 한 줄). regionKey 가 없거나 그 영지를 모르면 모든 영지 합계
std::vector<std::string> populationInfo(const PopulationStatus* population, const std::optional<std::string>& regionKey);

// [용병] 고용 창 상태. "고용 창: …", "고용 중: …", "띄우지 못함: 이름 — 이유", "참고: …"
std::vector<std::string> mercStatusLines(const MercenaryStatus* mercenaries);
}
```

<!-- file: native/overlay/core/view.cpp -->
```cpp
#include "view.h"

namespace mlt::ov {

std::string formatThousands(long long n) {
    const bool negative = n < 0;
    std::string digits = std::to_string(negative ? -n : n);
    std::string out;
    for (size_t i = 0; i < digits.size(); ++i) {
        if (i > 0 && (digits.size() - i) % 3 == 0) out.push_back(',');
        out.push_back(digits[i]);
    }
    return negative ? "-" + out : out;
}

std::vector<ScopeOption> spawnRegionOptions(const std::vector<RegionInfo>* regions) {
    std::vector<ScopeOption> options;
    if (regions) {
        for (const RegionInfo& r : *regions) options.push_back({ r.key, regionLabel(r.name, r.key) });
    }
    if (options.empty()) options.push_back({ std::nullopt, "내 첫 영지" });
    return options;
}

std::string reformText(const SpawnStatus* spawn) {
    if (!spawn) return "해제된 생성 분대: -";
    std::string text = "해제된 생성 분대: " + std::to_string(spawn->disbanded) + "개";
    if (!spawn->byUnit.empty()) {
        text += " (";
        for (size_t i = 0; i < spawn->byUnit.size(); ++i) {
            if (i > 0) text += ", ";
            text += unitLabel(spawn->byUnit[i].first) + " " + std::to_string(spawn->byUnit[i].second);
        }
        text += ")";
    }
    if (spawn->pending > 0) text += " — 빈 카드 정리 중 " + std::to_string(spawn->pending);
    return text;
}

bool canReform(const SpawnStatus* spawn) {
    return spawn && spawn->disbanded > 0 && spawn->pending == 0;
}

std::string retinueLabel(const RetinueSquad& squad) {
    std::string kind = squad.kind;
    if (kind == "spawned") kind = "생성";
    else if (kind == "mercenary") kind = "용병";
    return "#" + std::to_string(squad.id) + " " + unitLabel(squad.unit) + " ×" + std::to_string(squad.count) + " (" + kind + ")";
}

std::vector<ScopeOption> populationScopeOptions(const PopulationStatus* population) {
    std::vector<ScopeOption> options = { { std::nullopt, "공통 (모든 내 영지)" } };
    if (population) {
        for (const PopulationRegion& r : population->regions) options.push_back({ r.key, regionLabel(r.name, r.key) });
    }
    return options;
}

std::vector<std::string> populationInfo(const PopulationStatus* population, const std::optional<std::string>& regionKey) {
    if (!population) return { "현재: - (인구 기능이 꺼져 있거나 게임 밖)" };
    const PopulationRegion* region = nullptr;
    if (regionKey) {
        for (const PopulationRegion& r : population->regions) {
            if (r.key == *regionKey) region = &r;
        }
    }
    const std::string scope = region ? region->name : "모든 내 영지 합계";
    const int families = region ? region->families : population->families;
    const int people = region ? region->population : population->population;
    const int homeless = region ? region->homeless : population->homeless;
    const int freeSlots = region ? region->freeSlots : population->freeSlots;
    const int unassigned = region ? region->unassigned : population->unassigned;
    return {
        "현재(" + scope + "): 가족 " + std::to_string(families) + " · 인구 " + std::to_string(people) + " · 집 없는 가족 " + std::to_string(homeless)
            + " · 빈 자리 " + std::to_string(freeSlots) + " · 미배치 가족 " + std::to_string(unassigned),
        "이번 세션(전체): 자연 이민 " + std::to_string(population->natural) + "가족 → 배율로 추가 " + std::to_string(population->multiplied) + "가족",
    };
}

std::vector<std::string> mercStatusLines(const MercenaryStatus* m) {
    if (!m) return { "고용 창: - (용병 기능이 꺼져 있거나 게임 밖)" };
    std::string slots;
    for (const MercSlot& s : m->slots) {
        if (!slots.empty()) slots += ", ";
        slots += s.name + (s.custom ? "(커스텀)" : "") + " " + formatThousands(s.cost);
    }
    if (slots.empty()) slots = "(비어 있음)";
    std::vector<std::string> lines = {
        "고용 창: " + slots,
        "고용 중: 내 용병단 " + std::to_string(m->hiredMine) + "개, AI " + std::to_string(m->hiredAi) + "개 · 맵을 불러온 뒤 환급 " + formatThousands(m->refunded),
    };
    for (const MercSkipped& s : m->skipped) lines.push_back("띄우지 못함: " + s.name + " — " + s.reason);
    if (m->note && !m->note->empty()) lines.push_back("참고: " + *m->note);
    return lines;
}

}
```

- [ ] **Step 4: 빌드와 테스트 통과 확인**

Run: `pwsh E:/MLToybox/tools/build-native.ps1 -Test`
Expected: `100% tests passed, 0 tests failed out of 1`

Run: `& E:\MLToybox\native\build\native_tests.exe | Select-Object -Last 1`
Expected: `122 tests, 0 failed`

- [ ] **Step 5: 커밋**

```bash
git -C E:/MLToybox add native/overlay native/tests native/CMakeLists.txt
git -C E:/MLToybox commit -m "feat(overlay): mercenary company rules, resource rows, tab texts and options" -m "Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 5: 글자 입력 기반 — 한글 입력기, 클립보드, 입력 위젯

용병단 이름 칸에 한글을 칠 수 있게 한다(스펙 2.5).
- 게임이 창의 IME를 꺼 두었으면 오버레이의 글자 칸에 커서가 있는 동안만 창 스레드가 IME를 켜고, 끝나면 꺼 둔 상태로 되돌린다. 켜져 있으면 그대로 둔다. 조합 중인 글자는 입력 커서 자리에 띄운다. 언리얼 엔진은 자기 입력 칸에 커서가 없을 때 창의 IME를 꺼 두는데, 이 게임이 그런지는 재지 않았다(코드는 두 경우 모두 다룬다).
- 그동안의 조합 메시지(`WM_IME_STARTCOMPOSITION`, `WM_IME_COMPOSITION`, `WM_IME_ENDCOMPOSITION`, `WM_IME_CHAR`)는 게임에 넘기지 않는다. ImGui 백엔드는 `WM_IME_COMPOSITION`을 기본 처리까지 해 주고, 나머지는 훅이 기본 처리한다. 조합이 끝난 글자는 `WM_CHAR`로 와서 ImGui가 받는다.
- 복사(Ctrl+C, Ctrl+X)는 화면 스레드가 글만 적어 두고 작업 스레드가 Windows 클립보드에 쓴다. ImGui의 기본 처리는 화면 스레드가 잠금을 쥔 채 클립보드를 비우는데, 그때 Windows가 게임 창에 메시지를 보내면 창 스레드와 서로 멈출 수 있다. 붙여넣기(읽기)는 기본 처리를 그대로 쓴다.
- 클립보드는 사용자의 것이라 테스트에서 건드리지 않는다. 복사와 한글 조합은 Task 9의 사용자 확인 항목이다.

**Files:**
- Create: `native/overlay/render/clipboard.h`, `native/overlay/render/clipboard.cpp`
- Modify(파일 전체): `native/overlay/render/input.cpp`, `native/overlay/ui/widgets.h`, `native/overlay/ui/widgets.cpp`, `native/tests/overlay_input_tests.cpp`
- Modify: `native/overlay/ui/app.h`, `native/overlay/ui/app.cpp`, `native/overlay/render/dx12_hook.cpp`, `native/overlay/render/imgui_layer.cpp`, `native/overlay/worker.cpp`, `native/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 1의 `wakeWindowThread()`와 테스트의 `Fixture`, `g_reached`; Task 3의 `ScopeOption`; 계획 A의 `parseNumber`, `trim`
- Produces:
  - `overlay/ui/app.h`: `std::atomic<bool> App::wantText`, `std::atomic<int> App::imeX`, `App::imeY`, `std::string App::clipboardOut`, `bool App::clipboardPending`(뒤 둘은 `App::mutex`로 보호)
  - `overlay/render/clipboard.h`: `bool writeClipboardText(const std::string& utf8)`
  - `overlay/ui/widgets.h`: `float scaled(float px)`, `bool optionalNumberField(const char* id, std::optional<int>& value, int min, int max, float width)`, `bool comboOptions(const char* id, const std::vector<ScopeOption>& options, int& index, float width)`, `bool textField(const char* id, std::string& value, size_t maxBytes, float width)`

- [ ] **Step 1: 실패하는 테스트 작성**

`overlay_input_tests.cpp`는 파일 전체를 바꾼다(끝에 테스트 셋을 더하고 `<cstdio>`, `<imm.h>`를 포함한다).

<!-- file: native/tests/overlay_input_tests.cpp -->
```cpp
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

// 게임이 창의 IME(한글 입력기)를 꺼 두었으면 오버레이의 글자 칸에 커서가 있는 동안만 켜고, 끝나면 되돌린다
TEST(overlay_input_turns_the_ime_on_only_while_a_text_field_has_the_cursor) {
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
    CHECK(!imeOn());
    f.send(WM_NULL);
    CHECK(!imeOn());                               // 글자 칸에 커서가 없으면 건드리지 않는다
    f.a.wantText = true;
    f.send(WM_NULL);
    CHECK(imeOn());                                // 글자 칸에 커서가 있다: 한글을 칠 수 있다
    f.a.wantText = false;
    f.send(WM_NULL);
    CHECK(!imeOn());                               // 게임이 꺼 두었던 상태로 되돌린다

    f.a.wantText = true;
    f.send(WM_NULL);
    CHECK(imeOn());
    f.send(WM_KEYDOWN, VK_INSERT, 0x00000001);     // 글자 칸에 커서를 둔 채 창을 닫아도 되돌린다
    CHECK(!f.a.visible.load() && !imeOn());
    f.a.wantText = false;
}

TEST(overlay_input_leaves_the_ime_alone_when_the_game_had_it_on) {
    if (!GetSystemMetrics(SM_IMMENABLED)) return;
    Fixture f;
    const HIMC before = ImmGetContext(f.hwnd);     // 게임의 입력 칸에 커서가 있어 IME 가 켜져 있던 경우
    if (!before) return;
    ImmReleaseContext(f.hwnd, before);
    f.a.wantText = true;
    f.send(WM_NULL);
    f.a.wantText = false;
    f.send(WM_NULL);
    const HIMC after = ImmGetContext(f.hwnd);
    CHECK(after != nullptr);                       // 우리가 켠 것이 아니면 끄지 않는다
    if (after) ImmReleaseContext(f.hwnd, after);
}

// 글자 칸에 커서가 있을 때의 한글 조합 메시지는 게임에 넘기지 않는다. 조합이 끝난 글자는 WM_CHAR 로 와서 ImGui 가 받는다
TEST(overlay_input_keeps_korean_composition_away_from_the_game) {
    Fixture f;
    f.send(WM_IME_STARTCOMPOSITION);
    f.send(WM_IME_ENDCOMPOSITION);
    CHECK(g_reached[WM_IME_STARTCOMPOSITION] == 1 && g_reached[WM_IME_ENDCOMPOSITION] == 1);   // 글자 칸이 아니면 게임의 것
    f.a.wantText = true;
    f.a.wantKeyboard = true;
    f.send(WM_IME_STARTCOMPOSITION);
    f.send(WM_IME_CHAR, 0xAC00, 1);                // '가': 기본 처리가 WM_CHAR 로 바꿔 다시 보낸다
    f.send(WM_IME_ENDCOMPOSITION);
    CHECK(g_reached[WM_IME_STARTCOMPOSITION] == 1 && g_reached[WM_IME_ENDCOMPOSITION] == 1);
    CHECK(g_reached[WM_IME_CHAR] == 0 && g_reached[WM_CHAR] == 0);
    f.a.wantText = false;
}
```

- [ ] **Step 2: 빌드해서 실패 확인**

Run: `pwsh E:/MLToybox/tools/build-native.ps1 -Test`
Expected: 컴파일 오류. `App`에 `wantText`가 없다.

- [ ] **Step 3: 구현**

<!-- edit: native/overlay/ui/app.h -->
`native/overlay/ui/app.h` (1/2) — 찾을 부분:

```cpp
    std::string reason;                        // state 가 Disabled 일 때의 이유
```

바꿀 내용:

```cpp
    std::string reason;                        // state 가 Disabled 일 때의 이유
    std::string clipboardOut;                  // 화면에서 복사한 글. 작업 스레드가 Windows 클립보드에 쓴다
    bool clipboardPending = false;
```

<!-- edit: native/overlay/ui/app.h -->
`native/overlay/ui/app.h` (2/2) — 찾을 부분:

```cpp
    std::atomic<bool> wantKeyboard{false};
```

바꿀 내용:

```cpp
    std::atomic<bool> wantKeyboard{false};
    std::atomic<bool> wantText{false};         // 직전 프레임에 글자 입력 칸에 커서가 있었는가(창 스레드가 IME 를 켜고 끈다)
    std::atomic<int> imeX{0};                  // 입력 커서의 위치(게임 창 안 좌표). 조합 중인 글자를 여기에 띄운다
    std::atomic<int> imeY{0};
```

<!-- edit: native/overlay/ui/app.cpp -->
`native/overlay/ui/app.cpp` — 찾을 부분:

```cpp
    a.wantKeyboard = false;
    std::lock_guard<std::mutex> lock(a.mutex);
```

바꿀 내용:

```cpp
    a.wantKeyboard = false;
    a.wantText = false;
    std::lock_guard<std::mutex> lock(a.mutex);
```

`input.cpp`는 파일 전체를 바꾼다.

<!-- file: native/overlay/render/input.cpp -->
```cpp
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
```

<!-- file: native/overlay/render/clipboard.h -->
```cpp
#pragma once
#include <string>

namespace mlt::ov {
// 글(UTF-8)을 Windows 클립보드에 쓴다. 작업 스레드에서 부른다.
// ImGui 의 기본 처리는 프레임을 그리는 스레드에서 잠금을 쥔 채 클립보드를 비운다. 클립보드 주인이 게임 창이면
// Windows 가 그 창에 메시지를 보내고, 창 스레드가 그때 ImGui 잠금을 기다리고 있으면 서로 멈춘다.
bool writeClipboardText(const std::string& utf8);
}
```

<!-- file: native/overlay/render/clipboard.cpp -->
```cpp
#include "clipboard.h"
#include <windows.h>

namespace mlt::ov {

bool writeClipboardText(const std::string& utf8) {
    const int length = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);   // 끝의 0 을 포함한 글자 수
    if (length <= 0) return false;
    const HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, static_cast<SIZE_T>(length) * sizeof(wchar_t));
    if (!memory) return false;
    if (auto* dest = static_cast<wchar_t*>(GlobalLock(memory))) {
        MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, dest, length);
        GlobalUnlock(memory);
    }
    if (!OpenClipboard(nullptr)) {
        GlobalFree(memory);
        return false;
    }
    EmptyClipboard();
    const bool ok = SetClipboardData(CF_UNICODETEXT, memory) != nullptr;   // 성공하면 메모리는 클립보드의 것이 된다
    if (!ok) GlobalFree(memory);
    CloseClipboard();
    return ok;
}

}
```

<!-- edit: native/overlay/render/dx12_hook.cpp -->
`native/overlay/render/dx12_hook.cpp` (1/2) — 찾을 부분:

```cpp
        a.wantKeyboard = visible && (io.WantCaptureKeyboard || io.WantTextInput);
        if (openAtStart && !visible) wakeWindowThread();   // 화면에서 닫았다. 창 스레드가 눌린 채 남은 입력을 정리하게 한다
```

바꿀 내용:

```cpp
        a.wantKeyboard = visible && (io.WantCaptureKeyboard || io.WantTextInput);
        const bool text = visible && io.WantTextInput;
        // 화면에서 닫았거나 글자 칸에 커서가 들어오고 나갔다. 창 스레드가 눌린 입력을 정리하고 IME 를 켜고 끈다
        if ((openAtStart && !visible) || a.wantText.exchange(text) != text) wakeWindowThread();
```

<!-- edit: native/overlay/render/dx12_hook.cpp -->
`native/overlay/render/dx12_hook.cpp` (2/2) — 찾을 부분:

```cpp
    if (!wanted) {
        a.wantMouse = false;
        a.wantKeyboard = false;
        return;
    }
```

바꿀 내용:

```cpp
    if (!wanted) {
        a.wantMouse = false;
        a.wantKeyboard = false;
        if (a.wantText.exchange(false)) wakeWindowThread();
        return;
    }
```

<!-- edit: native/overlay/render/imgui_layer.cpp -->
`native/overlay/render/imgui_layer.cpp` (1/2) — 찾을 부분:

```cpp
#include "imgui_layer.h"
#include "overlay/ui/app.h"
```

바꿀 내용:

```cpp
#include "imgui_layer.h"
#include "input.h"
#include "overlay/ui/app.h"
```

<!-- edit: native/overlay/render/imgui_layer.cpp -->
`native/overlay/render/imgui_layer.cpp` (2/2) — 찾을 부분:

```cpp
    if (!ImGui_ImplWin32_Init(hwnd)) {
        err = "ImGui_ImplWin32_Init";
        return false;
    }
    return true;
```

바꿀 내용:

```cpp
    if (!ImGui_ImplWin32_Init(hwnd)) {
        err = "ImGui_ImplWin32_Init";
        return false;
    }

    // 둘 다 프레임 안에서(화면 스레드가 ImGui 잠금과 App::mutex 를 쥔 채) 불린다. 여기서는 적어 두기만 하고,
    // Windows 의 IME·클립보드 함수는 창 스레드와 작업 스레드가 부른다
    ImGuiPlatformIO& platform = ImGui::GetPlatformIO();
    platform.Platform_SetImeDataFn = [](ImGuiContext*, ImGuiViewport*, ImGuiPlatformImeData* data) {
        App& a = app();
        a.imeX = static_cast<int>(data->InputPos.x);
        a.imeY = static_cast<int>(data->InputPos.y + data->InputLineHeight);   // 입력 줄 바로 아래
        wakeWindowThread();
    };
    platform.Platform_SetClipboardTextFn = [](ImGuiContext*, const char* text) {
        App& a = app();
        a.clipboardOut = text ? text : "";
        a.clipboardPending = true;
    };
    return true;
```

<!-- edit: native/overlay/worker.cpp -->
`native/overlay/worker.cpp` (1/3) — 찾을 부분:

```cpp
#include "worker.h"
#include "overlay/render/dx12_hook.h"
```

바꿀 내용:

```cpp
#include "worker.h"
#include "overlay/render/clipboard.h"
#include "overlay/render/dx12_hook.h"
```

<!-- edit: native/overlay/worker.cpp -->
`native/overlay/worker.cpp` (2/3) — 찾을 부분:

```cpp
static void writeOverlayStatus(App& a, const Bridge& bridge) {
```

바꿀 내용:

```cpp
// 화면에서 복사한 글을 Windows 클립보드에 쓴다(잠금 밖에서)
static void flushClipboard(App& a) {
    std::string text;
    {
        std::lock_guard<std::mutex> lock(a.mutex);
        if (!a.clipboardPending) return;
        a.clipboardPending = false;
        text.swap(a.clipboardOut);
    }
    writeClipboardText(text);
}

static void writeOverlayStatus(App& a, const Bridge& bridge) {
```

<!-- edit: native/overlay/worker.cpp -->
`native/overlay/worker.cpp` (3/3) — 찾을 부분:

```cpp
        try {
            if (tick % 2 == 0 && tick >= saveRetryAt) {            // 0.2초마다. 실패했으면 1초 뒤에 다시 한다
```

바꿀 내용:

```cpp
        try {
            flushClipboard(a);
            if (tick % 2 == 0 && tick >= saveRetryAt) {            // 0.2초마다. 실패했으면 1초 뒤에 다시 한다
```

`widgets.h`와 `widgets.cpp`는 파일 전체를 바꾼다.

<!-- file: native/overlay/ui/widgets.h -->
```cpp
#pragma once
#include "overlay/core/units.h"
#include <optional>
#include <string>
#include <vector>

// 여러 탭이 쓰는 화면 조각. 모두 ImGui 프레임 안에서 부른다
namespace mlt::ov {
// 글자 배율을 곱한 길이
float scaled(float px);

// 숫자 칸. 포커스가 없을 때는 value 를 보여 준다. 편집을 끝내면(Enter 또는 포커스 이동) 입력을 해석해
// [min, max] 로 맞춘 값을 value 에 넣고, 값이 바뀌었으면 true 를 돌려준다. 해석할 수 없는 입력은 버린다.
// 전각 숫자·공백·쉼표를 허용한다(core/number_input).
bool numberField(const char* id, int& value, int min, int max, float width);

// 빈칸을 허용하는 숫자 칸. 빈칸으로 두고 편집을 끝내면 값 없음이 된다(자원 목표: 빈칸 = 관리하지 않음)
bool optionalNumberField(const char* id, std::optional<int>& value, int min, int max, float width);

// 선택지에서 하나 고르기. 고른 줄이 바뀌면 true. index 가 범위를 벗어나 있으면 첫 줄로 맞춘다
bool comboOptions(const char* id, const std::vector<ScopeOption>& options, int& index, float width);

// 글자 칸(UTF-8). 내용이 바뀌면 true. maxBytes 는 받을 수 있는 가장 긴 바이트 수(최대 255)
bool textField(const char* id, std::string& value, size_t maxBytes, float width);

// 게임 상태가 필요한 부분이 비어 있을 때의 안내 문구
void needGameText();
}
```

<!-- file: native/overlay/ui/widgets.cpp -->
```cpp
#include "widgets.h"
#include "overlay/core/number_input.h"
#include "overlay/core/text.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <imgui.h>
#include <unordered_map>

namespace mlt::ov {

float scaled(float px) {
    return px * ImGui::GetStyle().FontScaleMain;
}

namespace {
// 숫자 칸마다 편집 중인 글을 들고 있는다(ImGui 는 칸의 글을 호출하는 쪽이 들고 있어야 한다)
std::array<char, 32>& numberBuffer(const char* id) {
    static std::unordered_map<ImGuiID, std::array<char, 32>> buffers;
    return buffers[ImGui::GetID(id)];
}
}

bool numberField(const char* id, int& value, int min, int max, float width) {
    auto& buf = numberBuffer(id);
    ImGui::SetNextItemWidth(width);
    ImGui::InputText(id, buf.data(), buf.size(), ImGuiInputTextFlags_AutoSelectAll);
    bool changed = false;
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        if (auto parsed = parseNumber(buf.data())) {
            const int clamped = std::clamp(*parsed, min, max);
            if (clamped != value) {
                value = clamped;
                changed = true;
            }
        }
    }
    if (!ImGui::IsItemActive()) std::snprintf(buf.data(), buf.size(), "%d", value);
    return changed;
}

bool optionalNumberField(const char* id, std::optional<int>& value, int min, int max, float width) {
    auto& buf = numberBuffer(id);
    ImGui::SetNextItemWidth(width);
    ImGui::InputText(id, buf.data(), buf.size(), ImGuiInputTextFlags_AutoSelectAll);
    bool changed = false;
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        if (trim(buf.data()).empty()) {
            changed = value.has_value();
            value.reset();
        } else if (auto parsed = parseNumber(buf.data())) {
            const int clamped = std::clamp(*parsed, min, max);
            changed = value != clamped;
            value = clamped;
        }
    }
    if (!ImGui::IsItemActive()) {
        if (value) std::snprintf(buf.data(), buf.size(), "%d", *value);
        else buf[0] = '\0';
    }
    return changed;
}

bool comboOptions(const char* id, const std::vector<ScopeOption>& options, int& index, float width) {
    if (options.empty()) return false;
    if (index < 0 || index >= static_cast<int>(options.size())) index = 0;
    bool changed = false;
    ImGui::SetNextItemWidth(width);
    if (ImGui::BeginCombo(id, options[static_cast<size_t>(index)].label.c_str())) {
        for (int i = 0; i < static_cast<int>(options.size()); ++i) {
            ImGui::PushID(i);   // 같은 이름의 줄이 있어도 구별한다
            const bool selected = i == index;
            if (ImGui::Selectable(options[static_cast<size_t>(i)].label.c_str(), selected) && !selected) {
                index = i;
                changed = true;
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    return changed;
}

bool textField(const char* id, std::string& value, size_t maxBytes, float width) {
    char buf[256];
    const size_t capacity = std::min(maxBytes + 1, sizeof(buf));
    std::snprintf(buf, capacity, "%s", value.c_str());
    ImGui::SetNextItemWidth(width);
    if (!ImGui::InputText(id, buf, capacity)) return false;
    value = buf;
    return true;
}

void needGameText() {
    ImGui::TextDisabled("게임에 들어가면 표시됩니다");
}

}
```

<!-- edit: native/CMakeLists.txt -->
`native/CMakeLists.txt` (1/2) — 찾을 부분:

```cmake
    overlay/render/input.cpp
    overlay/render/guard.cpp
```

바꿀 내용:

```cmake
    overlay/render/input.cpp
    overlay/render/clipboard.cpp
    overlay/render/guard.cpp
```

<!-- edit: native/CMakeLists.txt -->
`native/CMakeLists.txt` (2/2) — 찾을 부분:

```cmake
target_link_libraries(overlay_impl PUBLIC overlay_core imgui minhook d3d12 dxgi user32)
```

바꿀 내용:

```cmake
target_link_libraries(overlay_impl PUBLIC overlay_core imgui minhook d3d12 dxgi user32 imm32)
```

- [ ] **Step 4: 빌드와 테스트 통과 확인**

Run: `pwsh E:/MLToybox/tools/build-native.ps1 -Test`
Expected: `100% tests passed, 0 tests failed out of 1`. `native/overlay/` 소스에서 경고가 없다.

Run: `& E:\MLToybox\native\build\native_tests.exe | Select-String 'SKIP|tests,'`
Expected: `125 tests, 0 failed`이고 `SKIP` 줄이 없다(한글 Windows에서는 IME 테스트가 실제로 돈다).

- [ ] **Step 5: 커밋**

```bash
git -C E:/MLToybox add native/overlay native/tests native/CMakeLists.txt
git -C E:/MLToybox commit -m "feat(overlay): text input - IME on the window thread, clipboard writes on the worker, text and combo widgets" -m "Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 6: 군사·인구 탭

**Files:**
- Create: `native/overlay/ui/tab_military.cpp`, `native/overlay/ui/tab_population.cpp`
- Modify: `native/overlay/ui/tabs.h`, `native/overlay/ui/window.cpp`, `native/CMakeLists.txt`

**Interfaces:**
- Consumes: `TabContext`, `markDirty`, `sendCommand`(계획 A); `ControlDoc::military()/population()`, `makeSpawnSquads`, `makeReformSquads`, `makeCustomizeRetinue`, `makeAddFamilies`, `units()`, `indexOfKey`(Task 3); `spawnRegionOptions`, `reformText`, `canReform`, `retinueLabel`, `populationScopeOptions`, `populationInfo`(Task 4); `numberField`, `comboOptions`, `scaled`, `needGameText`(Task 5)
- Produces: `void drawMilitaryTab(TabContext&)`, `void drawPopulationTab(TabContext&)`

- [ ] **Step 1: 군사 탭**

"꾸미기 열기"를 누르면 명령을 보내고 오버레이 창을 닫는다(게임의 꾸미기 화면을 가리지 않게. 스펙 2.4).

<!-- file: native/overlay/ui/tab_military.cpp -->
```cpp
#include "overlay/core/commands.h"
#include "overlay/core/view.h"
#include "tabs.h"
#include "widgets.h"
#include <imgui.h>
#include <optional>
#include <string>
#include <vector>

namespace mlt::ov {

namespace {
// 화면에서 고른 것(문서에 저장하지 않는다)
int g_unit = 1;                          // 병종 목록의 줄. 패널처럼 "민병대 - 창"에서 시작한다
int g_count = 1;
std::optional<std::string> g_region;     // 병력 생성·재구성·꾸미기의 위치(영지 키)
std::optional<std::string> g_squad;      // 고른 수행원 분대의 ID

std::vector<ScopeOption> unitChoices() {
    std::vector<ScopeOption> options;
    for (const UnitOption& u : units()) options.push_back({ u.id, u.label });
    return options;
}

void drawSpawn(TabContext& ctx, const StatusDoc* live) {
    static const std::vector<ScopeOption> unitOptions = unitChoices();
    ImGui::SeparatorText("병력 생성 (주민과 무관)");
    comboOptions("##unit", unitOptions, g_unit, scaled(180.0f));
    ImGui::SameLine();
    numberField("##count", g_count, 1, 5, scaled(50.0f));
    ImGui::SameLine();
    ImGui::TextUnformatted("개 분대, 위치:");
    ImGui::SameLine();
    const std::vector<ScopeOption> regions = spawnRegionOptions(live && live->playerRegions ? &*live->playerRegions : nullptr);
    int region = indexOfKey(regions, g_region);
    comboOptions("##region", regions, region, scaled(200.0f));
    g_region = regions[static_cast<size_t>(region)].key;   // 골라 둔 영지가 사라졌으면 첫 줄로 돌아간다
    ImGui::SameLine();
    ImGui::BeginDisabled(!ctx.inGame);
    if (ImGui::Button("분대 생성")) {
        sendCommand(ctx.app, makeSpawnSquads(units()[static_cast<size_t>(g_unit)].id, g_count, ctx.now, g_region));
    }
    ImGui::EndDisabled();

    // 생성 분대는 집이 없어 게임의 해제 → 집결이 안 된다(해제하면 0/N 빈 카드). 모드가 같은 병종으로 다시 만든다
    const SpawnStatus* spawn = live && live->spawn ? &*live->spawn : nullptr;
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(reformText(spawn).c_str());
    ImGui::SameLine();
    ImGui::BeginDisabled(!canReform(spawn));
    if (ImGui::Button("재구성")) sendCommand(ctx.app, makeReformSquads(ctx.now, g_region));
    ImGui::EndDisabled();
}

// 모드가 만든 수행원 분대에 게임의 꾸미기 화면을 연다(위 "위치" 영지의 영주 저택 기준)
void drawRetinue(TabContext& ctx, const StatusDoc* live) {
    ImGui::SeparatorText("수행원 꾸미기 (생성·고용한 친위대 분대)");
    const RetinueStatus* retinue = live && live->retinue ? &*live->retinue : nullptr;
    std::vector<ScopeOption> squads;
    if (retinue) {
        for (const RetinueSquad& s : retinue->squads) squads.push_back({ std::to_string(s.id), retinueLabel(s) });
    }
    if (squads.empty()) {
        ImGui::TextDisabled("꾸밀 수 있는 분대가 없습니다");
        return;
    }
    int squad = indexOfKey(squads, g_squad);
    comboOptions("##squad", squads, squad, scaled(260.0f));
    g_squad = squads[static_cast<size_t>(squad)].key;
    ImGui::SameLine();
    // 꾸미기 화면이 열려 있는 동안에는 다시 열 수 없다
    const std::string label = (retinue->editing ? "꾸미기 열림 (#" + std::to_string(*retinue->editing) + ")" : std::string("꾸미기 열기")) + "##retinue";
    ImGui::BeginDisabled(retinue->editing.has_value());
    if (ImGui::Button(label.c_str())) {
        sendCommand(ctx.app, makeCustomizeRetinue(retinue->squads[static_cast<size_t>(squad)].id, ctx.now, g_region));
        ctx.app.visible = false;   // 게임의 꾸미기 화면을 가리지 않게 창을 닫는다
    }
    ImGui::EndDisabled();
}
}

void drawMilitaryTab(TabContext& ctx) {
    App& a = ctx.app;
    MilitarySettings m = a.control.military();
    bool changed = false;
    changed |= ImGui::Checkbox("군사 기능 사용", &m.enabled);
    changed |= ImGui::Checkbox("민병대 장비 요구 무시", &m.ignoreEquipment);
    changed |= ImGui::Checkbox("징집 조건(집 레벨·훈련) 무시", &m.ignorePopulation);
    ImGui::Indent();
    ImGui::TextDisabled("주민 수보다 많은 병력은 아래 '병력 생성' 사용");
    ImGui::Unindent();
    changed |= ImGui::Checkbox("민병대 모집비 0", &m.zeroUpkeep);
    changed |= ImGui::Checkbox("부대 수 상한 해제", &m.unlimitedSquads);
    if (changed) {
        a.control.setMilitary(m);
        markDirty(a);
    }

    const StatusDoc* live = ctx.inGame ? ctx.status : nullptr;   // 게임 안일 때만 게임 상태를 쓴다
    drawSpawn(ctx, live);
    drawRetinue(ctx, live);
    if (!ctx.inGame) needGameText();
}

}
```

- [ ] **Step 2: 인구 탭**

<!-- file: native/overlay/ui/tab_population.cpp -->
```cpp
#include "overlay/core/commands.h"
#include "overlay/core/view.h"
#include "tabs.h"
#include "widgets.h"
#include <imgui.h>
#include <optional>
#include <string>
#include <vector>

namespace mlt::ov {

namespace {
// 화면에서 고른 것(문서에 저장하지 않는다)
std::optional<std::string> g_scope;   // 없음 = 공통(모든 내 영지), 있으면 영지 키
int g_addCount = 3;

// 목표 가족 수. 공통이면 "영지마다 최소 가족 수", 영지를 골랐으면 그 영지 값(따로 지정했을 때만 고칠 수 있다).
// 설정이 바뀌었으면 true
bool drawTarget(PopulationSettings& p) {
    bool changed = false;
    ImGui::AlignTextToFramePadding();
    if (!g_scope) {
        ImGui::TextUnformatted("영지마다 최소 가족 수(0 = 끔, 부족분만 채움):");
        ImGui::SameLine();
        return numberField("##target", p.targetFamilies, 0, 1000, scaled(70.0f));
    }
    const auto own = p.regionTargets.find(*g_scope);
    bool separate = own != p.regionTargets.end();
    int value = separate ? own->second : p.targetFamilies;   // 따로 지정하지 않았으면 공통 값을 보여 준다
    ImGui::TextUnformatted("이 영지 최소 가족 수(0 = 끔):");
    ImGui::SameLine();
    ImGui::PushID(g_scope->c_str());   // 영지마다 다른 칸이다(편집 중인 글이 섞이지 않게)
    ImGui::BeginDisabled(!separate);
    if (numberField("##regionTarget", value, 0, 1000, scaled(70.0f)) && separate) {
        p.regionTargets[*g_scope] = value;
        changed = true;
    }
    ImGui::EndDisabled();
    ImGui::PopID();
    ImGui::SameLine();
    if (ImGui::Checkbox("이 영지만 따로 지정", &separate)) {
        if (separate) p.regionTargets[*g_scope] = value;
        else p.regionTargets.erase(*g_scope);   // 끄면 공통 값을 따른다
        changed = true;
    }
    return changed;
}
}

void drawPopulationTab(TabContext& ctx) {
    App& a = ctx.app;
    PopulationSettings p = a.control.population();
    const PopulationStatus* live = (ctx.inGame && ctx.status && ctx.status->population) ? &*ctx.status->population : nullptr;

    bool changed = ImGui::Checkbox("인구 기능 사용", &p.enabled);

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("영지:");
    ImGui::SameLine();
    const std::vector<ScopeOption> scopes = populationScopeOptions(live);
    int scope = indexOfKey(scopes, g_scope);
    comboOptions("##scope", scopes, scope, scaled(230.0f));
    g_scope = scopes[static_cast<size_t>(scope)].key;   // 골라 둔 영지가 사라졌으면 공통으로 돌아간다

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("자연 이민 배율(배):");
    ImGui::SameLine();
    changed |= numberField("##multiplier", p.multiplier, 1, 10, scaled(50.0f));

    changed |= drawTarget(p);
    if (changed) {
        a.control.setPopulation(p);
        markDirty(a);
    }

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("지금 바로 들일 가족 수:");
    ImGui::SameLine();
    numberField("##add", g_addCount, 1, 20, scaled(50.0f));
    ImGui::SameLine();
    ImGui::BeginDisabled(!ctx.inGame);
    if (ImGui::Button("가족 추가")) sendCommand(a, makeAddFamilies(g_addCount, ctx.now, g_scope));
    ImGui::EndDisabled();

    ImGui::Spacing();
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("배율은 영지마다 그 영지의 자연 이민만큼 그 영지에 추가합니다. 가족 추가는 선택한 영지에, 공통이면 빈 자리가 많은 영지부터 들입니다.");
    ImGui::TextDisabled("빈 집(가족 0 → 1 → 2 순)에 들어오며 미배치 가족으로 들어옵니다. 빈 자리가 없으면 들어오지 않습니다.");
    ImGui::Spacing();
    for (const std::string& line : populationInfo(live, g_scope)) ImGui::TextUnformatted(line.c_str());
    ImGui::PopTextWrapPos();
}

}
```

- [ ] **Step 3: 탭 등록**

<!-- edit: native/overlay/ui/tabs.h -->
`native/overlay/ui/tabs.h` — 찾을 부분:

```cpp
void drawUpgradeTab(TabContext& ctx);
```

바꿀 내용:

```cpp
void drawUpgradeTab(TabContext& ctx);
void drawMilitaryTab(TabContext& ctx);
void drawPopulationTab(TabContext& ctx);
```

<!-- edit: native/overlay/ui/window.cpp -->
`native/overlay/ui/window.cpp` — 찾을 부분:

```cpp
    { "업그레이드", drawUpgradeTab },
    { "상태", drawStatusTab },
```

바꿀 내용:

```cpp
    { "업그레이드", drawUpgradeTab },
    { "군사", drawMilitaryTab },
    { "인구", drawPopulationTab },
    { "상태", drawStatusTab },
```

<!-- edit: native/CMakeLists.txt -->
`native/CMakeLists.txt` — 찾을 부분:

```cmake
    overlay/ui/tab_lord.cpp
    overlay/ui/tab_status.cpp)
```

바꿀 내용:

```cmake
    overlay/ui/tab_lord.cpp
    overlay/ui/tab_military.cpp
    overlay/ui/tab_population.cpp
    overlay/ui/tab_status.cpp)
```

- [ ] **Step 4: 빌드**

Run: `pwsh E:/MLToybox/tools/build-native.ps1 -Test`
Expected: `100% tests passed, 0 tests failed out of 1`. `native/overlay/` 소스에서 경고가 없다.

- [ ] **Step 5: 게임 확인**

게임과 패널이 꺼져 있는지 보고(첫 명령의 출력이 비어야 한다. 켜져 있으면 끄지 말고 사용자에게 묻는다), 사용자에게 게임 창을 누르지 말라고 알린 뒤 진행한다. 배포하고 탭 화면을 찍는다.

```powershell
Get-Process -Name 'ManorLords-Win64-Shipping', 'MLToybox.Panel' -ErrorAction SilentlyContinue | Select-Object Name, Id
pwsh E:\MLToybox\tools\backup-saves.ps1
pwsh E:\MLToybox\tools\deploy.ps1 -Mod MLToybox
pwsh E:\MLToybox\tools\deploy.ps1 -Mod MLToyboxLab
. E:\MLToybox\tools\common.ps1
$mod = Join-Path (Get-MLModsDir) 'MLToybox'
$shots = New-Item -ItemType Directory -Force (Join-Path $env:TEMP 'mltb-overlay-shots')
Copy-Item "$mod\bridge\control.json" "$shots\control.before.json" -Force
& E:\MLToybox\tools\lab-load.ps1 -Slot saveGame_8 -Start
foreach ($tab in @{ n = 'military'; t = '군사' }, @{ n = 'population'; t = '인구' }) {
    @{ toggleKey = 'Insert'; scale = 1.0; window = @{ x = 80; y = 80; w = 760; h = 820 }; startOpen = $true; devTab = $tab.t } | ConvertTo-Json | Set-Content "$mod\bridge\overlay.json" -Encoding utf8NoBOM
    Start-Sleep -Seconds 3
    & E:\MLToybox\tools\capture-game.ps1 -Out "$shots\$($tab.n).png"
}
$c = Get-Content "$mod\bridge\control.json" -Raw | ConvertFrom-Json
$c.features.military, $c.features.population
$s = Get-Content "$mod\bridge\status.json" -Raw | ConvertFrom-Json
$s.playerRegions, $s.spawn, $s.retinue, $s.population
1..3 | ForEach-Object { & E:\MLToybox\tools\capture-game.ps1 -Click 400, 500; Start-Sleep -Milliseconds 400 }
Get-Content "$mod\bridge\overlay_status.json"
```

캡처를 읽어 확인한다.
- `military.png`: 탭이 영주, 건설, 업그레이드, 군사, 인구, 상태 순서다. 체크 5개가 `features.military`와 같다. "병력 생성 (주민과 무관)" 아래에 병종("민병대 - 창"), 개수 1, "개 분대, 위치:", 영지 목록(`status.playerRegions`의 첫 영지), "분대 생성"이 있다. "해제된 생성 분대: N개"가 `status.spawn`과 같고, 해제된 분대가 없으면 "재구성"이 흐리다. 수행원 분대가 없으면 "꾸밀 수 있는 분대가 없습니다".
- `population.png`: "인구 기능 사용" 체크, "영지:" 목록("공통 (모든 내 영지)"), 배율, "영지마다 최소 가족 수(0 = 끔, 부족분만 채움):", "지금 바로 들일 가족 수:"와 "가족 추가", 설명 두 줄, "현재(모든 내 영지 합계): 가족 … · 인구 …"이 `status.population`과 같다.
- 버튼 메시지 뒤 `"state":"ready"`.

게임을 저장 없이 끄고 `control.json`이 바뀌지 않았는지 본다.

```powershell
Stop-Process -Name 'ManorLords-Win64-Shipping' -Force
Start-Sleep -Seconds 4
Get-Process -Name 'CrashReportClient' -ErrorAction SilentlyContinue | Stop-Process -Force
(Get-FileHash "$mod\bridge\control.json").Hash -eq (Get-FileHash "$shots\control.before.json").Hash
```

Expected: `True`

- [ ] **Step 6: 커밋**

```bash
git -C E:/MLToybox add native/overlay native/CMakeLists.txt
git -C E:/MLToybox commit -m "feat(overlay): military and population tabs" -m "Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 7: 자원 탭

**Files:**
- Create: `native/overlay/ui/tab_resources.cpp`
- Modify: `native/overlay/ui/tabs.h`, `native/overlay/ui/window.cpp`, `native/CMakeLists.txt`

**Interfaces:**
- Consumes: `ControlDoc::resources()/setResources()`(Task 3); `resourceScopeOptions`, `resourceTargets`, `storeResourceTargets`, `buildResourceRows`(Task 4); `numberField`, `optionalNumberField`, `comboOptions`, `scaled`(Task 5)
- Produces: `void drawResourcesTab(TabContext&)`

- [ ] **Step 1: 자원 탭**

목표 칸이 비면 그 자원은 관리하지 않는다(영지 범위에서는 공통 목표를 따른다). "모두 이 값으로"는 지금 보이는 범위의 모든 줄을 채우고 저장한다. 영지 목표가 하나도 없으면 그 영지 키를 지운다(`storeResourceTargets`).

<!-- file: native/overlay/ui/tab_resources.cpp -->
```cpp
#include "overlay/core/resources.h"
#include "tabs.h"
#include "widgets.h"
#include <algorithm>
#include <cfloat>
#include <imgui.h>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace mlt::ov {

namespace {
constexpr int kTargetMax = 1000000000;

// 화면에서 고른 것(문서에 저장하지 않는다)
std::optional<std::string> g_scope;   // 없음 = 공통(모든 내 영지의 합계와 공통 목표), 있으면 영지 키
int g_fill = 500;

// 표: 자원 / 현재 / 목표. 목표 칸을 고쳤으면 targets 를 바꾸고 true
bool drawTable(const std::vector<ResourceRow>& rows, std::map<std::string, int>& targets) {
    const ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp;
    const float height = std::max(scaled(120.0f), ImGui::GetContentRegionAvail().y);
    if (!ImGui::BeginTable("##resources", 3, flags, ImVec2(0.0f, height))) return false;
    bool changed = false;
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("자원", ImGuiTableColumnFlags_WidthStretch, 3.0f);
    ImGui::TableSetupColumn("현재", ImGuiTableColumnFlags_WidthStretch, 2.0f);
    ImGui::TableSetupColumn(g_scope ? "영지 목표 (빈칸=공통 목표 따름)" : "목표 (빈칸=관리 안 함)", ImGuiTableColumnFlags_WidthStretch, 3.0f);
    ImGui::TableHeadersRow();
    ImGui::PushID(g_scope ? g_scope->c_str() : "##common");   // 범위마다 다른 칸이다(편집 중인 글이 섞이지 않게)
    for (const ResourceRow& row : rows) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(row.id.c_str());
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        if (row.current) ImGui::Text("%.0f", *row.current);
        else ImGui::TextDisabled("-");
        ImGui::TableNextColumn();
        ImGui::PushID(row.id.c_str());
        std::optional<int> target;
        if (auto it = targets.find(row.id); it != targets.end()) target = it->second;
        if (optionalNumberField("##target", target, 0, kTargetMax, -FLT_MIN)) {
            if (target) targets[row.id] = *target;
            else targets.erase(row.id);   // 빈칸 = 관리하지 않는다(영지 범위에서는 공통 목표를 따른다)
            changed = true;
        }
        ImGui::PopID();
    }
    ImGui::PopID();
    ImGui::EndTable();
    return changed;
}
}

void drawResourcesTab(TabContext& ctx) {
    App& a = ctx.app;
    ResourcesSettings r = a.control.resources();
    const StatusDoc* live = ctx.inGame ? ctx.status : nullptr;   // 게임 안일 때만 게임 상태를 쓴다

    bool changed = ImGui::Checkbox("자원 목표값 유지", &r.enabled);
    ImGui::SameLine();
    ImGui::TextUnformatted("주기(초)");
    ImGui::SameLine();
    changed |= numberField("##interval", r.intervalSec, 1, 60, scaled(50.0f));

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("영지");
    ImGui::SameLine();
    const std::vector<ScopeOption> scopes = resourceScopeOptions(live);
    int scope = indexOfKey(scopes, g_scope);
    comboOptions("##scope", scopes, scope, scaled(260.0f));
    g_scope = scopes[static_cast<size_t>(scope)].key;   // 골라 둔 영지가 사라졌으면 공통으로 돌아간다
    ImGui::SameLine();
    numberField("##fill", g_fill, 0, 1000000, scaled(90.0f));
    ImGui::SameLine();
    const bool fill = ImGui::Button("모두 이 값으로");

    const std::vector<ResourceRow> rows = buildResourceRows(live, r, g_scope);
    std::map<std::string, int> targets = resourceTargets(r, g_scope);
    bool targetsChanged = false;
    if (fill && !rows.empty()) {   // 지금 보이는 범위의 모든 줄
        for (const ResourceRow& row : rows) targets[row.id] = g_fill;
        targetsChanged = true;
    }
    if (rows.empty()) {
        if (ctx.inGame) ImGui::TextDisabled("자원이 없습니다");
        else ImGui::TextDisabled("게임에 들어가면 자원 목록이 표시됩니다");
    } else {
        targetsChanged |= drawTable(rows, targets);
    }
    if (targetsChanged) {
        storeResourceTargets(r, g_scope, std::move(targets));   // 영지 목표가 하나도 없으면 그 영지 키를 지운다
        changed = true;
    }
    if (changed) {
        a.control.setResources(r);
        markDirty(a);
    }
}

}
```

- [ ] **Step 2: 탭 등록**

<!-- edit: native/overlay/ui/tabs.h -->
`native/overlay/ui/tabs.h` — 찾을 부분:

```cpp
void drawPopulationTab(TabContext& ctx);
```

바꿀 내용:

```cpp
void drawPopulationTab(TabContext& ctx);
void drawResourcesTab(TabContext& ctx);
```

<!-- edit: native/overlay/ui/window.cpp -->
`native/overlay/ui/window.cpp` — 찾을 부분:

```cpp
const Tab kTabs[] = {
    { "영주", drawLordTab },
```

바꿀 내용:

```cpp
const Tab kTabs[] = {
    { "자원", drawResourcesTab },
    { "영주", drawLordTab },
```

<!-- edit: native/CMakeLists.txt -->
`native/CMakeLists.txt` — 찾을 부분:

```cmake
    overlay/ui/tab_population.cpp
```

바꿀 내용:

```cmake
    overlay/ui/tab_population.cpp
    overlay/ui/tab_resources.cpp
```

- [ ] **Step 3: 빌드**

Run: `pwsh E:/MLToybox/tools/build-native.ps1 -Test`
Expected: `100% tests passed, 0 tests failed out of 1`. `native/overlay/` 소스에서 경고가 없다.

- [ ] **Step 4: 게임 확인**

게임과 패널이 꺼져 있는지 보고(첫 명령의 출력이 비어야 한다. 켜져 있으면 끄지 말고 사용자에게 묻는다), 사용자에게 게임 창을 누르지 말라고 알린 뒤 진행한다.

```powershell
Get-Process -Name 'ManorLords-Win64-Shipping', 'MLToybox.Panel' -ErrorAction SilentlyContinue | Select-Object Name, Id
pwsh E:\MLToybox\tools\backup-saves.ps1
pwsh E:\MLToybox\tools\deploy.ps1 -Mod MLToybox
pwsh E:\MLToybox\tools\deploy.ps1 -Mod MLToyboxLab
. E:\MLToybox\tools\common.ps1
$mod = Join-Path (Get-MLModsDir) 'MLToybox'
$shots = New-Item -ItemType Directory -Force (Join-Path $env:TEMP 'mltb-overlay-shots')
Copy-Item "$mod\bridge\control.json" "$shots\control.before.json" -Force
& E:\MLToybox\tools\lab-load.ps1 -Slot saveGame_8 -Start
@{ toggleKey = 'Insert'; scale = 1.0; window = @{ x = 80; y = 80; w = 760; h = 820 }; startOpen = $true; devTab = '자원' } | ConvertTo-Json | Set-Content "$mod\bridge\overlay.json" -Encoding utf8NoBOM
Start-Sleep -Seconds 3
& E:\MLToybox\tools\capture-game.ps1 -Out "$shots\resources.png"
$c = Get-Content "$mod\bridge\control.json" -Raw | ConvertFrom-Json
"enabled=$($c.features.resources.enabled) interval=$($c.features.resources.intervalSec) Ale=$($c.features.resources.targets.Ale)"
$s = Get-Content "$mod\bridge\status.json" -Raw | ConvertFrom-Json
"ids=$($s.resourceIds.Count) Ale=$($s.resources.Ale)"
```

`resources.png`를 읽어 확인한다: 탭 맨 앞이 "자원"이다. "자원 목표값 유지" 체크와 "주기(초)"가 `features.resources`와 같다. "영지" 목록은 "공통 (모든 내 영지, 현재=합계)", 그 옆에 채울 값(500)과 "모두 이 값으로". 표의 머리글은 자원 / 현재 / "목표 (빈칸=관리 안 함)"이고, 줄이 이름순이며 첫 줄들의 현재 값과 목표가 `status.resources`, `features.resources.targets`와 같다. "현재" 열의 숫자가 잘리지 않는다. 표 오른쪽에 세로 스크롤 막대가 있다.

게임을 저장 없이 끄고 `control.json`이 바뀌지 않았는지 본다.

```powershell
Stop-Process -Name 'ManorLords-Win64-Shipping' -Force
Start-Sleep -Seconds 4
Get-Process -Name 'CrashReportClient' -ErrorAction SilentlyContinue | Stop-Process -Force
(Get-FileHash "$mod\bridge\control.json").Hash -eq (Get-FileHash "$shots\control.before.json").Hash
```

Expected: `True`

- [ ] **Step 5: 커밋**

```bash
git -C E:/MLToybox add native/overlay native/CMakeLists.txt
git -C E:/MLToybox commit -m "feat(overlay): resources tab with per-region targets" -m "Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 8: 용병 탭

등록과 "사용" 체크의 규칙은 코어에 두고 테스트한다. 탭은 그것을 부르기만 한다.

**Files:**
- Modify: `native/overlay/core/merc_rules.h`, `native/overlay/core/merc_rules.cpp`, `native/tests/overlay_merc_rules_tests.cpp`
- Create: `native/overlay/ui/tab_mercenaries.cpp`
- Modify: `native/overlay/ui/tabs.h`, `native/overlay/ui/window.cpp`, `native/CMakeLists.txt`

**Interfaces:**
- Consumes: `ControlDoc::mercenaries()/setMercenaries()`(Task 3); `validateCompany`, `canEnableCompany`, `unitSummary`, `mercRegionOptions`, `bannerOptions`, `formatThousands`, `mercStatusLines`(Task 4); `textField`, `numberField`, `comboOptions`, `scaled`(Task 5)
- Produces:
  - `overlay/core/merc_rules.h`: `struct MercRegistration { bool ok; int index; std::string message; }`, `MercRegistration registerCompany(std::vector<MercCompany>& all, int editing, MercCompany draft)`, `std::optional<std::string> setCompanyEnabled(std::vector<MercCompany>& all, int index, bool enabled)`
  - `void drawMercenariesTab(TabContext&)`

- [ ] **Step 1: 실패하는 테스트 작성**

<!-- edit: native/tests/overlay_merc_rules_tests.cpp -->
`native/tests/overlay_merc_rules_tests.cpp` — 찾을 부분:

```cpp
TEST(overlay_merc_rules_vanilla_names_and_banners) {
```

바꿀 내용:

```cpp
TEST(overlay_merc_rules_register_adds_or_updates_and_reports) {
    std::vector<MercCompany> all;
    MercRegistration r = registerCompany(all, -1, { "  검사대  ", { "mercenary_infantry" }, 1000, "nus", "Battle_Brothers", false });
    CHECK(r.ok && r.index == 0 && r.message == "등록했습니다." && all.size() == 1);
    CHECK(all[0].name == "검사대" && all[0].banner == "battle_brothers" && all[0].enabled);   // 이름은 다듬고, 깃발은 표의 이름으로, 새 용병단은 켠 채로
    all[0].enabled = false;
    r = registerCompany(all, 0, { "검사대", { "mercenary_infantry", "mercenary_infantry" }, 2000, std::nullopt, "dragon", true });
    CHECK(r.ok && r.index == 0 && all.size() == 1);                                           // 고치기: 자리와 "사용"은 그대로
    CHECK(all[0].units.size() == 2 && all[0].cost == 2000 && !all[0].region && !all[0].banner && !all[0].enabled);
    r = registerCompany(all, -1, { "검사대", { "mercenary_infantry" }, 1, std::nullopt, std::nullopt, true });
    CHECK(!r.ok && r.message == "같은 이름의 용병단이 이미 있습니다." && all.size() == 1);    // 검증에 실패하면 바꾸지 않는다
    r = registerCompany(all, 0, { "", {}, 0, std::nullopt, std::nullopt, true });
    CHECK(!r.ok && all[0].name == "검사대" && all[0].units.size() == 2);
}

// R7: 등록 수에는 제한이 없고 "사용"은 최대 3개. 체크를 끄면 다른 것을 켤 수 있다
TEST(overlay_merc_rules_any_number_can_be_registered_but_only_three_are_in_use) {
    std::vector<MercCompany> all;
    for (const char* name : { "1", "2", "3" }) CHECK(registerCompany(all, -1, company(name)).ok);
    CHECK(all[0].enabled && all[1].enabled && all[2].enabled);
    const MercRegistration fourth = registerCompany(all, -1, company("4"));
    CHECK(fourth.ok && all.size() == 4 && !all[3].enabled);
    CHECK(fourth.message == "사용 중인 용병단이 3개라 '사용'을 끈 채로 등록했습니다.");
    CHECK(registerCompany(all, -1, company("5")).ok && all.size() == 5 && !all[4].enabled);
    CHECK(setCompanyEnabled(all, 3, true) == "사용은 최대 3개입니다." && !all[3].enabled);       // 네 번째는 켜지지 않는다
    CHECK(!setCompanyEnabled(all, 0, false).has_value() && !all[0].enabled);                  // 하나를 끄면
    CHECK(!setCompanyEnabled(all, 3, true).has_value() && all[3].enabled);                    // 다른 것을 켤 수 있다
    CHECK(!setCompanyEnabled(all, 1, true).has_value() && all[1].enabled);                    // 이미 켜진 것은 그대로
    CHECK(setCompanyEnabled(all, 9, true).has_value());                                       // 없는 자리
}

TEST(overlay_merc_rules_vanilla_names_and_banners) {
```

- [ ] **Step 2: 빌드해서 실패 확인**

Run: `pwsh E:/MLToybox/tools/build-native.ps1 -Test`
Expected: 컴파일 오류. `MercRegistration`, `registerCompany`, `setCompanyEnabled`가 없다.

- [ ] **Step 3: 규칙 구현**

<!-- edit: native/overlay/core/merc_rules.h -->
`native/overlay/core/merc_rules.h` — 찾을 부분:

```cpp
// self 를 "사용"으로 바꿔도 되는가(self 를 뺀 사용 수가 3 미만). 새 용병단이면 self = -1
bool canEnableCompany(const std::vector<MercCompany>& all, int self);
```

바꿀 내용:

```cpp
// self 를 "사용"으로 바꿔도 되는가(self 를 뺀 사용 수가 3 미만). 새 용병단이면 self = -1
bool canEnableCompany(const std::vector<MercCompany>& all, int self);

struct MercRegistration {
    bool ok = false;
    int index = -1;         // 등록된 자리
    std::string message;    // 사용자에게 보여 줄 결과(실패하면 이유)
};
// 편집한 내용(draft)을 등록한다. editing 이 -1 이면 새로 넣고, 아니면 그 자리를 고친다.
// 이름은 앞뒤 공백을 떼고, 깃발은 표의 이름으로 맞춘다. 새 용병단은 사용 중인 것이 3개 미만일 때만 켠 채로 넣고,
// 고칠 때는 "사용"을 그대로 둔다(draft.enabled 는 보지 않는다). 검증에 실패하면 all 을 바꾸지 않는다
MercRegistration registerCompany(std::vector<MercCompany>& all, int editing, MercCompany draft);
// "사용" 체크를 바꾼다. 켤 수 없으면(이미 3개가 사용 중) 바꾸지 않고 이유를 돌려준다
std::optional<std::string> setCompanyEnabled(std::vector<MercCompany>& all, int index, bool enabled);
```

<!-- edit: native/overlay/core/merc_rules.cpp -->
`native/overlay/core/merc_rules.cpp` — 찾을 부분:

```cpp
    return enabled < kMercMaxEnabled;
}
```

바꿀 내용:

```cpp
    return enabled < kMercMaxEnabled;
}

MercRegistration registerCompany(std::vector<MercCompany>& all, int editing, MercCompany draft) {
    const bool isNew = editing < 0 || editing >= static_cast<int>(all.size());
    if (isNew) editing = -1;
    draft.name = trim(draft.name);
    draft.banner = normalizeBanner(draft.banner);
    draft.enabled = isNew ? canEnableCompany(all, -1) : all[static_cast<size_t>(editing)].enabled;
    MercRegistration result;
    if (auto reason = validateCompany(draft, all, editing)) {
        result.message = *reason;
        return result;
    }
    result.ok = true;
    result.message = "등록했습니다.";
    if (isNew) {
        if (!draft.enabled) result.message = "사용 중인 용병단이 " + std::to_string(kMercMaxEnabled) + "개라 '사용'을 끈 채로 등록했습니다.";
        all.push_back(std::move(draft));
        result.index = static_cast<int>(all.size()) - 1;
    } else {
        all[static_cast<size_t>(editing)] = std::move(draft);
        result.index = editing;
    }
    return result;
}

std::optional<std::string> setCompanyEnabled(std::vector<MercCompany>& all, int index, bool enabled) {
    if (index < 0 || index >= static_cast<int>(all.size())) return "없는 용병단입니다.";
    if (enabled && !canEnableCompany(all, index)) return "사용은 최대 " + std::to_string(kMercMaxEnabled) + "개입니다.";
    all[static_cast<size_t>(index)].enabled = enabled;
    return std::nullopt;
}
```

- [ ] **Step 4: 테스트 통과 확인**

Run: `pwsh E:/MLToybox/tools/build-native.ps1 -Test`
Expected: `100% tests passed, 0 tests failed out of 1`

Run: `& E:\MLToybox\native\build\native_tests.exe | Select-Object -Last 1`
Expected: `127 tests, 0 failed`

- [ ] **Step 5: 용병 탭**

<!-- file: native/overlay/ui/tab_mercenaries.cpp -->
```cpp
#include "overlay/core/merc_rules.h"
#include "overlay/core/view.h"
#include "tabs.h"
#include "widgets.h"
#include <algorithm>
#include <imgui.h>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace mlt::ov {

namespace {
const ImVec4 kErrorColor(0.95f, 0.40f, 0.35f, 1.0f);

int infantryIndex() {
    for (size_t i = 0; i < units().size(); ++i) {
        if (units()[i].id == "mercenary_infantry") return static_cast<int>(i);
    }
    return 0;
}

// 편집 영역. "등록"을 누르기 전에는 문서에 반영하지 않는다
struct Editor {
    int editing = -1;                   // 고치고 있는 용병단의 자리. -1 = 새 용병단
    std::string name;
    std::vector<std::string> units;     // 분대마다 병종 id 하나
    int cost = 1000;
    std::optional<std::string> region;
    std::optional<std::string> banner;
    int unit = infantryIndex();         // "분대 추가"에서 고른 병종
    int count = 1;
    std::string group;                  // 구성 목록에서 고른 병종 id
    std::string message;                // 등록·사용 체크의 결과나 이유
    bool messageIsError = false;

    void say(std::string text, bool error) {
        message = std::move(text);
        messageIsError = error;
    }
    void load(const MercCompany& c, int index) {
        editing = index;
        name = c.name;
        units = c.units;
        cost = std::clamp(c.cost, 0, 10000000);
        region = c.region;
        banner = c.banner;
        group.clear();
        message.clear();
    }
    void clear() {
        editing = -1;
        name.clear();
        units.clear();
        cost = 1000;
        region.reset();
        banner.reset();
        group.clear();
        message.clear();
    }
};
Editor g_editor;

std::vector<ScopeOption> unitChoices() {
    std::vector<ScopeOption> options;
    for (const UnitOption& u : units()) options.push_back({ u.id, u.label });
    return options;
}

const std::string& labelOf(const std::vector<ScopeOption>& options, const std::optional<std::string>& key) {
    return options[static_cast<size_t>(indexOfKey(options, key))].label;
}

// 표: 사용 / 이름 / 구성 / 고용비 / 도착 영지 / 깃발, 그리고 "새 용병단"·"삭제". 문서를 바꿨으면 true
bool drawList(MercSettings& m, const std::vector<ScopeOption>& regions) {
    Editor& e = g_editor;
    bool changed = false;
    if (m.companies.empty()) {
        ImGui::TextDisabled("등록한 용병단이 없습니다");
    } else if (ImGui::BeginTable("##companies", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("사용", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("이름");
        ImGui::TableSetupColumn("구성");
        ImGui::TableSetupColumn("고용비");
        ImGui::TableSetupColumn("도착 영지");
        ImGui::TableSetupColumn("깃발");
        ImGui::TableHeadersRow();
        for (int i = 0; i < static_cast<int>(m.companies.size()); ++i) {
            const MercCompany& c = m.companies[static_cast<size_t>(i)];
            ImGui::PushID(i);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            bool use = c.enabled;
            if (ImGui::Checkbox("##use", &use)) {
                // 등록 수에는 제한이 없고 "사용"은 최대 3개다. 사용 중인 것만 고용 창에 올라간다(규칙은 core/merc_rules)
                if (auto reason = setCompanyEnabled(m.companies, i, use)) {
                    e.say(*reason, true);
                } else {
                    e.message.clear();
                    changed = true;
                }
            }
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            if (ImGui::Selectable((c.name + "##name").c_str(), e.editing == i)) e.load(c, i);   // 줄을 누르면 편집 영역에 싣는다
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(unitSummary(c.units).c_str());
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(formatThousands(c.cost).c_str());
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(labelOf(regions, c.region).c_str());
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(c.banner ? c.banner->c_str() : "-");
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (ImGui::Button("새 용병단")) e.clear();
    ImGui::SameLine();
    ImGui::BeginDisabled(e.editing < 0);
    if (ImGui::Button("삭제")) {
        m.companies.erase(m.companies.begin() + e.editing);
        e.clear();
        changed = true;
    }
    ImGui::EndDisabled();
    return changed;
}

void field(const char* label) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(scaled(90.0f));
}

// 구성 목록: 병종마다 한 줄("용병 - 보병 × 2"), 처음 넣은 순서
void drawComposition(Editor& e) {
    std::vector<std::string> order;
    for (const std::string& unit : e.units) {
        if (std::find(order.begin(), order.end(), unit) == order.end()) order.push_back(unit);
    }
    if (ImGui::BeginListBox("##composition", ImVec2(scaled(260.0f), scaled(76.0f)))) {
        for (const std::string& unit : order) {
            const auto count = std::count(e.units.begin(), e.units.end(), unit);
            const std::string label = unitLabel(unit) + " × " + std::to_string(count) + "##" + unit;
            if (ImGui::Selectable(label.c_str(), e.group == unit)) e.group = unit;
        }
        ImGui::EndListBox();
    }
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::BeginDisabled(e.group.empty());
    if (ImGui::Button("선택 병종 빼기")) {
        e.units.erase(std::remove(e.units.begin(), e.units.end(), e.group), e.units.end());
        e.group.clear();
    }
    ImGui::EndDisabled();
    ImGui::Text("합계 %d / %d", static_cast<int>(e.units.size()), kMercMaxSquads);
    ImGui::EndGroup();
}

// 문서를 바꿨으면 true("등록"이 성공했을 때만)
bool drawEditor(MercSettings& m, const std::vector<ScopeOption>& regions) {
    static const std::vector<ScopeOption> unitOptions = unitChoices();
    static const std::vector<ScopeOption> banners = bannerOptions();
    Editor& e = g_editor;

    field("이름");
    textField("##name", e.name, static_cast<size_t>(kMercNameMax) * 4, scaled(260.0f));   // 40자. 한 글자는 4바이트까지

    field("분대 추가");
    comboOptions("##unit", unitOptions, e.unit, scaled(180.0f));
    ImGui::SameLine();
    numberField("##count", e.count, 1, kMercMaxSquads, scaled(50.0f));
    ImGui::SameLine();
    ImGui::TextUnformatted("개 분대");
    ImGui::SameLine();
    if (ImGui::Button("추가")) {
        if (static_cast<int>(e.units.size()) + e.count > kMercMaxSquads) {
            e.say("분대는 " + std::to_string(kMercMaxSquads) + "개까지입니다.", true);
        } else {
            e.units.insert(e.units.end(), static_cast<size_t>(e.count), units()[static_cast<size_t>(e.unit)].id);
            e.message.clear();
        }
    }

    field("구성");
    drawComposition(e);

    field("고용비");
    numberField("##cost", e.cost, 0, 10000000, scaled(110.0f));

    field("도착 영지");
    int region = indexOfKey(regions, e.region);
    comboOptions("##region", regions, region, scaled(220.0f));
    e.region = regions[static_cast<size_t>(region)].key;

    field("깃발");
    int banner = indexOfKey(banners, e.banner);
    comboOptions("##banner", banners, banner, scaled(220.0f));
    e.banner = banners[static_cast<size_t>(banner)].key;

    bool changed = false;
    if (ImGui::Button("등록")) {
        // 검증하고 넣거나 고친다(규칙은 core/merc_rules). 실패하면 문서를 바꾸지 않고 이유를 보여 준다
        MercCompany draft;
        draft.name = e.name;
        draft.units = e.units;
        draft.cost = e.cost;
        draft.region = e.region;
        draft.banner = e.banner;
        const MercRegistration result = registerCompany(m.companies, e.editing, std::move(draft));
        e.say(result.message, !result.ok);
        if (result.ok) {
            e.editing = result.index;
            e.name = m.companies[static_cast<size_t>(result.index)].name;
            changed = true;
        }
    }
    if (!e.message.empty()) {
        ImGui::SameLine();
        if (e.messageIsError) ImGui::TextColored(kErrorColor, "%s", e.message.c_str());
        else ImGui::TextUnformatted(e.message.c_str());
    }
    return changed;
}
}

void drawMercenariesTab(TabContext& ctx) {
    App& a = ctx.app;
    Editor& e = g_editor;
    MercSettings m = a.control.mercenaries();
    if (e.editing >= static_cast<int>(m.companies.size())) e.editing = -1;   // 패널 등 밖에서 목록이 줄었다
    const StatusDoc* live = ctx.inGame ? ctx.status : nullptr;                // 게임 안일 때만 게임 상태를 쓴다

    bool changed = false;
    changed |= ImGui::Checkbox("용병 기능 사용 (고용 창 자동 보충)", &m.enabled);
    changed |= ImGui::Checkbox("내 용병단 고용비 환급, 유지비 0", &m.refund);
    changed |= ImGui::Checkbox("커스텀 용병단 AI 잠금 (고용 창을 열 때만 설정한 고용비)", &m.lockFromAi);

    // 도착 영지 선택지. 등록된 용병단의 도착 영지와 지금 고른 값은 영지 목록에 없어도 남긴다
    // (게임 밖에서 용병단을 고쳐 등록해도 저장된 도착 영지가 "내 첫 영지"로 바뀌지 않게)
    static const std::vector<RegionInfo> noRegions;
    std::vector<std::optional<std::string>> keep;
    for (const MercCompany& c : m.companies) keep.push_back(c.region);
    keep.push_back(e.region);
    const std::vector<ScopeOption> regions = mercRegionOptions(live && live->playerRegions ? *live->playerRegions : noRegions, keep);

    ImGui::SeparatorText(("등록한 용병단 (사용 최대 " + std::to_string(kMercMaxEnabled) + "개)").c_str());
    changed |= drawList(m, regions);
    ImGui::SeparatorText("용병단 편집");
    changed |= drawEditor(m, regions);
    if (changed) {
        a.control.setMercenaries(m);
        markDirty(a);
    }

    ImGui::Spacing();
    ImGui::PushTextWrapPos(0.0f);
    for (const std::string& line : mercStatusLines(live && live->mercenaries ? &*live->mercenaries : nullptr)) ImGui::TextUnformatted(line.c_str());
    ImGui::PopTextWrapPos();
}

}
```

<!-- edit: native/overlay/ui/tabs.h -->
`native/overlay/ui/tabs.h` — 찾을 부분:

```cpp
void drawResourcesTab(TabContext& ctx);
```

바꿀 내용:

```cpp
void drawResourcesTab(TabContext& ctx);
void drawMercenariesTab(TabContext& ctx);
```

<!-- edit: native/overlay/ui/window.cpp -->
`native/overlay/ui/window.cpp` — 찾을 부분:

```cpp
    { "군사", drawMilitaryTab },
    { "인구", drawPopulationTab },
```

바꿀 내용:

```cpp
    { "군사", drawMilitaryTab },
    { "용병", drawMercenariesTab },
    { "인구", drawPopulationTab },
```

<!-- edit: native/CMakeLists.txt -->
`native/CMakeLists.txt` — 찾을 부분:

```cmake
    overlay/ui/tab_resources.cpp
```

바꿀 내용:

```cmake
    overlay/ui/tab_resources.cpp
    overlay/ui/tab_mercenaries.cpp
```

- [ ] **Step 6: 빌드**

Run: `pwsh E:/MLToybox/tools/build-native.ps1 -Test`
Expected: `100% tests passed, 0 tests failed out of 1`. `native/overlay/` 소스에서 경고가 없다.

- [ ] **Step 7: 게임 확인 — 화면**

게임과 패널이 꺼져 있는지 보고(첫 명령의 출력이 비어야 한다. 켜져 있으면 끄지 말고 사용자에게 묻는다), 사용자에게 게임 창을 누르지 말라고 알린 뒤 진행한다. 게임은 Step 8까지 켜 둔다.

```powershell
Get-Process -Name 'ManorLords-Win64-Shipping', 'MLToybox.Panel' -ErrorAction SilentlyContinue | Select-Object Name, Id
pwsh E:\MLToybox\tools\backup-saves.ps1
pwsh E:\MLToybox\tools\deploy.ps1 -Mod MLToybox
pwsh E:\MLToybox\tools\deploy.ps1 -Mod MLToyboxLab
. E:\MLToybox\tools\common.ps1
$mod = Join-Path (Get-MLModsDir) 'MLToybox'
$shots = New-Item -ItemType Directory -Force (Join-Path $env:TEMP 'mltb-overlay-shots')
Copy-Item "$mod\bridge\control.json" "$shots\control.before.json" -Force
& E:\MLToybox\tools\lab-load.ps1 -Slot saveGame_8 -Start
@{ toggleKey = 'Insert'; scale = 1.0; window = @{ x = 80; y = 80; w = 760; h = 820 }; startOpen = $true; devTab = '용병' } | ConvertTo-Json | Set-Content "$mod\bridge\overlay.json" -Encoding utf8NoBOM
Start-Sleep -Seconds 3
& E:\MLToybox\tools\capture-game.ps1 -Out "$shots\mercenaries.png"
(Get-Content "$mod\bridge\control.json" -Raw | ConvertFrom-Json).features.mercenaries | ConvertTo-Json -Depth 5
(Get-Content "$mod\bridge\status.json" -Raw | ConvertFrom-Json).mercenaries | ConvertTo-Json -Depth 5
```

`mercenaries.png`를 읽어 확인한다: 탭이 자원, 영주, 건설, 업그레이드, 군사, 용병, 인구, 상태 순서다. 체크 3개가 `features.mercenaries`와 같다. "등록한 용병단 (사용 최대 3개)" 표(사용 / 이름 / 구성 / 고용비 / 도착 영지 / 깃발)의 줄이 `companies`와 같다(구성은 "용병 - 보병 × 4" 꼴, 고용비는 "1,000" 꼴, 도착 영지는 "이름 (키)" 꼴). "새 용병단", "삭제"(고른 줄이 없으면 흐림). "용병단 편집" 아래에 이름, 분대 추가, 구성과 "합계 0 / 10", 고용비 1000, 도착 영지 "내 첫 영지", 깃발 "칸의 것 그대로", "등록". 맨 아래 "고용 창: …", "고용 중: …"이 `status.mercenaries`와 같다.

- [ ] **Step 8: 게임 확인 — "사용"을 끄면 고용 창에서 빠지는가(R7)**

오버레이의 체크는 마우스가 있어야 누를 수 있으므로, 같은 결과를 내는 설정 변경을 파일로 넣어 모드의 동작을 본다. 사용 중인 첫 용병단의 `enabled`를 끄고 `seq`를 올린다.

```powershell
$c = Get-Content "$mod\bridge\control.json" -Raw | ConvertFrom-Json
$name = $c.features.mercenaries.companies[0].name
"before: $((Get-Content "$mod\bridge\status.json" -Raw | ConvertFrom-Json).mercenaries.slots.name -join ', ')"
$c.seq = $c.seq + 1
$c.features.mercenaries.companies[0].enabled = $false
$c | ConvertTo-Json -Depth 20 | Set-Content "$mod\bridge\control.json" -Encoding utf8NoBOM
$t0 = Get-Date
do {
    Start-Sleep -Seconds 2
    $slots = try { (Get-Content "$mod\bridge\status.json" -Raw | ConvertFrom-Json).mercenaries.slots.name } catch { @($name) }
} while (($slots -contains $name) -and ((Get-Date) - $t0).TotalSeconds -lt 90)
"after $([int]((Get-Date) - $t0).TotalSeconds)s: $($slots -join ', ')"
& E:\MLToybox\tools\capture-game.ps1 -Out "$shots\mercenaries-off.png"
```

Expected: `before`에는 그 용병단 이름(`$name`)이 있고 `after`에는 없다. 걸린 시간을 적어 둔다(모드는 고용 목록을 바로 고치거나, 다시 만들어야 하면 정해 둔 간격을 기다린다. 얼마나 걸리는지는 잰 적이 없다). `mercenaries-off.png`의 표에서 그 줄의 "사용" 체크가 꺼져 있다(오버레이가 밖에서 바뀐 설정을 다시 읽었다).
- 90초가 지나도 이름이 남아 있으면 R7이 지켜지지 않은 것이다. 추측으로 고치지 말고 `status.mercenaries`의 `note`·`skipped`와 `UE4SS.log`를 읽어 원인을 잰 뒤(superpowers:systematic-debugging) 고치고 진행한다.
- 등록한 용병단이 없거나 용병 기능이 꺼져 있으면 이 단계는 건너뛰고 그렇게 기록한다.

게임을 저장 없이 끄고 `control.json`을 되돌린다.

```powershell
Stop-Process -Name 'ManorLords-Win64-Shipping' -Force
Start-Sleep -Seconds 4
Get-Process -Name 'CrashReportClient' -ErrorAction SilentlyContinue | Stop-Process -Force
Copy-Item "$shots\control.before.json" "$mod\bridge\control.json" -Force
(Get-FileHash "$mod\bridge\control.json").Hash -eq (Get-FileHash "$shots\control.before.json").Hash
```

Expected: `True`

- [ ] **Step 9: 커밋**

```bash
git -C E:/MLToybox add native/overlay native/tests native/CMakeLists.txt
git -C E:/MLToybox commit -m "feat(overlay): mercenaries tab - register any number of companies, at most three in use" -m "Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 9: 문서, 마지막 확인, 병합

**Files:**
- Modify: `README.md`, `CLAUDE.md`, `docs/CHANGELOG.md`, `analysis/findings.md`

**Interfaces:**
- Consumes: Task 1, 6, 7, 8의 게임 확인 결과(실측값)
- Produces: 없음

- [ ] **Step 1: README — 오버레이가 기본 사용법, 패널은 비상용**

머리말 첫 문장과 구성 요소의 두 줄을 다음으로 바꾼다.

```markdown
Manor Lords(Steam, UE5)용 치트 모드입니다. 설정은 게임 안 창(오버레이)에서 합니다. 구성 요소는 네 가지입니다.
```

```markdown
- **오버레이 DLL**: 게임 화면 안에 설정 창을 띄웁니다(Insert). 모든 설정을 여기서 합니다.
- **.NET 8 WinForms 패널**: 게임 밖에서 같은 설정을 하는 프로그램입니다. 오버레이가 뜨지 않을 때 쓰는 비상용이고, 더 고치지 않습니다.
```

"게임 안 창 (오버레이)" 절의 **여닫기**, **지금 있는 탭**, **입력** 줄을 다음으로 바꾼다.

```markdown
- **여닫기**: Insert. 키는 [상태] 탭에서 Insert, Home, End, F7, F8, F9 가운데 고릅니다. 게임을 켤 때와 맵에 들어갈 때 화면 왼쪽 위에 8초 동안 안내가 뜹니다.
```

```markdown
- **탭**: 자원, 영주, 건설, 업그레이드, 군사, 용병, 인구, 상태. 패널의 탭과 같은 기능입니다(아래 "기능").
```

```markdown
- **입력**: 마우스가 창 위에 있거나 입력 칸에 커서가 있으면 그 입력은 게임에 가지 않습니다(2026-10-01 사용자 확인).
- **한글 이름**: 용병단 이름 칸에 커서가 있는 동안 한글 입력기가 켜집니다. 복사(Ctrl+C)와 붙여넣기(Ctrl+V)도 됩니다.
- **용병단**: 몇 개든 등록할 수 있고 "사용" 체크는 3개까지입니다. 체크한 것만 고용 창에 올라가고, 체크를 끄면 빠집니다.
- **깨진 설정 파일**: `bridge/control.json`을 손으로 고치다 문법을 틀려 읽을 수 없으면 오버레이는 기본값으로 뜹니다. 그 파일은 `control.json.bak`으로 남겨 둡니다.
```

같은 절의 **패널과 함께** 줄과 그 아래 **주의** 줄은 그대로 둔다.

"## 기능 (패널 탭별)" 제목을 "## 기능 (탭별)"로 바꾼다.

"설치"의 번호 목록을 다음으로 바꾼다.

```markdown
1. 게임을 실행합니다.
2. 게임 안에서 Insert를 눌러 MLToybox 창을 엽니다. 값을 바꾸면 바로 반영됩니다.
3. 창이 뜨지 않으면 `dist/panel/MLToybox.Panel.exe`를 켜서 설정하고 "적용"을 누릅니다(위 "창이 안 뜰 때" 참고).
```

- [ ] **Step 2: CLAUDE.md**

"작업 규칙 (이 레포)"의 마지막 줄을 다음으로 바꾼다.

```markdown
- Lua 모드 변경은 게임 재시작 후 적용, 패널은 실행 중이면 배포 불가(파일 잠김), 네이티브 DLL과 오버레이 DLL은 게임 실행 중 잠김.
- 오버레이 화면은 `bridge/overlay.json`의 `startOpen`·`devTab`과 `tools/capture-game.ps1`로 마우스 없이 확인한다. 게임을 켜는 확인은 한 번에 몰아서 하고 끝나면 바로 끈다. 켜기 전에 사용자에게 게임 창을 누르지 말라고 알린다.
```

- [ ] **Step 3: CHANGELOG**

`docs/CHANGELOG.md`의 맨 위 항목 앞에 넣는다. 날짜는 작업한 날로 적는다.

```markdown
## 2026-10-02 — 게임 안 창 2단계
### 추가
- **오버레이에 자원·군사·용병·인구 탭**을 더했습니다. 이제 패널의 기능을 모두 게임 안 창에서 씁니다. 패널은 오버레이가 뜨지 않을 때 쓰는 비상용으로 남깁니다.
- **한글 이름 입력**: 용병단 이름 칸에 커서가 있는 동안 한글 입력기를 켭니다. 복사·붙여넣기를 지원합니다.
- 안내("Insert: MLToybox")를 맵에 들어갈 때도 띄웁니다.
### 수정
- 버튼이나 키를 누른 채 창을 닫으면 다시 열 때 창이 마우스에 붙어 끌릴 수 있던 문제.
- 읽을 수 없는 `control.json`으로 시작했을 때 첫 변경이 파일을 기본값으로 덮던 문제. 이제 `control.json.bak`으로 사본을 남깁니다.
- 그 밖에 계획 A의 최종 리뷰에서 미룬 항목: 화면 출력용 큐 고르기, 꺼진 뒤의 큐 기록, 시험용 Present 호출, 작업 스레드 보호.
```

- [ ] **Step 4: findings**

`analysis/findings.md` 끝에 "게임 안 오버레이 창 — 계획 B 구현 검증 (날짜, saveGame_8)" 절을 더한다. Task 1, 6, 7, 8에서 **실측한 값만** 적는다. 확인하지 못한 것은 "확인 못 함"이라고 적는다. 항목:
- 안내: 세이브를 불러온 뒤 찍은 캡처에서 보였는가, 그때 화면이 무엇이었는가.
- 탭 화면: 탭마다 캡처에서 본 것과 `control.json`/`status.json`의 값.
- "사용"을 끈 용병단이 `status.mercenaries.slots`에서 빠지기까지 걸린 시간.
- 버튼 메시지, 해상도 변경 뒤의 상태.
- 사전 검증과 달랐던 점, 고친 것이 있으면 원인과 함께.
- 확인하지 못한 것: 마우스로 하는 조작과 한글 조합(사용자 확인 항목), 프레임 생성, HDR, 전체 화면 전용, 다중 모니터.

- [ ] **Step 5: 전체 확인**

```powershell
pwsh E:\MLToybox\tools\build-native.ps1 -Test
& E:\MLToybox\native\build\native_tests.exe | Select-Object -Last 1
dotnet test E:\MLToybox\panel\MLToybox.sln
dotnet build E:\MLToybox\panel\MLToybox.sln
pwsh E:\MLToybox\tools\tests\Tools.Tests.ps1
git -C E:/MLToybox diff --stat develop -- panel
```

Expected: `100% tests passed`, `127 tests, 0 failed`, `dotnet test` 모두 통과, 빌드 오류 0, `ALL PASS`. `panel/` 아래에 바뀐 파일이 없다(R8).

- [ ] **Step 6: 개발용 설정을 지우고 커밋**

```powershell
. E:\MLToybox\tools\common.ps1
$mod = Join-Path (Get-MLModsDir) 'MLToybox'
$shots = Join-Path $env:TEMP 'mltb-overlay-shots'
if (Test-Path "$shots\overlay.before.json") { Copy-Item "$shots\overlay.before.json" "$mod\bridge\overlay.json" -Force }
else { @{ toggleKey = 'Insert'; scale = 1.0; window = @{ x = 80; y = 80; w = 640; h = 720 }; startOpen = $false; devTab = $null } | ConvertTo-Json | Set-Content "$mod\bridge\overlay.json" -Encoding utf8NoBOM }
Get-Content "$mod\bridge\overlay.json"
pwsh E:\MLToybox\tools\deploy.ps1 -Mod MLToyboxLab -Remove
```

Expected: `overlay.json`이 Task 1 Step 9에서 따로 둔 사용자의 값이고(`startOpen`이 `false`, `devTab`이 없거나 `null`), `Removed MLToyboxLab`. 게임이 꺼져 있을 때 한다(켜져 있으면 오버레이가 `overlay.json`을 다시 쓴다).

```bash
git -C E:/MLToybox add README.md CLAUDE.md docs/CHANGELOG.md analysis/findings.md
git -C E:/MLToybox commit -m "docs: the overlay now carries every tab; the panel is the fallback" -m "Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

- [ ] **Step 7: 최종 리뷰 뒤 병합, 푸시, 배포**

브랜치 전체 리뷰(실행 스킬이 정한 절차)를 마치고 고칠 것을 고친 뒤 병합한다.

```bash
git -C E:/MLToybox checkout develop
git -C E:/MLToybox merge --no-ff feat/overlay-tabs -m "Merge branch 'feat/overlay-tabs' into develop" -m "Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
git -C E:/MLToybox push origin develop
```

게임이 꺼져 있을 때 최종본을 배포한다: `pwsh E:\MLToybox\tools\deploy.ps1 -Mod MLToybox`

**사용자 확인 항목** (마우스와 한글 입력은 자동으로 확인할 수 없다. 마지막 보고에 그대로 넣는다):
1. 자원 탭: 목표 칸에 숫자를 넣고 Enter. 칸을 비우면 그 자원의 관리가 풀리는가. "모두 이 값으로". "영지"를 바꾸면 그 영지의 재고와 영지 목표가 보이는가.
2. 군사 탭: "분대 생성"으로 분대가 생기는가. 해제한 생성 분대가 "재구성"으로 돌아오는가. "꾸미기 열기"를 누르면 오버레이 창이 닫히고 게임의 꾸미기 화면이 열리는가.
3. 인구 탭: 영지를 고르고 "이 영지만 따로 지정"을 켜고 끄기. "가족 추가".
4. 용병 탭: 새 용병단 등록(이름, 분대 추가, 고용비, 도착 영지, 깃발), 줄을 눌러 고치기, 삭제. 네 번째 "사용" 체크가 막히고 "사용은 최대 3개입니다."가 뜨는가. "사용"을 끄면 고용 창에서 그 카드가 빠지는가.
5. 한글 이름: 이름 칸에 한글이 쳐지는가. 조합 중인 글자가 어디에 보이는가. 글자가 두 번 들어가지 않는가. 이름 칸을 벗어난 뒤 게임의 단축키가 평소대로 듣는가.
6. 복사·붙여넣기: 이름 칸에서 Ctrl+C 한 글이 다른 프로그램에 붙는가. 다른 프로그램에서 복사한 글이 Ctrl+V 로 들어오는가.
7. 버튼을 누른 채 Insert 로 창을 닫았다 열어도 창이 마우스에 붙어 다니지 않는가.
8. 맵에 들어갈 때 왼쪽 위 안내가 보이는가.
