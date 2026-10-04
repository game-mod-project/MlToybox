#pragma once
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mlt {
// 자격·공공질서 가운데 한 값의 설정(features.mood). 모두 기본값이면 게임 그대로다
struct MoodStat {
    int fixed = 0;      // 고정값(1~100). 0 = 고정하지 않는다
    int good = 1;       // 오르는 요인 배율(1~10)
    int bad = 100;      // 깎이는 요인 비율(0~100 %)
    bool neutral() const { return fixed <= 0 && good <= 1 && bad >= 100; }
    bool operator==(const MoodStat&) const = default;
};
struct MoodSet {
    MoodStat approval, order;
    bool neutral() const { return approval.neutral() && order.neutral(); }
    bool operator==(const MoodSet&) const = default;
};
struct MoodControl {
    MoodSet common;                                          // 내 영지 전체
    std::vector<std::pair<std::string, MoodSet>> regions;    // 영지 키(regionUniqueTag) → 그 영지만의 설정. 있으면 공통 대신 쓴다
    bool neutral() const {
        if (!common.neutral()) return false;
        for (const auto& r : regions) if (!r.second.neutral()) return false;
        return true;
    }
    bool operator==(const MoodControl&) const = default;
};

// 광물 매장지(소금·철·점토) 하나의 최소 매장량(features.region 의 targets). 0 = 채우지 않는다.
// 영지 설정에서 -1 은 "그 종류는 공통 목표를 따른다"는 뜻이다
struct MineralTargets {
    int salt = 0;
    int iron = 0;
    int clay = 0;
    bool operator==(const MineralTargets&) const = default;
};
struct RegionControl {
    bool enabled = false;
    bool rich = false;                                              // 내 영지의 소금·철·점토·돌 매장지를 풍부하게(features.region.richDeposits)
    MineralTargets common;                                          // 내 영지 전체
    std::vector<std::pair<std::string, MineralTargets>> regions;    // 영지 키(regionUniqueTag) → 그 영지의 목표
    bool operator==(const RegionControl&) const = default;
};

struct NativeControl {
    long long seq = -1;
    bool instantBuild = false;
    bool ignorePlacement = false;
    bool noRegionLimit = false;
    bool noMaterials = false;
    bool upgradeFree = false;      // 업그레이드 조건·비용 무시(features.upgrade.enabled)
    int immigrationMonthly = 0;    // 월 자연 이민 가족 수(features.population.monthlyFamilies). 0 = 게임 그대로
    int immigrationMultiplier = 1; // 이민 속도 배율(features.population.multiplier). 1 = 게임 그대로
    int houseCapacity = 1;         // 집의 수용 가족 수 배율(features.population.houseCapacity). 1 = 게임 그대로
    MoodControl mood;              // 자격·공공질서(features.mood). 꺼져 있으면 비어 있다
    RegionControl region;          // 영지 기능 가운데 광물 매장량(features.region). 꺼져 있으면 비어 있다
};
std::optional<NativeControl> parseControl(std::string_view text);
}
