#pragma once
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// 분대로 만들 수 있는 병종(패널의 UnitCatalog 와 같은 13종)과, 여러 탭이 쓰는 선택지 한 줄
namespace mlt::ov {

struct UnitOption {
    std::string id;      // DT_UnitTemplates 의 행 이름
    std::string label;
};

const std::vector<UnitOption>& units();
// 병종의 한글 이름. 모르는 id 는 id 를 그대로 돌려준다
std::string unitLabel(std::string_view id);
bool isKnownUnit(std::string_view id);

// 선택지 한 줄. key 가 없으면 "공통", "내 첫 영지"처럼 특정 대상이 아닌 줄이다
struct ScopeOption {
    std::optional<std::string> key;
    std::string label;
};

// "Mandlach (gold)"
std::string regionLabel(const std::string& name, const std::string& key);
// 범위 선택지: 첫 줄은 공통(특정 영지가 아님), 이어서 영지마다 한 줄. regions 는 key 와 name 을 가진 것들의 목록이고, 없으면 공통 줄만
inline constexpr const char* kCommonScopeLabel = "공통 (모든 내 영지)";
template <class Regions>
std::vector<ScopeOption> commonAndRegionOptions(const Regions* regions, const char* commonLabel = kCommonScopeLabel) {
    std::vector<ScopeOption> options = { { std::nullopt, commonLabel } };
    if (regions) {
        for (const auto& r : *regions) options.push_back({ r.key, regionLabel(r.name, r.key) });
    }
    return options;
}
// key 가 있는 줄의 번호. 없으면 0(첫 줄)
int indexOfKey(const std::vector<ScopeOption>& options, const std::optional<std::string>& key);
}
