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
    src/features/placement.cpp)
target_include_directories(mlt_core PUBLIC src third_party/minhook/include)
target_link_libraries(mlt_core PUBLIC minhook)

add_library(mltoybox_native SHARED src/dllmain.cpp)
target_link_libraries(mltoybox_native PRIVATE mlt_core)

enable_testing()
add_executable(native_tests tests/main.cpp tests/smoke_tests.cpp tests/scanner_tests.cpp
    tests/json_tests.cpp tests/control_tests.cpp tests/status_tests.cpp tests/hooks_tests.cpp tests/instant_build_tests.cpp tests/probe_tests.cpp tests/placement_tests.cpp)
target_link_libraries(native_tests PRIVATE mlt_core)
add_test(NAME native_tests COMMAND native_tests)
