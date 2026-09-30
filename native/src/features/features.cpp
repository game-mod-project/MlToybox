#include "features/features.h"
#include "features/instant_build.h"
#include "features/placement.h"
#include "features/militia_guard.h"

namespace mlt {
void registerFeatures(HookManager& manager) {
    instant_build::registerHook(manager);
    placement::registerHook(manager);
    militia_guard::registerHook(manager);
}
}
