#include "features/features.h"
#include "features/instant_build.h"

namespace mlt {
void registerFeatures(HookManager& manager) {
    instant_build::registerHook(manager);
}
}
