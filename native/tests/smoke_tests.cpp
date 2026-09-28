#include "test.h"
#include "runtime.h"

TEST(epoch_seconds_is_plausible) {
    auto now = mlt::nowEpochSeconds();
    CHECK(now > 1'700'000'000);
}
