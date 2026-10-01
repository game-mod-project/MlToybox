cmake_minimum_required(VERSION 3.20)
set(CMAKE_POLICY_DEFAULT_CMP0091 NEW)
project(mltoybox_native LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
add_compile_definitions(UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
add_compile_options(/utf-8)

add_subdirectory(third_party/minhook EXCLUDE_FROM_ALL)

add_library(mlt_core STATIC
    src/runtime.cpp
    src/scanner.cpp
    src/json.cpp
    src/control.cpp
    src/status.cpp
    src/hooks.cpp
    src/features/features.cpp
    src/features/instant_build.cpp
    src/probe.cpp
    src/features/placement.cpp
    src/features/militia_guard.cpp)
target_include_directories(mlt_core PUBLIC src third_party/minhook/include)
target_link_libraries(mlt_core PUBLIC minhook)

add_library(mltoybox_native SHARED src/dllmain.cpp)
target_link_libraries(mltoybox_native PRIVATE mlt_core)

# 게임 안 오버레이 (docs/superpowers/specs/2026-10-01-ingame-overlay-design.md)
# overlay_core: 문서 모델·브리지·규칙. Windows 그래픽에 의존하지 않아 단위 테스트한다
add_library(overlay_core STATIC
    overlay/core/text.cpp
    overlay/core/number_input.cpp
    overlay/core/control_doc.cpp
    overlay/core/status_doc.cpp
    overlay/core/bridge.cpp
    overlay/core/commands.cpp
    overlay/core/settings.cpp
    overlay/core/status_file.cpp
    overlay/core/session.cpp)
target_include_directories(overlay_core PUBLIC . third_party)
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
# ImGui 의 기본 IME 처리는 프레임을 그리는 스레드에서 창에 IME 함수를 부른다. 우리는 그 스레드가 창을 가진 스레드가 아니므로 끈다
target_compile_definitions(imgui PUBLIC IMGUI_DISABLE_WIN32_DEFAULT_IME_FUNCTIONS)

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
    overlay/ui/widgets.cpp
    overlay/ui/tab_build.cpp
    overlay/ui/tab_lord.cpp
    overlay/ui/tab_status.cpp)
target_link_libraries(mltoybox_overlay PRIVATE overlay_core imgui minhook d3d12 dxgi user32)

enable_testing()
add_executable(native_tests tests/main.cpp tests/smoke_tests.cpp tests/scanner_tests.cpp
    tests/json_tests.cpp tests/control_tests.cpp tests/status_tests.cpp tests/hooks_tests.cpp tests/instant_build_tests.cpp tests/probe_tests.cpp tests/placement_tests.cpp tests/pin_tests.cpp tests/militia_guard_tests.cpp
    tests/overlay_text_tests.cpp
    tests/overlay_control_doc_tests.cpp
    tests/overlay_status_doc_tests.cpp
    tests/overlay_bridge_tests.cpp
    tests/overlay_settings_tests.cpp
    tests/overlay_session_tests.cpp
    # 창 프로시저 후킹은 실제 창으로 시험한다(오버레이 DLL 의 소스 가운데 그래픽 장치가 필요 없는 것을 함께 컴파일한다)
    tests/overlay_input_tests.cpp overlay/render/input.cpp overlay/render/guard.cpp overlay/ui/app.cpp)
target_link_libraries(native_tests PRIVATE mlt_core overlay_core imgui)
# 테스트가 읽는 견본 파일의 위치
target_compile_definitions(native_tests PRIVATE
    MLT_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures"
    MLT_LUA_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../mod/MLToybox/tests/fixtures")
add_test(NAME native_tests COMMAND native_tests)
