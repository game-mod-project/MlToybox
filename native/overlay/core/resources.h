#pragma once
#include "control_doc.h"
#include "status_doc.h"
#include "units.h"
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// [자원] 탭의 범위와 표. 범위는 공통(key 없음: 모든 내 영지의 합계와 공통 목표) 또는 영지 하나(그 영지의 재고와 영지 목표).
// 패널의 ResourceScope, ResourceRows 와 같은 규칙이다.
namespace mlt::ov {

struct ResourceRow {
    std::string id;
    std::optional<double> current;   // 모드가 알려 준 현재 값. 없으면 "-"
    std::optional<int> target;       // 없으면 관리하지 않는다(영지 범위에서는 공통 목표를 따른다)

    bool operator==(const ResourceRow&) const = default;
};

// 국고·영향력은 영주 전체 값이라 영지 범위에서는 숨긴다
bool isLordWide(std::string_view id);
// "공통 (모든 내 영지, 현재=합계)"과 영지들
std::vector<ScopeOption> resourceScopeOptions(const StatusDoc* status);
// 그 범위의 현재 값. 모르면 nullptr
const std::map<std::string, double>* resourceCurrent(const StatusDoc* status, const std::optional<std::string>& key);
// 그 범위의 목표
std::map<std::string, int> resourceTargets(const ResourcesSettings& settings, const std::optional<std::string>& key);
// 그 범위의 목표를 바꾼다. 영지 목표가 하나도 없으면 그 영지 키를 지운다
void storeResourceTargets(ResourcesSettings& settings, const std::optional<std::string>& key, std::map<std::string, int> targets);
// 표의 줄(이름순): 모드가 알려 준 자원 목록과 현재 값이 있는 자원. 모드가 목록을 알려 주지 않았을 때(게임 밖)는 목표가 있는 자원.
// 목록에 없는 자원의 목표는 줄로 보이지 않을 뿐 설정에는 남는다
std::vector<ResourceRow> buildResourceRows(const StatusDoc* status, const ResourcesSettings& settings, const std::optional<std::string>& key);
}
