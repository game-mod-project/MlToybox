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
