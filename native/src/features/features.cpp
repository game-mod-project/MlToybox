#include "features/features.h"
#include "features/instant_build.h"
#include "features/placement.h"

namespace mlt {
void registerFeatures(HookManager& manager) {
    instant_build::registerHook(manager);
    placement::registerHook(manager);
}
}
