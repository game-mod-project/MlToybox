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
    int immigrationMonthly = 0;    // 월 자연 이민 가족 수(features.population.monthlyFamilies). 0 = 게임 그대로
    int immigrationMultiplier = 1; // 이민 속도 배율(features.population.multiplier). 1 = 게임 그대로
};
std::optional<NativeControl> parseControl(std::string_view text);
}
