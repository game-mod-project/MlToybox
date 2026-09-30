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
