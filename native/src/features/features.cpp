#include "features/features.h"
#include "features/instant_build.h"
#include "features/placement.h"
#include "features/militia_guard.h"
#include "features/upgrade_scope.h"
#include "features/immigration.h"
#include "features/house_capacity.h"
#include "features/region_scope.h"
#include "features/mood.h"
#include "features/deposits.h"

namespace mlt {
void registerFeatures(HookManager& manager) {
    instant_build::registerHook(manager);
    placement::registerHook(manager);
    militia_guard::registerHook(manager);
    upgrade_scope::registerHook(manager);
    immigration::registerHook(manager);
    house_capacity::registerHook(manager);
    region_scope::registerHook(manager);
    mood::registerHook(manager);
    deposits::registerHook(manager);
}
}
