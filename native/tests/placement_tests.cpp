#include "test.h"
#include "features/placement.h"

using namespace mlt;

namespace { struct FakePawn { alignas(8) uint8_t bytes[0x1100]{}; }; }

TEST(placement_clears_invalid_flag_inside_borders) {
    FakePawn p;
    p.bytes[placement::kInsideBordersOffset] = 1;
    p.bytes[placement::kInvalidFlagOffset] = 1;
    CHECK(placement::allowPlacement(p.bytes) == true);
    CHECK(p.bytes[placement::kInvalidFlagOffset] == 0);
}

TEST(placement_keeps_flag_outside_borders) {
    FakePawn p;
    p.bytes[placement::kInsideBordersOffset] = 0;
    p.bytes[placement::kInvalidFlagOffset] = 1;
    CHECK(placement::allowPlacement(p.bytes) == false);
    CHECK(p.bytes[placement::kInvalidFlagOffset] == 1);
}

TEST(placement_ignores_null) {
    CHECK(placement::allowPlacement(nullptr) == false);
}

TEST(placement_keeps_flag_in_road_or_wall_mode) {
    // 실측 크래시(2026-09-30, reading 0x738): 도로/성벽 배치 함수는 같은 플래그(+0x60C)로 "지점이 어느 영지에도 없음"을 표시하고
    // 그 뒤 처리를 건너뛴다. 지우면 null 영지를 읽다가 튕긴다. 도로 모드에서는 건드리지 않는다
    FakePawn p;
    p.bytes[placement::kInsideBordersOffset] = 1;
    p.bytes[placement::kRoadModeOffset] = 1;
    p.bytes[placement::kInvalidFlagOffset] = 1;
    CHECK(placement::allowPlacement(p.bytes) == false);
    CHECK(p.bytes[placement::kInvalidFlagOffset] == 1);
}
