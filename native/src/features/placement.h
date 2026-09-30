#pragma once
#include "hooks.h"
#include <cstddef>
#include <cstdint>

// 배치 제한 무시: APawnCPP 건물 배치 갱신 함수 후킹 (Plan 3 부록 A.3, buildid 24905706)
namespace mlt::placement {
constexpr const char* kPattern = "4C 8B DC 55 57 41 54 41 57 49 8D AB 88 FC FF FF";  // 0x144AC61E0
constexpr std::ptrdiff_t kInvalidFlagOffset = 0x60C;    // 숨은 bool: 1 = 배치 불가 (메모리 비교로 확인)
constexpr std::ptrdiff_t kInsideBordersOffset = 0xFE0;  // APawnCPP::isInsideBorders (리플렉션)
constexpr std::ptrdiff_t kRoadModeOffset = 0x7B0;       // APawnCPP::roadmode (리플렉션). 도로·성벽 배치도 +0x60C 를 쓴다

// 건물 배치 중이고 영지 경계 안이면 배치 불가 플래그를 지운다. 지웠으면 true.
bool allowPlacement(uint8_t* pawn);
void registerHook(HookManager& manager);
}
