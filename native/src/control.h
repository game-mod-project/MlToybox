#pragma once
#include <optional>
#include <string_view>

namespace mlt {
struct NativeControl {
    long long seq = -1;
    bool instantBuild = false;
    bool ignorePlacement = false;
    bool ignorePopulation = false;
    bool noRegionLimit = false;
    bool noMaterials = false;
    bool upgradeFree = false;      // 업그레이드 조건·비용 무시(features.upgrade.enabled)
};
std::optional<NativeControl> parseControl(std::string_view text);
}
