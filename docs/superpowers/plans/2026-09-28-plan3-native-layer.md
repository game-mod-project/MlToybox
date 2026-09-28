# MLToybox Plan 3 — 네이티브 계층 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 독립 네이티브 DLL(`mltoybox_native.dll`)과 역분석 도구(`MLToybox.Re`)를 만들고, Lua로 불가능한 세 기능(즉시 완공, 주민 수를 넘는 징집, 배치 제한 무시)을 시간 제한 안에서 구현한다.

**Architecture:**
- `mod/MLToybox`의 `core/native.lua`가 DLL을 `package.loadlib(path, "*")`로 로드한다. DLL의 워커 스레드는 다음을 반복한다.
  1. 게임 exe `.text`를 패턴 스캔한다.
  2. MinHook으로 후킹을 설치한다.
  3. `bridge/control.json`을 1초마다 읽어 후킹을 켜고 끈다.
  4. `bridge/native_status.json`을 기록한다.
- Lua는 이 상태를 `status.json`의 `native`로 병합하고, 패널이 표시한다.
- 후킹 대상 주소는 `MLToybox.Re`로 찾는다. 방법은 두 가지다.
  - UFunction 이름 문자열 → `FNameNativePtrPair` → exec 썽크 → 구현 함수
  - 이유 문자열 참조 역추적

**Tech Stack:** C++20 / MSVC 14.44 / CMake 3.31 + Ninja(VS Build Tools 동봉), MinHook v1.3.4, .NET 8 + Iced 1.21.0, UE4SS 3.0.1 Lua, xUnit + NLua

**Spec:** `docs/superpowers/specs/2026-09-28-mltoybox-mod-design.md` §11. 게임 API 근거: `analysis/findings.md`.

## Global Constraints

- **다운로드(이 계획 승인 = 승인):**
  - MinHook `https://github.com/TsudaKageyu/minhook` 태그 `v1.3.4`(커밋 `c3fcafdc10146beb5919319d0683e44e3c30d537`). `native/third_party/minhook/`에 소스 벤더링하고 `LICENSE.txt`를 유지한다.
  - NuGet `Iced` 1.21.0
  - 그 밖의 다운로드는 금지한다.
- 빌드: `tools/build-native.ps1`이 VS Build Tools의 `vcvars64.bat` + 동봉 CMake/Ninja로 `native/build/mltoybox_native.dll`을 만든다. x64, 정적 CRT(`/MT`).
- DllMain은 워커 스레드 생성만 한다. 스캔·후킹·파일 IO는 워커 스레드에서 한다.
- 패턴 형식은 `"48 8B ?? 05"`(공백 구분 16진, `?`/`??`는 와일드카드, 첫 바이트는 와일드카드 불가)로 C++·C#이 같은 의미를 쓴다. **정확히 1곳에서 일치할 때만** 후킹을 설치한다.
- 후킹 detour는 예외를 던지지 않는다. 항상 원본을 호출한다(결과 후처리 방식). 게임 오브젝트 쓰기는 detour 안(게임 스레드)에서만 한다.
- control 해석(프로토콜 v1):
  - `instantBuild` = `build.enabled && build.instantBuild`
  - `ignorePlacement` = `build.enabled && build.ignorePlacement`
  - `ignorePopulation` = `military.enabled && military.ignorePopulation`
- `native_status.json` 스키마: `{version:1, heartbeat:<epoch s>, appliedSeq:<n|-1>, features:{<name>:{installed, active, lastError?}}}`. 기능이 없으면 `features`를 생략한다.
- **시간 제한(spec §11.5):** 즉시 완공 반나절, 징집 1일, 배치 1일. 제한 안에 "유일 패턴 + 인게임 동작 확인"에 도달하지 못하면 해당 태스크를 중단하고, 증거를 `findings.md`에 기록한 뒤 사용자에게 계속 여부를 묻는다(실행 규칙상 "정지 사유"로 취급).
- **분석 의존 태스크(8~10):** 분석 단계의 결과(주소·패턴·오프셋·함수 시그니처)를 이 계획 끝의 "부록 A" 해당 절에 먼저 기록한 뒤 구현한다. 구현 코드는 부록에 적힌 값만 쓴다.
- spec과 다른 점:
  - spec §11.3의 `native.probe` 프로브 모드는 만들지 않는다. 필드 오프셋은 구현 함수의 디스어셈블리에서 직접 읽는다(YAGNI). 필요해지면 태스크 8에서 ruling으로 추가한다.
- Lua 파일은 Write 도구로만 작성한다(백슬래시 보호).
- git: 브랜치 `feat/plan3-native`(`develop`에서 분기), 태스크마다 커밋, 끝에 `develop`으로 `--no-ff` 머지. 커밋 끝에 `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- 인게임 테스트 전 `tools\backup-saves.ps1`, 테스트 세이브 사용.

## Review Focus

1. **DLL이 없거나 로드 실패**(배포 전, 백신 차단): Lua 기능은 정상이어야 하고, 패널에 네이티브 "미로드"와 이유가 보여야 한다. → Task 5 `load_missing_dll_reports_not_deployed`, `load_failure_reports_error`
2. **게임 업데이트로 패턴이 0곳 또는 여러 곳에서 일치**: 해당 기능만 `installed=false`와 이유를 보고하고, 후킹하지 않아야 한다. → Task 2 `scan_unique_*`, Task 4 `HookManager` 설치 결과 테스트
3. **DLL 워커가 멈춤/크래시**(heartbeat 끊김): 패널이 네이티브 상태를 "응답 없음"으로 보여야 한다. → Task 5 `status_marks_stale_heartbeat`
4. **게임 실행 중 재배포**(DLL 잠김): `deploy.ps1`이 실패하지 않고 경고만 남겨야 한다. → Task 5 deploy 테스트 `dll copy tolerates locked target`
5. **깨지거나 반쯤 쓰인 control.json**: 네이티브는 직전 플래그를 유지해야 한다. → Task 3 `control_rejects_*`

---

## 파일 구조

```
native/
├─ CMakeLists.txt
├─ third_party/minhook/          (v1.3.4 벤더링)
├─ src/
│   ├─ scanner.h/.cpp            패턴 파싱·검색 (순수, 테스트 대상)
│   ├─ json.h/.cpp               최소 JSON 파서 (순수)
│   ├─ control.h/.cpp            control.json → NativeControl (순수)
│   ├─ status.h/.cpp             native_status.json 렌더링 (순수)
│   ├─ hooks.h/.cpp              HookManager (스캔 결과 → MinHook 설치/활성)
│   ├─ runtime.h/.cpp            경로·파일 IO·PE .text·워커 루프 (Windows 의존)
│   ├─ features/features.h/.cpp  기능 등록 (Task 8~10에서 채움)
│   └─ dllmain.cpp
└─ tests/ test.h, main.cpp, scanner_tests.cpp, json_tests.cpp, control_tests.cpp, status_tests.cpp, hooks_tests.cpp
tools/build-native.ps1
tools/re/MLToybox.Re/            .NET 8 콘솔 (PeImage, Pattern, ExecResolver, Disasm, SigMaker, Program)
panel/MLToybox.Tests/Re*Tests.cs Re 도구 테스트 (Tests 프로젝트가 Re 프로젝트 참조)
mod/MLToybox/Scripts/core/native.lua
```

---

### Task 1: 빌드 환경 + DLL 골격 + loadlib 실측

**Files:**
- Create: `native/CMakeLists.txt`, `native/src/dllmain.cpp`, `native/src/runtime.h`, `native/src/runtime.cpp`(최소), `native/tests/test.h`, `native/tests/main.cpp`, `native/tests/smoke_tests.cpp`
- Create: `tools/build-native.ps1`
- Create: `native/third_party/minhook/**`(다운로드)
- Modify: `.gitignore`(`native/build/`)

**Interfaces:**
- Produces:
  - `tools/build-native.ps1 [-Config Release|Debug] [-Test]` → `native/build/mltoybox_native.dll`. `-Test`면 CTest 실행.
  - 테스트 매크로 `TEST(name) { ... }`, `CHECK(cond)`
  - DLL은 로드되면 `<mod>\bridge\native_status.json`에 heartbeat를 1초마다 기록한다(이 태스크에서는 features 없음).

- [ ] **Step 1: 브랜치와 MinHook 벤더링**

```powershell
git -C E:\MLToybox switch -c feat/plan3-native develop
git clone --depth 1 --branch v1.3.4 https://github.com/TsudaKageyu/minhook.git E:\MLToybox\native\third_party\minhook
git -C E:\MLToybox\native\third_party\minhook rev-parse HEAD
Remove-Item -Recurse -Force E:\MLToybox\native\third_party\minhook\.git, E:\MLToybox\native\third_party\minhook\.github
```
Expected: `c3fcafdc10146beb5919319d0683e44e3c30d537`

- [ ] **Step 2: 테스트 하네스와 스모크 테스트 작성**

`native/tests/test.h`

```cpp
#pragma once
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

struct TestCase { const char* name; std::function<void()> fn; };
struct TestFailure { std::string msg; };
std::vector<TestCase>& testRegistry();

#define TEST(name) \
    static void name(); \
    static const bool name##_registered = (testRegistry().push_back({#name, name}), true); \
    static void name()

#define CHECK(cond) \
    do { if (!(cond)) throw TestFailure{std::string(__FILE__) + ":" + std::to_string(__LINE__) + ": " #cond}; } while (0)
```

`native/tests/main.cpp`

```cpp
#include "test.h"
#include <exception>

std::vector<TestCase>& testRegistry() { static std::vector<TestCase> r; return r; }

int main() {
    int failed = 0;
    for (auto& t : testRegistry()) {
        try { t.fn(); std::printf("PASS %s\n", t.name); }
        catch (const TestFailure& f) { ++failed; std::printf("FAIL %s: %s\n", t.name, f.msg.c_str()); }
        catch (const std::exception& e) { ++failed; std::printf("FAIL %s: exception %s\n", t.name, e.what()); }
    }
    std::printf("%zu tests, %d failed\n", testRegistry().size(), failed);
    return failed ? 1 : 0;
}
```

`native/tests/smoke_tests.cpp`

```cpp
#include "test.h"
#include "runtime.h"

TEST(epoch_seconds_is_plausible) {
    auto now = mlt::nowEpochSeconds();
    CHECK(now > 1'700'000'000);
}
```

- [ ] **Step 3: CMake·빌드 스크립트 작성**

`native/CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.20)
set(CMAKE_POLICY_DEFAULT_CMP0091 NEW)
project(mltoybox_native LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
add_compile_definitions(UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)

add_subdirectory(third_party/minhook EXCLUDE_FROM_ALL)

add_library(mlt_core STATIC
    src/runtime.cpp)
target_include_directories(mlt_core PUBLIC src)

add_library(mltoybox_native SHARED src/dllmain.cpp)
target_link_libraries(mltoybox_native PRIVATE mlt_core minhook)

enable_testing()
add_executable(native_tests tests/main.cpp tests/smoke_tests.cpp)
target_link_libraries(native_tests PRIVATE mlt_core)
add_test(NAME native_tests COMMAND native_tests)
```

`tools/build-native.ps1`

```powershell
param([ValidateSet('Release', 'Debug')][string]$Config = 'Release', [switch]$Test)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'Visual Studio C++ build tools not found' }
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
$cmakeDir = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake'
$cmake = Join-Path $cmakeDir 'CMake\bin\cmake.exe'
$ctest = Join-Path $cmakeDir 'CMake\bin\ctest.exe'
$ninja = Join-Path $cmakeDir 'Ninja\ninja.exe'
$src = Join-Path $repo 'native'
$build = Join-Path $src 'build'
$cmd = "`"$vcvars`" >nul && `"$cmake`" -S `"$src`" -B `"$build`" -G Ninja -DCMAKE_MAKE_PROGRAM=`"$ninja`" -DCMAKE_BUILD_TYPE=$Config && `"$cmake`" --build `"$build`""
if ($Test) { $cmd += " && `"$ctest`" --test-dir `"$build`" --output-on-failure" }
cmd /c $cmd
if ($LASTEXITCODE -ne 0) { throw "native build failed ($LASTEXITCODE)" }
Write-Host "Built $build\mltoybox_native.dll"
```

`.gitignore`에 `native/build/` 한 줄을 추가한다.

- [ ] **Step 4: 실행해서 실패 확인**

Run: `pwsh -NoProfile -File E:\MLToybox\tools\build-native.ps1 -Test`
Expected: 컴파일 오류(`runtime.h` 없음)

- [ ] **Step 5: 최소 구현**

`native/src/runtime.h`

```cpp
#pragma once
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace mlt {
long long nowEpochSeconds();
std::optional<std::string> readFileUtf8(const std::filesystem::path& path);
bool writeFileAtomic(const std::filesystem::path& path, std::string_view content);
struct TextSection { const uint8_t* data = nullptr; size_t size = 0; uintptr_t base = 0; };
TextSection mainModuleText();
void runWorker(void* selfModule);
}
```

`native/src/runtime.cpp`

```cpp
#include "runtime.h"
#include <chrono>
#include <fstream>
#include <sstream>
#include <windows.h>

namespace mlt {

long long nowEpochSeconds() {
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

std::optional<std::string> readFileUtf8(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return std::nullopt;
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

bool writeFileAtomic(const std::filesystem::path& path, std::string_view content) {
    auto tmp = path;
    tmp += L".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) return false;
        f.write(content.data(), static_cast<std::streamsize>(content.size()));
        if (!f) return false;
    }
    return MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
}

TextSection mainModuleText() {
    auto base = reinterpret_cast<const uint8_t*>(GetModuleHandleW(nullptr));
    auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    auto nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    auto sec = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec) {
        if (std::memcmp(sec->Name, ".text", 5) == 0) {
            return { base + sec->VirtualAddress, sec->Misc.VirtualSize, reinterpret_cast<uintptr_t>(base) };
        }
    }
    return {};
}

static std::filesystem::path bridgeDirFor(void* selfModule) {
    wchar_t buf[MAX_PATH]{};
    GetModuleFileNameW(static_cast<HMODULE>(selfModule), buf, MAX_PATH);
    // <mod>\native\mltoybox_native.dll → <mod>\bridge
    return std::filesystem::path(buf).parent_path().parent_path() / L"bridge";
}

void runWorker(void* selfModule) {
    const auto bridge = bridgeDirFor(selfModule);
    for (;;) {
        std::string status = "{\"version\":1,\"heartbeat\":" + std::to_string(nowEpochSeconds()) + ",\"appliedSeq\":-1}";
        writeFileAtomic(bridge / L"native_status.json", status);
        Sleep(1000);
    }
}

}
```

`runtime.cpp` 상단 include에 `#include <cstring>`을 추가한다(`std::memcmp`).

`native/src/dllmain.cpp`

```cpp
#include <windows.h>
#include "runtime.h"

static DWORD WINAPI Worker(LPVOID self) {
    mlt::runWorker(self);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        if (HANDLE t = CreateThread(nullptr, 0, Worker, module, 0, nullptr)) CloseHandle(t);
    }
    return TRUE;
}
```

- [ ] **Step 6: 빌드·테스트 통과 확인**

Run: `pwsh -NoProfile -File E:\MLToybox\tools\build-native.ps1 -Test`
Expected: `PASS epoch_seconds_is_plausible`, `100% tests passed`, `Built ...mltoybox_native.dll`

- [ ] **Step 7: loadlib 실측(인게임, 스파이크)**

`backup-saves.ps1` 후, DLL을 수동으로 `<Mods>\MLToybox\native\`에 복사하고 Lab을 배포한다(`deploy.ps1 -Mod MLToyboxLab`). 게임을 실행한 뒤 메뉴에서 Lab으로 다음을 실행한다.

```lua
local p = [[E:\SteamLibrary\steamapps\common\Manor Lords\ManorLords\Binaries\Win64\ue4ss\Mods\MLToybox\native\mltoybox_native.dll]]
print("loadlib", package.loadlib(p, "*"))
```
Expected: `loadlib	true`. 3초 뒤 `bridge\native_status.json`의 heartbeat가 현재 epoch와 5초 이내다.
**실패 시(loadlib 비활성 등):** 정지 사유다. 대안(UE4SS가 로드하는 다른 경로, `version.dll` 프록시)을 적어 사용자에게 보고한다.

- [ ] **Step 8: 커밋** — `git add native tools/build-native.ps1 .gitignore`, 메시지 `build(native): add CMake/MinHook build environment and DLL skeleton`

---

### Task 2: 패턴 스캐너

**Files:**
- Create: `native/src/scanner.h`, `native/src/scanner.cpp`, `native/tests/scanner_tests.cpp`
- Modify: `native/CMakeLists.txt`(`mlt_core`에 `src/scanner.cpp`, 테스트에 `tests/scanner_tests.cpp` 추가)

**Interfaces:**
- Produces(`namespace mlt`):
  - `struct Pattern { std::vector<int16_t> bytes; }`(-1 = 와일드카드)
  - `std::optional<Pattern> parsePattern(std::string_view)`
  - `std::vector<size_t> findAll(std::span<const uint8_t>, const Pattern&, size_t maxHits)`
  - `enum class ScanResult { Unique, NotFound, Ambiguous, BadPattern }`, `struct ScanOutcome { ScanResult result; size_t offset; }`, `ScanOutcome scanUnique(std::span<const uint8_t>, std::string_view)`

- [ ] **Step 1: 실패하는 테스트** — `native/tests/scanner_tests.cpp`

```cpp
#include "test.h"
#include "scanner.h"
#include <array>

using namespace mlt;

TEST(parse_pattern_with_wildcards) {
    auto p = parsePattern("48 8B ?? 05 ? ff");
    CHECK(p.has_value());
    CHECK(p->bytes.size() == 6);
    CHECK(p->bytes[0] == 0x48 && p->bytes[2] == -1 && p->bytes[4] == -1 && p->bytes[5] == 0xFF);
}

TEST(parse_pattern_rejects_bad_input) {
    CHECK(!parsePattern("").has_value());
    CHECK(!parsePattern("?? 48").has_value());   // 첫 바이트 와일드카드 금지
    CHECK(!parsePattern("4G").has_value());
    CHECK(!parsePattern("488B").has_value());    // 공백 필수
    CHECK(!parsePattern("4").has_value());
}

TEST(scan_unique_finds_single_match) {
    std::array<uint8_t, 10> hay{ 0x90, 0x48, 0x8B, 0x05, 0x11, 0x22, 0xC3, 0x90, 0x90, 0x90 };
    auto r = scanUnique(hay, "48 8B 05 ?? ?? C3");
    CHECK(r.result == ScanResult::Unique);
    CHECK(r.offset == 1);
}

TEST(scan_unique_reports_not_found_and_ambiguous) {
    std::array<uint8_t, 8> hay{ 0x48, 0x8B, 0xC3, 0x90, 0x48, 0x8B, 0xC3, 0x90 };
    CHECK(scanUnique(hay, "48 8B C3").result == ScanResult::Ambiguous);
    CHECK(scanUnique(hay, "48 8B C4").result == ScanResult::NotFound);
    CHECK(scanUnique(hay, "zz").result == ScanResult::BadPattern);
}

TEST(scan_handles_match_at_end_and_short_haystack) {
    std::array<uint8_t, 4> hay{ 0x90, 0x90, 0xAB, 0xCD };
    CHECK(scanUnique(hay, "AB CD").offset == 2);
    CHECK(scanUnique(std::span<const uint8_t>(hay.data(), 1), "AB CD").result == ScanResult::NotFound);
}
```

- [ ] **Step 2: 실패 확인** — `build-native.ps1 -Test` → 컴파일 오류(`scanner.h` 없음)

- [ ] **Step 3: 구현**

`native/src/scanner.h`

```cpp
#pragma once
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace mlt {
struct Pattern { std::vector<int16_t> bytes; };
std::optional<Pattern> parsePattern(std::string_view text);
std::vector<size_t> findAll(std::span<const uint8_t> haystack, const Pattern& pattern, size_t maxHits);
enum class ScanResult { Unique, NotFound, Ambiguous, BadPattern };
struct ScanOutcome { ScanResult result; size_t offset; };
ScanOutcome scanUnique(std::span<const uint8_t> haystack, std::string_view pattern);
}
```

`native/src/scanner.cpp`

```cpp
#include "scanner.h"
#include <cctype>

namespace mlt {

static int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::optional<Pattern> parsePattern(std::string_view text) {
    Pattern p;
    size_t i = 0;
    while (i < text.size()) {
        if (text[i] == ' ') { ++i; continue; }
        if (text[i] == '?') {
            p.bytes.push_back(-1);
            ++i;
            if (i < text.size() && text[i] == '?') ++i;
        } else {
            if (i + 1 >= text.size()) return std::nullopt;
            int hi = hexValue(text[i]), lo = hexValue(text[i + 1]);
            if (hi < 0 || lo < 0) return std::nullopt;
            p.bytes.push_back(static_cast<int16_t>(hi * 16 + lo));
            i += 2;
        }
        if (i < text.size() && text[i] != ' ') return std::nullopt;
    }
    if (p.bytes.empty() || p.bytes.front() < 0) return std::nullopt;
    return p;
}

std::vector<size_t> findAll(std::span<const uint8_t> hay, const Pattern& p, size_t maxHits) {
    std::vector<size_t> hits;
    const size_t n = p.bytes.size();
    if (n == 0 || hay.size() < n) return hits;
    const auto first = static_cast<uint8_t>(p.bytes[0]);
    for (size_t i = 0; i + n <= hay.size(); ++i) {
        if (hay[i] != first) continue;
        bool ok = true;
        for (size_t j = 1; j < n; ++j) {
            const int16_t b = p.bytes[j];
            if (b >= 0 && hay[i + j] != static_cast<uint8_t>(b)) { ok = false; break; }
        }
        if (ok) {
            hits.push_back(i);
            if (hits.size() >= maxHits) break;
        }
    }
    return hits;
}

ScanOutcome scanUnique(std::span<const uint8_t> hay, std::string_view text) {
    auto p = parsePattern(text);
    if (!p) return { ScanResult::BadPattern, 0 };
    auto hits = findAll(hay, *p, 2);
    if (hits.empty()) return { ScanResult::NotFound, 0 };
    if (hits.size() > 1) return { ScanResult::Ambiguous, 0 };
    return { ScanResult::Unique, hits[0] };
}

}
```

- [ ] **Step 4: 통과 확인** — `build-native.ps1 -Test` → 스캐너 5개 PASS
- [ ] **Step 5: 커밋** — 메시지 `feat(native): add byte pattern scanner`

---

### Task 3: 최소 JSON 파서 + control 해석 + status 렌더링

**Files:**
- Create: `native/src/json.h/.cpp`, `native/src/control.h/.cpp`, `native/src/status.h/.cpp`
- Create: `native/tests/json_tests.cpp`, `native/tests/control_tests.cpp`, `native/tests/status_tests.cpp`
- Modify: `native/CMakeLists.txt`(소스·테스트 추가)

**Interfaces:**
- Produces(`namespace mlt`):
  - `struct Json`(타입 `Null/Bool/Number/String/Array/Object`, `const Json* get(std::string_view) const`, `bool asBool(bool def) const`, `double asNumber(double def) const`), `std::optional<Json> parseJson(std::string_view)`(UTF-8 BOM 허용), `std::string escapeJson(std::string_view)`
  - `struct NativeControl { long long seq = -1; bool instantBuild = false; bool ignorePlacement = false; bool ignorePopulation = false; }`, `std::optional<NativeControl> parseControl(std::string_view)`(version 1이 아니거나 seq·features가 없으면 nullopt)
  - `struct FeatureState { std::string name; bool installed = false; bool active = false; std::string error; }`, `std::string renderStatus(long long heartbeat, long long appliedSeq, const std::vector<FeatureState>&)`

- [ ] **Step 1: 실패하는 테스트**

`native/tests/json_tests.cpp`

```cpp
#include "test.h"
#include "json.h"

using namespace mlt;

TEST(json_parses_nested_objects_and_types) {
    auto j = parseJson(R"({"a":{"b":true,"n":-2.5e1,"s":"x\"y\u00e9","arr":[1,null,false]},"z":null})");
    CHECK(j.has_value());
    CHECK(j->get("a")->get("b")->asBool(false) == true);
    CHECK(j->get("a")->get("n")->asNumber(0) == -25.0);
    CHECK(j->get("a")->get("s")->s == "x\"y\xC3\xA9");
    CHECK(j->get("a")->get("arr")->a->size() == 3);
    CHECK(j->get("z")->type == Json::Type::Null);
    CHECK(j->get("missing") == nullptr);
}

TEST(json_accepts_bom_and_whitespace) {
    auto j = parseJson("\xEF\xBB\xBF \r\n{ \"k\" : 1 }\n");
    CHECK(j.has_value() && j->get("k")->asNumber(0) == 1.0);
}

TEST(json_rejects_malformed) {
    CHECK(!parseJson("{").has_value());
    CHECK(!parseJson("{\"a\":}").has_value());
    CHECK(!parseJson("{\"a\":1} trailing").has_value());
    CHECK(!parseJson("").has_value());
}

TEST(json_escape_roundtrip) {
    CHECK(escapeJson("a\"b\\c\n") == "a\\\"b\\\\c\\n");
}
```

`native/tests/control_tests.cpp`

```cpp
#include "test.h"
#include "control.h"

using namespace mlt;

TEST(control_combines_enabled_and_flags) {
    auto c = parseControl(R"({"version":1,"seq":7,"features":{
        "build":{"enabled":true,"instantBuild":true,"ignorePlacement":false},
        "military":{"enabled":false,"ignorePopulation":true}}})");
    CHECK(c.has_value());
    CHECK(c->seq == 7);
    CHECK(c->instantBuild == true);
    CHECK(c->ignorePlacement == false);
    CHECK(c->ignorePopulation == false);   // military.enabled=false
}

TEST(control_missing_sections_default_off) {
    auto c = parseControl(R"({"version":1,"seq":1,"features":{}})");
    CHECK(c.has_value() && !c->instantBuild && !c->ignorePlacement && !c->ignorePopulation);
}

TEST(control_rejects_bad_version_or_shape) {
    CHECK(!parseControl(R"({"version":2,"seq":1,"features":{}})").has_value());
    CHECK(!parseControl(R"({"version":1,"features":{}})").has_value());
    CHECK(!parseControl(R"({"version":1,"seq":1})").has_value());
    CHECK(!parseControl("{broken").has_value());
}
```

`native/tests/status_tests.cpp`

```cpp
#include "test.h"
#include "json.h"
#include "status.h"

using namespace mlt;

TEST(status_renders_features_and_omits_empty_error) {
    std::vector<FeatureState> fs{ { "instant_build", true, true, "" }, { "placement", false, false, "pattern not found" } };
    auto j = parseJson(renderStatus(123, 7, fs));
    CHECK(j.has_value());
    CHECK(j->get("version")->asNumber(0) == 1);
    CHECK(j->get("heartbeat")->asNumber(0) == 123);
    CHECK(j->get("appliedSeq")->asNumber(0) == 7);
    CHECK(j->get("features")->get("instant_build")->get("active")->asBool(false));
    CHECK(j->get("features")->get("instant_build")->get("lastError") == nullptr);
    CHECK(j->get("features")->get("placement")->get("lastError")->s == "pattern not found");
}

TEST(status_without_features_omits_map) {
    auto j = parseJson(renderStatus(1, -1, {}));
    CHECK(j.has_value() && j->get("features") == nullptr);
}
```

- [ ] **Step 2: 실패 확인** — 컴파일 오류(헤더 없음)

- [ ] **Step 3: 구현**

`native/src/json.h`

```cpp
#pragma once
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mlt {
struct Json;
using JsonArray = std::vector<Json>;
using JsonObject = std::vector<std::pair<std::string, Json>>;

struct Json {
    enum class Type { Null, Bool, Number, String, Array, Object };
    Type type = Type::Null;
    bool b = false;
    double n = 0;
    std::string s;
    std::shared_ptr<JsonArray> a;
    std::shared_ptr<JsonObject> o;

    const Json* get(std::string_view key) const;
    bool asBool(bool def) const { return type == Type::Bool ? b : def; }
    double asNumber(double def) const { return type == Type::Number ? n : def; }
};

std::optional<Json> parseJson(std::string_view text);
std::string escapeJson(std::string_view text);
}
```

`native/src/json.cpp`

```cpp
#include "json.h"
#include <cstdlib>

namespace mlt {

const Json* Json::get(std::string_view key) const {
    if (type != Type::Object || !o) return nullptr;
    for (auto& [k, v] : *o) if (k == key) return &v;
    return nullptr;
}

namespace {
struct Parser {
    std::string_view t;
    size_t i = 0;

    void ws() { while (i < t.size() && (t[i] == ' ' || t[i] == '\t' || t[i] == '\r' || t[i] == '\n')) ++i; }
    bool eat(char c) { ws(); if (i < t.size() && t[i] == c) { ++i; return true; } return false; }
    bool lit(std::string_view w) { if (t.substr(i, w.size()) == w) { i += w.size(); return true; } return false; }

    static void utf8(std::string& out, unsigned cp) {
        if (cp < 0x80) out += static_cast<char>(cp);
        else if (cp < 0x800) { out += static_cast<char>(0xC0 | (cp >> 6)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
        else { out += static_cast<char>(0xE0 | (cp >> 12)); out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
    }

    bool str(std::string& out) {
        if (!eat('"')) return false;
        while (i < t.size()) {
            char c = t[i++];
            if (c == '"') return true;
            if (c != '\\') { out += c; continue; }
            if (i >= t.size()) return false;
            char e = t[i++];
            switch (e) {
                case '"': out += '"'; break;   case '\\': out += '\\'; break; case '/': out += '/'; break;
                case 'b': out += '\b'; break;  case 'f': out += '\f'; break;  case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;  case 't': out += '\t'; break;
                case 'u': {
                    if (i + 4 > t.size()) return false;
                    unsigned cp = 0;
                    for (int k = 0; k < 4; ++k) {
                        char h = t[i++]; cp <<= 4;
                        if (h >= '0' && h <= '9') cp |= h - '0';
                        else if (h >= 'a' && h <= 'f') cp |= h - 'a' + 10;
                        else if (h >= 'A' && h <= 'F') cp |= h - 'A' + 10;
                        else return false;
                    }
                    utf8(out, cp);
                    break;
                }
                default: return false;
            }
        }
        return false;
    }

    bool value(Json& v) {
        ws();
        if (i >= t.size()) return false;
        char c = t[i];
        if (c == '{') {
            ++i; v.type = Json::Type::Object; v.o = std::make_shared<JsonObject>();
            if (eat('}')) return true;
            do {
                std::string k; Json child;
                ws();
                if (!str(k) || !eat(':') || !value(child)) return false;
                v.o->emplace_back(std::move(k), std::move(child));
            } while (eat(','));
            return eat('}');
        }
        if (c == '[') {
            ++i; v.type = Json::Type::Array; v.a = std::make_shared<JsonArray>();
            if (eat(']')) return true;
            do { Json child; if (!value(child)) return false; v.a->push_back(std::move(child)); } while (eat(','));
            return eat(']');
        }
        if (c == '"') { v.type = Json::Type::String; return str(v.s); }
        if (lit("true")) { v.type = Json::Type::Bool; v.b = true; return true; }
        if (lit("false")) { v.type = Json::Type::Bool; v.b = false; return true; }
        if (lit("null")) { v.type = Json::Type::Null; return true; }
        size_t start = i;
        while (i < t.size() && (std::isdigit(static_cast<unsigned char>(t[i])) || t[i] == '-' || t[i] == '+' || t[i] == '.' || t[i] == 'e' || t[i] == 'E')) ++i;
        if (start == i) return false;
        std::string num(t.substr(start, i - start));
        char* end = nullptr;
        v.type = Json::Type::Number;
        v.n = std::strtod(num.c_str(), &end);
        return end && *end == '\0';
    }
};
}

std::optional<Json> parseJson(std::string_view text) {
    if (text.size() >= 3 && text.substr(0, 3) == "\xEF\xBB\xBF") text.remove_prefix(3);
    Parser p{ text };
    Json v;
    if (!p.value(v)) return std::nullopt;
    p.ws();
    if (p.i != text.size()) return std::nullopt;
    return v;
}

std::string escapeJson(std::string_view s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break; case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break; case '\r': out += "\\r"; break; case '\t': out += "\\t"; break;
            default: out += c;
        }
    }
    return out;
}

}
```

`json.cpp` 상단에 `#include <cctype>`을 추가한다.

`native/src/control.h`

```cpp
#pragma once
#include <optional>
#include <string_view>

namespace mlt {
struct NativeControl {
    long long seq = -1;
    bool instantBuild = false;
    bool ignorePlacement = false;
    bool ignorePopulation = false;
};
std::optional<NativeControl> parseControl(std::string_view text);
}
```

`native/src/control.cpp`

```cpp
#include "control.h"
#include "json.h"

namespace mlt {

static bool flag(const Json* features, const char* section, const char* name) {
    if (!features) return false;
    const Json* s = features->get(section);
    if (!s || !s->get("enabled") || !s->get("enabled")->asBool(false)) return false;
    const Json* f = s->get(name);
    return f && f->asBool(false);
}

std::optional<NativeControl> parseControl(std::string_view text) {
    auto j = parseJson(text);
    if (!j || j->type != Json::Type::Object) return std::nullopt;
    const Json* version = j->get("version");
    const Json* seq = j->get("seq");
    const Json* features = j->get("features");
    if (!version || version->asNumber(0) != 1) return std::nullopt;
    if (!seq || seq->type != Json::Type::Number) return std::nullopt;
    if (!features || features->type != Json::Type::Object) return std::nullopt;
    NativeControl c;
    c.seq = static_cast<long long>(seq->n);
    c.instantBuild = flag(features, "build", "instantBuild");
    c.ignorePlacement = flag(features, "build", "ignorePlacement");
    c.ignorePopulation = flag(features, "military", "ignorePopulation");
    return c;
}

}
```

`native/src/status.h`

```cpp
#pragma once
#include <string>
#include <vector>

namespace mlt {
struct FeatureState { std::string name; bool installed = false; bool active = false; std::string error; };
std::string renderStatus(long long heartbeat, long long appliedSeq, const std::vector<FeatureState>& features);
}
```

`native/src/status.cpp`

```cpp
#include "status.h"
#include "json.h"

namespace mlt {

std::string renderStatus(long long heartbeat, long long appliedSeq, const std::vector<FeatureState>& features) {
    std::string out = "{\"version\":1,\"heartbeat\":" + std::to_string(heartbeat) + ",\"appliedSeq\":" + std::to_string(appliedSeq);
    if (!features.empty()) {
        out += ",\"features\":{";
        for (size_t i = 0; i < features.size(); ++i) {
            const auto& f = features[i];
            if (i) out += ",";
            out += "\"" + escapeJson(f.name) + "\":{\"installed\":" + (f.installed ? "true" : "false") + ",\"active\":" + (f.active ? "true" : "false");
            if (!f.error.empty()) out += ",\"lastError\":\"" + escapeJson(f.error) + "\"";
            out += "}";
        }
        out += "}";
    }
    out += "}";
    return out;
}

}
```

- [ ] **Step 4: 통과 확인** — 전 테스트 PASS
- [ ] **Step 5: 커밋** — 메시지 `feat(native): add minimal JSON, control parsing and status rendering`

---

### Task 4: HookManager + 워커 루프

**Files:**
- Create: `native/src/hooks.h/.cpp`, `native/src/features/features.h/.cpp`, `native/tests/hooks_tests.cpp`
- Modify: `native/src/runtime.cpp`(`runWorker`를 실제 루프로), `native/CMakeLists.txt`

**Interfaces:**
- Produces(`namespace mlt`):
  - `struct HookSpec { std::string name; std::string pattern; void* detour; void** original; bool (*wanted)(const NativeControl&); }`
  - `class HookBackend { virtual bool create(void* target, void* detour, void** original, std::string& err) = 0; virtual bool setEnabled(void* target, bool on, std::string& err) = 0; }`(테스트는 가짜, 런타임은 MinHook)
  - `class HookManager { void add(HookSpec); void installAll(std::span<const uint8_t> text, uintptr_t textAddress, HookBackend&); void sync(const NativeControl&, HookBackend&); std::vector<FeatureState> states() const; }`
  - `void registerFeatures(HookManager&)`: 이 태스크에서는 빈 구현. Task 8~10에서 채운다.
  - 추가로 기능이 detour 안에서 읽는 전역 플래그 `std::atomic<bool>`를 기능별 파일이 소유한다(Task 8~10).

- [ ] **Step 1: 실패하는 테스트** — `native/tests/hooks_tests.cpp`

```cpp
#include "test.h"
#include "hooks.h"
#include <array>

using namespace mlt;

namespace {
struct FakeBackend : HookBackend {
    int created = 0; std::vector<std::pair<void*, bool>> toggles; bool failCreate = false;
    bool create(void*, void*, void** original, std::string& err) override {
        if (failCreate) { err = "MH_ERROR"; return false; }
        ++created; *original = reinterpret_cast<void*>(0x1234); return true;
    }
    bool setEnabled(void* target, bool on, std::string&) override { toggles.push_back({ target, on }); return true; }
};
void* g_orig = nullptr;
int detour() { return 0; }
bool wantsBuild(const NativeControl& c) { return c.instantBuild; }
}

TEST(install_only_on_unique_match_and_report_reasons) {
    std::array<uint8_t, 12> text{ 0x90, 0xAA, 0xBB, 0xCC, 0x90, 0x11, 0x22, 0x90, 0x11, 0x22, 0x90, 0x90 };
    HookManager m; FakeBackend be;
    m.add({ "unique", "AA BB CC", reinterpret_cast<void*>(&detour), &g_orig, &wantsBuild });
    m.add({ "missing", "DE AD", reinterpret_cast<void*>(&detour), &g_orig, &wantsBuild });
    m.add({ "ambiguous", "11 22", reinterpret_cast<void*>(&detour), &g_orig, &wantsBuild });
    m.installAll(text, 0x1000, be);
    auto s = m.states();
    CHECK(be.created == 1);
    CHECK(s[0].installed && s[0].error.empty());
    CHECK(!s[1].installed && s[1].error == "pattern not found");
    CHECK(!s[2].installed && s[2].error == "pattern ambiguous");
}

TEST(sync_toggles_only_on_change) {
    std::array<uint8_t, 4> text{ 0xAA, 0xBB, 0xCC, 0x90 };
    HookManager m; FakeBackend be;
    m.add({ "b", "AA BB CC", reinterpret_cast<void*>(&detour), &g_orig, &wantsBuild });
    m.installAll(text, 0x2000, be);
    NativeControl on; on.instantBuild = true;
    m.sync(on, be); m.sync(on, be);
    CHECK(be.toggles.size() == 1 && be.toggles[0].second == true);
    CHECK(be.toggles[0].first == reinterpret_cast<void*>(0x2000));
    m.sync(NativeControl{}, be);
    CHECK(be.toggles.size() == 2 && be.toggles[1].second == false);
    CHECK(m.states()[0].active == false);
}

TEST(create_failure_reported_and_never_enabled) {
    std::array<uint8_t, 4> text{ 0xAA, 0xBB, 0xCC, 0x90 };
    HookManager m; FakeBackend be; be.failCreate = true;
    m.add({ "b", "AA BB CC", reinterpret_cast<void*>(&detour), &g_orig, &wantsBuild });
    m.installAll(text, 0, be);
    NativeControl on; on.instantBuild = true;
    m.sync(on, be);
    CHECK(!m.states()[0].installed && m.states()[0].error == "MH_ERROR" && be.toggles.empty());
}
```

- [ ] **Step 2: 실패 확인** — 컴파일 오류

- [ ] **Step 3: 구현**

`native/src/hooks.h`

```cpp
#pragma once
#include "control.h"
#include "status.h"
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace mlt {
struct HookSpec {
    std::string name;
    std::string pattern;
    void* detour;
    void** original;
    bool (*wanted)(const NativeControl&);
};

class HookBackend {
public:
    virtual ~HookBackend() = default;
    virtual bool create(void* target, void* detour, void** original, std::string& err) = 0;
    virtual bool setEnabled(void* target, bool on, std::string& err) = 0;
};

class HookManager {
public:
    void add(HookSpec spec);
    void installAll(std::span<const uint8_t> text, uintptr_t textAddress, HookBackend& backend);
    void sync(const NativeControl& control, HookBackend& backend);
    std::vector<FeatureState> states() const;
private:
    struct Entry { HookSpec spec; void* target = nullptr; FeatureState state; };
    std::vector<Entry> entries_;
};

void registerFeatures(HookManager& manager);
}
```

`native/src/hooks.cpp`

```cpp
#include "hooks.h"
#include "scanner.h"

namespace mlt {

void HookManager::add(HookSpec spec) {
    Entry e{ std::move(spec) };
    e.state.name = e.spec.name;
    entries_.push_back(std::move(e));
}

void HookManager::installAll(std::span<const uint8_t> text, uintptr_t textAddress, HookBackend& backend) {
    for (auto& e : entries_) {
        auto r = scanUnique(text, e.spec.pattern);
        if (r.result == ScanResult::NotFound) { e.state.error = "pattern not found"; continue; }
        if (r.result == ScanResult::Ambiguous) { e.state.error = "pattern ambiguous"; continue; }
        if (r.result == ScanResult::BadPattern) { e.state.error = "bad pattern"; continue; }
        void* target = reinterpret_cast<void*>(textAddress + r.offset);
        std::string err;
        if (!backend.create(target, e.spec.detour, e.spec.original, err)) { e.state.error = err; continue; }
        e.target = target;
        e.state.installed = true;
    }
}

void HookManager::sync(const NativeControl& control, HookBackend& backend) {
    for (auto& e : entries_) {
        if (!e.state.installed) continue;
        bool want = e.spec.wanted(control);
        if (want == e.state.active) continue;
        std::string err;
        if (backend.setEnabled(e.target, want, err)) { e.state.active = want; e.state.error.clear(); }
        else e.state.error = err;
    }
}

std::vector<FeatureState> HookManager::states() const {
    std::vector<FeatureState> out;
    for (auto& e : entries_) out.push_back(e.state);
    return out;
}

}
```

`native/src/features/features.h`

```cpp
#pragma once
#include "hooks.h"
```

`native/src/features/features.cpp`

```cpp
#include "features/features.h"

namespace mlt {
// Task 8~10 에서 기능별 HookSpec 을 추가한다
void registerFeatures(HookManager&) {}
}
```

`native/src/runtime.cpp`의 `runWorker`를 다음으로 바꾼다(상단에 `#include "hooks.h"`, `#include "control.h"`, `#include "status.h"`, `#include <MinHook.h>` 추가).

```cpp
namespace {
struct MinHookBackend : mlt::HookBackend {
    bool create(void* target, void* detour, void** original, std::string& err) override {
        MH_STATUS s = MH_CreateHook(target, detour, original);
        if (s != MH_OK) { err = MH_StatusToString(s); return false; }
        return true;
    }
    bool setEnabled(void* target, bool on, std::string& err) override {
        MH_STATUS s = on ? MH_EnableHook(target) : MH_DisableHook(target);
        if (s != MH_OK) { err = MH_StatusToString(s); return false; }
        return true;
    }
};
}

void runWorker(void* selfModule) {
    const auto bridge = bridgeDirFor(selfModule);
    HookManager hooks;
    registerFeatures(hooks);
    MinHookBackend backend;
    if (MH_Initialize() == MH_OK) {
        auto text = mainModuleText();
        hooks.installAll(std::span<const uint8_t>(text.data, text.size), reinterpret_cast<uintptr_t>(text.data), backend);
    }
    NativeControl control;
    for (;;) {
        if (auto s = readFileUtf8(bridge / L"control.json")) {
            if (auto c = parseControl(*s)) control = *c;   // 깨진 파일이면 직전 값 유지
        }
        hooks.sync(control, backend);
        writeFileAtomic(bridge / L"native_status.json", renderStatus(nowEpochSeconds(), control.seq, hooks.states()));
        Sleep(1000);
    }
}
```

`native/CMakeLists.txt`:
- `mlt_core` 소스: `src/runtime.cpp src/scanner.cpp src/json.cpp src/control.cpp src/status.cpp src/hooks.cpp src/features/features.cpp`
- `target_link_libraries(mlt_core PUBLIC minhook)`, `target_include_directories(mlt_core PUBLIC src third_party/minhook/include)`
- 테스트 소스에 `tests/hooks_tests.cpp` 추가

- [ ] **Step 4: 통과 확인** — 전 테스트 PASS, DLL 빌드 성공
- [ ] **Step 5: 커밋** — 메시지 `feat(native): add hook manager and control-driven worker loop`

---

### Task 5: Lua 로더·상태 병합 + 배포 + 패널 표시

**Files:**
- Create: `mod/MLToybox/Scripts/core/native.lua`, `mod/MLToybox/tests/native_spec.lua`
- Modify: `mod/MLToybox/Scripts/main.lua`(로드·병합), `tools/deploy.ps1`(DLL 복사), `tools/tests/Tools.Tests.ps1`
- Modify: `panel/MLToybox.Panel.Core/StatusDocument.cs`(`Native`), `panel/MLToybox.Tests/BridgeClientTests.cs`, `panel/MLToybox.Panel/MainForm.cs`(상태 탭)

**Interfaces:**
- Produces:
  - `native.STALE_SEC = 5`, `native.loadlib`(테스트 교체용, 기본 `package.loadlib`), `native.load(nativeDir) -> ok:boolean, err:string|nil`, `native.status(statusPath, now, loaded, loadErr) -> table`(`{loaded, error, heartbeat, stale, features}`)
  - `status.json`의 `native` 항목. C#: `StatusDocument.Native : NativeStatus?`, `NativeStatus { bool Loaded; string? Error; long? Heartbeat; bool Stale; Dictionary<string,NativeFeature>? Features }`, `NativeFeature { bool Installed; bool Active; string? LastError }`

- [ ] **Step 1: 실패하는 테스트**

`mod/MLToybox/tests/native_spec.lua`

```lua
local T = require("t")
local native = require("core.native")

local function write(path, text) local f = assert(io.open(path, "wb")); f:write(text); f:close() end

T.run({
  load_missing_dll_reports_not_deployed = function()
    local ok, err = native.load(TEST_TMP .. "\\nope")
    T.eq(ok, false, "not ok"); T.eq(err, "not deployed", "reason")
  end,
  load_failure_reports_error = function()
    os.execute('mkdir "' .. TEST_TMP .. '\\n1"')
    write(TEST_TMP .. "\\n1\\mltoybox_native.dll", "x")
    native.loadlib = function() return nil, "blocked", "open" end
    local ok, err = native.load(TEST_TMP .. "\\n1")
    T.eq(ok, false, "not ok"); T.truthy(err:find("blocked", 1, true), "reason")
  end,
  load_success = function()
    native.loadlib = function() return true end
    T.eq((native.load(TEST_TMP .. "\\n1")), true, "ok")
  end,
  status_merges_fresh_native_status = function()
    local p = TEST_TMP .. "\\ns.json"
    write(p, '{"version":1,"heartbeat":100,"appliedSeq":3,"features":{"instant_build":{"installed":true,"active":false}}}')
    local s = native.status(p, 102, true, nil)
    T.eq(s.loaded, true, "loaded"); T.eq(s.stale, false, "fresh"); T.eq(s.heartbeat, 100, "hb")
    T.eq(s.features.instant_build.installed, true, "feature")
  end,
  status_marks_stale_heartbeat = function()
    local p = TEST_TMP .. "\\ns2.json"
    write(p, '{"version":1,"heartbeat":100,"appliedSeq":-1}')
    local s = native.status(p, 106, true, nil)
    T.eq(s.stale, true, "stale"); T.eq(s.features, nil, "no features")
  end,
  status_without_file_or_load = function()
    local s = native.status(TEST_TMP .. "\\none.json", 1, false, "not deployed")
    T.eq(s.loaded, false, "not loaded"); T.eq(s.error, "not deployed", "error"); T.eq(s.stale, true, "stale when missing")
  end,
})
```

`BridgeClientTests.cs`에 추가:

```csharp
    [Fact]
    public void ReadStatus_ParsesNativeSection()
    {
        var c = NewClient(out var dir);
        Directory.CreateDirectory(dir);
        File.WriteAllText(c.StatusPath,
            """{"version":1,"heartbeat":1800000000,"inGame":true,"native":{"loaded":true,"stale":false,"heartbeat":1800000000,"features":{"instant_build":{"installed":true,"active":true}}}}""");
        var s = c.ReadStatus()!;
        Assert.True(s.Native!.Loaded);
        Assert.False(s.Native.Stale);
        Assert.True(s.Native.Features!["instant_build"].Active);
    }
```

`Tools.Tests.ps1`에 추가(`if ($script:failed ...` 앞):

```powershell
Test-Case 'deploy copies native dll when built and tolerates locked target' {
    $repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
    $built = Join-Path $repo 'native\build\mltoybox_native.dll'
    $created = $false
    if (-not (Test-Path $built)) { New-Item -ItemType Directory -Force (Split-Path $built) | Out-Null; Set-Content $built 'fake'; $created = $true }
    try {
        $g = New-FakeGame
        $mods = Join-Path $g 'ManorLords\Binaries\Win64\ue4ss\Mods'
        & "$PSScriptRoot\..\deploy.ps1" -Mod MLToybox -GameDir $g | Out-Null
        $target = "$mods\MLToybox\native\mltoybox_native.dll"
        Assert-True (Test-Path $target) 'dll copied'
        $lock = [System.IO.File]::Open($target, 'Open', 'Read', 'None')
        try { & "$PSScriptRoot\..\deploy.ps1" -Mod MLToybox -GameDir $g 3>$null | Out-Null } finally { $lock.Dispose() }
    } finally { if ($created) { Remove-Item $built } }
}
```

- [ ] **Step 2: 실패 확인** — `native_spec.lua`(모듈 없음), C# 컴파일 오류(`Native` 없음), deploy 케이스 FAIL

- [ ] **Step 3: 구현**

`mod/MLToybox/Scripts/core/native.lua`

```lua
local json = require("lib.json")
local fileio = require("core.fileio")

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

function M.status(statusPath, now, loaded, loadErr)
  local out = { loaded = loaded, error = loadErr, stale = true }
  local text = fileio.read(statusPath)
  if text then
    local ok, d = pcall(json.decode, text)
    if ok and type(d) == "table" and type(d.heartbeat) == "number" then
      out.heartbeat = d.heartbeat
      out.stale = (now - d.heartbeat) > M.STALE_SEC
      if type(d.features) == "table" and next(d.features) then out.features = d.features end
    end
  end
  return out
end

return M
```

`main.lua` 변경:
- `local gamemode = require("core.gamemode")` 아래에 `local native = require("core.native")` 추가
- `local bridge = bridgeLib.new(bridgeDir)` 아래에 추가:

```lua
local nativeLoaded, nativeErr = native.load(paths.parentDir(scriptsDir) .. "\\native")
log.info("native: %s", nativeLoaded and "loaded" or tostring(nativeErr))
```
- 루프 안 `bridge:writeStatus(registry:status(now, appliedSeq, bridge.lastError))`를 다음으로 바꾼다:

```lua
        local status = registry:status(now, appliedSeq, bridge.lastError)
        status.native = native.status(bridgeDir .. "\\native_status.json", now, nativeLoaded, nativeErr)
        local wrote, werr = bridge:writeStatus(status)
```

`tools/deploy.ps1`: `if ($Mod -eq 'MLToybox') { ... 'bridge' ... }` 줄 아래에 추가:

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

`StatusDocument.cs`에 추가:

```csharp
    public NativeStatus? Native { get; set; }
}

public sealed class NativeStatus
{
    public bool Loaded { get; set; }
    public string? Error { get; set; }
    public long? Heartbeat { get; set; }
    public bool Stale { get; set; }
    public Dictionary<string, NativeFeature>? Features { get; set; }
}

public sealed class NativeFeature
{
    public bool Installed { get; set; }
    public bool Active { get; set; }
    public string? LastError { get; set; }
```
(기존 `StatusDocument` 클래스의 마지막 속성 뒤, 닫는 중괄호 위치를 위와 같이 확장한다.)

`MainForm.cs` `RefreshStatus`의 `_statusText.Text = ...` 직전에 추가:

```csharp
        lines.Add("");
        lines.Add("[네이티브]");
        var n = status.Native;
        if (n is null) lines.Add("(정보 없음)");
        else
        {
            lines.Add(!n.Loaded ? $"미로드: {n.Error ?? "-"}" : n.Stale ? "응답 없음 (heartbeat 끊김)" : "동작 중");
            if (n.Features is not null)
                foreach (var (name, f) in n.Features.OrderBy(p => p.Key))
                    lines.Add($"{name,-16} installed={f.Installed,-5} active={f.Active,-5} error={f.LastError ?? "-"}");
        }
```

- [ ] **Step 4: 통과 확인** — `dotnet test` 전부 PASS, `Tools.Tests.ps1` ALL PASS, `dotnet build -warnaserror:nullable` 오류 0
- [ ] **Step 5: 커밋** — 메시지 `feat: load native DLL from Lua and surface native status in panel`

---

### Task 6: 역분석 도구 MLToybox.Re

**Files:**
- Create: `tools/re/MLToybox.Re/MLToybox.Re.csproj`(`dotnet new console`, net8.0, `Iced` 1.21.0), `PeImage.cs`, `Pattern.cs`, `ExecResolver.cs`, `Disasm.cs`, `SigMaker.cs`, `Program.cs`
- Create: `panel/MLToybox.Tests/RePatternTests.cs`, `panel/MLToybox.Tests/ReSigMakerTests.cs`, `panel/MLToybox.Tests/ReGameExeTests.cs`
- Modify: `panel/MLToybox.sln`(Re 프로젝트 추가), `panel/MLToybox.Tests/MLToybox.Tests.csproj`(Re 참조)

**Interfaces:**
- Produces(`namespace MLToybox.Re`):
  - `PeImage.Load(path)`, `PeImage.FromText(byte[] text, ulong textVa)`(테스트용), `ImageBase`, `Sections`, `Text`, `SectionBytes(section)`, `RvaToOffset(uint)`, `ReadU64(uint rva)`
  - `Pattern.Parse(string)`(C++과 같은 형식), `Pattern.Count(ReadOnlySpan<byte>, int max)`, `Pattern.ToString()`
  - `ExecResolver.Resolve(PeImage, string ufunctionName) -> IReadOnlyList<ulong>`(exec 썽크 VA 후보)
  - `Disasm.Lines(PeImage, ulong va, int maxInstructions) -> IReadOnlyList<string>`(`"VA  bytes  asm"`)
  - `SigMaker.Make(PeImage, ulong va, int maxBytes = 64) -> string?`(`.text`에서 유일해지는 최소 패턴. 분기 rel32·RIP 상대 변위는 와일드카드)
  - CLI:
    - `dotnet run --project tools/re/MLToybox.Re -- exec <UFunction>`
    - `disasm <va hex> [n]`
    - `sig <va hex>`
    - `count "<pattern>"`
    - `strings <text>`(ASCII·UTF-16 위치와 참조하는 코드 VA)
  - 기본 exe 경로는 `common.ps1`과 같은 탐지 대신 `--exe <path>` 인자로 받고, 없으면 `E:\SteamLibrary\steamapps\common\Manor Lords\ManorLords\Binaries\Win64\ManorLords-Win64-Shipping.exe`

- [ ] **Step 1: 프로젝트 생성**

```powershell
cd E:\MLToybox\tools\re
dotnet new console -n MLToybox.Re -f net8.0
dotnet add MLToybox.Re package Iced --version 1.21.0
cd E:\MLToybox\panel
dotnet sln add ..\tools\re\MLToybox.Re\MLToybox.Re.csproj
dotnet add MLToybox.Tests reference ..\tools\re\MLToybox.Re\MLToybox.Re.csproj
```
Re csproj의 `<TargetFramework>`를 `net8.0-windows`로 맞춘다(테스트 프로젝트와 동일).

- [ ] **Step 2: 실패하는 테스트**

`panel/MLToybox.Tests/RePatternTests.cs`

```csharp
using MLToybox.Re;
using Xunit;

namespace MLToybox.Tests;

public class RePatternTests
{
    [Fact]
    public void Parse_SameFormatAsNative()
    {
        var p = Pattern.Parse("48 8B ?? 05 ? ff");
        Assert.Equal("48 8B ?? 05 ?? FF", p.ToString());
        Assert.Throws<FormatException>(() => Pattern.Parse("?? 48"));
        Assert.Throws<FormatException>(() => Pattern.Parse("488B"));
    }

    [Fact]
    public void Count_StopsAtMax()
    {
        byte[] hay = { 0x11, 0x22, 0x90, 0x11, 0x22, 0x90, 0x11, 0x22 };
        Assert.Equal(2, Pattern.Parse("11 22").Count(hay, 2));
        Assert.Equal(3, Pattern.Parse("11 22").Count(hay, 10));
        Assert.Equal(0, Pattern.Parse("33").Count(hay, 10));
    }
}
```

`panel/MLToybox.Tests/ReSigMakerTests.cs`

```csharp
using MLToybox.Re;
using Xunit;

namespace MLToybox.Tests;

public class ReSigMakerTests
{
    // 두 함수가 같은 프롤로그로 시작하고, 첫 번째만 뒤에 고유한 call 이 있다
    private static readonly byte[] Text =
    {
        0x48, 0x89, 0x5C, 0x24, 0x08,             // mov [rsp+8], rbx
        0x57,                                     // push rdi
        0x48, 0x83, 0xEC, 0x20,                   // sub rsp, 0x20
        0xE8, 0x10, 0x00, 0x00, 0x00,             // call rel32  (변위는 와일드카드 대상)
        0x48, 0x8B, 0x05, 0x44, 0x33, 0x22, 0x11, // mov rax, [rip+disp32] (와일드카드 대상)
        0xC3,                                     // ret
        0xCC, 0xCC,
        0x48, 0x89, 0x5C, 0x24, 0x08,
        0x57,
        0x48, 0x83, 0xEC, 0x20,
        0x33, 0xC0,                               // xor eax, eax
        0xC3,
    };

    [Fact]
    public void Make_ProducesUniquePatternWithWildcardedRelatives()
    {
        var pe = PeImage.FromText(Text, 0x140001000);
        var sig = SigMaker.Make(pe, 0x140001000)!;
        Assert.NotNull(sig);
        Assert.StartsWith("48 89 5C 24 08 57 48 83 EC 20 E8 ?? ?? ?? ??", sig);
        Assert.Equal(1, Pattern.Parse(sig).Count(Text, 5));
    }

    [Fact]
    public void Make_ReturnsNullWhenNeverUnique()
    {
        byte[] twice = { 0x90, 0xC3, 0x90, 0xC3 };
        var pe = PeImage.FromText(twice, 0x1000);
        Assert.Null(SigMaker.Make(pe, 0x1000, maxBytes: 2));
    }
}
```

`panel/MLToybox.Tests/ReGameExeTests.cs`

```csharp
using MLToybox.Re;
using Xunit;

namespace MLToybox.Tests;

public class ReGameExeTests
{
    private const string Exe = @"E:\SteamLibrary\steamapps\common\Manor Lords\ManorLords\Binaries\Win64\ManorLords-Win64-Shipping.exe";

    [Fact]
    public void ExecResolver_FindsThunkForKnownUFunction()
    {
        var pe = PeImage.Load(Exe);
        var thunks = ExecResolver.Resolve(pe, "getConstructionProgress");
        Assert.NotEmpty(thunks);
        var text = pe.Text;
        Assert.All(thunks, va => Assert.InRange(va, pe.ImageBase + text.VirtualAddress, pe.ImageBase + text.VirtualAddress + text.VirtualSize));
    }
}
```

- [ ] **Step 3: 실패 확인** — 컴파일 오류(타입 없음)

- [ ] **Step 4: 구현**

`tools/re/MLToybox.Re/PeImage.cs`

```csharp
using System.Text;

namespace MLToybox.Re;

public sealed record Section(string Name, uint VirtualAddress, uint VirtualSize, uint RawOffset, uint RawSize);

public sealed class PeImage
{
    public byte[] Bytes { get; }
    public ulong ImageBase { get; }
    public IReadOnlyList<Section> Sections { get; }

    private PeImage(byte[] bytes, ulong imageBase, IReadOnlyList<Section> sections)
    {
        Bytes = bytes; ImageBase = imageBase; Sections = sections;
    }

    public Section Text => Sections.First(s => s.Name == ".text");

    public static PeImage Load(string path) => Parse(File.ReadAllBytes(path));

    public static PeImage Parse(byte[] b)
    {
        int pe = BitConverter.ToInt32(b, 0x3C);
        if (b[pe] != 'P' || b[pe + 1] != 'E') throw new InvalidDataException("not a PE file");
        ushort count = BitConverter.ToUInt16(b, pe + 6);
        ushort optSize = BitConverter.ToUInt16(b, pe + 20);
        int opt = pe + 24;
        if (BitConverter.ToUInt16(b, opt) != 0x20B) throw new InvalidDataException("not PE32+");
        ulong imageBase = BitConverter.ToUInt64(b, opt + 24);
        var sections = new List<Section>();
        int sec = opt + optSize;
        for (int i = 0; i < count; i++, sec += 40)
        {
            string name = Encoding.ASCII.GetString(b, sec, 8).TrimEnd('\0');
            sections.Add(new Section(name,
                BitConverter.ToUInt32(b, sec + 12), BitConverter.ToUInt32(b, sec + 8),
                BitConverter.ToUInt32(b, sec + 20), BitConverter.ToUInt32(b, sec + 16)));
        }
        return new PeImage(b, imageBase, sections);
    }

    // 테스트용: .text 하나만 있는 가상 이미지 (파일 오프셋 0 = textVa)
    public static PeImage FromText(byte[] text, ulong textVa) =>
        new(text, textVa, new[] { new Section(".text", 0, (uint)text.Length, 0, (uint)text.Length) });

    public ReadOnlySpan<byte> SectionBytes(Section s) =>
        Bytes.AsSpan((int)s.RawOffset, (int)Math.Min(s.RawSize, s.VirtualSize));

    public int? RvaToOffset(uint rva)
    {
        foreach (var s in Sections)
            if (rva >= s.VirtualAddress && rva < s.VirtualAddress + Math.Max(s.VirtualSize, s.RawSize))
            {
                uint off = rva - s.VirtualAddress;
                return off < s.RawSize ? (int)(s.RawOffset + off) : null;
            }
        return null;
    }

    public uint? OffsetToRva(int offset)
    {
        foreach (var s in Sections)
            if (offset >= s.RawOffset && offset < s.RawOffset + s.RawSize)
                return (uint)(s.VirtualAddress + (offset - s.RawOffset));
        return null;
    }

    public ulong ReadU64(uint rva) => BitConverter.ToUInt64(Bytes, RvaToOffset(rva) ?? throw new ArgumentOutOfRangeException(nameof(rva)));
}
```

`tools/re/MLToybox.Re/Pattern.cs`

```csharp
using System.Globalization;

namespace MLToybox.Re;

public sealed class Pattern
{
    private readonly short[] _bytes;
    private Pattern(short[] bytes) => _bytes = bytes;

    public static Pattern Parse(string text)
    {
        var parts = text.Split(' ', StringSplitOptions.RemoveEmptyEntries);
        var bytes = new List<short>();
        foreach (var p in parts)
        {
            if (p is "?" or "??") { bytes.Add(-1); continue; }
            if (p.Length != 2 || !byte.TryParse(p, NumberStyles.HexNumber, null, out var v)) throw new FormatException($"bad token '{p}'");
            bytes.Add(v);
        }
        if (bytes.Count == 0 || bytes[0] < 0) throw new FormatException("empty or leading wildcard");
        return new Pattern(bytes.ToArray());
    }

    public static Pattern FromBytes(IEnumerable<short> bytes) => new(bytes.ToArray());

    public int Count(ReadOnlySpan<byte> hay, int max)
    {
        int n = _bytes.Length, hits = 0;
        byte first = (byte)_bytes[0];
        for (int i = 0; i + n <= hay.Length; i++)
        {
            if (hay[i] != first) continue;
            bool ok = true;
            for (int j = 1; j < n; j++)
                if (_bytes[j] >= 0 && hay[i + j] != (byte)_bytes[j]) { ok = false; break; }
            if (ok && ++hits >= max) break;
        }
        return hits;
    }

    public override string ToString() => string.Join(' ', _bytes.Select(b => b < 0 ? "??" : ((byte)b).ToString("X2")));
}
```

`tools/re/MLToybox.Re/ExecResolver.cs`

```csharp
using System.Text;

namespace MLToybox.Re;

// UE5 는 UFunction 네이티브 등록 테이블에 { const char* Name; FNativeFuncPtr Pointer } (FNameNativePtrPair) 을 둔다.
// 이름 문자열 VA 를 가리키는 포인터를 데이터 섹션에서 찾고, 바로 다음 qword 를 exec 썽크로 본다.
public static class ExecResolver
{
    public static IReadOnlyList<ulong> Resolve(PeImage pe, string name)
    {
        var needle = Encoding.ASCII.GetBytes("\0" + name + "\0");
        var text = pe.Text;
        var results = new List<ulong>();
        foreach (var s in pe.Sections.Where(s => s.Name != ".text"))
        {
            var bytes = pe.SectionBytes(s);
            for (int i = bytes.IndexOf(needle); i >= 0; i = Next(bytes, needle, i + 1))
            {
                ulong strVa = pe.ImageBase + s.VirtualAddress + (uint)i + 1;
                foreach (var refVa in FindPointers(pe, strVa))
                {
                    ulong thunk = pe.ReadU64((uint)(refVa - pe.ImageBase + 8));
                    ulong textStart = pe.ImageBase + text.VirtualAddress;
                    if (thunk >= textStart && thunk < textStart + text.VirtualSize) results.Add(thunk);
                }
            }
        }
        return results.Distinct().ToList();
    }

    private static int Next(ReadOnlySpan<byte> hay, byte[] needle, int from)
    {
        if (from >= hay.Length) return -1;
        int j = hay[from..].IndexOf(needle);
        return j < 0 ? -1 : from + j;
    }

    // Span 지역 변수는 yield 이터레이터에서 쓸 수 없으므로 List 로 모아 반환한다
    public static List<ulong> FindPointers(PeImage pe, ulong va)
    {
        var found = new List<ulong>();
        foreach (var s in pe.Sections.Where(s => s.Name is ".rdata" or ".data"))
        {
            var bytes = pe.SectionBytes(s);
            for (int i = 0; i + 8 <= bytes.Length; i += 8)
                if (BitConverter.ToUInt64(bytes.Slice(i, 8)) == va)
                    found.Add(pe.ImageBase + s.VirtualAddress + (uint)i);
        }
        return found;
    }
}
```

`tools/re/MLToybox.Re/Disasm.cs`

```csharp
using Iced.Intel;

namespace MLToybox.Re;

public static class Disasm
{
    public static IEnumerable<(Instruction Instr, byte[] Bytes, ConstantOffsets Offsets)> Decode(PeImage pe, ulong va, int maxInstructions)
    {
        int off = pe.RvaToOffset((uint)(va - pe.ImageBase)) ?? throw new ArgumentOutOfRangeException(nameof(va));
        var reader = new ByteArrayCodeReader(pe.Bytes, off, pe.Bytes.Length - off);
        var decoder = Decoder.Create(64, reader, va);
        for (int n = 0; n < maxInstructions; n++)
        {
            decoder.Decode(out var instr);
            if (instr.IsInvalid) yield break;
            var bytes = pe.Bytes.AsSpan(off + (int)(instr.IP - va), instr.Length).ToArray();
            yield return (instr, bytes, decoder.GetConstantOffsets(instr));
        }
    }

    public static IReadOnlyList<string> Lines(PeImage pe, ulong va, int maxInstructions)
    {
        var formatter = new NasmFormatter();
        var output = new StringOutput();
        var lines = new List<string>();
        foreach (var (instr, bytes, _) in Decode(pe, va, maxInstructions))
        {
            formatter.Format(instr, output);
            lines.Add($"{instr.IP:X16}  {Convert.ToHexString(bytes),-24}  {output.ToStringAndReset()}");
        }
        return lines;
    }
}
```

`tools/re/MLToybox.Re/SigMaker.cs`

```csharp
using Iced.Intel;

namespace MLToybox.Re;

public static class SigMaker
{
    public static string? Make(PeImage pe, ulong va, int maxBytes = 64)
    {
        var text = pe.SectionBytes(pe.Text);
        var acc = new List<short>();
        foreach (var (instr, bytes, offs) in Disasm.Decode(pe, va, 64))
        {
            var masked = bytes.Select(b => (short)b).ToArray();
            bool branch = instr.FlowControl is FlowControl.Call or FlowControl.UnconditionalBranch or FlowControl.ConditionalBranch;
            if (instr.IsIPRelativeMemoryOperand && offs.HasDisplacement)
                for (int k = 0; k < offs.DisplacementSize; k++) masked[offs.DisplacementOffset + k] = -1;
            if (branch && offs.HasImmediate)
                for (int k = 0; k < offs.ImmediateSize; k++) masked[offs.ImmediateOffset + k] = -1;
            foreach (var m in masked)
            {
                if (acc.Count >= maxBytes) return null;
                acc.Add(m);
            }
            // 명령어 경계에서만 유일성 검사. 끝의 와일드카드는 남겨 둔다(스캐너가 허용, 명령어 길이 보존)
            if (acc.Count >= 8 && Pattern.FromBytes(acc).Count(text, 2) == 1)
                return Pattern.FromBytes(acc).ToString();
        }
        return null;
    }
}
```

`tools/re/MLToybox.Re/Program.cs`

```csharp
using System.Globalization;
using System.Text;
using MLToybox.Re;

var argList = args.ToList();
string exe = @"E:\SteamLibrary\steamapps\common\Manor Lords\ManorLords\Binaries\Win64\ManorLords-Win64-Shipping.exe";
int exeIdx = argList.IndexOf("--exe");
if (exeIdx >= 0) { exe = argList[exeIdx + 1]; argList.RemoveRange(exeIdx, 2); }
if (argList.Count == 0) { Console.WriteLine("usage: exec <UFunction> | disasm <va> [n] | sig <va> | count \"<pattern>\" | strings <text>"); return 1; }

var pe = PeImage.Load(exe);
ulong Hex(string s) => ulong.Parse(s.Replace("0x", ""), NumberStyles.HexNumber);

switch (argList[0])
{
    case "exec":
        foreach (var t in ExecResolver.Resolve(pe, argList[1])) Console.WriteLine($"{t:X16}");
        break;
    case "disasm":
        foreach (var l in Disasm.Lines(pe, Hex(argList[1]), argList.Count > 2 ? int.Parse(argList[2]) : 40)) Console.WriteLine(l);
        break;
    case "sig":
        Console.WriteLine(SigMaker.Make(pe, Hex(argList[1])) ?? "NOT UNIQUE within 64 bytes");
        break;
    case "count":
        Console.WriteLine(Pattern.Parse(argList[1]).Count(pe.SectionBytes(pe.Text), 10));
        break;
    case "strings":
        foreach (var enc in new[] { Encoding.ASCII, Encoding.Unicode })
        {
            var needle = enc.GetBytes(argList[1]);
            var all = pe.Bytes.AsSpan();
            for (int i = all.IndexOf(needle); i >= 0; )
            {
                uint? rva = pe.OffsetToRva(i);
                if (rva is not null) Console.WriteLine($"{(enc == Encoding.ASCII ? "ascii" : "utf16")} {pe.ImageBase + rva.Value:X16}");
                int next = all[(i + 1)..].IndexOf(needle);
                i = next < 0 ? -1 : i + 1 + next;
            }
        }
        break;
    default:
        Console.WriteLine($"unknown command {argList[0]}"); return 1;
}
return 0;
```

- [ ] **Step 5: 통과 확인** — `dotnet test` 전부 PASS(Re 테스트 5개 포함)
- [ ] **Step 6: 실측** — `dotnet run --project E:\MLToybox\tools\re\MLToybox.Re -- exec getConstructionProgress` → 썽크 VA 1개 이상. `disasm <그 VA> 30`이 정상 출력되고, 썽크 안의 `call` 대상(구현 함수 후보)이 보인다.
- [ ] **Step 7: 커밋** — `git add tools/re panel`, 메시지 `feat(re): add PE/exec-thunk/disasm/signature analysis tool`

---

### Task 7: README·빌드 절차 문서화 + 중간 머지 가능 상태 점검

**Files:**
- Modify: `README.md`("네이티브 계층" 절: 빌드, 배포, 게임 업데이트 후 패턴 재생성 절차)

- [ ] **Step 1: README 추가 내용**

```markdown
## 네이티브 계층 (spec §11)
- 빌드: `pwsh tools/build-native.ps1 -Test` → `native/build/mltoybox_native.dll`
- 배포: `pwsh tools/deploy.ps1 -Mod MLToybox` (DLL이 있으면 `Mods/MLToybox/native/`에 복사. 게임 실행 중에는 DLL이 잠겨 경고만 남음)
- 상태: 패널 [상태] 탭 "[네이티브]" (미로드/응답 없음/동작 중, 기능별 installed/active)
- 분석 도구: `dotnet run --project tools/re/MLToybox.Re -- <exec|disasm|sig|count|strings> ...`
- 게임 업데이트로 기능이 `pattern not found`/`pattern ambiguous`가 되면:
  1. `analysis/findings.md`의 해당 기능 절에 적힌 UFunction 이름 또는 문자열로 `exec`/`strings`를 다시 실행해 대상 함수를 찾는다
  2. `sig <VA>`로 새 패턴을 만들고 `count`로 1인지 확인한다
  3. `native/src/features/<기능>.cpp`의 패턴 상수를 바꾸고 빌드·배포한다
```

- [ ] **Step 2: 전체 테스트** — `build-native.ps1 -Test`, `dotnet test`, `Tools.Tests.ps1` 모두 PASS
- [ ] **Step 3: 커밋** — 메시지 `docs: document native build, deploy and re-signature procedure`

---

### Task 8: 즉시 완공 (분석 → 부록 A.1 → 구현) — 시간 제한 반나절

**Files(예상):**
- Create: `native/src/features/instant_build.cpp`
- Modify: `native/src/features/features.cpp`(등록), `mod/MLToybox/Scripts/features/build.lua`(트리거 호출), `mod/MLToybox/tests/build_spec.lua`, `analysis/findings.md`, 이 계획의 부록 A.1

**접근:**
- `ASMBuildingMaster::getConstructionProgress()`의 네이티브 구현을 후킹한다(`float(__fastcall*)(void* self)`).
- detour: `instantBuild` 플래그가 켜져 있으면 진행도를 결정하는 필드(들)를 완공 값으로 쓴 뒤 원본을 호출한다.
- 이 함수가 모든 건물에 대해 호출된다는 보장이 없으므로, Lua `build.tick`이 `instantBuild` 설정일 때 내 지역 미완공 건물마다 `b:getConstructionProgress()`를 호출해 **게임 스레드에서** detour를 유발한다.

- [ ] **Step 1: 분석**

```powershell
dotnet run --project E:\MLToybox\tools\re\MLToybox.Re -- exec getConstructionProgress
dotnet run --project E:\MLToybox\tools\re\MLToybox.Re -- disasm <썽크VA> 40
```
- 썽크에서 `P_FINISH` 이후의 `call <impl>`을 찾고 `disasm <impl> 60`으로 구현을 읽는다.
- 진행도 계산식(예: `movss xmm0,[rcx+OFF1]` / `divss xmm0,[rcx+OFF2]`)에서 필드 오프셋과 의미(현재 hp, 최대 hp 등)를 확정한다.
- `sig <impl>`로 패턴을 만들고, `count`가 1인지 확인한다.

- [ ] **Step 2: 부록 A.1 기록** — 썽크 VA, 구현 VA, 패턴, 필드 오프셋·타입·의미, 완공으로 만들 값(예: `hp = maxHp`)을 이 계획 부록 A.1과 `findings.md`에 적는다.

- [ ] **Step 3: Lua 트리거 테스트(RED)** — `build_spec.lua`에 추가:

```lua
  instant_build_triggers_progress_read_on_unbuilt = function()
    local _, unbuilt, built = setup()
    local calls = 0
    unbuilt.getConstructionProgress = function() calls = calls + 1; return 0.5 end
    built.getConstructionProgress = function() error("must not be called on built") end
    build.tick({}, { enabled = true, instantBuild = true })
    T.eq(calls, 1, "triggered once")
  end,
```
Run: `dotnet test` → FAIL(calls 0)

- [ ] **Step 4: Lua 구현(GREEN)** — `build.lua`의 `clearConstructionSites`를 `forEachUnbuilt(fn)`으로 일반화하고, `tick`에 `if settings.instantBuild then forEachUnbuilt(function(b) b:getConstructionProgress() end) end`를 추가한다. 테스트 PASS를 확인한다.

- [ ] **Step 5: 네이티브 구현** — `native/src/features/instant_build.cpp`(부록 A.1의 값만 사용):

```cpp
#include "features/features.h"
#include <atomic>
#include <cstdint>

namespace mlt {
namespace {
using GetProgressFn = float(__fastcall*)(void* self);
GetProgressFn g_original = nullptr;

// 부록 A.1 에서 확정한 값
constexpr const char* kPattern = /* A.1 패턴 */ "";
constexpr std::ptrdiff_t kCurrentOffset = /* A.1 */ 0;
constexpr std::ptrdiff_t kTargetOffset = /* A.1 */ 0;

float __fastcall Detour(void* self) {
    if (self) {
        auto base = static_cast<uint8_t*>(self);
        auto& current = *reinterpret_cast<float*>(base + kCurrentOffset);
        const float target = *reinterpret_cast<float*>(base + kTargetOffset);
        if (current < target) current = target;
    }
    return g_original(self);
}

bool wanted(const NativeControl& c) { return c.instantBuild; }
}

void registerInstantBuild(HookManager& m) {
    m.add({ "instant_build", kPattern, reinterpret_cast<void*>(&Detour), reinterpret_cast<void**>(&g_original), &wanted });
}
}
```
- 주석 자리(`/* A.1 */`)는 부록 A.1의 실제 값으로 바꾼다. 필드 타입·계산식이 위 가정(float 두 개)과 다르면, 부록 A.1에 적은 계산식에 맞춰 detour 본문만 바꾼다.
- `features.cpp`의 `registerFeatures`에서 `registerInstantBuild(m)`을 호출한다(선언은 `features.h`에 추가).
- 빌드: `build-native.ps1 -Test`

- [ ] **Step 6: 인게임 검증**
  1. `backup-saves.ps1`, `deploy.ps1 -Mod MLToybox`, `deploy.ps1 -Mod MLToyboxLab`을 실행한다.
  2. 게임을 재시작하고 사용자에게 테스트 세이브 로드와 건물 배치를 요청한다.
  3. 패널에서 건설 + 즉시 완공을 켠다.
  4. Expected:
     - `status.json` `native.features.instant_build.installed=true, active=true`
     - 30초 안에 배치한 건물이 `IsConstructed()=true`(Lab 확인)
     - 크래시가 없다
  - 완공되지 않으면: 진행도 필드만으로는 완공 처리가 일어나지 않는다는 뜻이다. 시간 제한 안에서 "진행도 ≥ 1을 검사하는 호출자"를 `disasm`으로 추적하고, 결과를 부록 A.1에 추가한 뒤 재시도한다.

- [ ] **Step 7: 기록·커밋** — `findings.md`에 결과를 적는다. 메시지 `feat(native): instant build via construction progress hook`

---

### Task 9: 주민 수를 넘는 징집 (분석 → 부록 A.2 → 구현) — 시간 제한 1일

**접근(분석 우선):**
- UFunction `getAllAvailableRecruits`, `getAvailableRecruits`(`ARegion`), `canAddNewMilitiaSquad`(`APawnCPP`)의 exec 썽크 → 구현을 추적한다.
- 민병대 분대를 채우는 네이티브 경로가 어떤 함수로 가용 인원을 얻는지 확인한다.
- 목표 동작: 분대 정원이 가용 인원보다 크면, 부족분을 채울 수 있게 제한을 제거한다. 가용 인원 필터(집 레벨·훈련·이미 징집됨)는 Lua가 템플릿 값으로 이미 완화했다. 남은 것은 "주민 수 자체" 제한이다.

- [ ] **Step 1: 분석** — 위 세 UFunction에 대해 `exec` → `disasm`을 실행한다. 구현 함수가 분대 채우기 경로에서 호출되는지를 호출자 역추적(`strings`/`disasm`)으로 확인한다. 후보 분기(인원 < 정원 비교)를 식별한다.
- [ ] **Step 2: 판단** — 다음 중 하나를 부록 A.2에 기록한다.
  - (a) **후킹 가능:** 대상 함수, 패턴, 시그니처, detour에서 바꿀 반환값 또는 인자를 적는다.
  - (b) **불가:** 유닛이 실제 주민 오브젝트여서 새로 만들 수 없는 경우. 이때는 정지하고 사용자에게 보고한다.
- [ ] **Step 3: 구현(a일 때)** — `native/src/features/recruits.cpp`를 Task 8 Step 5의 구조(원본 호출 후 결과 후처리)로 작성한다. `wanted`는 `c.ignorePopulation`, 등록은 `registerRecruits`. 빌드·테스트.
- [ ] **Step 4: 인게임 검증** — 주민 수보다 큰 민병대 부대를 소집해 정원이 채워지는지, 크래시가 없는지 확인한다(사용자 조작 요청).
- [ ] **Step 5: 기록·커밋** — 메시지 `feat(native): recruit beyond population via <함수명> hook`(불가 판정 시 `docs: record recruit limit analysis`)

---

### Task 10: 배치 제한 무시 (분석 → 부록 A.3 → 구현) — 시간 제한 1일

**접근(분석 우선):**
- 배치 불가 사유 문자열(`strings`로 `obstruct`, `blocked`, `too_steep`, `not_enough_space`, `cant_place`, `invalid_location` 등 후보 검색)과 `APawnCPP` 배치 관련 필드(`placeBuilding`, `resourcesInCollisionOfPlacebuilding`)를 사용하는 코드를 역추적한다.
- 배치 가능 판정(불리언 또는 사유 코드)을 결정하는 함수를 특정한다.

- [ ] **Step 1: 분석** — 사유 문자열 VA → 그 문자열을 참조하는 코드 → 함수 시작 → `disasm`. 판정 함수의 반환·출력 형식을 파악한다.
- [ ] **Step 2: 부록 A.3 기록** — 함수, 패턴, 시그니처, 강제할 결과를 적는다. 도달하지 못하면 증거를 기록하고 정지해 보고한다.
- [ ] **Step 3: 구현** — `native/src/features/placement.cpp`(원본 호출 후 결과를 "유효"로 강제), `wanted`는 `c.ignorePlacement`, 등록은 `registerPlacement`
- [ ] **Step 4: 인게임 검증** — 평소 빨간 미리보기 위치(건물 겹침, 급경사)에 배치할 수 있는지, 배치된 건물이 공사를 시작하는지, 크래시가 없는지 확인한다.
- [ ] **Step 5: 기록·커밋** — 메시지 `feat(native): ignore placement restrictions via <함수명> hook`

---

## 부록 A — 분석 결과 (실행 중 채움)

### A.1 즉시 완공 (2026-09-28 분석, buildid 24905706)
- exec 후보: `0x144A8E200`(UFunction 생성 함수), **`0x144A95C20`(exec 썽크)** → 구현 **`0x144CBC7B0`** `float ASMBuildingMaster::getConstructionProgress(this)`
- 패턴: `48 8B C4 48 89 58 08 48 89 70 10 57 48 83 EC 50 48 8B F1` (count = 1)
- 계산식: `progress = (Σ part.hp / Σ part.maxHp + materialRatio) × 0.5`
  - `materialRatio` = (요구 합 − 부족 합) / 요구 합. 요구 합이 0이면 1.0(상수 `0x14703D4C8` = 1.0 double). 요구 = `this+0x3C0`(`constructionGoods`), 보유 = `this+0x438`(`Inventory`)
  - 파츠 배열 = `this+0x2F8`: `TArray<ASMBuilding*>`(data 포인터 +0x2F8, Num int32 +0x300)
  - 파츠 필드: `float hp` = part+0x314, `float maxHp` = part+0x318 (둘 다 리플렉션 없음)
  - 상수 0.5 = `0x147052070`
- detour 동작: 원본 호출 전에, 파츠 배열의 각 파츠(널 제외)에 대해 `hp < maxHp`이면 `hp = maxHp`로 설정한다. 자재 부분은 Plan 2의 "자재 불필요"(요구 목록 비움)가 1.0으로 만든다.
- 트리거: Lua `build.tick`이 `instantBuild` 설정일 때 내 지역 미완공 건물마다 `getConstructionProgress()`를 호출한다(게임 스레드).
- 완공 처리: `IsConstructed()` = `byte [this+0x3B1]`(구현 `0x144C97830`). 진행도 1.0만으로 즉시 플래그가 서지는 않고, 인부 작업·게임 틱이 마무리한다. 인게임에서 배치 후 24초 안에 완공됐다. 인부가 배정되지 않은 건물(`not_enough_workers`)은 마무리가 더 늦었지만 결국 완공됐다.
- 인게임 결과(2026-09-28 17:33~17:52): 후킹 installed/active, 진행도 즉시 1.000, 완공 24초 이내, 크래시 없음.

### A.2 주민 수를 넘는 징집
- 대상 함수 / 패턴 / 시그니처:
- 판단(a/b)과 근거:

### A.3 배치 제한 무시
- 사유 문자열 / 참조 코드 / 판정 함수 / 패턴:
- detour 동작:
