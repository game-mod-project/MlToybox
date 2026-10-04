#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace mlt {
struct FeatureState { std::string name; bool installed = false; bool active = false; std::string error; };
// extra: 기능이 덧붙이는 JSON 조각("이름":값 꼴. 예: 광물 매장지의 지금 값). 비어 있으면 넣지 않는다
std::string renderStatus(long long heartbeat, long long appliedSeq, const std::vector<FeatureState>& features, std::string_view extra = {});
}
