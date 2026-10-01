# 게임 안 오버레이 창 — 계획 A (기반, 코어, 간단한 탭) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 게임 화면 안에 MLToybox 창을 띄우고(Insert로 여닫기), 영주·건설·업그레이드·상태 탭에서 값을 바꾸면 즉시 `control.json`에 저장되게 한다.

**Architecture:** 새 DLL `mltoybox_overlay.dll`을 Lua 모드가 게임 시작 때 올린다. DLL은 DXGI `Present`/`ResizeBuffers`와 D3D12 `ExecuteCommandLists`를 후킹해 Dear ImGui 창을 게임 화면 위에 그리고, 게임 창의 창 프로시저를 바꿔 입력을 받는다. 설정·상태 문서와 저장 규칙은 그래픽에 의존하지 않는 정적 라이브러리 `overlay_core`에 두어 단위 테스트하고, 파일 입출력은 작업 스레드가 맡는다.

**Tech Stack:** C++20(MSVC, CMake + Ninja), Dear ImGui v1.92.9b(DX12·Win32 백엔드), nlohmann/json v3.12.0, MinHook(기존), UE4SS 3.0.1 Lua 5.4, PowerShell 7

**Spec:** `docs/superpowers/specs/2026-10-01-ingame-overlay-design.md`. 실측 근거는 `analysis/findings.md` "게임 안 오버레이 창 — DX12 후킹 + Dear ImGui (2026-10-01, 스파이크)".

**범위:** 스펙 8절의 7단계 가운데 1~3단계(기반, 코어, 간단한 탭)다. 4~7단계(군사, 인구, 자원, 용병 탭, 한글 입력, 패널 대체 문서)는 계획 B로 따로 쓴다. 계획 A가 끝나면 오버레이에는 탭 4개가 있고, 나머지 탭은 패널로 설정한다.

**사전 검증:** 이 문서의 코드는 레포 밖 사본에서 태스크 순서대로 적용해 빌드와 테스트를 통과시킨 것이다. 2026-10-01에 게임에서도 한 번 돌렸다(Lab으로 게임 도중에 DLL을 올림, `saveGame_8`): 창과 탭 4개 표시, Insert 토글, 밖에서 고친 `control.json` 반영, 해상도 변경 뒤 계속 그리기를 확인했다. 그 뒤에 고친 부분(저장 규칙을 `core/session`으로 옮김, 저장 실패 뒤 1초 쉬기, 상태 탭의 표, 해상도 변경 뒤 창 위치 다시 맞추기, 큐 찾기 제한 시간의 시작점)과 **게임 시작 때 DLL을 올리는 경로**, 마우스 입력은 게임에서 확인하지 못했다. Task 6과 Task 7에서 확인한다.

## Global Constraints

- 외부 라이브러리 버전: Dear ImGui `v1.92.9b`, nlohmann/json `v3.12.0`. 다른 버전을 쓰지 않는다.
- 패널(`panel/`) 코드는 바꾸지 않는다(R8). 기존 `mlt_core`와 `mltoybox_native`의 소스(`native/src/`)도 바꾸지 않는다.
- 브리지 프로토콜(`control.json` version 1, `status.json`)의 모양을 바꾸지 않는다. 추가는 `status.overlay` 하나다(R6).
- 오버레이가 실패해도 게임을 튕기게 하지 않는다(R5). 실패는 `disableOverlay(이유)`로 물러나고, 이유는 `overlay_status.json`의 `reason`에 남는다.
- 상수: 토글 키 기본 `Insert`(선택지 `Insert`, `Home`, `End`, `F7`, `F8`, `F9`), 글자 배율 0.8~1.5, 창 기본 640×720 위치 (80, 80), 글꼴 맑은 고딕 18px, 안내 8초, heartbeat 5초, 큐 찾기 20초, 설정 저장 확인 0.2초, 상태 읽기 1초, 저장 실패 뒤 다시 시도 1초, 파일 이름 바꾸기 재시도 5번(50ms 간격).
- 스레드 규칙: ImGui 함수는 `imguiMutex()` 아래에서만 부른다. 잠금 순서는 `imguiMutex()` 다음 `App::mutex`다. 화면 스레드(Present 후킹 안)에서는 파일을 읽거나 쓰지 않는다.
- 화면 문구는 패널과 같게 한다(`panel/MLToybox.Panel/MainForm.cs`).
- 이름공간은 `mlt::ov`. 포함 경로의 뿌리는 `native/`와 `native/third_party`다(`#include "overlay/core/text.h"`, `#include <nlohmann/json.hpp>`). 기존 `mlt_core`의 헤더는 `#include "runtime.h"`처럼 쓴다.
- C++ 소스는 UTF-8(BOM 없음)로 쓴다(프로젝트가 `/utf-8`로 컴파일한다). Lua 파일은 Write/Edit 도구로만 고친다(셸 heredoc은 백슬래시를 깨뜨린다).
- 확인 명령: `pwsh E:/MLToybox/tools/build-native.ps1 -Test`(네이티브 빌드와 테스트), `dotnet test E:/MLToybox/panel/MLToybox.sln`(Lua 스펙 포함), `pwsh E:/MLToybox/tools/tests/Tools.Tests.ps1`.
- 오버레이 DLL과 네이티브 DLL은 게임 실행 중에는 잠겨 있다. 배포는 게임이 꺼져 있을 때 한다.
- 게임 작업 규칙: 사용자의 게임이나 패널이 켜져 있으면 끄지 말고 먼저 묻는다. 시작 전에 `pwsh E:/MLToybox/tools/backup-saves.ps1`을 실행한다. 기존 세이브를 덮어쓰지 않고, 확인이 끝나면 저장하지 않고 게임을 끈다. 화면은 `tools/capture-game.ps1`로 게임 창만 찍는다(화면 전체 캡처 금지). 실제 마우스와 전경 창은 건드리지 않는다. 게임이 튕겼으면 `CrashReportClient.exe`를 끈 뒤 다시 켠다.
- git: 작업 브랜치 `feat/overlay`(`develop`에서 분기). 태스크마다 커밋한다. 끝나면 `develop`에 `--no-ff`로 병합하고 `develop`을 푸시한다. 커밋 메시지 끝에 `Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>`.
- 모든 git·파일 명령은 절대경로 또는 `git -C E:/MLToybox`를 쓴다(`cd` 뒤 상대경로 금지).
- 고치기 표기: "찾을 부분"은 파일에 정확히 한 번 나온다. 그 부분을 "바꿀 내용"으로 바꾼다.

## Review Focus

1. **저장이 실패한 순간**(모드가 `control.json`을 읽는 중이거나 다른 프로그램이 파일을 잡고 있음): 바꾼 값과 눌러 둔 일회성 명령이 사라지면 안 되고, 다음 시도에 누른 순서대로 실려야 한다. → Task 4 `overlay_session_failed_save_keeps_changes_and_commands`
2. **패널과 함께 쓸 때**: 패널이 방금 저장한 것을 오버레이가 옛 문서로 덮으면 안 되고(더 큰 `seq`로 써야 한다), 오버레이에 저장 대기 중인 변경을 패널 것으로 날려도 안 된다. → Task 3 `overlay_bridge_save_goes_past_a_newer_seq_in_the_file`, Task 4 `overlay_session_reloads_only_newer_files_and_never_over_pending_changes`
3. **손으로 고쳤거나 쓰다 만 파일**(`control.json`, `status.json`, `overlay.json`에 형식이 다른 값, Lua가 쓴 빈 표 `[]`, 잘린 내용): 기본값으로 읽고 오버레이가 죽지 않아야 한다. → Task 2 `overlay_control_broken_or_odd_input_falls_back_to_defaults`, Task 3 `overlay_status_treats_lua_empty_tables_as_empty_objects`와 `overlay_status_rejects_broken_text`, Task 4 `overlay_settings_reads_values_and_repairs_bad_ones`
4. **숫자 칸의 이상한 입력**(음수, 글자, 소수, int 범위를 넘는 수, 전각 숫자와 쉼표): 받아들일 수 없는 것은 버리고 이전 값을 유지해야 한다. → Task 1 `overlay_parse_number_rejects_everything_else`
5. **오버레이를 껐거나 DLL이 없는데 지난 실행의 `overlay_status.json`이 남은 경우**: 상태에 "ready"로 보이면 안 된다. → Task 5 `overlay_status_without_the_dll_ignores_a_leftover_file`

단위 테스트로 잡을 수 없는 것(게임에서만 확인): 화면 스레드와 창 스레드의 잠금 경합, 해상도 변경, 게임 시작 때의 후킹. Task 6의 게임 확인 단계가 맡는다.

## File Structure

| 파일 | 작업 | 책임 |
|---|---|---|
| `native/third_party/imgui/` | 생성 | Dear ImGui v1.92.9b (코어 + `backends/imgui_impl_dx12`, `imgui_impl_win32`, 라이선스) |
| `native/third_party/nlohmann/` | 생성 | nlohmann/json v3.12.0 (`json.hpp`, 라이선스) |
| `native/CMakeLists.txt` | 수정 | 타깃 `overlay_core`, `imgui`, `mltoybox_overlay`, 테스트 추가 |
| `native/overlay/core/text.h/.cpp` | 생성 | UTF-8 글자 수, 앞뒤 공백 떼기, ASCII 소문자 비교 |
| `native/overlay/core/number_input.h/.cpp` | 생성 | 숫자 칸 입력 해석 |
| `native/overlay/core/control_doc.h/.cpp` | 생성 | `control.json` 문서: 읽기(기본값, 옛 설정 옮기기), 고치기, 쓰기(모르는 키 보존) |
| `native/overlay/core/status_doc.h/.cpp` | 생성 | `status.json` 읽기 |
| `native/overlay/core/bridge.h/.cpp` | 생성 | 파일 경로, `control.json` 저장(seq), `status.json` 읽기, 연결 상태 판정 |
| `native/overlay/core/commands.h/.cpp` | 생성 | 일회성 명령 만들기 |
| `native/overlay/core/settings.h/.cpp` | 생성 | `overlay.json` |
| `native/overlay/core/status_file.h/.cpp` | 생성 | `overlay_status.json` |
| `native/overlay/core/session.h/.cpp` | 생성 | 나눠 쓰는 문서 상태와 저장·다시 읽기 규칙 |
| `native/overlay/render/dx12_hook.h/.cpp` | 생성 | 가상 함수 표 찾기, 후킹, 큐 선택, 백버퍼 관리, 프레임 |
| `native/overlay/render/imgui_layer.h/.cpp` | 생성 | ImGui 초기화, 글꼴, DX12 백엔드 |
| `native/overlay/render/input.h/.cpp` | 생성 | 창 프로시저 후킹, 토글 키, 입력 전달 |
| `native/overlay/render/guard.h/.cpp` | 생성 | 구조적 예외 보호, 잠금 추적 |
| `native/overlay/ui/app.h/.cpp` | 생성 | 스레드들이 나눠 쓰는 상태 |
| `native/overlay/ui/window.h/.cpp` | 생성 | 메인 창, 상태 줄, 탭 막대, 안내 |
| `native/overlay/ui/tabs.h` | 생성 | 탭 함수 선언, `TabContext` |
| `native/overlay/ui/widgets.h/.cpp` | 생성 | 숫자 칸, 안내 문구 |
| `native/overlay/ui/tab_status.cpp`, `tab_build.cpp`, `tab_lord.cpp` | 생성 | 상태 / 건설·업그레이드 / 영주 탭 |
| `native/overlay/worker.h/.cpp`, `dllmain.cpp` | 생성 | 작업 스레드, DLL 진입점 |
| `native/tests/overlay_*_tests.cpp`, `native/tests/fixtures/` | 생성 | `overlay_core` 테스트와 견본 |
| `mod/MLToybox/Scripts/core/native.lua` | 수정 | 파일 이름으로 DLL 올리기, `overlay_status.json` 합치기 |
| `mod/MLToybox/Scripts/config.lua`, `main.lua` | 수정 | `overlay = true`, 오버레이 DLL 올리기, `status.overlay` |
| `mod/MLToybox/tests/` | 생성·수정 | `native_spec`, `entrypoints_spec`, `overlay_fixture_spec`, 견본 |
| `tools/deploy.ps1`, `tools/tests/Tools.Tests.ps1` | 수정 | 오버레이 DLL 복사 |
| `tools/capture-game.ps1`, `tools/lab/resize.lua` | 생성 | 게임 창 캡처·키 메시지, 해상도 변경(개발용) |
| `README.md`, `docs/CHANGELOG.md`, `analysis/findings.md` | 수정 | 문서 |

---

### Task 1: 브랜치, 외부 라이브러리, `overlay_core` 뼈대 (글자·숫자 도우미)

**Files:**
- Create: `native/third_party/imgui/`, `native/third_party/nlohmann/`
- Create: `native/overlay/core/text.h`, `native/overlay/core/text.cpp`, `native/overlay/core/number_input.h`, `native/overlay/core/number_input.cpp`
- Create: `native/tests/overlay_text_tests.cpp`
- Modify: `native/CMakeLists.txt`

**Interfaces:**
- Consumes: 없음
- Produces:
  - `std::string mlt::ov::trim(std::string_view)`, `size_t codePointCount(std::string_view)`, `std::string lowerAscii(std::string_view)`, `bool equalsIgnoreCaseAscii(std::string_view, std::string_view)` (`overlay/core/text.h`)
  - `std::optional<int> mlt::ov::parseNumber(std::string_view)` (`overlay/core/number_input.h`)
  - CMake 타깃 `overlay_core`(정적 라이브러리, `mlt_core`에 연결, 포함 경로 `native/`와 `native/third_party`)

- [ ] **Step 1: 브랜치 준비**

스펙과 이 계획이 있는 `docs/overlay-design`을 `develop`에 병합하고 작업 브랜치를 만든다.

```bash
git -C E:/MLToybox checkout develop
git -C E:/MLToybox merge --no-ff docs/overlay-design -m "Merge branch 'docs/overlay-design' into develop" -m "Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
git -C E:/MLToybox push origin develop
git -C E:/MLToybox checkout -b feat/overlay
```

- [ ] **Step 2: 외부 라이브러리 가져오기**

```bash
tmp="$(mktemp -d)"
git clone --depth 1 --branch v1.92.9b https://github.com/ocornut/imgui.git "$tmp/imgui"
dst=E:/MLToybox/native/third_party/imgui
mkdir -p "$dst/backends"
cp "$tmp"/imgui/{imgui.cpp,imgui.h,imgui_draw.cpp,imgui_internal.h,imgui_tables.cpp,imgui_widgets.cpp,imconfig.h,imstb_rectpack.h,imstb_textedit.h,imstb_truetype.h,LICENSE.txt} "$dst/"
cp "$tmp"/imgui/backends/{imgui_impl_dx12.cpp,imgui_impl_dx12.h,imgui_impl_win32.cpp,imgui_impl_win32.h} "$dst/backends/"
printf 'v1.92.9b\nhttps://github.com/ocornut/imgui\n' > "$dst/VERSION.txt"
nl=E:/MLToybox/native/third_party/nlohmann
mkdir -p "$nl"
curl -fL -o "$nl/json.hpp" https://github.com/nlohmann/json/releases/download/v3.12.0/json.hpp
curl -fL -o "$nl/LICENSE.MIT" https://raw.githubusercontent.com/nlohmann/json/v3.12.0/LICENSE.MIT
printf 'v3.12.0\nhttps://github.com/nlohmann/json\n' > "$nl/VERSION.txt"
grep -n 'define IMGUI_VERSION ' "$dst/imgui.h"
sha256sum "$nl/json.hpp"
```

Expected: `#define IMGUI_VERSION       "1.92.9b"`, 그리고 `json.hpp`의 SHA-256이 `aaf127c04cb31c406e5b04a63f1ae89369fccde6d8fa7cdda1ed4f32dfc5de63`.

버전을 적는 파일 이름은 `VERSION.txt`여야 한다. 포함 경로에 `VERSION`이라는 파일이 있으면 Windows에서 C++20 표준 헤더 `<version>`을 가려 컴파일이 깨진다.

- [ ] **Step 3: 실패하는 테스트 작성**

<!-- file: native/tests/overlay_text_tests.cpp -->
```cpp
#include "test.h"
#include "overlay/core/number_input.h"
#include "overlay/core/text.h"

using namespace mlt::ov;

TEST(overlay_trim_removes_ascii_space_at_both_ends_only) {
    CHECK(trim("  \t토이박스 용병단\r\n ") == "토이박스 용병단");
    CHECK(trim("   ") == "");
    CHECK(trim("") == "");
    CHECK(trim("a  b") == "a  b");
}

TEST(overlay_code_point_count_counts_characters_not_bytes) {
    CHECK(codePointCount("") == 0);
    CHECK(codePointCount("abc") == 3);
    CHECK(codePointCount("검사대") == 3);                 // 9 바이트
    CHECK(codePointCount("\xF0\x9F\x98\x80" "a") == 2);    // 4 바이트 문자 하나 + a
    std::string forty;
    for (int i = 0; i < 40; ++i) forty += "가";
    CHECK(codePointCount(forty) == 40);
}

TEST(overlay_lower_ascii_leaves_other_bytes_alone) {
    CHECK(lowerAscii("Greencaps") == "greencaps");
    CHECK(lowerAscii("검사대 A") == "검사대 a");
    CHECK(equalsIgnoreCaseAscii("HILDEBOLTS_ARMY", "hildebolts_army"));
    CHECK(!equalsIgnoreCaseAscii("alpha", "alpha "));
}

TEST(overlay_parse_number_accepts_fullwidth_digits_spaces_and_commas) {
    CHECK(parseNumber("90000") == 90000);
    CHECK(parseNumber("９００００") == 90000);
    CHECK(parseNumber(" 1,000 ") == 1000);
    CHECK(parseNumber("１，０００") == 1000);
    CHECK(parseNumber("1　000") == 1000);                 // 전각 공백
    CHECK(parseNumber("0") == 0);
    CHECK(parseNumber("2147483647") == 2147483647);
}

TEST(overlay_parse_number_rejects_everything_else) {
    CHECK(!parseNumber(""));
    CHECK(!parseNumber("   "));
    CHECK(!parseNumber("-5"));
    CHECK(!parseNumber("12a"));
    CHECK(!parseNumber("1.5"));
    CHECK(!parseNumber("2147483648"));
    CHECK(!parseNumber("99999999999"));
    CHECK(!parseNumber("가"));
}
```

- [ ] **Step 4: CMake에 `overlay_core`와 테스트 추가**

<!-- edit: native/CMakeLists.txt -->
`native/CMakeLists.txt` (1/2) — 찾을 부분:

```cmake
add_library(mltoybox_native SHARED src/dllmain.cpp)
target_link_libraries(mltoybox_native PRIVATE mlt_core)
```

바꿀 내용:

```cmake
add_library(mltoybox_native SHARED src/dllmain.cpp)
target_link_libraries(mltoybox_native PRIVATE mlt_core)

# 게임 안 오버레이 (docs/superpowers/specs/2026-10-01-ingame-overlay-design.md)
# overlay_core: 문서 모델·브리지·규칙. Windows 그래픽에 의존하지 않아 단위 테스트한다
add_library(overlay_core STATIC
    overlay/core/text.cpp
    overlay/core/number_input.cpp)
target_include_directories(overlay_core PUBLIC . third_party)
target_link_libraries(overlay_core PUBLIC mlt_core)
```

<!-- edit: native/CMakeLists.txt -->
`native/CMakeLists.txt` (2/2) — 찾을 부분:

```cmake
tests/pin_tests.cpp tests/militia_guard_tests.cpp)
target_link_libraries(native_tests PRIVATE mlt_core)
```

바꿀 내용:

```cmake
tests/pin_tests.cpp tests/militia_guard_tests.cpp
    tests/overlay_text_tests.cpp)
target_link_libraries(native_tests PRIVATE mlt_core overlay_core)
```

- [ ] **Step 5: 빌드해서 실패 확인**

Run: `pwsh E:/MLToybox/tools/build-native.ps1 -Test`
Expected: CMake 구성 단계에서 실패한다. `Cannot find source file: overlay/core/text.cpp`

- [ ] **Step 6: 구현**

<!-- file: native/overlay/core/text.h -->
```cpp
#pragma once
#include <string>
#include <string_view>

// UTF-8 문자열 도우미. 모드(Lua)와 같은 기준을 쓴다: 공백은 ASCII 공백, 대소문자 변환은 ASCII 만, 길이는 코드 포인트 수
namespace mlt::ov {
std::string trim(std::string_view s);
size_t codePointCount(std::string_view utf8);
std::string lowerAscii(std::string_view s);
bool equalsIgnoreCaseAscii(std::string_view a, std::string_view b);
}
```

<!-- file: native/overlay/core/text.cpp -->
```cpp
#include "text.h"

namespace mlt::ov {

static bool isSpace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}

std::string trim(std::string_view s) {
    size_t b = 0, e = s.size();
    while (b < e && isSpace(s[b])) ++b;
    while (e > b && isSpace(s[e - 1])) --e;
    return std::string(s.substr(b, e - b));
}

// 이어지는 바이트(10xxxxxx)가 아닌 바이트를 센다. 깨진 UTF-8 도 멈추지 않고 센다
size_t codePointCount(std::string_view utf8) {
    size_t n = 0;
    for (unsigned char c : utf8) {
        if ((c & 0xC0) != 0x80) ++n;
    }
    return n;
}

std::string lowerAscii(std::string_view s) {
    std::string out(s);
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return out;
}

bool equalsIgnoreCaseAscii(std::string_view a, std::string_view b) {
    return lowerAscii(a) == lowerAscii(b);
}

}
```

<!-- file: native/overlay/core/number_input.h -->
```cpp
#pragma once
#include <optional>
#include <string_view>

namespace mlt::ov {
// 숫자 칸 입력 해석: 전각 숫자(０-９), 공백, 쉼표(, ，)를 허용한다. 0 이상의 int 가 아니면 값 없음 (패널의 NumberInput.Parse 와 같다)
std::optional<int> parseNumber(std::string_view utf8);
}
```

<!-- file: native/overlay/core/number_input.cpp -->
```cpp
#include "number_input.h"
#include <string>

namespace mlt::ov {

std::optional<int> parseNumber(std::string_view s) {
    std::string digits;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c >= '0' && c <= '9') { digits.push_back(static_cast<char>(c)); ++i; continue; }
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f' || c == ',') { ++i; continue; }
        if (c == 0xEF && i + 2 < s.size() && static_cast<unsigned char>(s[i + 1]) == 0xBC) {
            unsigned char t = static_cast<unsigned char>(s[i + 2]);
            if (t >= 0x90 && t <= 0x99) { digits.push_back(static_cast<char>('0' + (t - 0x90))); i += 3; continue; }   // ０-９
            if (t == 0x8C) { i += 3; continue; }                                                                       // ，
        }
        if (c == 0xE3 && i + 2 < s.size() && static_cast<unsigned char>(s[i + 1]) == 0x80 && static_cast<unsigned char>(s[i + 2]) == 0x80) { i += 3; continue; }   // 전각 공백
        return std::nullopt;
    }
    if (digits.empty() || digits.size() > 10) return std::nullopt;
    long long v = 0;
    for (char d : digits) v = v * 10 + (d - '0');
    if (v > 2147483647LL) return std::nullopt;
    return static_cast<int>(v);
}

}
```

- [ ] **Step 7: 빌드와 테스트 통과 확인**

Run: `pwsh E:/MLToybox/tools/build-native.ps1 -Test`
Expected: `100% tests passed, 0 tests failed out of 1`

Run: `& E:\MLToybox\native\build\native_tests.exe | Select-Object -Last 1`
Expected: `45 tests, 0 failed`

- [ ] **Step 8: 커밋**

```bash
git -C E:/MLToybox add native/third_party/imgui native/third_party/nlohmann native/overlay native/tests native/CMakeLists.txt
git -C E:/MLToybox commit -m "feat(overlay): vendor Dear ImGui and nlohmann/json; core text and number helpers" -m "Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 2: `control_doc` — 설정 문서

패널의 `ControlDocument.cs`와 같은 기본값으로 `control.json`을 읽고 쓴다. JSON을 그대로 들고 있어 아직 구조체가 없는 기능(군사, 용병 등)과 모르는 키를 보존한다. 이 계획에서는 건설, 업그레이드, 영주만 구조체로 다룬다.

**Files:**
- Create: `native/overlay/core/control_doc.h`, `native/overlay/core/control_doc.cpp`
- Create: `native/tests/overlay_control_doc_tests.cpp`, `native/tests/fixtures/control_from_panel.json`
- Modify: `native/CMakeLists.txt`

**Interfaces:**
- Consumes: `<nlohmann/json.hpp>` (Task 1)
- Produces (`overlay/core/control_doc.h`, 이름공간 `mlt::ov`):
  - `using Json = nlohmann::ordered_json;`
  - `struct BuildSettings { bool enabled, ignorePlacement, instantBuild, instantRepair, noMaterials, noRegionLimit; }`
  - `struct UpgradeSettings { bool enabled; }`
  - `struct LordSettings { bool enabled; int intervalSec; std::optional<int> treasury, influence, kingsFavour; }`
  - `class ControlDoc`: `static ControlDoc parse(std::string_view)`, `std::string dump() const`, `long long seq() const`, `void setSeq(long long)`, `build()/setBuild()`, `upgrade()/setUpgrade()`, `lord()/setLord()`, `void setCommands(const std::vector<Json>&)`, `Json& raw()`, `Json& feature(const char*)`, `const Json* findFeature(const char*) const`
  - 읽기 도우미: `bool boolOr(const Json*, const char*, bool)`, `int intOr(const Json*, const char*, int)`, `std::optional<int> optInt(const Json*, const char*)`, `const Json* objectAt(const Json*, const char*)`
  - 테스트용 매크로 `MLT_FIXTURES_DIR`, `MLT_LUA_FIXTURES_DIR`(CMake가 정의한다)

- [ ] **Step 1: 견본 파일 작성**

패널의 직렬화기가 실제로 쓴 `control.json`이다(2026-10-01, `seq` 132). 오버레이 코어가 패널이 쓴 파일을 읽을 수 있는지 본다(스펙 7.2).

<!-- file: native/tests/fixtures/control_from_panel.json -->
```json
{
  "version": 1,
  "seq": 132,
  "features": {
    "resources": {
      "enabled": true,
      "intervalSec": 2,
      "targets": {
        "Ale": 500,
        "Barley": 500,
        "Beer": 500,
        "Berries": 500,
        "Candle": 500,
        "Charcoal": 500,
        "Clay": 500,
        "Cloth_Linen": 500,
        "Clothes": 500,
        "DressedStone": 500,
        "Eggs": 500,
        "Firewood": 500,
        "Flax": 500,
        "Hides": 500,
        "Honey": 500,
        "Hops": 500,
        "IronOre": 500,
        "IronSlabs": 500,
        "Irontools": 500,
        "Leather": 500,
        "Malt": 500,
        "Pastries": 500,
        "Pelts": 500,
        "PlateArmor": 500,
        "RegionalWealth": 500,
        "RoughStone": 500,
        "RyeBread": 500,
        "RyeFlour": 500,
        "RyeGrain": 500,
        "Salt": 500,
        "Shoes": 500,
        "Timber": 500,
        "Wax": 500,
        "WheatBread": 500,
        "WheatFlour": 500,
        "WheatGrain": 500,
        "Wool": 500,
        "Yarn": 500,
        "apples": 500,
        "clayTILES": 500,
        "crossbows": 500,
        "dyes": 500,
        "fish": 500,
        "gambesons": 500,
        "mail_armor": 500,
        "meat": 500,
        "militia_helmets_resource": 500,
        "mushrooms": 500,
        "planks": 500,
        "shields_large": 500,
        "shields_small": 500,
        "spears": 500,
        "vegetables": 500,
        "warbows": 500,
        "weapons_polearms": 500,
        "weapons_sidearms": 500
      },
      "regionTargets": {
        "sel": {
          "Ale": 1000,
          "Barley": 1000,
          "Beer": 1000,
          "Berries": 1000,
          "Candle": 1000,
          "Charcoal": 1000,
          "Clay": 1000,
          "Cloth_Linen": 1000,
          "Clothes": 1000,
          "DressedStone": 1000,
          "Eggs": 1000,
          "Firewood": 1000,
          "Flax": 1000,
          "Hides": 1000,
          "Honey": 1000,
          "Hops": 1000,
          "IronOre": 1000,
          "IronSlabs": 1000,
          "Irontools": 1000,
          "Leather": 1000,
          "Malt": 1000,
          "Pastries": 1000,
          "Pelts": 1000,
          "PlateArmor": 1000,
          "RegionalWealth": 1000,
          "RoughStone": 1000,
          "RyeBread": 1000,
          "RyeFlour": 1000,
          "RyeGrain": 1000,
          "Salt": 1000,
          "Shoes": 1000,
          "Timber": 1000,
          "Wax": 1000,
          "WheatBread": 1000,
          "WheatFlour": 1000,
          "WheatGrain": 1000,
          "Wool": 1000,
          "Yarn": 1000,
          "apples": 1000,
          "clayTILES": 1000,
          "crossbows": 1000,
          "dyes": 1000,
          "fish": 1000,
          "gambesons": 1000,
          "mail_armor": 1000,
          "meat": 1000,
          "militia_helmets_resource": 1000,
          "mushrooms": 1000,
          "planks": 1000,
          "shields_large": 1000,
          "shields_small": 1000,
          "spears": 1000,
          "vegetables": 1000,
          "warbows": 1000,
          "weapons_polearms": 1000,
          "weapons_sidearms": 1000
        },
        "hof": {
          "Ale": 600,
          "Barley": 600,
          "Beer": 600,
          "Berries": 600,
          "Candle": 600,
          "Charcoal": 600,
          "Clay": 600,
          "Cloth_Linen": 600,
          "Clothes": 600,
          "DressedStone": 600,
          "Eggs": 600,
          "Firewood": 600,
          "Flax": 600,
          "Hides": 600,
          "Honey": 600,
          "Hops": 600,
          "IronOre": 600,
          "IronSlabs": 600,
          "Irontools": 600,
          "Leather": 600,
          "Malt": 600,
          "Pastries": 600,
          "Pelts": 600,
          "PlateArmor": 600,
          "RegionalWealth": 600,
          "RoughStone": 600,
          "RyeBread": 600,
          "RyeFlour": 600,
          "RyeGrain": 600,
          "Salt": 600,
          "Shoes": 600,
          "Timber": 600,
          "Wax": 600,
          "WheatBread": 600,
          "WheatFlour": 600,
          "WheatGrain": 600,
          "Wool": 600,
          "Yarn": 600,
          "apples": 600,
          "clayTILES": 600,
          "crossbows": 600,
          "dyes": 600,
          "fish": 600,
          "gambesons": 600,
          "mail_armor": 600,
          "meat": 600,
          "militia_helmets_resource": 600,
          "mushrooms": 600,
          "planks": 600,
          "shields_large": 600,
          "shields_small": 600,
          "spears": 600,
          "vegetables": 600,
          "warbows": 600,
          "weapons_polearms": 600,
          "weapons_sidearms": 600
        }
      }
    },
    "lord": {
      "enabled": false,
      "intervalSec": 2,
      "treasury": 150000,
      "influence": 20000,
      "kingsFavour": 50000
    },
    "build": {
      "enabled": true,
      "ignorePlacement": true,
      "instantBuild": true,
      "instantRepair": true,
      "noMaterials": true,
      "noRegionLimit": true
    },
    "upgrade": {
      "enabled": true
    },
    "military": {
      "enabled": true,
      "ignoreEquipment": true,
      "ignorePopulation": true,
      "zeroUpkeep": true,
      "unlimitedSquads": true
    },
    "mercenaries": {
      "enabled": true,
      "refund": true,
      "lockFromAi": true,
      "companies": [
        {
          "name": "\uAC80\uC0AC\uB300",
          "units": [
            "mercenary_infantry",
            "mercenary_infantry",
            "mercenary_infantry",
            "mercenary_infantry"
          ],
          "cost": 1000,
          "region": "nus",
          "banner": "battle_brothers",
          "enabled": true
        }
      ]
    },
    "population": {
      "enabled": true,
      "multiplier": 1,
      "targetFamilies": 0,
      "regionTargets": {}
    }
  },
  "commands": []
}
```

- [ ] **Step 2: 실패하는 테스트 작성**

<!-- file: native/tests/overlay_control_doc_tests.cpp -->
```cpp
#include "test.h"
#include "overlay/core/control_doc.h"
#include "runtime.h"
#include <filesystem>

using namespace mlt::ov;

static std::string fixture(const char* name) {
    auto text = mlt::readFileUtf8(std::filesystem::path(MLT_FIXTURES_DIR) / name);
    CHECK(text.has_value());
    return *text;
}

TEST(overlay_control_empty_doc_has_panel_defaults) {
    ControlDoc doc;
    CHECK(doc.seq() == 0);
    auto b = doc.build();
    CHECK(!b.enabled && b.ignorePlacement && b.instantBuild && b.instantRepair && b.noMaterials && b.noRegionLimit);
    CHECK(!doc.upgrade().enabled);
    auto l = doc.lord();
    CHECK(!l.enabled && l.intervalSec == 2 && !l.treasury && !l.influence && !l.kingsFavour);
    auto j = Json::parse(doc.dump());
    CHECK(j["version"] == 1 && j["seq"] == 0 && j["features"].is_object() && j["commands"].is_array());
}

TEST(overlay_control_reads_a_file_written_by_the_panel) {
    auto doc = ControlDoc::parse(fixture("control_from_panel.json"));
    CHECK(doc.seq() == 132);
    CHECK(doc.build().enabled && doc.build().noRegionLimit);
    CHECK(doc.upgrade().enabled);
    auto l = doc.lord();
    CHECK(!l.enabled && l.intervalSec == 2 && l.treasury == 150000 && l.influence == 20000 && l.kingsFavour == 50000);
    // 아직 구조체가 없는 기능도 그대로 들고 있다
    CHECK(doc.raw()["features"]["military"]["unlimitedSquads"] == true);
    CHECK(doc.raw()["features"]["mercenaries"]["companies"][0]["name"] == "검사대");
}

TEST(overlay_control_writes_keep_unknown_keys_and_other_features) {
    auto doc = ControlDoc::parse(R"({"version":1,"seq":5,"extra":"keep","features":{"build":{"enabled":false,"future":7},"military":{"enabled":true}},"commands":[]})");
    auto b = doc.build();
    b.enabled = true;
    b.instantRepair = false;
    doc.setBuild(b);
    auto j = Json::parse(doc.dump());
    CHECK(j["extra"] == "keep");
    CHECK(j["features"]["build"]["future"] == 7);
    CHECK(j["features"]["build"]["enabled"] == true);
    CHECK(j["features"]["build"]["instantRepair"] == false);
    CHECK(j["features"]["build"]["instantBuild"] == true);       // 빠져 있던 키는 기본값으로 채워 쓴다
    CHECK(j["features"]["military"]["enabled"] == true);
    CHECK(j["seq"] == 5);
}

TEST(overlay_control_lord_keys_are_written_only_when_managed) {
    ControlDoc doc;
    LordSettings l;
    l.enabled = true;
    l.treasury = 150000;
    doc.setLord(l);
    auto j = Json::parse(doc.dump());
    CHECK(j["features"]["lord"]["treasury"] == 150000);
    CHECK(!j["features"]["lord"].contains("influence"));
    CHECK(!j["features"]["lord"].contains("kingsFavour"));
    l.treasury.reset();
    l.kingsFavour = 0;
    doc.setLord(l);
    j = Json::parse(doc.dump());
    CHECK(!j["features"]["lord"].contains("treasury"));
    CHECK(j["features"]["lord"]["kingsFavour"] == 0);
    CHECK(doc.lord().kingsFavour == 0 && !doc.lord().treasury);
}

TEST(overlay_control_moves_old_treasury_and_influence_targets_to_lord) {
    auto doc = ControlDoc::parse(R"({"version":1,"seq":3,"features":{"resources":{"enabled":true,"targets":{"Treasury":90000,"Influence":500,"Timber":50}}}})");
    auto l = doc.lord();
    CHECK(l.enabled && l.treasury == 90000 && l.influence == 500 && !l.kingsFavour);
    const Json& targets = doc.raw()["features"]["resources"]["targets"];
    CHECK(!targets.contains("Treasury") && !targets.contains("Influence") && targets["Timber"] == 50);
}

TEST(overlay_control_existing_lord_section_wins_over_old_targets) {
    auto doc = ControlDoc::parse(R"({"version":1,"seq":3,"features":{"lord":{"enabled":false},"resources":{"enabled":true,"targets":{"Treasury":90000}}}})");
    CHECK(!doc.lord().enabled && !doc.lord().treasury);
    CHECK(!doc.raw()["features"]["resources"]["targets"].contains("Treasury"));
}

TEST(overlay_control_broken_or_odd_input_falls_back_to_defaults) {
    CHECK(ControlDoc::parse("").seq() == 0);
    CHECK(ControlDoc::parse("{").seq() == 0);
    CHECK(ControlDoc::parse("[1,2]").seq() == 0);
    auto doc = ControlDoc::parse(R"({"version":1,"seq":"x","features":[],"commands":{}})");
    CHECK(doc.seq() == 0);
    CHECK(doc.build().instantBuild);
    doc = ControlDoc::parse(R"({"version":1,"seq":2,"features":{"build":{"enabled":"yes","instantBuild":0},"lord":{"treasury":"many","intervalSec":2.0}}})");
    CHECK(!doc.build().enabled && doc.build().instantBuild);     // 형식이 다르면 기본값
    CHECK(!doc.lord().treasury && doc.lord().intervalSec == 2);
}

TEST(overlay_control_commands_are_replaced_as_a_whole) {
    ControlDoc doc;
    Json c = Json::object();
    c["id"] = "abc";
    c["type"] = "setLord";
    doc.setCommands({ c });
    CHECK(Json::parse(doc.dump())["commands"].size() == 1);
    doc.setCommands({});
    CHECK(Json::parse(doc.dump())["commands"].empty());
}

TEST(overlay_control_keeps_korean_text_as_utf8) {
    auto doc = ControlDoc::parse(fixture("control_from_panel.json"));
    std::string text = doc.dump();
    CHECK(text.find("검사대") != std::string::npos);
    CHECK(ControlDoc::parse(text).raw()["features"]["mercenaries"]["companies"][0]["name"] == "검사대");
}
```

- [ ] **Step 3: CMake에 추가**

<!-- edit: native/CMakeLists.txt -->
`native/CMakeLists.txt` (1/2) — 찾을 부분:

```cmake
    overlay/core/number_input.cpp)
```

바꿀 내용:

```cmake
    overlay/core/number_input.cpp
    overlay/core/control_doc.cpp)
```

<!-- edit: native/CMakeLists.txt -->
`native/CMakeLists.txt` (2/2) — 찾을 부분:

```cmake
    tests/overlay_text_tests.cpp)
target_link_libraries(native_tests PRIVATE mlt_core overlay_core)
```

바꿀 내용:

```cmake
    tests/overlay_text_tests.cpp
    tests/overlay_control_doc_tests.cpp)
target_link_libraries(native_tests PRIVATE mlt_core overlay_core)
# 테스트가 읽는 견본 파일의 위치
target_compile_definitions(native_tests PRIVATE
    MLT_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures"
    MLT_LUA_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../mod/MLToybox/tests/fixtures")
```

- [ ] **Step 4: 빌드해서 실패 확인**

Run: `pwsh E:/MLToybox/tools/build-native.ps1 -Test`
Expected: `Cannot find source file: overlay/core/control_doc.cpp`

- [ ] **Step 5: 구현**

<!-- file: native/overlay/core/control_doc.h -->
```cpp
#pragma once
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

class ControlDoc {
public:
    ControlDoc();
    // 깨졌거나 객체가 아니면 빈 문서. 옛 설정(자원 목표에 든 국고·영향력)을 영주 설정으로 옮긴다
    static ControlDoc parse(std::string_view text);
    std::string dump() const;

    long long seq() const;
    void setSeq(long long seq);

    BuildSettings build() const;
    void setBuild(const BuildSettings& v);
    UpgradeSettings upgrade() const;
    void setUpgrade(const UpgradeSettings& v);
    LordSettings lord() const;
    void setLord(const LordSettings& v);

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
const Json* objectAt(const Json* obj, const char* key);
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

bool boolOr(const Json* obj, const char* key, bool def) {
    if (!obj || !obj->is_object()) return def;
    auto it = obj->find(key);
    return (it != obj->end() && it->is_boolean()) ? it->get<bool>() : def;
}

std::optional<int> optInt(const Json* obj, const char* key) {
    if (!obj || !obj->is_object()) return std::nullopt;
    auto it = obj->find(key);
    if (it == obj->end()) return std::nullopt;
    auto n = asInteger(*it);
    if (!n || *n < -2147483648LL || *n > 2147483647LL) return std::nullopt;
    return static_cast<int>(*n);
}

int intOr(const Json* obj, const char* key, int def) {
    auto v = optInt(obj, key);
    return v ? *v : def;
}

const Json* objectAt(const Json* obj, const char* key) {
    if (!obj || !obj->is_object()) return nullptr;
    auto it = obj->find(key);
    return (it != obj->end() && it->is_object()) ? &*it : nullptr;
}

ControlDoc::ControlDoc() {
    root_ = Json::object();
    root_["version"] = 1;
    root_["seq"] = 0;
    root_["features"] = Json::object();
    root_["commands"] = Json::array();
}

ControlDoc ControlDoc::parse(std::string_view text) {
    ControlDoc doc;
    Json parsed = Json::parse(text.begin(), text.end(), nullptr, false);
    if (parsed.is_discarded() || !parsed.is_object()) return doc;
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
    return root_.dump(2);
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

void ControlDoc::setLord(const LordSettings& v) {
    Json& f = feature("lord");
    f["enabled"] = v.enabled;
    f["intervalSec"] = v.intervalSec;
    setOptional(f, "treasury", v.treasury);
    setOptional(f, "influence", v.influence);
    setOptional(f, "kingsFavour", v.kingsFavour);
}

void ControlDoc::setCommands(const std::vector<Json>& commands) {
    Json list = Json::array();
    for (const auto& c : commands) list.push_back(c);
    root_["commands"] = std::move(list);
}

}
```

`parse`에서 영주 설정을 옮길 때는 값을 먼저 읽고 자원 목표에서 지운 뒤에 `setLord`를 부른다. 기능 객체를 새로 넣으면 `ordered_json`이 저장소를 다시 잡아 다른 기능을 가리키던 참조가 무효가 되기 때문이다.

- [ ] **Step 6: 빌드와 테스트 통과 확인**

Run: `pwsh E:/MLToybox/tools/build-native.ps1 -Test`
Expected: `100% tests passed, 0 tests failed out of 1`

Run: `& E:\MLToybox\native\build\native_tests.exe | Select-Object -Last 1`
Expected: `54 tests, 0 failed`

- [ ] **Step 7: 커밋**

```bash
git -C E:/MLToybox add native/overlay native/tests native/CMakeLists.txt
git -C E:/MLToybox commit -m "feat(overlay): control document model (build, upgrade, lord) that keeps unknown keys" -m "Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 3: `status_doc`, `bridge`, `commands`와 프로토콜 견본

**Files:**
- Create: `native/overlay/core/status_doc.h/.cpp`, `native/overlay/core/bridge.h/.cpp`, `native/overlay/core/commands.h/.cpp`
- Create: `native/tests/overlay_status_doc_tests.cpp`, `native/tests/overlay_bridge_tests.cpp`, `native/tests/fixtures/status_from_mod.json`
- Create: `mod/MLToybox/tests/fixtures/control_from_overlay.json`(테스트가 만든다), `mod/MLToybox/tests/overlay_fixture_spec.lua`
- Modify: `native/CMakeLists.txt`

**Interfaces:**
- Consumes: `ControlDoc`, `Json`, `boolOr`, `intOr`, `optInt`, `objectAt` (Task 2). `mlt_core`의 `std::optional<std::string> mlt::readFileUtf8(const std::filesystem::path&)`, `bool mlt::writeFileAtomic(const std::filesystem::path&, std::string_view)`(임시 파일에 쓰고 이름을 바꾼다) (`native/src/runtime.h`)
- Produces (이름공간 `mlt::ov`):
  - `overlay/core/status_doc.h`: `struct FeatureStatus`, `NativeFeature`, `NativeStatus`, `CommandResult`, `LordStatus`, `StatusDoc { heartbeat, inGame, appliedSeq, bridgeError, features, commands, native, lord, raw }`, `std::optional<StatusDoc> parseStatus(std::string_view)`, `optString`, `optNumber`
  - `overlay/core/bridge.h`: `enum class BridgeState { Disconnected, MainMenu, Pending, Applied }`, `class Bridge`: `controlPath()`, `statusPath()`, `settingsPath()`, `overlayStatusPath()`, `ControlDoc loadControl() const`, `std::optional<long long> peekSeq() const`, `std::optional<long long> saveControl(ControlDoc&) const`, `std::optional<StatusDoc> readStatus() const`, `static BridgeState evaluate(const StatusDoc*, long long lastSentSeq, long long now)`
  - `overlay/core/commands.h`: `std::string newCommandId()`, `Json makeSetLord(const std::string& key, int value, long long now)`

- [ ] **Step 1: 견본 파일 작성**

모드가 실제로 쓴 `status.json`이다(2026-10-01, `saveGame_8`, 한 줄).

<!-- file: native/tests/fixtures/status_from_mod.json -->
```json
{"playerRegions":[{"name":"Mandlach","key":"gold"},{"name":"Trummer Höhe","key":"Lei"},{"name":"Haderwand","key":"nus"}],"lord":{"influence":27910,"kingsFavour":50000,"treasury":148500},"resourceIds":["RegionalWealth","Timber","planks","Firewood","Charcoal","RoughStone","DressedStone","Clay","clayTILES","IronOre","IronSlabs","Salt","WheatGrain","WheatFlour","WheatBread","RyeGrain","RyeFlour","RyeBread","Berries","mushrooms","meat","fish","Eggs","vegetables","apples","Honey","Pastries","Barley","Malt","Ale","Hops","Beer","Hides","Leather","Pelts","Shoes","Flax","Cloth_Linen","Wool","Yarn","Clothes","dyes","Wax","Candle","Irontools","spears","weapons_sidearms","weapons_polearms","warbows","crossbows","shields_small","shields_large","militia_helmets_resource","gambesons","mail_armor","PlateArmor"],"heartbeat":1790833555,"inGame":true,"mercenaries":{"refunded":0,"hiredMine":1,"hiredAi":1,"skipped":[],"slots":[{"cost":10000000,"name":"검사대","custom":true},{"cost":90,"name":"greencaps","custom":false},{"cost":45,"name":"wayward_sons","custom":false}]},"retinue":{"squads":[]},"resources":{"Clay":1500,"Yarn":1500,"Beer":1500,"WheatFlour":1500,"crossbows":1500,"WheatGrain":1500,"RegionalWealth":2635,"warbows":1500,"weapons_sidearms":1500,"dyes":1500,"Irontools":1560,"Charcoal":1773,"WheatBread":1601,"Ale":1500,"Hides":1500,"Wool":1500,"PlateArmor":1500,"mail_armor":1500,"Pelts":1500,"militia_helmets_resource":1500,"DressedStone":1500,"Flax":1500,"meat":1500,"fish":1526,"Firewood":1748,"shields_large":1520,"RyeBread":1500,"RoughStone":1500,"weapons_polearms":1500,"spears":1520,"Barley":1500,"RyeGrain":1500,"Candle":1500,"planks":2115,"apples":1500,"IronSlabs":1573,"Pastries":1500,"RyeFlour":1500,"Cloth_Linen":1500,"Shoes":1500,"Malt":1500,"gambesons":1500,"Eggs":1500,"Timber":1849,"clayTILES":1500,"IronOre":1500,"Honey":1500,"mushrooms":1624,"Berries":1500,"Wax":1500,"Salt":1636,"Leather":1500,"vegetables":1500,"shields_small":1500,"Clothes":1500,"Hops":1500},"spawn":{"disbanded":0,"pending":0,"byUnit":[]},"native":{"stale":false,"loaded":true,"heartbeat":1790833555,"features":{"militia_guard":{"installed":true,"active":true},"militia_guard_2":{"installed":true,"active":true},"instant_build":{"installed":true,"active":true},"placement":{"installed":true,"active":true}}},"regions":[{"values":{"Clay":500,"Yarn":500,"Beer":500,"WheatFlour":500,"crossbows":500,"WheatGrain":500,"RegionalWealth":788,"warbows":500,"weapons_sidearms":500,"dyes":500,"Irontools":530,"Charcoal":500,"WheatBread":541,"Ale":500,"Hides":500,"Wool":500,"PlateArmor":500,"mail_armor":500,"Pelts":500,"militia_helmets_resource":500,"DressedStone":500,"Flax":500,"meat":500,"fish":500,"Firewood":688,"shields_large":500,"RyeBread":500,"RoughStone":500,"weapons_polearms":500,"spears":500,"Barley":500,"RyeGrain":500,"Candle":500,"planks":500,"apples":500,"IronSlabs":500,"Pastries":500,"RyeFlour":500,"Cloth_Linen":500,"Shoes":500,"Malt":500,"gambesons":500,"Eggs":500,"Timber":722,"clayTILES":500,"IronOre":500,"Honey":500,"mushrooms":500,"Berries":500,"Wax":500,"Salt":500,"Leather":500,"vegetables":500,"shields_small":500,"Clothes":500,"Hops":500},"key":"gold","name":"Mandlach"},{"values":{"Clay":500,"Yarn":500,"Beer":500,"WheatFlour":500,"crossbows":500,"WheatGrain":500,"RegionalWealth":650,"warbows":500,"weapons_sidearms":500,"dyes":500,"Irontools":530,"Charcoal":500,"WheatBread":560,"Ale":500,"Hides":500,"Wool":500,"PlateArmor":500,"mail_armor":500,"Pelts":500,"militia_helmets_resource":500,"DressedStone":500,"Flax":500,"meat":500,"fish":500,"Firewood":560,"shields_large":500,"RyeBread":500,"RoughStone":500,"weapons_polearms":500,"spears":500,"Barley":500,"RyeGrain":500,"Candle":500,"planks":500,"apples":500,"IronSlabs":500,"Pastries":500,"RyeFlour":500,"Cloth_Linen":500,"Shoes":500,"Malt":500,"gambesons":500,"Eggs":500,"Timber":524,"clayTILES":500,"IronOre":500,"Honey":500,"mushrooms":500,"Berries":500,"Wax":500,"Salt":500,"Leather":500,"vegetables":500,"shields_small":500,"Clothes":500,"Hops":500},"key":"Lei","name":"Trummer Höhe"},{"values":{"Clay":500,"Yarn":500,"Beer":500,"WheatFlour":500,"crossbows":500,"WheatGrain":500,"RegionalWealth":1197,"warbows":500,"weapons_sidearms":500,"dyes":500,"Irontools":500,"Charcoal":773,"WheatBread":500,"Ale":500,"Hides":500,"Wool":500,"PlateArmor":500,"mail_armor":500,"Pelts":500,"militia_helmets_resource":500,"DressedStone":500,"Flax":500,"meat":500,"fish":526,"Firewood":500,"shields_large":520,"RyeBread":500,"RoughStone":500,"weapons_polearms":500,"spears":520,"Barley":500,"RyeGrain":500,"Candle":500,"planks":1115,"apples":500,"IronSlabs":573,"Pastries":500,"RyeFlour":500,"Cloth_Linen":500,"Shoes":500,"Malt":500,"gambesons":500,"Eggs":500,"Timber":603,"clayTILES":500,"IronOre":500,"Honey":500,"mushrooms":624,"Berries":500,"Wax":500,"Salt":636,"Leather":500,"vegetables":500,"shields_small":500,"Clothes":500,"Hops":500},"key":"nus","name":"Haderwand"}],"population":{"regions":[{"key":"gold","unassigned":14,"families":44,"freeSlots":16,"name":"Mandlach","homeless":0,"population":137},{"key":"Lei","unassigned":5,"families":5,"freeSlots":15,"name":"Trummer Höhe","homeless":0,"population":20},{"key":"nus","unassigned":48,"families":108,"freeSlots":27,"name":"Haderwand","homeless":0,"population":329}],"multiplied":0,"unassigned":67,"families":157,"freeSlots":58,"population":486,"natural":2,"homeless":0},"appliedSeq":132,"features":{"resources":{"active":true},"military":{"active":true},"upgrade":{"active":true},"lord":{"active":false},"population":{"active":true},"mercenaries":{"active":true},"build":{"active":true}},"version":1}
```

- [ ] **Step 2: 실패하는 테스트 작성**

<!-- file: native/tests/overlay_status_doc_tests.cpp -->
```cpp
#include "test.h"
#include "overlay/core/status_doc.h"
#include "runtime.h"
#include <filesystem>

using namespace mlt::ov;

TEST(overlay_status_reads_a_file_written_by_the_mod) {
    auto text = mlt::readFileUtf8(std::filesystem::path(MLT_FIXTURES_DIR) / "status_from_mod.json");
    CHECK(text.has_value());
    auto s = parseStatus(*text);
    CHECK(s.has_value());
    CHECK(s->heartbeat == 1790833555 && s->inGame && s->appliedSeq == 132 && !s->bridgeError);
    CHECK(s->features.has_value() && s->features->size() == 7);
    CHECK(s->features->at("build").active && !s->features->at("lord").active);
    CHECK(s->lord.has_value() && s->lord->treasury == 148500.0 && s->lord->influence == 27910 && s->lord->kingsFavour == 50000);
    CHECK(s->native.has_value() && s->native->loaded && !s->native->stale);
    CHECK(s->native->features.at("instant_build").installed && s->native->features.at("instant_build").active);
    CHECK(!s->commands.has_value());
    CHECK(s->raw["spawn"]["disbanded"] == 0);
}

TEST(overlay_status_reads_command_results_in_file_order) {
    auto s = parseStatus(R"({"version":1,"heartbeat":10,"inGame":true,"commands":{
        "zz01":{"ok":true,"squads":[63,64]},
        "aa02":{"ok":false,"error":"not in game"},
        "mm03":{"ok":true,"added":2,"requested":3},
        "kk04":{"ok":true,"reformed":4,"squads":[]}}})");
    CHECK(s.has_value() && s->commands.has_value() && s->commands->size() == 4);
    const auto& c = *s->commands;
    CHECK(c[0].first == "zz01" && c[0].second.ok && c[0].second.squads->size() == 2 && (*c[0].second.squads)[1] == 64);
    CHECK(c[1].first == "aa02" && !c[1].second.ok && c[1].second.error == "not in game" && !c[1].second.squads);
    CHECK(c[2].second.added == 2 && c[2].second.requested == 3);
    CHECK(c[3].second.reformed == 4 && c[3].second.squads->empty());
}

TEST(overlay_status_treats_lua_empty_tables_as_empty_objects) {
    auto s = parseStatus(R"({"version":1,"heartbeat":10,"inGame":false,"features":[],"commands":[],"native":{"loaded":false,"error":"not deployed","stale":true,"features":[]}})");
    CHECK(s.has_value());
    CHECK(s->features.has_value() && s->features->empty());
    CHECK(!s->commands.has_value());
    CHECK(s->native.has_value() && !s->native->loaded && s->native->error == "not deployed" && s->native->stale && s->native->features.empty());
    CHECK(!s->lord.has_value() && !s->appliedSeq.has_value());
}

TEST(overlay_status_feature_errors_and_missing_parts) {
    auto s = parseStatus(R"({"heartbeat":10,"inGame":true,"appliedSeq":4,"bridgeError":"parse error","features":{"build":{"active":false,"lastError":"boom"},"odd":5}})");
    CHECK(s.has_value() && s->appliedSeq == 4 && s->bridgeError == "parse error");
    CHECK(s->features->size() == 1 && s->features->at("build").lastError == "boom");
    CHECK(!s->native.has_value());
}

TEST(overlay_status_rejects_broken_text) {
    CHECK(!parseStatus("").has_value());
    CHECK(!parseStatus("{\"heartbeat\":").has_value());
    CHECK(!parseStatus("[]").has_value());
}
```

<!-- file: native/tests/overlay_bridge_tests.cpp -->
```cpp
#include "test.h"
#include "overlay/core/bridge.h"
#include "overlay/core/commands.h"
#include "runtime.h"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <windows.h>

using namespace mlt::ov;
namespace fs = std::filesystem;

static fs::path freshDir(const char* tag) {
    static int n = 0;
    fs::path dir = fs::temp_directory_path() / ("mltb-overlay-" + std::to_string(GetCurrentProcessId()) + "-" + tag + std::to_string(++n));
    fs::remove_all(dir);
    return dir;
}

static void writeText(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    f << text;
}

static std::string normalized(std::string text) {
    text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
    while (!text.empty() && text.back() == '\n') text.pop_back();
    return text;
}

TEST(overlay_bridge_missing_files_give_empty_doc_and_no_status) {
    Bridge b(freshDir("missing"));
    CHECK(b.loadControl().seq() == 0);
    CHECK(!b.peekSeq().has_value());
    CHECK(!b.readStatus().has_value());
}

TEST(overlay_bridge_save_increments_seq_and_creates_the_folder) {
    Bridge b(freshDir("save"));
    ControlDoc doc;
    auto bs = doc.build();
    bs.enabled = true;
    doc.setBuild(bs);
    CHECK(b.saveControl(doc) == 1);
    CHECK(doc.seq() == 1 && b.peekSeq() == 1);
    CHECK(b.saveControl(doc) == 2);
    auto loaded = b.loadControl();
    CHECK(loaded.seq() == 2 && loaded.build().enabled);
    CHECK(!fs::exists(b.controlPath().wstring() + L".tmp"));
}

TEST(overlay_bridge_save_goes_past_a_newer_seq_in_the_file) {
    Bridge b(freshDir("newer"));
    writeText(b.controlPath(), R"({"version":1,"seq":40,"features":{}})");
    ControlDoc doc;                       // 메모리의 seq 는 0, 파일은 40 (패널이 그 사이에 저장한 경우)
    CHECK(b.saveControl(doc) == 41);
    ControlDoc ahead;
    ahead.setSeq(100);                    // 메모리가 더 큰 경우
    CHECK(b.saveControl(ahead) == 101);
}

TEST(overlay_bridge_commands_travel_with_one_save_only) {
    Bridge b(freshDir("commands"));
    ControlDoc doc;
    doc.setCommands({ makeSetLord("treasury", 5000, 1790000000) });
    CHECK(b.saveControl(doc).has_value());
    CHECK(b.loadControl().raw()["commands"].size() == 1);
    doc.setCommands({});
    CHECK(b.saveControl(doc).has_value());
    CHECK(b.loadControl().raw()["commands"].empty());
}

TEST(overlay_bridge_korean_names_survive_the_file) {
    Bridge b(freshDir("korean"));
    ControlDoc doc;
    doc.feature("mercenaries")["companies"] = Json::array({ Json::object({ { "name", "토이박스 용병단" } }) });
    CHECK(b.saveControl(doc).has_value());
    CHECK(b.loadControl().raw()["features"]["mercenaries"]["companies"][0]["name"] == "토이박스 용병단");
}

TEST(overlay_bridge_reads_status_and_ignores_broken_files) {
    Bridge b(freshDir("status"));
    writeText(b.statusPath(), R"({"version":1,"heartbeat":50,"inGame":true,"appliedSeq":7})");
    auto s = b.readStatus();
    CHECK(s.has_value() && s->heartbeat == 50 && s->appliedSeq == 7);
    writeText(b.statusPath(), "{\"heartbeat\":");
    CHECK(!b.readStatus().has_value());
}

TEST(overlay_bridge_state_follows_heartbeat_menu_and_applied_seq) {
    StatusDoc s;
    s.heartbeat = 1000;
    s.inGame = true;
    s.appliedSeq = 5;
    CHECK(Bridge::evaluate(nullptr, 5, 1000) == BridgeState::Disconnected);
    CHECK(Bridge::evaluate(&s, 5, 1006) == BridgeState::Disconnected);     // 5초 넘게 조용함
    CHECK(Bridge::evaluate(&s, 5, 1005) == BridgeState::Applied);
    CHECK(Bridge::evaluate(&s, 6, 1001) == BridgeState::Pending);
    s.appliedSeq.reset();
    CHECK(Bridge::evaluate(&s, 0, 1001) == BridgeState::Pending);          // 아직 아무것도 적용하지 않음
    s.inGame = false;
    CHECK(Bridge::evaluate(&s, 0, 1001) == BridgeState::MainMenu);
}

TEST(overlay_command_set_lord_has_the_fields_the_mod_reads) {
    Json c = makeSetLord("kingsFavour", 50000, 1790000123);
    CHECK(c["type"] == "setLord" && c["key"] == "kingsFavour" && c["value"] == 50000 && c["issuedAt"] == 1790000123);
    std::string id = c["id"];
    CHECK(id.size() == 32 && id.find_first_not_of("0123456789abcdef") == std::string::npos);
    CHECK(newCommandId() != newCommandId());
}

// Lua 스펙(overlay_fixture_spec.lua)이 읽는 견본. 코어의 출력이 바뀌면 이 테스트가 실패한다.
// 견본을 다시 만들려면 환경 변수 MLT_WRITE_FIXTURES=1 로 테스트를 한 번 돌린다.
TEST(overlay_fixture_for_the_lua_spec_matches_core_output) {
    ControlDoc doc;
    doc.setSeq(7);
    BuildSettings build;
    build.enabled = true;
    build.instantRepair = false;
    doc.setBuild(build);
    UpgradeSettings upgrade;
    upgrade.enabled = true;
    doc.setUpgrade(upgrade);
    LordSettings lord;
    lord.enabled = true;
    lord.treasury = 150000;
    lord.kingsFavour = 0;
    doc.setLord(lord);
    Json command = makeSetLord("influence", 20000, 1790000000);
    command["id"] = "0123456789abcdef0123456789abcdef";
    doc.setCommands({ command });
    const std::string text = doc.dump();

    const fs::path path = fs::path(MLT_LUA_FIXTURES_DIR) / "control_from_overlay.json";
    char* write = nullptr;
    size_t len = 0;
    _dupenv_s(&write, &len, "MLT_WRITE_FIXTURES");
    const bool regenerate = write && std::string(write) == "1";
    free(write);
    if (regenerate) writeText(path, text + "\n");
    auto saved = mlt::readFileUtf8(path);
    CHECK(saved.has_value());
    CHECK(normalized(*saved) == normalized(text));
}
```

- [ ] **Step 3: CMake에 추가**

<!-- edit: native/CMakeLists.txt -->
`native/CMakeLists.txt` (1/2) — 찾을 부분:

```cmake
    overlay/core/control_doc.cpp)
```

바꿀 내용:

```cmake
    overlay/core/control_doc.cpp
    overlay/core/status_doc.cpp
    overlay/core/bridge.cpp
    overlay/core/commands.cpp)
```

<!-- edit: native/CMakeLists.txt -->
`native/CMakeLists.txt` (2/2) — 찾을 부분:

```cmake
    tests/overlay_control_doc_tests.cpp)
```

바꿀 내용:

```cmake
    tests/overlay_control_doc_tests.cpp
    tests/overlay_status_doc_tests.cpp
    tests/overlay_bridge_tests.cpp)
```

- [ ] **Step 4: 빌드해서 실패 확인**

Run: `pwsh E:/MLToybox/tools/build-native.ps1 -Test`
Expected: `Cannot find source file: overlay/core/status_doc.cpp`

- [ ] **Step 5: 구현**

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

struct StatusDoc {
    long long heartbeat = 0;
    bool inGame = false;
    std::optional<long long> appliedSeq;
    std::optional<std::string> bridgeError;
    std::optional<std::map<std::string, FeatureStatus>> features;                 // 이름순
    std::optional<std::vector<std::pair<std::string, CommandResult>>> commands;   // 파일에 적힌 순서
    std::optional<NativeStatus> native;
    std::optional<LordStatus> lord;
    Json raw;   // 탭이 더 읽을 항목(자원, 영지, 인구, 용병 등)
};

// 깨졌거나 객체가 아니면 값 없음
std::optional<StatusDoc> parseStatus(std::string_view text);

// 읽기 도우미
std::optional<std::string> optString(const Json* obj, const char* key);
std::optional<double> optNumber(const Json* obj, const char* key);
}
```

<!-- file: native/overlay/core/status_doc.cpp -->
```cpp
#include "status_doc.h"

namespace mlt::ov {

std::optional<std::string> optString(const Json* obj, const char* key) {
    if (!obj || !obj->is_object()) return std::nullopt;
    auto it = obj->find(key);
    if (it == obj->end() || !it->is_string()) return std::nullopt;
    return it->get<std::string>();
}

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

    s.raw = std::move(root);
    return s;
}

}
```

<!-- file: native/overlay/core/bridge.h -->
```cpp
#pragma once
#include "control_doc.h"
#include "status_doc.h"
#include <filesystem>
#include <optional>

// bridge 폴더의 control.json / status.json 읽고 쓰기 (패널의 BridgeClient 와 같은 동작)
namespace mlt::ov {

enum class BridgeState { Disconnected, MainMenu, Pending, Applied };

class Bridge {
public:
    static constexpr long long kHeartbeatTimeoutSec = 5;

    explicit Bridge(std::filesystem::path dir) : dir_(std::move(dir)) {}

    std::filesystem::path controlPath() const { return dir_ / L"control.json"; }
    std::filesystem::path statusPath() const { return dir_ / L"status.json"; }
    std::filesystem::path settingsPath() const { return dir_ / L"overlay.json"; }
    std::filesystem::path overlayStatusPath() const { return dir_ / L"overlay_status.json"; }

    // 파일이 없거나 깨졌으면 빈 문서
    ControlDoc loadControl() const;
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

namespace mlt::ov {

ControlDoc Bridge::loadControl() const {
    auto text = readFileUtf8(controlPath());
    if (!text) return ControlDoc();
    return ControlDoc::parse(*text);
}

std::optional<long long> Bridge::peekSeq() const {
    auto text = readFileUtf8(controlPath());
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
    auto text = readFileUtf8(statusPath());
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

<!-- file: native/overlay/core/commands.h -->
```cpp
#pragma once
#include "control_doc.h"
#include <string>

// 모드가 한 번만 실행하는 일회성 명령(control.json 의 commands). issuedAt 이 60초 넘게 지나면 모드가 버린다.
namespace mlt::ov {
// 32자리 16진수
std::string newCommandId();
// 영주 값(treasury / influence / kingsFavour)을 그 값으로 한 번 맞춘다
Json makeSetLord(const std::string& key, int value, long long nowEpochSeconds);
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

Json makeSetLord(const std::string& key, int value, long long nowEpochSeconds) {
    Json c = base("setLord", nowEpochSeconds);
    c["key"] = key;
    c["value"] = value;
    return c;
}

}
```

- [ ] **Step 6: 빌드하고, 견본이 없어서 실패하는 것을 확인**

Run: `pwsh E:/MLToybox/tools/build-native.ps1 -Test`
Expected: 빌드는 되고 테스트 하나가 실패한다. `FAIL overlay_fixture_for_the_lua_spec_matches_core_output` (`saved.has_value()`). Lua 스펙이 읽을 견본이 아직 없기 때문이다.

- [ ] **Step 7: 견본을 코어의 출력으로 만든다**

```powershell
$env:MLT_WRITE_FIXTURES = '1'
E:\MLToybox\native\build\native_tests.exe | Select-Object -Last 1
Remove-Item Env:MLT_WRITE_FIXTURES
E:\MLToybox\native\build\native_tests.exe | Select-Object -Last 1
```

Expected: 두 번 모두 `68 tests, 0 failed`. `mod/MLToybox/tests/fixtures/control_from_overlay.json`이 생기고 내용은 다음과 같다.

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
    }
  },
  "commands": [
    {
      "id": "0123456789abcdef0123456789abcdef",
      "type": "setLord",
      "issuedAt": 1790000000,
      "key": "influence",
      "value": 20000
    }
  ]
}
```

- [ ] **Step 8: 모드가 그 견본을 읽는지 보는 Lua 스펙 작성**

<!-- file: mod/MLToybox/tests/overlay_fixture_spec.lua -->
```lua
local T = require("t")
local bridge = require("core.bridge")
local fileio = require("core.fileio")

-- 오버레이 코어(C++)가 쓴 control.json 견본을 모드가 읽을 수 있는가.
-- 견본은 네이티브 테스트(overlay_fixture_for_the_lua_spec_matches_core_output)가 코어의 출력과 같은지 지킨다.
T.run({
  control_written_by_the_overlay_core_is_valid_for_the_mod = function()
    local text = fileio.read(SCRIPTS_DIR .. "\\..\\tests\\fixtures\\control_from_overlay.json")
    T.truthy(text, "fixture exists")
    local control, err = bridge.parseControl(text)
    T.truthy(control, "parses: " .. tostring(err))
    T.eq(control.version, 1, "version"); T.eq(control.seq, 7, "seq")
    local f = control.features
    T.eq(f.build.enabled, true, "build.enabled"); T.eq(f.build.instantRepair, false, "build.instantRepair"); T.eq(f.build.instantBuild, true, "build.instantBuild")
    T.eq(f.upgrade.enabled, true, "upgrade.enabled")
    T.eq(f.lord.enabled, true, "lord.enabled"); T.eq(f.lord.intervalSec, 2, "lord.intervalSec")
    T.eq(f.lord.treasury, 150000, "lord.treasury"); T.eq(f.lord.influence, nil, "unmanaged key is absent"); T.eq(f.lord.kingsFavour, 0, "zero is a value")
    T.eq(#control.commands, 1, "one command")
    local c = control.commands[1]
    T.eq(c.type, "setLord", "type"); T.eq(c.key, "influence", "key"); T.eq(c.value, 20000, "value")
    T.eq(c.id, "0123456789abcdef0123456789abcdef", "id"); T.eq(c.issuedAt, 1790000000, "issuedAt")
  end,
})
```

- [ ] **Step 9: Lua 스펙 통과 확인**

Run: `dotnet test E:/MLToybox/panel/MLToybox.sln --filter "FullyQualifiedName~LuaSpec"`
Expected: 모두 통과. `overlay_fixture_spec.lua`가 목록에 있다.

- [ ] **Step 10: 커밋**

```bash
git -C E:/MLToybox add native/overlay native/tests native/CMakeLists.txt mod/MLToybox/tests
git -C E:/MLToybox commit -m "feat(overlay): status document, bridge (seq, atomic save, link state), commands, protocol fixtures" -m "Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 4: 오버레이 설정, 상태 파일, 저장 규칙(`session`)

**Files:**
- Create: `native/overlay/core/settings.h/.cpp`, `native/overlay/core/status_file.h/.cpp`, `native/overlay/core/session.h/.cpp`
- Create: `native/tests/overlay_settings_tests.cpp`, `native/tests/overlay_session_tests.cpp`
- Modify: `native/CMakeLists.txt`

**Interfaces:**
- Consumes: `ControlDoc`, `Json`, `boolOr`, `intOr`, `objectAt` (Task 2), `optString`, `optNumber`, `makeSetLord` (Task 3)
- Produces (이름공간 `mlt::ov`):
  - `overlay/core/settings.h`: `struct OverlaySettings { std::string toggleKey; float scale; int x, y, w, h; bool startOpen; std::string devTab; }`, `kScaleMin`, `kScaleMax`, `OverlaySettings parseSettings(std::string_view)`, `std::string dumpSettings(const OverlaySettings&)`, `const std::vector<std::string>& toggleKeyNames()`, `int toggleKeyCode(const std::string&)`
  - `overlay/core/status_file.h`: `enum class OverlayState { Starting, Waiting, Ready, Disabled }`, `const char* overlayStateName(OverlayState)`, `struct OverlayStatus { heartbeat, state, reason, visible, frames, font }`, `std::string renderOverlayStatus(const OverlayStatus&)`
  - `overlay/core/session.h`: `struct Session { ControlDoc control; bool dirty; std::vector<Json> commands; long long lastSentSeq; bool saveFailed; }`, `struct PendingSave { ControlDoc doc; std::vector<Json> sent; }`, `std::optional<PendingSave> beginSave(Session&)`, `void finishSave(Session&, const PendingSave&, std::optional<long long> seq)`, `bool wantsReload(const Session&, long long fileSeq)`, `bool adoptReloaded(Session&, ControlDoc loaded)`

- [ ] **Step 1: 실패하는 테스트 작성**

<!-- file: native/tests/overlay_settings_tests.cpp -->
```cpp
#include "test.h"
#include "overlay/core/control_doc.h"
#include "overlay/core/settings.h"
#include "overlay/core/status_file.h"

using namespace mlt::ov;

TEST(overlay_settings_defaults_when_missing_or_broken) {
    const OverlaySettings d;
    CHECK(d.toggleKey == "Insert" && d.scale == 1.0f && d.x == 80 && d.y == 80 && d.w == 640 && d.h == 720 && !d.startOpen && d.devTab.empty());
    CHECK(parseSettings("") == d);
    CHECK(parseSettings("{") == d);
    CHECK(parseSettings("[1]") == d);
}

TEST(overlay_settings_reads_values_and_repairs_bad_ones) {
    auto s = parseSettings(R"({"toggleKey":"F8","scale":1.3,"window":{"x":10,"y":20,"w":900,"h":800},"startOpen":true,"devTab":"영주"})");
    CHECK(s.toggleKey == "F8" && s.scale > 1.29f && s.scale < 1.31f && s.x == 10 && s.y == 20 && s.w == 900 && s.h == 800 && s.startOpen && s.devTab == "영주");
    s = parseSettings(R"({"toggleKey":"Escape","scale":9,"window":{"w":10,"h":99999},"devTab":null})");
    CHECK(s.toggleKey == "Insert");                 // 모르는 키
    CHECK(s.scale == kScaleMax);                    // 범위 밖
    CHECK(s.w == 320 && s.h == 4000 && s.x == 80);  // 너무 작거나 큰 창, 빠진 값
    CHECK(s.devTab.empty());
    CHECK(parseSettings(R"({"scale":0.1})").scale == kScaleMin);
    CHECK(parseSettings(R"({"scale":"big"})").scale == 1.0f);
}

TEST(overlay_settings_round_trip) {
    OverlaySettings s;
    s.toggleKey = "Home";
    s.scale = 1.2f;
    s.x = 300;
    s.w = 700;
    s.devTab = "상태";
    auto back = parseSettings(dumpSettings(s));
    CHECK(back.toggleKey == "Home" && back.x == 300 && back.w == 700 && back.devTab == "상태");
    CHECK(back.scale > 1.19f && back.scale < 1.21f);
    CHECK(Json::parse(dumpSettings(OverlaySettings()))["devTab"].is_null());
}

TEST(overlay_settings_toggle_keys) {
    CHECK(toggleKeyNames().size() == 6 && toggleKeyNames().front() == "Insert");
    CHECK(toggleKeyCode("Insert") == 0x2D && toggleKeyCode("Home") == 0x24 && toggleKeyCode("End") == 0x23);
    CHECK(toggleKeyCode("F7") == 0x76 && toggleKeyCode("F8") == 0x77 && toggleKeyCode("F9") == 0x78);
    CHECK(toggleKeyCode("nope") == 0x2D);
}

TEST(overlay_status_file_reports_state_and_reason) {
    OverlayStatus s;
    s.heartbeat = 1790000000;
    s.state = OverlayState::Ready;
    s.visible = true;
    s.frames = 12345;
    s.font = "malgun";
    auto j = Json::parse(renderOverlayStatus(s));
    CHECK(j["heartbeat"] == 1790000000 && j["state"] == "ready" && j["reason"].is_null() && j["visible"] == true && j["frames"] == 12345 && j["font"] == "malgun");
    s.state = OverlayState::Disabled;
    s.reason = "present queue not found";
    j = Json::parse(renderOverlayStatus(s));
    CHECK(j["state"] == "disabled" && j["reason"] == "present queue not found");
    CHECK(std::string(overlayStateName(OverlayState::Starting)) == "starting" && std::string(overlayStateName(OverlayState::Waiting)) == "waiting");
}
```

<!-- file: native/tests/overlay_session_tests.cpp -->
```cpp
#include "test.h"
#include "overlay/core/commands.h"
#include "overlay/core/session.h"

using namespace mlt::ov;

TEST(overlay_session_nothing_to_save_when_clean) {
    Session s;
    CHECK(!beginSave(s).has_value());
}

TEST(overlay_session_save_carries_commands_once) {
    Session s;
    s.commands.push_back(makeSetLord("treasury", 100, 1790000000));
    s.dirty = true;
    auto p = beginSave(s);
    CHECK(p.has_value() && p->doc.raw()["commands"].size() == 1 && p->sent.size() == 1);
    CHECK(!s.dirty && s.commands.empty() && s.control.raw()["commands"].empty());
    finishSave(s, *p, 8);
    CHECK(s.lastSentSeq == 8 && s.control.seq() == 8 && !s.saveFailed && !s.dirty);
    CHECK(!beginSave(s).has_value());   // 같은 명령이 다시 실리지 않는다
}

TEST(overlay_session_failed_save_keeps_changes_and_commands) {
    Session s;
    s.control.setSeq(5);
    s.lastSentSeq = 5;
    const Json first = makeSetLord("treasury", 100, 1790000000);
    s.commands.push_back(first);
    s.dirty = true;
    auto p = beginSave(s);
    CHECK(p.has_value());
    s.commands.push_back(makeSetLord("influence", 7, 1790000001));   // 저장하는 사이 화면에서 누른 명령
    finishSave(s, *p, std::nullopt);
    CHECK(s.saveFailed && s.dirty && s.lastSentSeq == 5 && s.control.seq() == 5);
    CHECK(s.commands.size() == 2 && s.commands[0]["id"] == first["id"]);   // 먼저 누른 것이 앞에 온다
    auto retry = beginSave(s);
    CHECK(retry.has_value() && retry->doc.raw()["commands"].size() == 2);
    finishSave(s, *retry, 6);
    CHECK(!s.saveFailed && s.lastSentSeq == 6 && s.commands.empty());
}

TEST(overlay_session_change_made_during_a_save_is_saved_next) {
    Session s;
    s.dirty = true;
    auto p = beginSave(s);
    CHECK(p.has_value() && !p->doc.build().enabled);
    BuildSettings b = s.control.build();   // 저장하는 사이 화면에서 바꿈
    b.enabled = true;
    s.control.setBuild(b);
    s.dirty = true;
    finishSave(s, *p, 1);
    CHECK(s.dirty && s.control.build().enabled && s.control.seq() == 1);
    auto next = beginSave(s);
    CHECK(next.has_value() && next->doc.build().enabled);
}

TEST(overlay_session_reloads_only_newer_files_and_never_over_pending_changes) {
    Session s;
    s.control.setSeq(10);
    s.lastSentSeq = 10;
    CHECK(!wantsReload(s, 10) && !wantsReload(s, 9) && wantsReload(s, 11));
    CHECK(adoptReloaded(s, ControlDoc::parse(R"({"version":1,"seq":11,"features":{"upgrade":{"enabled":true}},"commands":[{"id":"x","type":"setLord"}]})")));
    CHECK(s.control.upgrade().enabled && s.lastSentSeq == 11);
    CHECK(s.control.raw()["commands"].empty());      // 파일에 있던 남의 명령은 다시 보내지 않는다
    s.dirty = true;                                  // 저장 대기 중인 변경이 있으면 우리 쪽이 이긴다
    CHECK(!wantsReload(s, 12));
    CHECK(!adoptReloaded(s, ControlDoc::parse(R"({"version":1,"seq":12,"features":{"upgrade":{"enabled":false}}})")));
    CHECK(s.control.upgrade().enabled && s.control.seq() == 11);
    s.dirty = false;
    CHECK(!adoptReloaded(s, ControlDoc::parse(R"({"version":1,"seq":11,"features":{}})")));   // 더 새롭지 않다
}
```

- [ ] **Step 2: CMake에 추가**

<!-- edit: native/CMakeLists.txt -->
`native/CMakeLists.txt` (1/2) — 찾을 부분:

```cmake
    overlay/core/commands.cpp)
```

바꿀 내용:

```cmake
    overlay/core/commands.cpp
    overlay/core/settings.cpp
    overlay/core/status_file.cpp
    overlay/core/session.cpp)
```

<!-- edit: native/CMakeLists.txt -->
`native/CMakeLists.txt` (2/2) — 찾을 부분:

```cmake
    tests/overlay_bridge_tests.cpp)
```

바꿀 내용:

```cmake
    tests/overlay_bridge_tests.cpp
    tests/overlay_settings_tests.cpp
    tests/overlay_session_tests.cpp)
```

- [ ] **Step 3: 빌드해서 실패 확인**

Run: `pwsh E:/MLToybox/tools/build-native.ps1 -Test`
Expected: `Cannot find source file: overlay/core/settings.cpp`

- [ ] **Step 4: 구현**

<!-- file: native/overlay/core/settings.h -->
```cpp
#pragma once
#include <string>
#include <string_view>
#include <vector>

// bridge/overlay.json: 오버레이 자체 설정(토글 키, 글자 배율, 창 위치·크기)과 개발용 설정
namespace mlt::ov {

struct OverlaySettings {
    std::string toggleKey = "Insert";
    float scale = 1.0f;
    int x = 80, y = 80, w = 640, h = 720;
    bool startOpen = false;   // 개발·검증용: 열린 채로 시작
    std::string devTab;       // 개발·검증용: 이 이름의 탭을 연다(비어 있으면 없음)

    bool operator==(const OverlaySettings&) const = default;
};

inline constexpr float kScaleMin = 0.8f;
inline constexpr float kScaleMax = 1.5f;

// 파일이 없거나 깨졌으면 기본값. 모르는 토글 키는 Insert, 범위를 벗어난 값은 가까운 끝값
OverlaySettings parseSettings(std::string_view text);
std::string dumpSettings(const OverlaySettings& s);

// 고를 수 있는 토글 키 이름(화면 표시 순서)
const std::vector<std::string>& toggleKeyNames();
// 키 이름의 가상 키 코드. 모르는 이름이면 Insert 의 코드
int toggleKeyCode(const std::string& name);
}
```

<!-- file: native/overlay/core/settings.cpp -->
```cpp
#include "settings.h"
#include "control_doc.h"
#include "status_doc.h"
#include <algorithm>
#include <cmath>
#include <utility>

namespace mlt::ov {

namespace {
// 가상 키 코드(VK_*). windows.h 없이 쓰도록 값으로 적는다
const std::vector<std::pair<std::string, int>> kKeys = {
    { "Insert", 0x2D }, { "Home", 0x24 }, { "End", 0x23 }, { "F7", 0x76 }, { "F8", 0x77 }, { "F9", 0x78 },
};
}

const std::vector<std::string>& toggleKeyNames() {
    static const std::vector<std::string> names = [] {
        std::vector<std::string> out;
        for (const auto& k : kKeys) out.push_back(k.first);
        return out;
    }();
    return names;
}

int toggleKeyCode(const std::string& name) {
    for (const auto& k : kKeys) {
        if (k.first == name) return k.second;
    }
    return kKeys.front().second;
}

static bool knownKey(const std::string& name) {
    for (const auto& k : kKeys) {
        if (k.first == name) return true;
    }
    return false;
}

OverlaySettings parseSettings(std::string_view text) {
    OverlaySettings s;
    Json root = Json::parse(text.begin(), text.end(), nullptr, false);
    if (root.is_discarded() || !root.is_object()) return s;
    if (auto key = optString(&root, "toggleKey"); key && knownKey(*key)) s.toggleKey = *key;
    if (auto scale = optNumber(&root, "scale"); scale && std::isfinite(*scale)) {
        s.scale = std::clamp(static_cast<float>(*scale), kScaleMin, kScaleMax);
    }
    if (const Json* window = objectAt(&root, "window")) {
        s.x = std::clamp(intOr(window, "x", s.x), -10000, 10000);
        s.y = std::clamp(intOr(window, "y", s.y), -10000, 10000);
        s.w = std::clamp(intOr(window, "w", s.w), 320, 4000);
        s.h = std::clamp(intOr(window, "h", s.h), 240, 4000);
    }
    s.startOpen = boolOr(&root, "startOpen", false);
    if (auto tab = optString(&root, "devTab")) s.devTab = *tab;
    return s;
}

std::string dumpSettings(const OverlaySettings& s) {
    Json root = Json::object();
    root["toggleKey"] = s.toggleKey;
    root["scale"] = std::round(s.scale * 100.0f) / 100.0f;
    Json window = Json::object();
    window["x"] = s.x;
    window["y"] = s.y;
    window["w"] = s.w;
    window["h"] = s.h;
    root["window"] = std::move(window);
    root["startOpen"] = s.startOpen;
    if (s.devTab.empty()) root["devTab"] = nullptr;
    else root["devTab"] = s.devTab;
    return root.dump(2);
}

}
```

<!-- file: native/overlay/core/status_file.h -->
```cpp
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
```

<!-- file: native/overlay/core/status_file.cpp -->
```cpp
#include "status_file.h"
#include "control_doc.h"

namespace mlt::ov {

const char* overlayStateName(OverlayState s) {
    switch (s) {
        case OverlayState::Starting: return "starting";
        case OverlayState::Waiting: return "waiting";
        case OverlayState::Ready: return "ready";
        case OverlayState::Disabled: return "disabled";
    }
    return "starting";
}

std::string renderOverlayStatus(const OverlayStatus& s) {
    Json root = Json::object();
    root["heartbeat"] = s.heartbeat;
    root["state"] = overlayStateName(s.state);
    if (s.reason.empty()) root["reason"] = nullptr;
    else root["reason"] = s.reason;
    root["visible"] = s.visible;
    root["frames"] = s.frames;
    root["font"] = s.font;
    return root.dump();
}

}
```

<!-- file: native/overlay/core/session.h -->
```cpp
#pragma once
#include "control_doc.h"
#include <optional>
#include <vector>

// 화면과 작업 스레드가 나눠 쓰는 설정 문서 상태와, 저장·다시 읽기의 순서 규칙.
// 잠금은 쓰는 쪽(ui/app.h 의 App::mutex)이 잡는다. 파일 입출력은 여기서 하지 않는다.
namespace mlt::ov {

struct Session {
    ControlDoc control;              // 메모리의 설정 문서
    bool dirty = false;              // 저장해야 한다
    std::vector<Json> commands;      // 다음 저장에 실어 보낼 일회성 명령
    long long lastSentSeq = 0;       // 마지막으로 저장했거나 읽은 seq
    bool saveFailed = false;
};

struct PendingSave {
    ControlDoc doc;                  // 파일에 쓸 문서(명령 포함)
    std::vector<Json> sent;          // 이 저장에 실은 명령
};

// 저장할 것이 없으면 값 없음. 있으면 명령을 실은 사본을 돌려주고, 메모리에서는 명령과 변경 표시를 비운다
std::optional<PendingSave> beginSave(Session& s);
// 저장 결과를 반영한다. seq 가 없으면 실패한 것이다: 명령을 되돌려 놓고 변경 표시를 다시 켠다
void finishSave(Session& s, const PendingSave& pending, std::optional<long long> seq);
// 파일의 seq 가 메모리보다 크고 저장 대기 중인 변경이 없으면 참(패널 등 밖에서 바꿨으니 다시 읽는다)
bool wantsReload(const Session& s, long long fileSeq);
// 다시 읽은 문서를 받아들인다. 그 사이 변경이 생겼거나 더 새롭지 않으면 버리고 false
bool adoptReloaded(Session& s, ControlDoc loaded);
}
```

<!-- file: native/overlay/core/session.cpp -->
```cpp
#include "session.h"
#include <utility>

namespace mlt::ov {

std::optional<PendingSave> beginSave(Session& s) {
    if (!s.dirty) return std::nullopt;
    PendingSave pending;
    pending.sent.swap(s.commands);
    s.control.setCommands(pending.sent);
    pending.doc = s.control;
    s.control.setCommands({});   // 명령은 이번 저장에만 실린다
    s.dirty = false;
    return pending;
}

void finishSave(Session& s, const PendingSave& pending, std::optional<long long> seq) {
    if (seq) {
        s.control.setSeq(*seq);
        s.lastSentSeq = *seq;
        s.saveFailed = false;
        return;
    }
    // 저장하는 사이 화면에서 누른 명령보다 앞에 되돌려 놓는다
    s.commands.insert(s.commands.begin(), pending.sent.begin(), pending.sent.end());
    s.dirty = true;
    s.saveFailed = true;
}

bool wantsReload(const Session& s, long long fileSeq) {
    return !s.dirty && fileSeq > s.control.seq();
}

bool adoptReloaded(Session& s, ControlDoc loaded) {
    if (s.dirty || loaded.seq() <= s.control.seq()) return false;
    s.control = std::move(loaded);
    s.control.setCommands({});   // 파일에 남아 있던 남의 명령을 우리가 다시 보내지 않는다
    s.lastSentSeq = s.control.seq();
    return true;
}

}
```

- [ ] **Step 5: 빌드와 테스트 통과 확인**

Run: `pwsh E:/MLToybox/tools/build-native.ps1 -Test`
Expected: `100% tests passed, 0 tests failed out of 1`

Run: `& E:\MLToybox\native\build\native_tests.exe | Select-Object -Last 1`
Expected: `78 tests, 0 failed`

- [ ] **Step 6: 커밋**

```bash
git -C E:/MLToybox add native/overlay native/tests native/CMakeLists.txt
git -C E:/MLToybox commit -m "feat(overlay): overlay settings, status file, save and reload rules" -m "Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 5: Lua에서 오버레이 올리기, 배포·캡처 도구

**Files:**
- Modify: `mod/MLToybox/Scripts/core/native.lua`, `mod/MLToybox/Scripts/config.lua`, `mod/MLToybox/Scripts/main.lua`
- Modify: `mod/MLToybox/tests/native_spec.lua`, `mod/MLToybox/tests/entrypoints_spec.lua`
- Modify: `tools/deploy.ps1`, `tools/tests/Tools.Tests.ps1`
- Create: `tools/capture-game.ps1`, `tools/lab/resize.lua`

**Interfaces:**
- Consumes: `overlay_status.json`의 모양 `{ heartbeat, state, reason, visible, frames, font }` (Task 4의 `renderOverlayStatus`)
- Produces:
  - Lua `native.loadFile(nativeDir, fileName)` → `true, nil` 또는 `false, 이유`; `native.NATIVE_DLL`, `native.OVERLAY_DLL`; `native.overlayStatus(statusPath, now, loaded, loadErr)` → `{ loaded, error, stale, state?, reason? }`
  - `config.overlay`(기본 `true`), `status.json`의 `overlay`
  - `tools/deploy.ps1 -Mod MLToybox`가 `native/build/mltoybox_overlay.dll`도 복사한다
  - `tools/capture-game.ps1 [-Out <png>] [-Key Insert|Home|End|F7|F8|F9] [-WaitMs <n>]`, `tools/lab/resize.lua`(`lab.ps1 -Vars @{ W; H; MODE }`)

- [ ] **Step 1: 실패하는 Lua 스펙 작성**

<!-- edit: mod/MLToybox/tests/native_spec.lua -->
`mod/MLToybox/tests/native_spec.lua` — 찾을 부분:

```lua
  status_merges_fresh_native_status = function()
```

바꿀 내용:

```lua
  load_file_loads_the_named_dll = function()
    os.execute('mkdir "' .. TEST_TMP .. '\\n3"')
    write(TEST_TMP .. "\\n3\\mltoybox_overlay.dll", "x")
    local seen
    native.loadlib = function(path) seen = path; return true end
    T.eq((native.loadFile(TEST_TMP .. "\\n3", native.OVERLAY_DLL)), true, "ok")
    T.truthy(seen:find("mltoybox_overlay.dll", 1, true), "path: " .. tostring(seen))
    local ok, err = native.loadFile(TEST_TMP .. "\\n3", "missing.dll")
    T.eq(ok, false, "missing file"); T.eq(err, "not deployed", "reason")
  end,
  overlay_status_merges_state_and_reason = function()
    local p = TEST_TMP .. "\\os1.json"
    write(p, '{"heartbeat":100,"state":"disabled","reason":"present queue not found","visible":false,"frames":0,"font":"malgun"}')
    local s = native.overlayStatus(p, 103, true, nil)
    T.eq(s.loaded, true, "loaded"); T.eq(s.stale, false, "fresh"); T.eq(s.state, "disabled", "state"); T.eq(s.reason, "present queue not found", "reason")
    write(p, '{"heartbeat":100,"state":"ready","reason":null,"visible":true,"frames":9,"font":"malgun"}')
    s = native.overlayStatus(p, 106, true, nil)
    T.eq(s.stale, true, "old heartbeat"); T.eq(s.state, "ready", "state kept"); T.eq(s.reason, nil, "null reason")
  end,
  overlay_status_without_the_dll_ignores_a_leftover_file = function()
    local p = TEST_TMP .. "\\os2.json"
    write(p, '{"heartbeat":100,"state":"ready"}')
    local s = native.overlayStatus(p, 100, false, "disabled in config")
    T.eq(s.loaded, false, "not loaded"); T.eq(s.error, "disabled in config", "error"); T.eq(s.state, nil, "leftover state ignored"); T.eq(s.stale, true, "stale")
    s = native.overlayStatus(TEST_TMP .. "\\none.json", 1, true, nil)
    T.eq(s.loaded, true, "loaded without a file yet"); T.eq(s.stale, true, "no heartbeat"); T.eq(s.state, nil, "no state")
    write(p, "{broken")
    T.eq(native.overlayStatus(p, 1, true, nil).state, nil, "broken file")
  end,
  status_merges_fresh_native_status = function()
```

<!-- edit: mod/MLToybox/tests/entrypoints_spec.lua -->
`mod/MLToybox/tests/entrypoints_spec.lua` — 찾을 부분:

```lua
    T.eq(type(cfg.featureModules), "table", "featureModules")
```

바꿀 내용:

```lua
    T.eq(type(cfg.featureModules), "table", "featureModules")
    T.eq(cfg.overlay, true, "overlay on by default")
```

- [ ] **Step 2: 실패 확인**

Run: `dotnet test E:/MLToybox/panel/MLToybox.sln --filter "FullyQualifiedName~LuaSpec"`
Expected: `native_spec.lua`가 `attempt to call a nil value (field 'loadFile')`로, `entrypoints_spec.lua`가 `overlay on by default`로 실패한다.

- [ ] **Step 3: Lua 구현**

<!-- edit: mod/MLToybox/Scripts/core/native.lua -->
`mod/MLToybox/Scripts/core/native.lua` (1/2) — 찾을 부분:

```lua
-- 독립 네이티브 DLL(spec §11) 로드와 native_status.json 병합
local M = { STALE_SEC = 5 }
M.loadlib = package.loadlib

function M.load(nativeDir)
  local path = nativeDir .. "\\mltoybox_native.dll"
  local f = io.open(path, "rb")
  if not f then return false, "not deployed" end
  f:close()
  local ok, err = M.loadlib(path, "*")
  if not ok then return false, tostring(err) end
  return true, nil
end
```

바꿀 내용:

```lua
-- 독립 네이티브 DLL(spec §11)과 오버레이 DLL 로드, native_status.json / overlay_status.json 병합
local M = { STALE_SEC = 5, NATIVE_DLL = "mltoybox_native.dll", OVERLAY_DLL = "mltoybox_overlay.dll" }
M.loadlib = package.loadlib

function M.loadFile(nativeDir, fileName)
  local path = nativeDir .. "\\" .. fileName
  local f = io.open(path, "rb")
  if not f then return false, "not deployed" end
  f:close()
  local ok, err = M.loadlib(path, "*")
  if not ok then return false, tostring(err) end
  return true, nil
end

function M.load(nativeDir)
  return M.loadFile(nativeDir, M.NATIVE_DLL)
end
```

<!-- edit: mod/MLToybox/Scripts/core/native.lua -->
`mod/MLToybox/Scripts/core/native.lua` (2/2) — 찾을 부분:

```lua
  return out
end

return M
```

바꿀 내용:

```lua
  return out
end

-- 오버레이 DLL 이 쓰는 overlay_status.json 을 합친다. 올리지 못했으면 파일을 보지 않는다(지난 실행의 파일이 남아 있을 수 있다)
function M.overlayStatus(statusPath, now, loaded, loadErr)
  local out = { loaded = loaded, error = loadErr, stale = true }
  if not loaded then return out end
  local text = fileio.read(statusPath)
  if text then
    local ok, d = pcall(json.decode, text)
    if ok and type(d) == "table" and type(d.heartbeat) == "number" then
      out.stale = (now - d.heartbeat) > M.STALE_SEC
      if type(d.state) == "string" then out.state = d.state end
      if type(d.reason) == "string" then out.reason = d.reason end
    end
  end
  return out
end

return M
```

<!-- edit: mod/MLToybox/Scripts/config.lua -->
`mod/MLToybox/Scripts/config.lua` — 찾을 부분:

```lua
  -- control.json 이 아직 없을 때 쓰는 기본 설정 (features 테이블과 같은 모양)
  defaults = {},
```

바꿀 내용:

```lua
  -- control.json 이 아직 없을 때 쓰는 기본 설정 (features 테이블과 같은 모양)
  defaults = {},
  -- 게임 안 오버레이 창(native/mltoybox_overlay.dll)을 올린다. 오버레이가 말썽이면 false 로 끈다(다른 기능은 그대로 동작)
  overlay = true,
```

<!-- edit: mod/MLToybox/Scripts/main.lua -->
`mod/MLToybox/Scripts/main.lua` (1/2) — 찾을 부분:

```lua
local nativeLoaded, nativeErr = native.load(paths.parentDir(scriptsDir) .. "\\native")
log.info("native: %s", nativeLoaded and "loaded" or tostring(nativeErr))
```

바꿀 내용:

```lua
local nativeDir = paths.parentDir(scriptsDir) .. "\\native"
local nativeLoaded, nativeErr = native.load(nativeDir)
log.info("native: %s", nativeLoaded and "loaded" or tostring(nativeErr))
local overlayLoaded, overlayErr = false, "disabled in config"
if config.overlay then overlayLoaded, overlayErr = native.loadFile(nativeDir, native.OVERLAY_DLL) end
log.info("overlay: %s", overlayLoaded and "loaded" or tostring(overlayErr))
```

<!-- edit: mod/MLToybox/Scripts/main.lua -->
`mod/MLToybox/Scripts/main.lua` (2/2) — 찾을 부분:

```lua
        status.native = native.status(bridgeDir .. "\\native_status.json", now, nativeLoaded, nativeErr)
```

바꿀 내용:

```lua
        status.native = native.status(bridgeDir .. "\\native_status.json", now, nativeLoaded, nativeErr)
        status.overlay = native.overlayStatus(bridgeDir .. "\\overlay_status.json", now, overlayLoaded, overlayErr)
```

- [ ] **Step 4: Lua 스펙 통과 확인**

Run: `dotnet test E:/MLToybox/panel/MLToybox.sln --filter "FullyQualifiedName~LuaSpec"`
Expected: 모두 통과

- [ ] **Step 5: 배포 도구의 실패하는 테스트 작성**

<!-- edit: tools/tests/Tools.Tests.ps1 -->
`tools/tests/Tools.Tests.ps1` — 찾을 부분:

```powershell
Test-Case 'lab.ps1 replaces __KEY__ tokens from -Vars' {
```

바꿀 내용:

```powershell
Test-Case 'deploy copies the overlay dll when built' {
    $repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
    $built = Join-Path $repo 'native\build\mltoybox_overlay.dll'
    $created = $false
    if (-not (Test-Path $built)) { New-Item -ItemType Directory -Force (Split-Path $built) | Out-Null; Set-Content $built 'fake'; $created = $true }
    try {
        $g = New-FakeGame
        $mods = Join-Path $g 'ManorLords\Binaries\Win64\ue4ss\Mods'
        & "$PSScriptRoot\..\deploy.ps1" -Mod MLToybox -GameDir $g | Out-Null
        Assert-True (Test-Path "$mods\MLToybox\native\mltoybox_overlay.dll") 'overlay dll copied'
    } finally { if ($created) { Remove-Item $built } }
}

Test-Case 'lab.ps1 replaces __KEY__ tokens from -Vars' {
```

Run: `pwsh E:/MLToybox/tools/tests/Tools.Tests.ps1`
Expected: `FAIL deploy copies the overlay dll when built : overlay dll copied`

- [ ] **Step 6: 배포 도구 고치기**

<!-- edit: tools/deploy.ps1 -->
`tools/deploy.ps1` — 찾을 부분:

```powershell
if ($Mod -eq 'MLToybox') {
    $dll = Join-Path $repo 'native\build\mltoybox_native.dll'
    if (Test-Path $dll) {
        $nativeDir = New-Item -ItemType Directory -Force (Join-Path $target 'native')
        try { Copy-Item $dll $nativeDir -Force -ErrorAction Stop }
        catch { Write-Warning "native dll not copied (game running?): $($_.Exception.Message)" }
    }
}
```

바꿀 내용:

```powershell
if ($Mod -eq 'MLToybox') {
    # 네이티브 DLL 과 오버레이 DLL. 빌드돼 있는 것만 복사한다. 게임이 켜져 있으면 잠겨 있어 경고만 남긴다
    foreach ($name in 'mltoybox_native.dll', 'mltoybox_overlay.dll') {
        $dll = Join-Path $repo "native\build\$name"
        if (Test-Path $dll) {
            $nativeDir = New-Item -ItemType Directory -Force (Join-Path $target 'native')
            try { Copy-Item $dll $nativeDir -Force -ErrorAction Stop }
            catch { Write-Warning "$name not copied (game running?): $($_.Exception.Message)" }
        }
    }
}
```

Run: `pwsh E:/MLToybox/tools/tests/Tools.Tests.ps1`
Expected: `ALL PASS`

- [ ] **Step 7: 개발 도구 작성**

게임 창만 찍고 키 메시지를 보내는 도구. 마우스 메시지는 게임 창이 앞에 없으면 ImGui가 받지 않으므로(스파이크에서 확인) 지원하지 않는다.

<!-- file: tools/capture-game.ps1 -->
```powershell
# 개발용: 게임 창만 캡처하고(다른 창이나 화면 전체는 찍지 않는다), 필요하면 그 전에 키 메시지를 보낸다.
# 창이 가려져 있어도 된다(PrintWindow). 실제 마우스·키보드와 전경 창은 건드리지 않는다.
# 마우스 메시지는 게임 창이 앞에 없으면 ImGui 에 먹히지 않으므로(findings 오버레이 스파이크) 지원하지 않는다.
#   -Out <png>      저장할 파일. 없으면 캡처하지 않는다
#   -Key <이름>     보낼 키: Insert, Home, End, F7, F8, F9
#   -WaitMs <n>     키를 보낸 뒤 캡처하기 전에 기다릴 시간
param([string]$Out, [ValidateSet('Insert', 'Home', 'End', 'F7', 'F8', 'F9')][string]$Key, [int]$WaitMs = 700)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
if (-not ('MLToyboxCapture' -as [type])) {
    Add-Type @'
using System; using System.Runtime.InteropServices;
public static class MLToyboxCapture {
  public delegate bool EnumWindowsProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsProc cb, IntPtr l);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
}
'@
}
$game = Get-Process -Name 'ManorLords-Win64-Shipping' -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $game) { throw 'game is not running' }
$gamePid = $game.Id
$script:found = [IntPtr]::Zero
$script:best = 0
[MLToyboxCapture]::EnumWindows({ param($h, $l)
    [uint32]$p = 0
    [void][MLToyboxCapture]::GetWindowThreadProcessId($h, [ref]$p)
    if ($p -eq $gamePid -and [MLToyboxCapture]::IsWindowVisible($h)) {
        $r = New-Object MLToyboxCapture+RECT
        [void][MLToyboxCapture]::GetWindowRect($h, [ref]$r)
        $area = ($r.R - $r.L) * ($r.B - $r.T)
        if ($area -gt $script:best) { $script:best = $area; $script:found = $h }
    }
    $true }, [IntPtr]::Zero) | Out-Null
$hwnd = $script:found
if ($hwnd -eq [IntPtr]::Zero) { throw 'game window not found' }
$rect = New-Object MLToyboxCapture+RECT
[void][MLToyboxCapture]::GetWindowRect($hwnd, [ref]$rect)
$w = $rect.R - $rect.L
$h = $rect.B - $rect.T

if ($Key) {
    $vk = @{ Insert = 0x2D; Home = 0x24; End = 0x23; F7 = 0x76; F8 = 0x77; F9 = 0x78 }[$Key]
    [void][MLToyboxCapture]::PostMessage($hwnd, 0x100, [IntPtr]$vk, [IntPtr]1)                       # WM_KEYDOWN
    Start-Sleep -Milliseconds 60
    [void][MLToyboxCapture]::PostMessage($hwnd, 0x101, [IntPtr]$vk, [IntPtr]([Int64]0xC0000001))    # WM_KEYUP
    Write-Host "posted $Key"
}
if (-not $Out) { return }
Start-Sleep -Milliseconds $WaitMs
$bmp = New-Object System.Drawing.Bitmap $w, $h
$g = [System.Drawing.Graphics]::FromImage($bmp)
$hdc = $g.GetHdc()
$ok = [MLToyboxCapture]::PrintWindow($hwnd, $hdc, 2)   # PW_RENDERFULLCONTENT
$g.ReleaseHdc($hdc)
$g.Dispose()
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
if (-not $ok) { throw 'PrintWindow failed' }
Write-Host "saved $Out (${w}x${h})"
```

해상도를 바꾸는 Lab 스크립트(오버레이가 `ResizeBuffers`를 견디는지 볼 때 쓴다).

<!-- file: tools/lab/resize.lua -->
```lua
-- 개발용: 해상도와 창 모드를 바꾼다(설정 파일에는 저장하지 않는다). 오버레이가 ResizeBuffers 를 견디는지 볼 때 쓴다.
-- lab.ps1 -Vars @{ W = 1280; H = 720; MODE = 2 }   MODE: 0 전체 화면, 1 테두리 없는 전체 창, 2 창
local gus = StaticFindObject("/Script/Engine.Default__GameUserSettings"):GetGameUserSettings()
if not gus:IsValid() then print("NO_SETTINGS") return end
local before = gus:GetScreenResolution()
print("before", before.X, before.Y, "mode=" .. tostring(gus:GetFullscreenMode()))
gus:SetScreenResolution({ X = __W__, Y = __H__ })
gus:SetFullscreenMode(__MODE__)
gus:ApplyResolutionSettings(false)
local after = gus:GetScreenResolution()
print("after", after.X, after.Y, "mode=" .. tostring(gus:GetFullscreenMode()))
```

구문 확인:

```powershell
$tokens = $null; $errors = $null
[void][System.Management.Automation.Language.Parser]::ParseFile('E:\MLToybox\tools\capture-game.ps1', [ref]$tokens, [ref]$errors)
"parse errors: $($errors.Count)"
```

Expected: `parse errors: 0`

- [ ] **Step 8: 전체 확인과 커밋**

Run: `dotnet test E:/MLToybox/panel/MLToybox.sln`
Expected: 모두 통과(Lua 스펙에 `overlay_fixture_spec.lua`가 들어 있다)

```bash
git -C E:/MLToybox add mod/MLToybox tools
git -C E:/MLToybox commit -m "feat(overlay): load the overlay dll from Lua, status.overlay, deploy and capture tools" -m "Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 6: 오버레이 DLL — 후킹, 입력, 안전장치, 작업 스레드, 창과 "상태" 탭

화면 출력과 입력은 단위 테스트를 할 수 없다. 빌드한 뒤 게임에서 확인한다.

**Files:**
- Create: `native/overlay/dllmain.cpp`, `native/overlay/worker.h/.cpp`
- Create: `native/overlay/render/guard.h/.cpp`, `input.h/.cpp`, `imgui_layer.h/.cpp`, `dx12_hook.h/.cpp`
- Create: `native/overlay/ui/app.h/.cpp`, `window.h/.cpp`, `tabs.h`, `tab_status.cpp`
- Modify: `native/CMakeLists.txt`

**Interfaces:**
- Consumes: `overlay_core` 전부(Task 1~4). `mlt_core`의 `mlt::nowEpochSeconds()`, `mlt::readFileUtf8`, `mlt::writeFileAtomic`, `mlt::pinModuleContaining(const void*)`. MinHook(`MH_Initialize`, `MH_CreateHook`, `MH_EnableHook`, `MH_StatusToString`). Dear ImGui와 그 백엔드.
- Produces (이름공간 `mlt::ov`):
  - `overlay/ui/app.h`: `struct App : Session { std::mutex mutex; std::shared_ptr<const StatusDoc> status; OverlaySettings settings; bool settingsDirty; std::string reason; std::atomic<…> state, visible, wantMouse, wantKeyboard, toggleVk, scale, frames, koreanFont, applyWindowRect; }`, `App& app()`, `void disableOverlay(const std::string& reason)`, `void syncSettingsAtoms(App&)`
  - `overlay/ui/tabs.h`: `struct TabContext { App& app; const StatusDoc* status; bool inGame; long long now; }`, `void drawStatusTab(TabContext&)`, `void markDirty(App&)`, `void sendCommand(App&, Json)`
  - `overlay/ui/window.h`: `bool overlayWantsFrame(App&)`, `void drawOverlay(App&)`, `void notifyOverlayReady()`
  - `overlay/render/input.h`: `std::mutex& imguiMutex()`, `bool installWndProc(HWND)`
  - `overlay/render/guard.h`: `bool runGuarded(void (*fn)(void*), void* arg, unsigned long* code)`, `class TrackedLock`
  - `overlay/render/imgui_layer.h`: `initImGuiContext`, `initImGuiDx12`, `shutdownImGuiDx12`, `imguiSrvHeap`
  - `overlay/render/dx12_hook.h`: `bool installRenderHooks(std::string& err)`
  - `overlay/worker.h`: `void runOverlayWorker(void* selfModule)`
  - 산출물 `native/build/mltoybox_overlay.dll`

스레드와 잠금(스펙 3.3):
- 화면 스레드(Present 후킹): `imguiMutex()`와 `App::mutex`를 잡고 ImGui 프레임을 만든다. 파일을 만지지 않는다.
- 창 스레드(창 프로시저): 토글 키를 처리하고, 창이 열려 있으면 `imguiMutex()` 아래에서 `ImGui_ImplWin32_WndProcHandler`를 부른다. Win32의 캡처·키 상태 함수는 창을 가진 스레드에서 불러야 하기 때문이다. 게임에 넘길지는 직전 프레임의 `wantMouse`/`wantKeyboard`로 정한다.
- 작업 스레드: 후킹 설치, 파일 입출력. `App::mutex`만 잡는다.
- 구조적 예외(접근 위반 등)가 나면 C++ 소멸자가 불리지 않아 잠금이 풀리지 않는다. `TrackedLock`이 잠금을 쥐고 있는지 적어 두고, 예외 처리기(`afterCrash`)가 직접 푼다.

- [ ] **Step 1: CMake에 `imgui`와 `mltoybox_overlay` 추가**

<!-- edit: native/CMakeLists.txt -->
`native/CMakeLists.txt` — 찾을 부분:

```cmake
target_link_libraries(overlay_core PUBLIC mlt_core)
```

바꿀 내용:

```cmake
target_link_libraries(overlay_core PUBLIC mlt_core)

# Dear ImGui (third_party/imgui/VERSION.txt 의 태그로 고정)
add_library(imgui STATIC
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_widgets.cpp
    third_party/imgui/backends/imgui_impl_dx12.cpp
    third_party/imgui/backends/imgui_impl_win32.cpp)
target_include_directories(imgui PUBLIC third_party/imgui third_party/imgui/backends)

# mltoybox_overlay: 렌더·입력·화면. 게임 안에서만 확인할 수 있다
add_library(mltoybox_overlay SHARED
    overlay/dllmain.cpp
    overlay/worker.cpp
    overlay/render/dx12_hook.cpp
    overlay/render/imgui_layer.cpp
    overlay/render/input.cpp
    overlay/render/guard.cpp
    overlay/ui/app.cpp
    overlay/ui/window.cpp
    overlay/ui/tab_status.cpp)
target_link_libraries(mltoybox_overlay PRIVATE overlay_core imgui minhook d3d12 dxgi user32)
```

- [ ] **Step 2: 나눠 쓰는 상태**

<!-- file: native/overlay/ui/app.h -->
```cpp
#pragma once
#include "overlay/core/bridge.h"
#include "overlay/core/session.h"
#include "overlay/core/settings.h"
#include "overlay/core/status_file.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <string>

// 화면(RHI 스레드), 창 프로시저(게임 스레드), 작업 스레드가 나눠 쓰는 상태.
// 잠금 순서: imguiMutex(render/input) -> App::mutex. 작업 스레드는 App::mutex 만 잡는다.
namespace mlt::ov {

// Session(설정 문서, 변경 표시, 보낼 명령, 마지막 seq, 저장 실패)도 mutex 로 보호한다
struct App : Session {
    std::mutex mutex;

    // --- mutex 로 보호 ---
    std::shared_ptr<const StatusDoc> status;   // 최신 status.json. 없으면 비어 있다
    OverlaySettings settings;
    bool settingsDirty = false;
    std::string reason;                        // state 가 Disabled 일 때의 이유

    // --- 잠금 없이 읽고 쓴다 ---
    std::atomic<OverlayState> state{OverlayState::Starting};
    std::atomic<bool> visible{false};
    std::atomic<bool> wantMouse{false};        // 직전 프레임에 ImGui 가 마우스를 원했는가
    std::atomic<bool> wantKeyboard{false};
    std::atomic<int> toggleVk{0x2D};
    std::atomic<float> scale{1.0f};
    std::atomic<long long> frames{0};
    std::atomic<bool> koreanFont{false};
    std::atomic<bool> applyWindowRect{true};   // 설정의 창 위치·크기를 다음 프레임에 적용한다
};

App& app();

// 오버레이를 끈다(그 세션에서 다시 그리지 않는다). 이유는 overlay_status.json 에 적힌다
void disableOverlay(const std::string& reason);

// 설정을 바꾼 뒤 원자 값(토글 키, 배율)을 맞춘다. mutex 를 잡은 채로 부른다
void syncSettingsAtoms(App& a);
}
```

<!-- file: native/overlay/ui/app.cpp -->
```cpp
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
    std::lock_guard<std::mutex> lock(a.mutex);
    a.reason = reason;
}

void syncSettingsAtoms(App& a) {
    a.toggleVk = toggleKeyCode(a.settings.toggleKey);
    a.scale = a.settings.scale;
}

}
```

- [ ] **Step 3: 예외 보호와 입력**

<!-- file: native/overlay/render/guard.h -->
```cpp
#pragma once
#include <mutex>

namespace mlt::ov {

// fn(arg) 를 구조적 예외(접근 위반 등)로 감싼다. 예외가 나면 false 를 돌려주고 code 에 예외 코드를 적는다
bool runGuarded(void (*fn)(void*), void* arg, unsigned long* code);

// 구조적 예외가 나면 C++ 소멸자가 불리지 않아 잠금이 풀리지 않는다.
// 잠금을 쥐고 있는지를 held 에 적어 두고, 예외 처리기가 held 가 참인 잠금을 직접 푼다
class TrackedLock {
public:
    TrackedLock(std::mutex& m, bool& held) : m_(m), held_(held) { m_.lock(); held_ = true; }
    ~TrackedLock() { held_ = false; m_.unlock(); }
    TrackedLock(const TrackedLock&) = delete;
    TrackedLock& operator=(const TrackedLock&) = delete;
private:
    std::mutex& m_;
    bool& held_;
};
}
```

<!-- file: native/overlay/render/guard.cpp -->
```cpp
#include "guard.h"
#include <windows.h>

namespace mlt::ov {

// __try 를 쓰는 함수에는 소멸자가 있는 지역 객체를 둘 수 없다. 그래서 이 함수는 호출만 한다
bool runGuarded(void (*fn)(void*), void* arg, unsigned long* code) {
    __try {
        fn(arg);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        if (code) *code = GetExceptionCode();
        return false;
    }
}

}
```

<!-- file: native/overlay/render/input.h -->
```cpp
#pragma once
#include <mutex>
#include <windows.h>

namespace mlt::ov {
// ImGui 는 스레드 안전하지 않다. 화면 스레드(프레임)와 창 스레드(입력)가 이 잠금 아래에서만 ImGui 를 부른다
std::mutex& imguiMutex();
// 게임 창의 창 프로시저를 바꿔 토글 키와 입력을 가로챈다. 이전 프로시저는 이어 부른다
bool installWndProc(HWND hwnd);
}
```

<!-- file: native/overlay/render/input.cpp -->
```cpp
#include "input.h"
#include "overlay/ui/app.h"
#include <imgui.h>
#include <imgui_impl_win32.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace mlt::ov {

std::mutex& imguiMutex() {
    static std::mutex m;
    return m;
}

namespace {
WNDPROC g_original = nullptr;

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
        {
            // Win32 의 캡처·키 상태 함수는 창을 가진 스레드에서 불러야 한다. 그래서 입력은 이 스레드에서 ImGui 에 넣는다
            std::lock_guard<std::mutex> lock(imguiMutex());
            if (ImGui::GetCurrentContext()) ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);
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
```

- [ ] **Step 4: ImGui 초기화**

Dear ImGui 1.92의 DX12 백엔드는 텍스처마다 SRV 서술자를 달라고 하므로 작은 자유 목록으로 내준다. 글꼴은 크기를 주지 않고 올리고(`AddFontFromFileTTF(path)`), 기본 크기는 `style.FontSizeBase`, 배율은 `style.FontScaleMain`으로 정한다(1.92의 방식).

<!-- file: native/overlay/render/imgui_layer.h -->
```cpp
#pragma once
#include <d3d12.h>
#include <dxgiformat.h>
#include <string>
#include <windows.h>

namespace mlt::ov {
// ImGui 컨텍스트, 글꼴(맑은 고딕), 스타일, win32 백엔드. 한 번만 부른다
bool initImGuiContext(HWND hwnd, std::string& err);
// DX12 백엔드와 그 SRV 힙. 백버퍼 형식이나 개수가 바뀌면 shutdown 뒤 다시 부른다
bool initImGuiDx12(ID3D12Device* device, ID3D12CommandQueue* queue, int framesInFlight, DXGI_FORMAT format, std::string& err);
void shutdownImGuiDx12();
ID3D12DescriptorHeap* imguiSrvHeap();
}
```

<!-- file: native/overlay/render/imgui_layer.cpp -->
```cpp
#include "imgui_layer.h"
#include "overlay/ui/app.h"
#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>
#include <vector>

namespace mlt::ov {

namespace {
constexpr UINT kSrvCount = 64;
constexpr float kFontSize = 18.0f;

ID3D12Device* g_device = nullptr;
ID3D12DescriptorHeap* g_srvHeap = nullptr;
std::vector<UINT> g_free;

// ImGui 1.92 의 DX12 백엔드는 텍스처마다 SRV 서술자를 달라고 한다. 작은 자유 목록으로 내준다
void srvAlloc(ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* cpu, D3D12_GPU_DESCRIPTOR_HANDLE* gpu) {
    cpu->ptr = 0;
    gpu->ptr = 0;
    if (g_free.empty() || !g_srvHeap) return;
    const UINT index = g_free.back();
    g_free.pop_back();
    const UINT step = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    cpu->ptr = g_srvHeap->GetCPUDescriptorHandleForHeapStart().ptr + static_cast<SIZE_T>(index) * step;
    gpu->ptr = g_srvHeap->GetGPUDescriptorHandleForHeapStart().ptr + static_cast<UINT64>(index) * step;
}

void srvFree(ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_GPU_DESCRIPTOR_HANDLE) {
    if (!g_srvHeap || !cpu.ptr) return;
    const UINT step = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    g_free.push_back(static_cast<UINT>((cpu.ptr - g_srvHeap->GetCPUDescriptorHandleForHeapStart().ptr) / step));
}

std::string koreanFontPath() {
    char dir[MAX_PATH]{};
    if (!GetWindowsDirectoryA(dir, MAX_PATH)) return {};
    std::string path = std::string(dir) + "\\Fonts\\malgun.ttf";
    return GetFileAttributesA(path.c_str()) == INVALID_FILE_ATTRIBUTES ? std::string() : path;
}
}

bool initImGuiContext(HWND hwnd, std::string& err) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;   // 창 위치·크기는 overlay.json 에 우리가 저장한다
    io.LogFilename = nullptr;

    const std::string font = koreanFontPath();
    bool korean = false;
    if (!font.empty()) korean = io.Fonts->AddFontFromFileTTF(font.c_str()) != nullptr;
    if (!korean) io.Fonts->AddFontDefault();
    app().koreanFont = korean;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.FontSizeBase = kFontSize;
    style.WindowRounding = 4.0f;
    style.FrameRounding = 3.0f;
    style.WindowBorderSize = 1.0f;

    if (!ImGui_ImplWin32_Init(hwnd)) {
        err = "ImGui_ImplWin32_Init";
        return false;
    }
    return true;
}

bool initImGuiDx12(ID3D12Device* device, ID3D12CommandQueue* queue, int framesInFlight, DXGI_FORMAT format, std::string& err) {
    g_device = device;
    D3D12_DESCRIPTOR_HEAP_DESC desc{};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = kSrvCount;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_srvHeap)))) {
        err = "srv heap";
        return false;
    }
    g_free.clear();
    for (UINT i = kSrvCount; i > 0; --i) g_free.push_back(i - 1);

    ImGui_ImplDX12_InitInfo info;
    info.Device = device;
    info.CommandQueue = queue;
    info.NumFramesInFlight = framesInFlight;
    info.RTVFormat = format;
    info.SrvDescriptorHeap = g_srvHeap;
    info.SrvDescriptorAllocFn = srvAlloc;
    info.SrvDescriptorFreeFn = srvFree;
    if (!ImGui_ImplDX12_Init(&info)) {
        err = "ImGui_ImplDX12_Init";
        g_srvHeap->Release();
        g_srvHeap = nullptr;
        return false;
    }
    return true;
}

void shutdownImGuiDx12() {
    ImGui_ImplDX12_Shutdown();
    if (g_srvHeap) {
        g_srvHeap->Release();
        g_srvHeap = nullptr;
    }
    g_free.clear();
}

ID3D12DescriptorHeap* imguiSrvHeap() {
    return g_srvHeap;
}

}
```

- [ ] **Step 5: 화면 — 창, 상태 줄, 탭 막대, "상태" 탭**

<!-- file: native/overlay/ui/window.h -->
```cpp
#pragma once

namespace mlt::ov {
struct App;

// 그릴 것이 있는가(창이 열려 있거나 안내가 떠 있다). App::mutex 를 잡은 채로 부른다
bool overlayWantsFrame(App& a);
// 메인 창과 안내를 그린다. ImGui 프레임 안에서, App::mutex 를 잡은 채로 부른다
void drawOverlay(App& a);
// 그릴 준비가 끝났을 때 한 번 부른다(안내 표시 시작)
void notifyOverlayReady();
}
```

<!-- file: native/overlay/ui/tabs.h -->
```cpp
#pragma once
#include "app.h"

// 탭 하나가 파일 하나다. 모두 ImGui 프레임 안에서, App::mutex 를 잡은 채로 불린다.
// status 는 최신 status.json(없으면 nullptr), inGame 은 게임 안이고 모드가 응답 중일 때 참
namespace mlt::ov {

struct TabContext {
    App& app;
    const StatusDoc* status;   // nullptr 가능
    bool inGame;
    long long now;             // UTC 초
};

void drawStatusTab(TabContext& ctx);

// 설정 문서를 바꿨다고 표시한다(작업 스레드가 0.2초 안에 저장한다)
inline void markDirty(App& a) { a.dirty = true; }
// 일회성 명령을 다음 저장에 실어 보낸다
inline void sendCommand(App& a, Json command) {
    a.commands.push_back(std::move(command));
    a.dirty = true;
}
}
```

<!-- file: native/overlay/ui/window.cpp -->
```cpp
#include "window.h"
#include "runtime.h"
#include "tabs.h"
#include <algorithm>
#include <cmath>
#include <imgui.h>
#include <windows.h>

namespace mlt::ov {

namespace {
constexpr ULONGLONG kHintMs = 8000;
ULONGLONG g_readyTick = 0;

struct Tab {
    const char* name;
    void (*draw)(TabContext&);
};
// 패널과 같은 이름. 탭은 뒤 태스크에서 더한다
const Tab kTabs[] = {
    { "상태", drawStatusTab },
};

const ImVec4 kGreen(0.35f, 0.80f, 0.45f, 1.0f);
const ImVec4 kOrange(1.00f, 0.65f, 0.20f, 1.0f);
const ImVec4 kBlue(0.45f, 0.65f, 0.95f, 1.0f);
const ImVec4 kRed(0.95f, 0.40f, 0.35f, 1.0f);

bool hintActive() {
    return g_readyTick != 0 && GetTickCount64() - g_readyTick < kHintMs;
}

void drawHint(App& a) {
    ImGui::SetNextWindowPos(ImVec2(16.0f, 16.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.6f);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize
        | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
    if (ImGui::Begin("##mltoybox_hint", nullptr, flags)) ImGui::Text("%s: MLToybox", a.settings.toggleKey.c_str());
    ImGui::End();
}

void drawStateLine(App& a, BridgeState state) {
    if (a.saveFailed) {
        ImGui::TextColored(kRed, "● 저장 실패 (다시 시도 중)");
        return;
    }
    switch (state) {
        case BridgeState::Applied: ImGui::TextColored(kGreen, "● 적용됨"); break;
        case BridgeState::Pending: ImGui::TextColored(kOrange, "● 적용 대기 중"); break;
        case BridgeState::MainMenu: ImGui::TextColored(kBlue, "● 메인 메뉴"); break;
        case BridgeState::Disconnected: ImGui::TextColored(kRed, "● 모드 응답 없음"); break;
    }
}

// 설정에 적힌 창 위치·크기를 화면 안으로 맞춰 적용한다
void applyWindowRect(App& a) {
    const ImVec2 display = ImGui::GetMainViewport()->Size;
    OverlaySettings& s = a.settings;
    const float w = std::min(static_cast<float>(s.w), std::max(320.0f, display.x));
    const float h = std::min(static_cast<float>(s.h), std::max(240.0f, display.y));
    const float x = std::clamp(static_cast<float>(s.x), 0.0f, std::max(0.0f, display.x - w));
    const float y = std::clamp(static_cast<float>(s.y), 0.0f, std::max(0.0f, display.y - h));
    ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(w, h), ImGuiCond_Always);
}

// 창을 옮기거나 크기를 바꿨으면 설정에 적는다(작업 스레드가 1초 안에 저장한다)
void rememberWindowRect(App& a) {
    const ImVec2 pos = ImGui::GetWindowPos();
    const ImVec2 size = ImGui::GetWindowSize();
    const int x = static_cast<int>(std::lround(pos.x)), y = static_cast<int>(std::lround(pos.y));
    const int w = static_cast<int>(std::lround(size.x)), h = static_cast<int>(std::lround(size.y));
    OverlaySettings& s = a.settings;
    if (x == s.x && y == s.y && w == s.w && h == s.h) return;
    s.x = x;
    s.y = y;
    s.w = w;
    s.h = h;
    a.settingsDirty = true;
}

void drawMainWindow(App& a) {
    if (a.applyWindowRect.exchange(false)) applyWindowRect(a);
    bool open = true;
    if (ImGui::Begin("MLToybox", &open, ImGuiWindowFlags_NoCollapse)) {
        const long long now = nowEpochSeconds();
        const StatusDoc* status = a.status.get();
        const BridgeState state = Bridge::evaluate(status, a.lastSentSeq, now);
        drawStateLine(a, state);
        ImGui::Separator();
        TabContext ctx{ a, status, state == BridgeState::Applied || state == BridgeState::Pending, now };
        if (ImGui::BeginTabBar("##mltoybox_tabs")) {
            for (const Tab& tab : kTabs) {
                // 개발용 설정 devTab 이 있으면 그 탭을 연다(마우스 없이 화면을 캡처해 확인하기 위한 것)
                const ImGuiTabItemFlags flags = a.settings.devTab == tab.name ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
                if (ImGui::BeginTabItem(tab.name, nullptr, flags)) {
                    ImGui::BeginChild("##body");
                    tab.draw(ctx);
                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }
            }
            ImGui::EndTabBar();
        }
    }
    rememberWindowRect(a);
    ImGui::End();
    if (!open) a.visible = false;
}
}

void notifyOverlayReady() {
    g_readyTick = GetTickCount64();
}

bool overlayWantsFrame(App& a) {
    return a.visible.load() || hintActive();
}

void drawOverlay(App& a) {
    if (a.visible.load()) drawMainWindow(a);
    else if (hintActive()) drawHint(a);
}

}
```

<!-- file: native/overlay/ui/tab_status.cpp -->
```cpp
#include "tabs.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <imgui.h>
#include <initializer_list>
#include <string>

namespace mlt::ov {

namespace {
std::string localTime(long long epochSeconds) {
    const std::time_t t = static_cast<std::time_t>(epochSeconds);
    std::tm tm{};
    if (localtime_s(&tm, &t) != 0) return "-";
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
    return buf;
}

const char* orDash(const std::optional<std::string>& s) {
    return s ? s->c_str() : "-";
}

// 이름 / 값 … 표의 한 줄. 열 수는 BeginTable 에 준 수와 같아야 한다
void row(std::initializer_list<const char*> cells) {
    ImGui::TableNextRow();
    for (const char* cell : cells) {
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(cell);
    }
}

void drawModStatus(TabContext& ctx) {
    const StatusDoc* s = ctx.status;
    if (!s) {
        ImGui::TextUnformatted("status.json 없음");
        return;
    }
    ImGui::Text("heartbeat: %s", localTime(s->heartbeat).c_str());
    ImGui::Text("inGame: %s", s->inGame ? "true" : "false");
    if (s->appliedSeq) ImGui::Text("appliedSeq: %lld / sent: %lld", *s->appliedSeq, ctx.app.lastSentSeq);
    else ImGui::Text("appliedSeq: - / sent: %lld", ctx.app.lastSentSeq);
    ImGui::Text("bridgeError: %s", orDash(s->bridgeError));

    ImGui::SeparatorText("기능");
    if (!s->features) ImGui::TextUnformatted("(모드에 등록된 기능 없음)");
    else if (ImGui::BeginTable("##features", 3, ImGuiTableFlags_SizingFixedFit)) {
        for (const auto& [name, f] : *s->features) {
            row({ name.c_str(), f.active ? "active=true" : "active=false", (std::string("error=") + orDash(f.lastError)).c_str() });
        }
        ImGui::EndTable();
    }

    if (s->commands) {
        ImGui::SeparatorText("명령 결과");
        for (const auto& [id, r] : *s->commands) {
            std::string line = id.substr(0, 8) + (r.ok ? " 성공" : " 실패");
            if (r.squads) {
                line += " 분대";
                for (size_t i = 0; i < r.squads->size(); ++i) line += (i ? "," : " ") + std::to_string((*r.squads)[i]);
            }
            if (r.reformed) line += " 재구성 " + std::to_string(*r.reformed) + "개";
            if (r.added) line += " 가족 " + std::to_string(*r.added) + "/" + std::to_string(r.requested.value_or(0));
            if (r.error) line += " " + *r.error;
            ImGui::TextUnformatted(line.c_str());
        }
    }

    ImGui::SeparatorText("네이티브");
    if (!s->native) ImGui::TextUnformatted("(정보 없음)");
    else {
        const NativeStatus& n = *s->native;
        if (!n.loaded) ImGui::Text("미로드: %s", orDash(n.error));
        else ImGui::TextUnformatted(n.stale ? "응답 없음 (heartbeat 끊김)" : "동작 중");
        if (!n.features.empty() && ImGui::BeginTable("##native", 4, ImGuiTableFlags_SizingFixedFit)) {
            for (const auto& [name, f] : n.features) {
                row({ name.c_str(), f.installed ? "installed=true" : "installed=false", f.active ? "active=true" : "active=false",
                      (std::string("error=") + orDash(f.lastError)).c_str() });
            }
            ImGui::EndTable();
        }
    }
}

void drawOverlaySettings(App& a) {
    ImGui::SeparatorText("오버레이");
    ImGui::Text("빌드: %s", __DATE__);
    ImGui::Text("글꼴: %s", a.koreanFont.load() ? "맑은 고딕" : "기본 글꼴(한글이 표시되지 않습니다)");

    const float width = 160.0f * ImGui::GetStyle().FontScaleMain;
    ImGui::SetNextItemWidth(width);
    if (ImGui::BeginCombo("여닫는 키", a.settings.toggleKey.c_str())) {
        for (const std::string& name : toggleKeyNames()) {
            const bool selected = name == a.settings.toggleKey;
            if (ImGui::Selectable(name.c_str(), selected) && !selected) {
                a.settings.toggleKey = name;
                syncSettingsAtoms(a);
                a.settingsDirty = true;
            }
        }
        ImGui::EndCombo();
    }

    int percent = static_cast<int>(std::lround(a.settings.scale * 100.0f));
    ImGui::SetNextItemWidth(width);
    if (ImGui::SliderInt("글자 배율", &percent, 80, 150, "%d%%")) {
        percent = std::clamp((percent + 5) / 10 * 10, 80, 150);   // 10% 단위
        const float scale = static_cast<float>(percent) / 100.0f;
        if (std::fabs(scale - a.settings.scale) > 0.001f) {
            a.settings.scale = scale;
            syncSettingsAtoms(a);
            a.settingsDirty = true;
        }
    }
}
}

void drawStatusTab(TabContext& ctx) {
    drawModStatus(ctx);
    drawOverlaySettings(ctx.app);
}

}
```

- [ ] **Step 6: DX12 후킹**

스파이크에서 확인한 것: 이 게임에는 직접(DIRECT) 명령 큐가 둘 있고, 화면 출력용이 아닌 큐로 그리면 GPU 크래시가 난다. `ExecuteCommandLists` 후킹에서 큐마다 마지막으로 실행한 스레드를 적어 두고, `Present`를 부른 스레드와 같은 스레드의 큐를 쓴다. 가상 함수 표의 위치는 `IDXGISwapChain::Present` 8, `ResizeBuffers` 13, `ID3D12CommandQueue::ExecuteCommandLists` 10이다.

<!-- file: native/overlay/render/dx12_hook.h -->
```cpp
#pragma once
#include <string>

namespace mlt::ov {
// DXGI 스왑체인의 Present / ResizeBuffers 와 D3D12 명령 큐의 ExecuteCommandLists 를 후킹한다.
// 게임 코드의 주소가 아니라 DXGI·D3D12 의 가상 함수 표에 걸므로 게임이 업데이트돼도 그대로 쓴다.
bool installRenderHooks(std::string& err);
}
```

<!-- file: native/overlay/render/dx12_hook.cpp -->
```cpp
#include "dx12_hook.h"
#include "guard.h"
#include "imgui_layer.h"
#include "input.h"
#include "overlay/ui/app.h"
#include "overlay/ui/window.h"
#include <MinHook.h>
#include <atomic>
#include <cstdio>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <exception>
#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>
#include <stdexcept>
#include <string>
#include <vector>
#include <windows.h>

// findings "게임 안 오버레이 창 — DX12 후킹 + Dear ImGui (2026-10-01, 스파이크)" 를 따른다.
namespace mlt::ov {
namespace {

using PresentFn = HRESULT(WINAPI*)(IDXGISwapChain*, UINT, UINT);
using ResizeBuffersFn = HRESULT(WINAPI*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
using ExecuteFn = void(WINAPI*)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);

constexpr ULONGLONG kQueueTimeoutMs = 20000;

PresentFn g_origPresent = nullptr;
ResizeBuffersFn g_origResize = nullptr;
ExecuteFn g_origExecute = nullptr;

// 직접(DIRECT) 큐와 그 큐를 마지막으로 실행한 스레드. 이 게임에는 직접 큐가 둘 있고, 화면 출력용이 아닌 큐로 그리면 GPU 크래시가 난다
struct SeenQueue {
    ID3D12CommandQueue* queue;
    DWORD thread;
};
std::mutex g_seenMutex;
SeenQueue g_seen[8] = {};
int g_seenCount = 0;
std::atomic<bool> g_recordQueues{true};

IDXGISwapChain* g_swap = nullptr;   // 우리가 그리는 스왑체인
ID3D12Device* g_device = nullptr;
ID3D12CommandQueue* g_queue = nullptr;
ID3D12DescriptorHeap* g_rtvHeap = nullptr;
ID3D12GraphicsCommandList* g_list = nullptr;
std::vector<ID3D12CommandAllocator*> g_allocators;
std::vector<ID3D12Resource*> g_backBuffers;
std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> g_rtvs;
UINT g_bufferCount = 0;
DXGI_FORMAT g_format = DXGI_FORMAT_UNKNOWN;
bool g_contextReady = false;        // ImGui 컨텍스트와 창 프로시저
bool g_initialized = false;
ULONGLONG g_firstPresentTick = 0;

// 구조적 예외가 났을 때 처리기가 풀어야 하는 잠금(guard.h 의 TrackedLock)
bool g_holdsImgui = false;
bool g_holdsApp = false;

std::string hex(unsigned long value) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "0x%08lX", value);
    return buf;
}

// 이 프로세스의 보이는 최상위 창 가운데 가장 큰 것
HWND mainWindow() {
    struct Best {
        HWND hwnd = nullptr;
        long long area = 0;
    } best;
    EnumWindows([](HWND hwnd, LPARAM param) -> BOOL {
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid != GetCurrentProcessId() || !IsWindowVisible(hwnd)) return TRUE;
        RECT r{};
        GetWindowRect(hwnd, &r);
        const long long area = static_cast<long long>(r.right - r.left) * (r.bottom - r.top);
        auto* b = reinterpret_cast<Best*>(param);
        if (area > b->area) {
            b->area = area;
            b->hwnd = hwnd;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&best));
    return best.hwnd;
}

void releaseBackBuffers() {
    for (auto* r : g_backBuffers) {
        if (r) r->Release();
    }
    g_backBuffers.clear();
    g_rtvs.clear();
}

bool createBackBuffers(IDXGISwapChain* swap) {
    const UINT step = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE handle = g_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < g_bufferCount; ++i) {
        ID3D12Resource* buffer = nullptr;
        if (FAILED(swap->GetBuffer(i, IID_PPV_ARGS(&buffer)))) {
            releaseBackBuffers();
            return false;
        }
        g_device->CreateRenderTargetView(buffer, nullptr, handle);
        g_backBuffers.push_back(buffer);
        g_rtvs.push_back(handle);
        handle.ptr += step;
    }
    return true;
}

void destroyFrameObjects() {
    releaseBackBuffers();
    if (g_list) { g_list->Release(); g_list = nullptr; }
    for (auto* a : g_allocators) a->Release();
    g_allocators.clear();
    if (g_rtvHeap) { g_rtvHeap->Release(); g_rtvHeap = nullptr; }
}

// 백버퍼 수에 맞춰 RTV 힙, 명령 할당자, 명령 목록을 만든다
bool createFrameObjects(std::string& err) {
    D3D12_DESCRIPTOR_HEAP_DESC rtv{};
    rtv.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtv.NumDescriptors = g_bufferCount;
    if (FAILED(g_device->CreateDescriptorHeap(&rtv, IID_PPV_ARGS(&g_rtvHeap)))) { err = "rtv heap"; return false; }
    for (UINT i = 0; i < g_bufferCount; ++i) {
        ID3D12CommandAllocator* allocator = nullptr;
        if (FAILED(g_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)))) { err = "command allocator"; return false; }
        g_allocators.push_back(allocator);
    }
    if (FAILED(g_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_allocators[0], nullptr, IID_PPV_ARGS(&g_list)))) { err = "command list"; return false; }
    g_list->Close();
    return true;
}

// Present 를 부르는 지금 스레드에서 실행된 직접 큐
ID3D12CommandQueue* queueForThisThread() {
    const DWORD tid = GetCurrentThreadId();
    std::lock_guard<std::mutex> lock(g_seenMutex);
    for (int i = 0; i < g_seenCount; ++i) {
        if (g_seen[i].thread == tid) return g_seen[i].queue;
    }
    return nullptr;
}

void tryInit(IDXGISwapChain* swap) {
    DXGI_SWAP_CHAIN_DESC desc{};
    if (FAILED(swap->GetDesc(&desc)) || !desc.OutputWindow) return;
    if (desc.OutputWindow != mainWindow()) return;   // 게임 메인 창의 스왑체인만
    if (!g_firstPresentTick) g_firstPresentTick = GetTickCount64();   // 큐 찾기 제한 시간은 메인 창에 처음 그릴 때부터 잰다

    ID3D12CommandQueue* queue = queueForThisThread();
    if (!queue) {
        if (GetTickCount64() - g_firstPresentTick > kQueueTimeoutMs) disableOverlay("present queue not found");
        return;
    }

    std::string err;
    g_queue = queue;
    if (FAILED(g_queue->GetDevice(IID_PPV_ARGS(&g_device)))) { disableOverlay("init failed: queue device"); return; }
    g_bufferCount = desc.BufferCount;
    g_format = desc.BufferDesc.Format;
    if (!createFrameObjects(err)) { disableOverlay("init failed: " + err); return; }
    if (!g_contextReady) {
        if (!initImGuiContext(desc.OutputWindow, err)) { disableOverlay("init failed: " + err); return; }
        if (!installWndProc(desc.OutputWindow)) { disableOverlay("init failed: window procedure"); return; }
        g_contextReady = true;
    }
    if (!initImGuiDx12(g_device, g_queue, static_cast<int>(g_bufferCount), g_format, err)) { disableOverlay("init failed: " + err); return; }

    g_swap = swap;
    g_initialized = true;
    g_recordQueues = false;
    notifyOverlayReady();
    OverlayState expected = OverlayState::Waiting;
    if (!app().state.compare_exchange_strong(expected, OverlayState::Ready)) {
        expected = OverlayState::Starting;
        app().state.compare_exchange_strong(expected, OverlayState::Ready);
    }
}

void renderFrame(IDXGISwapChain* swap) {
    App& a = app();
    {
        TrackedLock imguiLock(imguiMutex(), g_holdsImgui);
        TrackedLock appLock(a.mutex, g_holdsApp);
        ImGui::GetStyle().FontScaleMain = a.scale.load();
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        drawOverlay(a);
        ImGui::Render();
        const ImGuiIO& io = ImGui::GetIO();
        const bool visible = a.visible.load();
        a.wantMouse = visible && io.WantCaptureMouse;
        a.wantKeyboard = visible && (io.WantCaptureKeyboard || io.WantTextInput);
    }

    UINT index = 0;
    IDXGISwapChain3* swap3 = nullptr;
    if (FAILED(swap->QueryInterface(IID_PPV_ARGS(&swap3)))) return;
    index = swap3->GetCurrentBackBufferIndex();
    swap3->Release();
    if (index >= g_backBuffers.size()) return;

    ID3D12CommandAllocator* allocator = g_allocators[index];
    allocator->Reset();
    g_list->Reset(allocator, nullptr);
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = g_backBuffers[index];
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    g_list->ResourceBarrier(1, &barrier);
    g_list->OMSetRenderTargets(1, &g_rtvs[index], FALSE, nullptr);
    ID3D12DescriptorHeap* heaps[] = { imguiSrvHeap() };
    g_list->SetDescriptorHeaps(1, heaps);
    {
        TrackedLock imguiLock(imguiMutex(), g_holdsImgui);   // 그리기 자료는 다음 NewFrame 전까지 유효하지만, 텍스처 갱신이 ImGui 상태를 만진다
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), g_list);
    }
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    g_list->ResourceBarrier(1, &barrier);
    g_list->Close();
    ID3D12CommandList* lists[] = { g_list };
    g_queue->ExecuteCommandLists(1, lists);
    ++a.frames;
}

void frame(IDXGISwapChain* swap) {
    App& a = app();
    if (!g_initialized) {
        tryInit(swap);
        if (!g_initialized) return;
    }
    if (swap != g_swap) return;
    const HRESULT removed = g_device->GetDeviceRemovedReason();
    if (removed != S_OK) {
        disableOverlay("device removed " + hex(static_cast<unsigned long>(removed)));
        return;
    }
    bool wanted;
    {
        TrackedLock appLock(a.mutex, g_holdsApp);
        wanted = overlayWantsFrame(a);
    }
    if (!wanted) {
        a.wantMouse = false;
        a.wantKeyboard = false;
        return;
    }
    if (g_backBuffers.empty() && !createBackBuffers(swap)) return;
    renderFrame(swap);
}

// C++ 예외는 여기서 잡는다(소멸자가 돌아 잠금이 풀린다). 구조적 예외는 runGuarded 가 잡는다
void frameThunk(void* swap) {
    try {
        frame(static_cast<IDXGISwapChain*>(swap));
    } catch (const std::exception& e) {
        disableOverlay(std::string("frame exception ") + e.what());
    } catch (...) {
        disableOverlay("frame exception (unknown)");
    }
}

// 구조적 예외 뒤처리: 프레임 코드가 쥐고 있던 잠금을 풀고 오버레이를 끈다
void afterCrash(unsigned long code) {
    if (g_holdsApp) { g_holdsApp = false; app().mutex.unlock(); }
    if (g_holdsImgui) { g_holdsImgui = false; imguiMutex().unlock(); }
    disableOverlay("frame exception " + hex(code));
}

HRESULT WINAPI HookedPresent(IDXGISwapChain* swap, UINT sync, UINT flags) {
    if (app().state.load() != OverlayState::Disabled) {
        unsigned long code = 0;
        if (!runGuarded(frameThunk, swap, &code)) afterCrash(code);
    }
    return g_origPresent(swap, sync, flags);
}

struct ResizeArgs {
    IDXGISwapChain* swap;
    bool after;
};

// 크기를 바꾸기 전에 백버퍼 참조를 놓고, 바꾼 뒤 개수나 형식이 달라졌으면 그에 맞춰 다시 만든다
void resizeThunk(void* p) {
    auto* args = static_cast<ResizeArgs*>(p);
    if (!g_initialized || args->swap != g_swap) return;
    TrackedLock imguiLock(imguiMutex(), g_holdsImgui);
    if (!args->after) {
        releaseBackBuffers();
        return;
    }
    app().applyWindowRect = true;   // 해상도가 줄었으면 창을 화면 안으로 옮긴다
    DXGI_SWAP_CHAIN_DESC desc{};
    if (FAILED(args->swap->GetDesc(&desc))) return;
    if (desc.BufferCount == g_bufferCount && desc.BufferDesc.Format == g_format) return;   // 백버퍼는 다음 프레임에 다시 얻는다
    std::string err;
    shutdownImGuiDx12();
    destroyFrameObjects();
    g_bufferCount = desc.BufferCount;
    g_format = desc.BufferDesc.Format;
    if (!createFrameObjects(err) || !initImGuiDx12(g_device, g_queue, static_cast<int>(g_bufferCount), g_format, err)) {
        g_initialized = false;
        throw std::runtime_error(err);
    }
}

void guardedResize(IDXGISwapChain* swap, bool after) {
    if (app().state.load() == OverlayState::Disabled) return;
    ResizeArgs args{ swap, after };
    unsigned long code = 0;
    auto thunk = [](void* p) {
        try {
            resizeThunk(p);
        } catch (const std::exception& e) {
            disableOverlay(std::string("init failed: ") + e.what());
        }
    };
    if (!runGuarded(thunk, &args, &code)) afterCrash(code);
}

HRESULT WINAPI HookedResizeBuffers(IDXGISwapChain* swap, UINT count, UINT width, UINT height, DXGI_FORMAT format, UINT flags) {
    guardedResize(swap, false);
    const HRESULT hr = g_origResize(swap, count, width, height, format, flags);
    guardedResize(swap, true);
    return hr;
}

void WINAPI HookedExecute(ID3D12CommandQueue* queue, UINT count, ID3D12CommandList* const* lists) {
    if (g_recordQueues.load() && queue->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT) {
        const DWORD tid = GetCurrentThreadId();
        std::lock_guard<std::mutex> lock(g_seenMutex);
        int slot = -1;
        for (int i = 0; i < g_seenCount; ++i) {
            if (g_seen[i].queue == queue) slot = i;
        }
        if (slot < 0 && g_seenCount < 8) {
            slot = g_seenCount++;
            g_seen[slot].queue = queue;
        }
        if (slot >= 0) g_seen[slot].thread = tid;
    }
    g_origExecute(queue, count, lists);
}

// 더미 창·장치·스왑체인을 만들어 가상 함수 표에서 함수 주소를 얻는다
bool findVTables(void** present, void** resize, void** execute, std::string& err) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"mltoybox_overlay_probe";
    RegisterClassExW(&wc);
    HWND dummy = CreateWindowExW(0, wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);
    if (!dummy) {
        err = "probe window";
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return false;
    }
    IDXGIFactory4* factory = nullptr;
    ID3D12Device* device = nullptr;
    ID3D12CommandQueue* queue = nullptr;
    IDXGISwapChain1* swap = nullptr;
    bool ok = false;
    do {
        if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) { err = "dxgi factory"; break; }
        if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)))) { err = "d3d12 device"; break; }
        D3D12_COMMAND_QUEUE_DESC qd{};
        qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        if (FAILED(device->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue)))) { err = "command queue"; break; }
        DXGI_SWAP_CHAIN_DESC1 sd{};
        sd.BufferCount = 2;
        sd.Width = 100;
        sd.Height = 100;
        sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        sd.SampleDesc.Count = 1;
        if (FAILED(factory->CreateSwapChainForHwnd(queue, dummy, &sd, nullptr, nullptr, &swap))) { err = "probe swapchain"; break; }
        void** swapTable = *reinterpret_cast<void***>(swap);
        void** queueTable = *reinterpret_cast<void***>(queue);
        *present = swapTable[8];     // IDXGISwapChain::Present
        *resize = swapTable[13];     // IDXGISwapChain::ResizeBuffers
        *execute = queueTable[10];   // ID3D12CommandQueue::ExecuteCommandLists
        ok = true;
    } while (false);
    if (swap) swap->Release();
    if (queue) queue->Release();
    if (device) device->Release();
    if (factory) factory->Release();
    DestroyWindow(dummy);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return ok;
}

bool hook(void* target, void* detour, void** original, const char* name, std::string& err) {
    const MH_STATUS created = MH_CreateHook(target, detour, original);
    if (created != MH_OK) {
        err = std::string(name) + ": " + MH_StatusToString(created);
        return false;
    }
    const MH_STATUS enabled = MH_EnableHook(target);
    if (enabled != MH_OK) {
        err = std::string(name) + ": " + MH_StatusToString(enabled);
        return false;
    }
    return true;
}
}

bool installRenderHooks(std::string& err) {
    void* present = nullptr;
    void* resize = nullptr;
    void* execute = nullptr;
    if (!findVTables(&present, &resize, &execute, err)) return false;
    const MH_STATUS init = MH_Initialize();
    if (init != MH_OK && init != MH_ERROR_ALREADY_INITIALIZED) {
        err = std::string("minhook: ") + MH_StatusToString(init);
        return false;
    }
    // 큐 기록을 먼저 켜야 Present 가 처음 불릴 때 큐를 알 수 있다
    return hook(execute, reinterpret_cast<void*>(HookedExecute), reinterpret_cast<void**>(&g_origExecute), "ExecuteCommandLists", err)
        && hook(resize, reinterpret_cast<void*>(HookedResizeBuffers), reinterpret_cast<void**>(&g_origResize), "ResizeBuffers", err)
        && hook(present, reinterpret_cast<void*>(HookedPresent), reinterpret_cast<void**>(&g_origPresent), "Present", err);
}

}
```

- [ ] **Step 7: 작업 스레드와 DLL 진입점**

<!-- file: native/overlay/worker.h -->
```cpp
#pragma once

namespace mlt::ov {
// 오버레이 작업 스레드: 후킹 설치, status.json 읽기, control.json 저장, 설정·상태 파일 쓰기. 돌아오지 않는다
void runOverlayWorker(void* selfModule);
}
```

<!-- file: native/overlay/worker.cpp -->
```cpp
#include "worker.h"
#include "overlay/render/dx12_hook.h"
#include "overlay/ui/app.h"
#include "runtime.h"
#include <windows.h>

namespace mlt::ov {

static std::filesystem::path bridgeDirFor(void* selfModule) {
    wchar_t buf[MAX_PATH]{};
    GetModuleFileNameW(static_cast<HMODULE>(selfModule), buf, MAX_PATH);
    // <mod>\native\mltoybox_overlay.dll -> <mod>\bridge
    return std::filesystem::path(buf).parent_path().parent_path() / L"bridge";
}

// 바뀐 설정을 저장한다(규칙은 core/session). 파일 쓰기는 잠금 밖에서 한다. 저장에 실패했으면 false
static bool saveControlIfDirty(App& a, const Bridge& bridge) {
    std::optional<PendingSave> pending;
    {
        std::lock_guard<std::mutex> lock(a.mutex);
        pending = beginSave(a);
    }
    if (!pending) return true;
    const auto seq = bridge.saveControl(pending->doc);
    std::lock_guard<std::mutex> lock(a.mutex);
    finishSave(a, *pending, seq);
    return seq.has_value();
}

// 패널 등 밖에서 control.json 을 바꿨으면 다시 읽는다. 저장 대기 중인 변경이 있으면 건드리지 않는다(우리 쪽을 저장한다)
static void reloadControlIfChangedOutside(App& a, const Bridge& bridge) {
    const auto fileSeq = bridge.peekSeq();
    if (!fileSeq) return;
    {
        std::lock_guard<std::mutex> lock(a.mutex);
        if (!wantsReload(a, *fileSeq)) return;
    }
    ControlDoc loaded = bridge.loadControl();
    std::lock_guard<std::mutex> lock(a.mutex);
    adoptReloaded(a, std::move(loaded));
}

static void readStatus(App& a, const Bridge& bridge) {
    auto status = bridge.readStatus();
    std::shared_ptr<const StatusDoc> shared;
    if (status) shared = std::make_shared<const StatusDoc>(std::move(*status));
    std::lock_guard<std::mutex> lock(a.mutex);
    if (shared || !a.status) a.status = std::move(shared);
    // 읽기에 실패하면(모드가 쓰는 순간) 직전 값을 둔다. heartbeat 로 오래된 것을 가려낸다
}

static void saveSettingsIfDirty(App& a, const Bridge& bridge, std::filesystem::file_time_type& known) {
    OverlaySettings copy;
    {
        std::lock_guard<std::mutex> lock(a.mutex);
        if (!a.settingsDirty) return;
        copy = a.settings;
        a.settingsDirty = false;
    }
    if (!writeFileAtomic(bridge.settingsPath(), dumpSettings(copy))) return;
    std::error_code ec;
    known = std::filesystem::last_write_time(bridge.settingsPath(), ec);   // 우리가 쓴 것은 밖에서 바뀐 것으로 보지 않는다
}

// overlay.json 을 밖에서 고쳤으면 다시 읽는다(개발·검증용 설정을 게임을 끄지 않고 바꾸기 위한 것)
static void reloadSettingsIfChangedOutside(App& a, const Bridge& bridge, std::filesystem::file_time_type& known) {
    std::error_code ec;
    const auto written = std::filesystem::last_write_time(bridge.settingsPath(), ec);
    if (ec || written == known) return;
    known = written;
    auto text = readFileUtf8(bridge.settingsPath());
    if (!text) return;
    std::lock_guard<std::mutex> lock(a.mutex);
    if (a.settingsDirty) return;
    a.settings = parseSettings(*text);
    syncSettingsAtoms(a);
    a.applyWindowRect = true;
    if (a.settings.startOpen) a.visible = true;
}

static void writeOverlayStatus(App& a, const Bridge& bridge) {
    OverlayStatus s;
    s.heartbeat = nowEpochSeconds();
    s.state = a.state.load();
    s.visible = a.visible.load();
    s.frames = a.frames.load();
    s.font = a.koreanFont.load() ? "malgun" : "default";
    {
        std::lock_guard<std::mutex> lock(a.mutex);
        s.reason = a.reason;
    }
    writeFileAtomic(bridge.overlayStatusPath(), renderOverlayStatus(s));
}

void runOverlayWorker(void* selfModule) {
    const Bridge bridge(bridgeDirFor(selfModule));
    App& a = app();
    {
        std::lock_guard<std::mutex> lock(a.mutex);
        a.settings = parseSettings(readFileUtf8(bridge.settingsPath()).value_or(""));
        syncSettingsAtoms(a);
        a.visible = a.settings.startOpen;
        a.control = bridge.loadControl();
        a.lastSentSeq = a.control.seq();
    }
    std::error_code ec;
    auto settingsWritten = std::filesystem::last_write_time(bridge.settingsPath(), ec);
    writeOverlayStatus(a, bridge);

    std::string err;
    if (installRenderHooks(err)) {
        OverlayState expected = OverlayState::Starting;
        a.state.compare_exchange_strong(expected, OverlayState::Waiting);
    } else {
        disableOverlay("hook setup failed: " + err);
    }

    unsigned saveRetryAt = 0;
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
}

}
```

<!-- file: native/overlay/dllmain.cpp -->
```cpp
#include <windows.h>
#include "overlay/worker.h"
#include "runtime.h"

static DWORD WINAPI Worker(LPVOID self) {
    mlt::ov::runOverlayWorker(self);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        // 후킹과 창 프로시저가 살아 있는 동안 DLL 이 내려가면 게임이 튕긴다. 프로세스가 끝날 때까지 고정한다
        mlt::pinModuleContaining(reinterpret_cast<const void*>(&Worker));
        if (HANDLE t = CreateThread(nullptr, 0, Worker, module, 0, nullptr)) CloseHandle(t);
    }
    return TRUE;
}
```

- [ ] **Step 8: 빌드**

Run: `pwsh E:/MLToybox/tools/build-native.ps1 -Test`
Expected: `100% tests passed, 0 tests failed out of 1`. `native/overlay/` 소스에서 경고가 없다. `E:/MLToybox/native/build/mltoybox_overlay.dll`이 생긴다(약 1 MB).

- [ ] **Step 9: 게임 확인 준비**

게임과 패널이 꺼져 있는지 본다. 켜져 있으면 끄지 말고 사용자에게 묻는다.

```powershell
Get-Process -Name 'ManorLords-Win64-Shipping', 'MLToybox.Panel' -ErrorAction SilentlyContinue | Select-Object Name, Id
pwsh E:\MLToybox\tools\backup-saves.ps1
pwsh E:\MLToybox\tools\deploy.ps1 -Mod MLToybox
pwsh E:\MLToybox\tools\deploy.ps1 -Mod MLToyboxLab
. E:\MLToybox\tools\common.ps1
$mod = Join-Path (Get-MLModsDir) 'MLToybox'
$shots = New-Item -ItemType Directory -Force (Join-Path $env:TEMP 'mltb-overlay-shots')
Get-ChildItem "$mod\native" | Select-Object Name, Length
```

Expected: `mltoybox_native.dll`과 `mltoybox_overlay.dll`이 있다.

열린 채로 시작하게 개발용 설정을 쓴다(마우스 없이 창 캡처로 확인하기 위한 것).

```powershell
@{ toggleKey = 'Insert'; scale = 1.0; window = @{ x = 80; y = 80; w = 640; h = 720 }; startOpen = $true; devTab = '상태' } | ConvertTo-Json | Set-Content "$mod\bridge\overlay.json" -Encoding utf8NoBOM
```

- [ ] **Step 10: 게임을 켜고 상태 확인**

```powershell
& E:\MLToybox\tools\lab-load.ps1 -Slot saveGame_8 -Start
Start-Sleep -Seconds 5
(Get-Content "$mod\bridge\status.json" -Raw | ConvertFrom-Json).overlay
Get-Content "$mod\bridge\overlay_status.json"
```

Expected: `overlay`가 `loaded = True`, `state = ready`, `stale = False`. `overlay_status.json`은 `"state":"ready"`, `"visible":true`, `"font":"malgun"`이고 몇 초 뒤 다시 읽으면 `frames`가 늘어 있다.

게임 시작 때 DLL을 올리는 경로는 사전 검증에서 확인하지 못했다. `state`가 `ready`가 되지 않으면 `reason`과 `ue4ss/UE4SS.log`의 `overlay:` 줄을 읽고, 원인을 실측으로 확인해 고친다. 게임이 튕기면 `CrashReportClient.exe`를 끄고 크래시 덤프의 예외 주소를 확인한 뒤 멈춰 보고한다.

- [ ] **Step 11: 화면 캡처**

```powershell
& E:\MLToybox\tools\capture-game.ps1 -Out "$shots\status.png"
```

`$shots\status.png`를 읽어 확인한다: 화면 (80, 80)에 제목 "MLToybox"인 창이 있고, 맨 위 줄이 "● 적용됨", 탭은 "상태" 하나다. 탭 안에 heartbeat, inGame, appliedSeq / sent, bridgeError, "기능"(7줄, 열이 맞아 있다), "네이티브"(4줄), "오버레이"(빌드 날짜, 글꼴: 맑은 고딕, 여닫는 키, 글자 배율)가 보인다. 한글이 깨지지 않는다.

- [ ] **Step 12: 토글 키**

```powershell
& E:\MLToybox\tools\capture-game.ps1 -Key Insert; Start-Sleep -Seconds 2; Get-Content "$mod\bridge\overlay_status.json"
& E:\MLToybox\tools\capture-game.ps1 -Key Insert; Start-Sleep -Seconds 2; Get-Content "$mod\bridge\overlay_status.json"
```

Expected: 첫 번째는 `"visible":false`, 두 번째는 `"visible":true`.

- [ ] **Step 13: 해상도 변경과 창 위치**

창을 화면 오른쪽에 둔 뒤 해상도를 줄여, 계속 그려지고 창이 화면 안으로 들어오는지 본다. `overlay.json`을 밖에서 고치면 작업 스레드가 1초 안에 다시 읽는다.

```powershell
@{ toggleKey = 'Insert'; scale = 1.0; window = @{ x = 1250; y = 300; w = 640; h = 720 }; startOpen = $true; devTab = '상태' } | ConvertTo-Json | Set-Content "$mod\bridge\overlay.json" -Encoding utf8NoBOM
Start-Sleep -Seconds 3
& E:\MLToybox\tools\lab.ps1 -File E:\MLToybox\tools\lab\resize.lua -Vars @{ W = 1280; H = 720; MODE = 2 }
Start-Sleep -Seconds 4
& E:\MLToybox\tools\capture-game.ps1 -Out "$shots\resized.png"
Get-Content "$mod\bridge\overlay_status.json"
& E:\MLToybox\tools\lab.ps1 -File E:\MLToybox\tools\lab\resize.lua -Vars @{ W = 1920; H = 1080; MODE = 1 }
Start-Sleep -Seconds 4
Get-Content "$mod\bridge\overlay_status.json"
```

Expected: 두 번 모두 `"state":"ready"`이고 `frames`가 계속 는다. `resized.png`에서 창 전체가 1280×720 화면 안에 있다(오른쪽이나 아래가 잘리지 않는다).

- [ ] **Step 14: 게임을 끄고 안전장치 확인**

저장하지 않고 끈다.

```powershell
Stop-Process -Name 'ManorLords-Win64-Shipping' -Force
Start-Sleep -Seconds 4
Get-Process -Name 'CrashReportClient' -ErrorAction SilentlyContinue | Stop-Process -Force
```

(a) 설정으로 끄기: 배포된 `config.lua`(레포가 아니라 게임 폴더의 사본)에서 `overlay = true`를 `overlay = false`로 바꾸고, 지난 실행의 상태 파일이 남아 있는 채로 게임을 켠다.

```powershell
$cfg = "$mod\Scripts\config.lua"
(Get-Content $cfg -Raw).Replace('overlay = true', 'overlay = false') | Set-Content $cfg -Encoding utf8NoBOM -NoNewline
& E:\MLToybox\tools\lab-load.ps1 -Slot saveGame_8 -Start
(Get-Content "$mod\bridge\status.json" -Raw | ConvertFrom-Json).overlay
(Get-Content "$mod\bridge\status.json" -Raw | ConvertFrom-Json).features.build
& E:\MLToybox\tools\capture-game.ps1 -Out "$shots\disabled.png"
```

Expected: `overlay`가 `loaded = False`, `error = disabled in config`, `stale = True`이고 `state`가 없다. `features.build.active`는 `True`(다른 기능은 그대로). `disabled.png`에 오버레이 창이 없다. 게임을 끈다(위 명령).

(b) DLL이 없을 때: 설정을 되돌리고(`pwsh E:\MLToybox\tools\deploy.ps1 -Mod MLToybox`), 배포된 `mltoybox_overlay.dll`을 다른 이름으로 바꾼 뒤 게임을 켠다.

```powershell
pwsh E:\MLToybox\tools\deploy.ps1 -Mod MLToybox
Rename-Item "$mod\native\mltoybox_overlay.dll" 'mltoybox_overlay.dll.off'
& E:\MLToybox\tools\lab-load.ps1 -Slot saveGame_8 -Start
(Get-Content "$mod\bridge\status.json" -Raw | ConvertFrom-Json).overlay
(Get-Content "$mod\bridge\status.json" -Raw | ConvertFrom-Json).native.loaded
```

Expected: `overlay`가 `loaded = False`, `error = not deployed`. `native.loaded`는 `True`. 게임을 끄고 이름을 되돌린다.

```powershell
Rename-Item "$mod\native\mltoybox_overlay.dll.off" 'mltoybox_overlay.dll'
```

- [ ] **Step 15: 닫힌 채로 시작할 때의 안내**

`startOpen`을 끄고 게임을 켠다. 오버레이가 준비되면 8초 동안 안내가 뜬다.

```powershell
@{ toggleKey = 'Insert'; scale = 1.0; window = @{ x = 80; y = 80; w = 640; h = 720 }; startOpen = $false; devTab = $null } | ConvertTo-Json | Set-Content "$mod\bridge\overlay.json" -Encoding utf8NoBOM
& (Get-ItemProperty 'HKCU:\Software\Valve\Steam').SteamExe -applaunch 1363080
$deadline = (Get-Date).AddSeconds(240)
while ((Get-Date) -lt $deadline) {
    $s = try { Get-Content "$mod\bridge\overlay_status.json" -Raw | ConvertFrom-Json } catch { $null }
    if ($s -and $s.state -eq 'ready' -and ([DateTimeOffset]::UtcNow.ToUnixTimeSeconds() - $s.heartbeat) -le 2) { break }
    Start-Sleep -Milliseconds 300
}
& E:\MLToybox\tools\capture-game.ps1 -Out "$shots\hint.png" -WaitMs 200
Start-Sleep -Seconds 10
& E:\MLToybox\tools\capture-game.ps1 -Out "$shots\hint-gone.png" -WaitMs 200
Get-Content "$mod\bridge\overlay_status.json"
```

Expected: `hint.png`의 왼쪽 위에 "Insert: MLToybox"가 있고 `hint-gone.png`에는 없다. `"visible":false`. 게임을 끈다.

Steam 앱 번호 `1363080`은 `tools/common.ps1`의 `$script:MLAppId`와 같은 값이어야 한다. 다르면 그 값을 쓴다.

- [ ] **Step 16: 커밋**

```bash
git -C E:/MLToybox add native/overlay native/CMakeLists.txt
git -C E:/MLToybox commit -m "feat(overlay): overlay dll - DX12 present hook, input, crash guards, worker, window with the status tab" -m "Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 7: 탭 — 영주, 건설, 업그레이드

**Files:**
- Create: `native/overlay/ui/widgets.h`, `native/overlay/ui/widgets.cpp`, `native/overlay/ui/tab_build.cpp`, `native/overlay/ui/tab_lord.cpp`
- Modify: `native/overlay/ui/tabs.h`, `native/overlay/ui/window.cpp`, `native/CMakeLists.txt`

**Interfaces:**
- Consumes: `TabContext`, `markDirty(App&)`, `sendCommand(App&, Json)` (Task 6), `ControlDoc::build()/setBuild()`, `upgrade()/setUpgrade()`, `lord()/setLord()` (Task 2), `makeSetLord` (Task 3), `parseNumber` (Task 1), `StatusDoc::lord` (Task 3)
- Produces (이름공간 `mlt::ov`):
  - `overlay/ui/widgets.h`: `bool numberField(const char* id, int& value, int min, int max, float width)`(편집을 끝냈고 값이 바뀌었으면 `true`), `void needGameText()`
  - `overlay/ui/tabs.h`: `void drawLordTab(TabContext&)`, `void drawBuildTab(TabContext&)`, `void drawUpgradeTab(TabContext&)`

즉시 반영 규칙(스펙 2.3): 체크박스는 바꾸는 즉시, 숫자 칸은 Enter를 누르거나 포커스가 떠날 때 값이 바뀌었으면 저장한다. 탭 코드는 문서를 고친 뒤 `markDirty`만 부른다. 저장은 작업 스레드가 0.2초 안에 한다.

- [ ] **Step 1: 숫자 칸**

<!-- file: native/overlay/ui/widgets.h -->
```cpp
#pragma once

namespace mlt::ov {
// 숫자 칸. 포커스가 없을 때는 value 를 보여 준다. 편집을 끝내면(Enter 또는 포커스 이동) 입력을 해석해
// [min, max] 로 맞춘 값을 value 에 넣고, 값이 바뀌었으면 true 를 돌려준다. 해석할 수 없는 입력은 버린다.
// 전각 숫자·공백·쉼표를 허용한다(core/number_input).
bool numberField(const char* id, int& value, int min, int max, float width);

// 게임 상태가 필요한 부분이 비어 있을 때의 안내 문구
void needGameText();
}
```

<!-- file: native/overlay/ui/widgets.cpp -->
```cpp
#include "widgets.h"
#include "overlay/core/number_input.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <imgui.h>
#include <unordered_map>

namespace mlt::ov {

bool numberField(const char* id, int& value, int min, int max, float width) {
    static std::unordered_map<ImGuiID, std::array<char, 32>> buffers;
    auto& buf = buffers[ImGui::GetID(id)];
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

void needGameText() {
    ImGui::TextDisabled("게임에 들어가면 표시됩니다");
}

}
```

- [ ] **Step 2: 건설·업그레이드 탭**

패널의 "지역당 개수 제한 해제 (…)"는 문구가 길어 체크 이름과 설명 줄로 나눈다.

<!-- file: native/overlay/ui/tab_build.cpp -->
```cpp
#include "tabs.h"
#include <imgui.h>

namespace mlt::ov {

void drawBuildTab(TabContext& ctx) {
    App& a = ctx.app;
    BuildSettings b = a.control.build();
    bool changed = false;
    changed |= ImGui::Checkbox("건설 기능 사용", &b.enabled);
    changed |= ImGui::Checkbox("배치 제한 무시 (영지 경계 안, 네이티브 DLL)", &b.ignorePlacement);
    changed |= ImGui::Checkbox("즉시 완공 (네이티브 DLL)", &b.instantBuild);
    changed |= ImGui::Checkbox("즉시 수리", &b.instantRepair);
    changed |= ImGui::Checkbox("자재 불필요 (건설 자재 없이 공사)", &b.noMaterials);
    changed |= ImGui::Checkbox("지역당 개수 제한 해제", &b.noRegionLimit);
    ImGui::Indent();
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("영주 저택 모듈·세금 징수소·장작/식량 수레. 끈 뒤에는 게임을 다시 켜야 원래대로");
    ImGui::PopTextWrapPos();
    ImGui::Unindent();
    if (changed) {
        a.control.setBuild(b);
        markDirty(a);
    }
}

void drawUpgradeTab(TabContext& ctx) {
    App& a = ctx.app;
    UpgradeSettings u = a.control.upgrade();
    if (ImGui::Checkbox("업그레이드 조건·비용·해금 무시", &u.enabled)) {
        a.control.setUpgrade(u);
        markDirty(a);
    }
}

}
```

- [ ] **Step 3: 영주 탭**

체크를 끄면 그 키를 설정에서 뺀다(관리 안 함). 입력해 둔 값은 화면에 남겨, 다시 켜거나 "지금 설정"을 누를 때 쓴다. "지금 설정"은 게임 안일 때만 누를 수 있다(스펙 2.6).

<!-- file: native/overlay/ui/tab_lord.cpp -->
```cpp
#include "overlay/core/commands.h"
#include "tabs.h"
#include "widgets.h"
#include <cstdio>
#include <imgui.h>
#include <optional>
#include <string>

namespace mlt::ov {

namespace {
constexpr int kLordMax = 10000000;

// 체크를 꺼도 입력해 둔 값은 남긴다(다시 켜면 그 값, '지금 설정'도 그 값을 쓴다)
struct Row {
    const char* label;
    const char* key;
    int remembered = 0;
    bool seeded = false;
};

std::string currentText(const std::optional<double>& value) {
    if (!value) return "현재: -";
    char buf[48];
    std::snprintf(buf, sizeof(buf), "현재: %.0f", *value);
    return buf;
}

// 줄 하나: [체크] 이름  목표 [값] [지금 설정]  현재: N. 설정이 바뀌었으면 true
bool drawRow(TabContext& ctx, Row& row, std::optional<int>& target, const std::optional<double>& current) {
    bool changed = false;
    if (!row.seeded || target) {   // 저장된 목표가 있으면 그 값을 보여 준다
        if (target) row.remembered = *target;
        row.seeded = true;
    }
    ImGui::PushID(row.key);
    bool use = target.has_value();
    if (ImGui::Checkbox(row.label, &use)) {
        if (use) target = row.remembered;
        else target.reset();
        changed = true;
    }
    ImGui::SameLine(130.0f * ImGui::GetStyle().FontScaleMain);
    ImGui::TextUnformatted("목표");
    ImGui::SameLine();
    int value = row.remembered;
    if (numberField("##target", value, 0, kLordMax, 120.0f * ImGui::GetStyle().FontScaleMain)) {
        row.remembered = value;
        if (target) {
            target = value;
            changed = true;
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!ctx.inGame);
    if (ImGui::Button("지금 설정")) sendCommand(ctx.app, makeSetLord(row.key, row.remembered, ctx.now));
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextUnformatted(currentText(current).c_str());
    ImGui::PopID();
    return changed;
}
}

void drawLordTab(TabContext& ctx) {
    static Row treasury{ "국고", "treasury" };
    static Row influence{ "영향력", "influence" };
    static Row favour{ "왕의 총애", "kingsFavour" };

    App& a = ctx.app;
    LordSettings l = a.control.lord();
    const std::optional<LordStatus> status = (ctx.inGame && ctx.status) ? ctx.status->lord : std::nullopt;
    auto asDouble = [](const std::optional<int>& v) { return v ? std::optional<double>(*v) : std::nullopt; };

    bool changed = ImGui::Checkbox("영주 자원 목표값 유지 (영지와 무관한 전체 값)", &l.enabled);
    changed |= drawRow(ctx, treasury, l.treasury, status ? status->treasury : std::nullopt);
    changed |= drawRow(ctx, influence, l.influence, status ? asDouble(status->influence) : std::nullopt);
    changed |= drawRow(ctx, favour, l.kingsFavour, status ? asDouble(status->kingsFavour) : std::nullopt);
    if (changed) {
        a.control.setLord(l);
        markDirty(a);
    }

    ImGui::Spacing();
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("체크한 항목은 목표값 아래로 내려가면 목표까지 채웁니다(더 많으면 그대로). 체크 해제 = 관리 안 함.");
    ImGui::TextDisabled("'지금 설정'은 체크와 상관없이 입력한 값으로 한 번 정확히 맞춥니다(내리기도 가능).");
    ImGui::PopTextWrapPos();
    if (!ctx.inGame) needGameText();
}

}
```

- [ ] **Step 4: 탭 등록**

<!-- edit: native/overlay/ui/tabs.h -->
`native/overlay/ui/tabs.h` — 찾을 부분:

```cpp
void drawStatusTab(TabContext& ctx);
```

바꿀 내용:

```cpp
void drawLordTab(TabContext& ctx);
void drawBuildTab(TabContext& ctx);
void drawUpgradeTab(TabContext& ctx);
void drawStatusTab(TabContext& ctx);
```

<!-- edit: native/overlay/ui/window.cpp -->
`native/overlay/ui/window.cpp` — 찾을 부분:

```cpp
// 패널과 같은 이름. 탭은 뒤 태스크에서 더한다
const Tab kTabs[] = {
    { "상태", drawStatusTab },
};
```

바꿀 내용:

```cpp
// 패널과 같은 이름·순서
const Tab kTabs[] = {
    { "영주", drawLordTab },
    { "건설", drawBuildTab },
    { "업그레이드", drawUpgradeTab },
    { "상태", drawStatusTab },
};
```

<!-- edit: native/CMakeLists.txt -->
`native/CMakeLists.txt` — 찾을 부분:

```cmake
    overlay/ui/window.cpp
    overlay/ui/tab_status.cpp)
```

바꿀 내용:

```cmake
    overlay/ui/window.cpp
    overlay/ui/widgets.cpp
    overlay/ui/tab_build.cpp
    overlay/ui/tab_lord.cpp
    overlay/ui/tab_status.cpp)
```

- [ ] **Step 5: 빌드**

Run: `pwsh E:/MLToybox/tools/build-native.ps1 -Test`
Expected: `100% tests passed, 0 tests failed out of 1`. `native/overlay/` 소스에서 경고가 없다.

- [ ] **Step 6: 게임에서 탭 화면 확인**

게임이 꺼져 있는지 보고 배포한 뒤 켠다(Task 6 Step 9의 `$mod`, `$shots`를 쓴다).

```powershell
pwsh E:\MLToybox\tools\deploy.ps1 -Mod MLToybox
& E:\MLToybox\tools\lab-load.ps1 -Slot saveGame_8 -Start
$n = 0
foreach ($tab in '영주', '건설', '업그레이드', '상태') {
    @{ toggleKey = 'Insert'; scale = 1.0; window = @{ x = 80; y = 80; w = 640; h = 720 }; startOpen = $true; devTab = $tab } | ConvertTo-Json | Set-Content "$mod\bridge\overlay.json" -Encoding utf8NoBOM
    Start-Sleep -Seconds 3
    & E:\MLToybox\tools\capture-game.ps1 -Out "$shots\tab-$n.png"
    $n++
}
Get-Content "$mod\bridge\control.json" -Raw | ConvertFrom-Json | ForEach-Object { $_.features.lord, $_.features.build, $_.features.upgrade }
(Get-Content "$mod\bridge\status.json" -Raw | ConvertFrom-Json).lord
```

캡처 네 장을 읽어 확인한다.
- `tab-0.png`(영주): 탭이 영주, 건설, 업그레이드, 상태 순서다. "영주 자원 목표값 유지 (영지와 무관한 전체 값)" 체크와 국고·영향력·왕의 총애 줄(체크, 목표, "지금 설정", "현재: N")이 있고, 체크와 목표가 `control.json`의 `features.lord`와, "현재"가 `status.json`의 `lord`와 같다. 설명 두 줄이 있다.
- `tab-1.png`(건설): 체크 6개가 `features.build`와 같다.
- `tab-2.png`(업그레이드): 체크가 `features.upgrade.enabled`와 같다.
- `tab-3.png`(상태): Task 6과 같다.

- [ ] **Step 7: 밖에서 바꾼 설정이 반영되는지(패널과 함께 쓰기)**

`control.json`을 직접 고쳐 `seq`를 1 올리고 영주 국고 목표를 바꾼다. 원래 내용은 따로 둔다.

```powershell
Copy-Item "$mod\bridge\control.json" "$shots\control.before.json" -Force
$c = Get-Content "$mod\bridge\control.json" -Raw | ConvertFrom-Json
$before = $c.features.lord.treasury
$c.seq = $c.seq + 1
$c.features.lord.treasury = 123456
$c | ConvertTo-Json -Depth 20 | Set-Content "$mod\bridge\control.json" -Encoding utf8NoBOM
@{ toggleKey = 'Insert'; scale = 1.0; window = @{ x = 80; y = 80; w = 640; h = 720 }; startOpen = $true; devTab = '영주' } | ConvertTo-Json | Set-Content "$mod\bridge\overlay.json" -Encoding utf8NoBOM
Start-Sleep -Seconds 3
& E:\MLToybox\tools\capture-game.ps1 -Out "$shots\external.png"
```

Expected: `external.png`의 국고 목표가 `123456`이고 맨 위 줄이 "● 적용됨"이다. 확인이 끝나면 게임을 끄고 원래 파일로 되돌린다.

```powershell
Stop-Process -Name 'ManorLords-Win64-Shipping' -Force
Start-Sleep -Seconds 4
Get-Process -Name 'CrashReportClient' -ErrorAction SilentlyContinue | Stop-Process -Force
Copy-Item "$shots\control.before.json" "$mod\bridge\control.json" -Force
```

`features.lord`가 없는 설정이면(영주 탭을 쓴 적이 없음) `treasury` 대신 `features.build.instantRepair`를 뒤집어 건설 탭으로 확인한다.

- [ ] **Step 8: 메인 메뉴에서의 모습**

세이브를 불러오지 않고 게임만 켠다.

```powershell
& (Get-ItemProperty 'HKCU:\Software\Valve\Steam').SteamExe -applaunch 1363080
$deadline = (Get-Date).AddSeconds(240)
while ((Get-Date) -lt $deadline) {
    $s = try { Get-Content "$mod\bridge\status.json" -Raw | ConvertFrom-Json } catch { $null }
    if ($s -and ([DateTimeOffset]::UtcNow.ToUnixTimeSeconds() - $s.heartbeat) -le 3 -and $s.overlay.state -eq 'ready') { break }
    Start-Sleep -Seconds 2
}
Start-Sleep -Seconds 15
& E:\MLToybox\tools\capture-game.ps1 -Out "$shots\menu.png"
```

Expected: `menu.png`의 맨 위 줄이 "● 메인 메뉴", 영주 탭의 "지금 설정" 버튼이 흐리게(누를 수 없게) 보이고 "현재: -", 맨 아래에 "게임에 들어가면 표시됩니다"가 있다. 게임을 끈다.

- [ ] **Step 9: 개발용 설정을 지우고 Lab 모드를 뺀다**

```powershell
@{ toggleKey = 'Insert'; scale = 1.0; window = @{ x = 80; y = 80; w = 640; h = 720 }; startOpen = $false; devTab = $null } | ConvertTo-Json | Set-Content "$mod\bridge\overlay.json" -Encoding utf8NoBOM
pwsh E:\MLToybox\tools\deploy.ps1 -Mod MLToyboxLab -Remove
```

- [ ] **Step 10: 커밋**

```bash
git -C E:/MLToybox add native/overlay native/CMakeLists.txt
git -C E:/MLToybox commit -m "feat(overlay): lord, build and upgrade tabs with immediate apply" -m "Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

**사용자 확인 항목** (마우스 입력은 게임 창이 앞에 있어야 해서 자동으로 확인할 수 없다. 마지막 보고에 그대로 넣는다):
1. Insert로 창이 열리고 닫히는가. 게임을 켠 직후 왼쪽 위 안내가 뜨는가.
2. 건설·업그레이드 탭의 체크를 누르면 맨 위 줄이 "적용 대기 중"을 거쳐 "적용됨"이 되는가.
3. 영주 탭의 목표 칸에 숫자를 넣고 Enter를 누르거나 다른 곳을 누르면 저장되는가. 글자나 음수를 넣으면 이전 값으로 돌아가는가.
4. "지금 설정"을 누르면 국고·영향력·왕의 총애가 그 값이 되는가.
5. 창 위에서의 클릭·휠·키 입력이 게임(카메라, 건물 선택, 단축키)으로 새지 않는가. 창 밖에서는 게임이 평소대로 움직이는가.
6. 창을 끌어 옮기고 크기를 바꾼 뒤 게임을 다시 켜면 그 위치와 크기로 뜨는가.
7. 상태 탭에서 여닫는 키와 글자 배율을 바꾸면 바로 적용되고, 다시 켜도 유지되는가.
8. 오래 켜 두어도 멈추거나 끊기지 않는가. 프레임 생성(FSR·DLSS FG)을 쓰는 경우 그 상태에서도 뜨는가.

---

### Task 8: 문서와 병합

**Files:**
- Modify: `README.md`, `docs/CHANGELOG.md`, `analysis/findings.md`

**Interfaces:**
- Consumes: Task 6·7의 게임 확인 결과(실측값)
- Produces: 없음

- [ ] **Step 1: README**

머리말의 "구성 요소는 세 가지입니다."를 "구성 요소는 네 가지입니다."로 바꾸고, **네이티브 DLL** 줄 아래에 다음 줄을 더한다.

```markdown
- **오버레이 DLL**: 게임 화면 안에 설정 창을 띄웁니다(Insert). 지금은 영주·건설·업그레이드·상태 탭이 있습니다.
```

"구성" 표의 `native/` 줄을 다음으로 바꾼다.

```markdown
| `native/` | C++20 네이티브 DLL(MinHook)과 오버레이 DLL(Dear ImGui). Lua가 `package.loadlib`로 불러옴 |
```

"동작 구조"의 목록 끝에 다음 줄을 더한다.

```markdown
- 오버레이 DLL도 패널과 같은 방식으로 `control.json`을 쓰고 `status.json`을 읽습니다. 자기 설정은 `bridge/overlay.json`, 상태는 `bridge/overlay_status.json`에 둡니다.
```

"## 기능 (패널 탭별)" 앞에 새 절을 넣는다.

```markdown
## 게임 안 창 (오버레이)
게임 화면 위에 MLToybox 창을 띄웁니다. 패널을 켜지 않고 게임 안에서 설정을 바꿉니다.

- **여닫기**: Insert. 게임을 켠 뒤 8초 동안 화면 왼쪽 위에 안내가 뜹니다. 키는 [상태] 탭에서 Insert, Home, End, F7, F8, F9 가운데 고릅니다.
- **즉시 반영**: "적용" 버튼이 없습니다. 체크는 바꾸는 즉시, 숫자 칸은 Enter를 누르거나 다른 곳을 누를 때 저장됩니다. 창 맨 위 줄이 "적용 대기 중"에서 "적용됨"으로 바뀌면 모드가 받은 것입니다.
- **지금 있는 탭**: 영주, 건설, 업그레이드, 상태. 자원·군사·용병·인구는 아직 패널에서 설정합니다(다음 단계에서 옮깁니다).
- **패널과 함께**: 둘 다 같은 `control.json`을 씁니다. 나중에 저장한 쪽이 반영되고, 오버레이는 패널이 바꾼 값을 1~2초 안에 다시 읽습니다.
- **입력**: 마우스가 창 위에 있거나 입력 칸에 커서가 있으면 그 입력은 게임에 가지 않습니다.
- **글자 크기**: [상태] 탭의 "글자 배율"(80~150%).
- **끄기**: 게임 폴더 `ue4ss/Mods/MLToybox/Scripts/config.lua`의 `overlay = true`를 `false`로 바꾸거나 `native/mltoybox_overlay.dll`을 지웁니다. 다른 기능은 그대로 동작합니다.
- **창이 안 뜰 때**: `bridge/overlay_status.json`의 `state`와 `reason`을 봅니다. `status.json`의 `overlay`에도 같은 내용이 있습니다. 오버레이는 그리기에 실패하면 게임을 튕기게 하지 않고 스스로 꺼집니다(`state`가 `disabled`).
- **확인하지 못한 조건**: 프레임 생성(FSR·DLSS FG), HDR, 전체 화면 전용 모드, 모니터 사이 이동. 문제가 있으면 위 방법으로 끄고 패널을 쓰세요.
```

"설치"의 번호 목록을 다음으로 바꾼다.

```markdown
1. 게임을 실행합니다.
2. 게임 안에서 Insert를 눌러 MLToybox 창을 엽니다(영주·건설·업그레이드·상태).
3. 나머지 탭(자원·군사·용병·인구)은 `dist/panel/MLToybox.Panel.exe`를 켜서 설정하고 "적용"을 누릅니다.
```

"개발 도구"의 목록 끝에 다음 줄을 더한다.

```markdown
- **게임 창 캡처**: `pwsh tools/capture-game.ps1 -Out shot.png`는 게임 창만 찍습니다(가려져 있어도 됩니다). `-Key Insert`는 키 메시지를 보냅니다. `bridge/overlay.json`의 `startOpen`(열린 채 시작)과 `devTab`(열 탭 이름)을 쓰면 마우스 없이 오버레이 화면을 확인할 수 있습니다. `tools/lab/resize.lua`는 해상도를 바꿉니다.
```

"네이티브 계층"의 **빌드**와 **배포** 줄을 다음으로 바꾸고, 목록 끝에 두 줄을 더한다.

```markdown
- **빌드**: `pwsh tools/build-native.ps1 -Test`를 실행하면 `native/build/mltoybox_native.dll`과 `mltoybox_overlay.dll`이 만들어집니다.
- **배포**: `pwsh tools/deploy.ps1 -Mod MLToybox`가 두 DLL을 `Mods/MLToybox/native/`에 복사합니다. 게임 실행 중에는 DLL이 잠겨 경고만 남깁니다.
```

```markdown
- **오버레이**(`native/overlay/`): `core/`는 설정·상태 문서와 저장 규칙(단위 테스트), `render/`는 DXGI `Present`·`ResizeBuffers`와 D3D12 `ExecuteCommandLists` 후킹과 입력, `ui/`는 Dear ImGui 화면입니다. 게임 코드의 주소가 아니라 DXGI·D3D12의 가상 함수 표에 후킹하므로 게임이 업데이트돼도 패턴을 다시 찾지 않습니다.
- **외부 라이브러리**: MinHook, Dear ImGui v1.92.9b, nlohmann/json v3.12.0(`native/third_party/`, 폴더마다 라이선스 파일).
```

- [ ] **Step 2: CHANGELOG**

`docs/CHANGELOG.md`의 맨 위 항목 앞에 넣는다. 날짜는 작업한 날로 적는다.

```markdown
## 2026-10-02 — 게임 안 창 1단계
### 추가
- **게임 안 창(오버레이)**: Insert로 게임 화면 안에 MLToybox 창을 엽니다. 영주, 건설, 업그레이드, 상태 탭이 있고 값을 바꾸면 즉시 저장됩니다("적용" 버튼 없음). 자원·군사·용병·인구 탭은 다음 단계에서 옮기며, 그때까지는 패널을 씁니다.
- 새 DLL `native/mltoybox_overlay.dll`(DX12 화면 출력 후킹 + Dear ImGui). 그리기에 실패하면 스스로 꺼지고 게임과 다른 기능은 계속 동작합니다. `config.lua`의 `overlay = false`로 끕니다.
- 상태에 `overlay`(올렸는지, 상태, 이유)가 추가됐습니다. 오버레이 설정은 `bridge/overlay.json`에 저장됩니다.
- 개발 도구: `tools/capture-game.ps1`(게임 창 캡처, 키 메시지), `tools/lab/resize.lua`.
```

- [ ] **Step 3: findings**

`analysis/findings.md` 끝에 "게임 안 오버레이 창 — 계획 A 구현 검증 (날짜, saveGame_8)" 절을 더한다. Task 6과 Task 7에서 **실측한 값만** 적는다. 확인하지 못한 것은 "확인 못 함"이라고 적는다. 항목:
- 게임 시작 때 올린 결과: `status.overlay`의 값, `overlay_status.json`이 `ready`가 되기까지 걸린 시간.
- 화면: 탭 4개 캡처에서 본 것(문구, 값이 `control.json`/`status.json`과 같은지).
- 토글 키, 밖에서 바꾼 설정 반영, 해상도 변경과 창 위치, 안내 표시.
- 안전장치: `overlay = false`일 때와 DLL이 없을 때의 `status.overlay`.
- 사전 검증과 달랐던 점, 고친 것이 있으면 원인과 함께.
- 확인하지 못한 것: 마우스 입력(사용자 확인 항목), 프레임 생성, HDR, 전체 화면 전용, 다중 모니터.

- [ ] **Step 4: 전체 확인**

```powershell
pwsh E:\MLToybox\tools\build-native.ps1 -Test
dotnet test E:\MLToybox\panel\MLToybox.sln
dotnet build E:\MLToybox\panel\MLToybox.sln
pwsh E:\MLToybox\tools\tests\Tools.Tests.ps1
git -C E:/MLToybox status --short panel
```

Expected: 네이티브 `100% tests passed`, `dotnet test` 모두 통과, 빌드 오류 0, `ALL PASS`. `panel/` 아래에 바뀐 파일이 없다(R8).

- [ ] **Step 5: 커밋, 병합, 푸시, 배포**

```bash
git -C E:/MLToybox add README.md docs/CHANGELOG.md analysis/findings.md
git -C E:/MLToybox commit -m "docs: in-game overlay stage 1 (README, changelog, in-game verification)" -m "Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
git -C E:/MLToybox checkout develop
git -C E:/MLToybox merge --no-ff feat/overlay -m "Merge branch 'feat/overlay' into develop" -m "Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
git -C E:/MLToybox push origin develop
```

게임이 꺼져 있을 때 최종본을 배포한다: `pwsh E:\MLToybox\tools\deploy.ps1 -Mod MLToybox`

마지막 보고에는 Task 7의 사용자 확인 항목과, 계획 B(군사, 인구, 자원, 용병 탭)가 남아 있다는 것을 넣는다.
