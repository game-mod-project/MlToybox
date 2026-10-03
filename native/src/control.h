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
};
std::optional<NativeControl> parseControl(std::string_view text);
}
