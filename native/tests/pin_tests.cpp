#include "test.h"
#include "runtime.h"

TEST(pin_module_containing_address_succeeds_for_own_code) {
    CHECK(mlt::pinModuleContaining(reinterpret_cast<const void*>(&mlt::nowEpochSeconds)));
}

TEST(pin_module_rejects_null) {
    CHECK(!mlt::pinModuleContaining(nullptr));
}
