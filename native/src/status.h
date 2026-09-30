#pragma once
#include <string>
#include <vector>

namespace mlt {
struct FeatureState { std::string name; bool installed = false; bool active = false; std::string error; };
std::string renderStatus(long long heartbeat, long long appliedSeq, const std::vector<FeatureState>& features);
}
