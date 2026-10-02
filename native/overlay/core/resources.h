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
// 패널의 ResourceScope, ResourceRows 와 같은 규칙이다. 이름·분류·정렬·검색은 오버레이에만 있다.
namespace mlt::ov {

// 자원 하나의 표시 정보. 모드가 시작할 때 bridge/catalog.json 에 쓴다(features/resources_catalog.lua)
struct ResourceInfo {
    std::string id;         // EItemType 이름. 설정의 키
    std::string name;       // 게임의 한글 이름
    std::string category;   // 건설, 식량, 제작 재료, 일용품, 군사 …
    std::string group;      // 분류 안의 묶음(고기, 채소 …). 없으면 빈 문자열
    int order = 0;          // 모드가 적은 순서. 게임의 영지 창과 같은 순서다
};

class ResourceCatalog {
public:
    // {"resources":[{"id","name","category","group"}…]}. 깨졌거나 비었으면 빈 표
    static ResourceCatalog parse(std::string_view json);
    const ResourceInfo* find(std::string_view id) const;
    bool empty() const { return byId_.empty(); }

private:
    std::map<std::string, ResourceInfo, std::less<>> byId_;
};

struct ResourceRow {
    std::string id;
    std::optional<double> current;   // 모드가 알려 준 현재 값. 없으면 "-"
    std::optional<int> target;       // 없으면 관리하지 않는다(영지 범위에서는 공통 목표를 따른다)
    std::string name;                // 화면에 보일 이름. 이름 표에 없으면 id
    std::string category;            // 이름 표에 없으면 "기타"
    std::string group;
    int order = 0;                   // 게임의 순서. 이름 표에 없는 자원은 맨 뒤

    bool operator==(const ResourceRow&) const = default;
};

// 이름 표에 없는 자원의 분류
inline constexpr const char* kOtherCategory = "기타";

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
// 표의 줄(id 순): 모드가 알려 준 자원 목록과 현재 값이 있는 자원. 모드가 목록을 알려 주지 않았을 때(게임 밖)는 목표가 있는 자원.
// 목록에 없는 자원의 목표는 줄로 보이지 않을 뿐 설정에는 남는다. catalog 가 있으면 이름과 분류를 채운다
std::vector<ResourceRow> buildResourceRows(const StatusDoc* status, const ResourcesSettings& settings, const std::optional<std::string>& key,
                                           const ResourceCatalog* catalog = nullptr);

// "식량 · 고기". 묶음이 없으면 "식량"
std::string resourceClassLabel(const ResourceRow& row);
// 줄들에 있는 분류. 게임의 순서
std::vector<std::string> resourceCategories(const std::vector<ResourceRow>& rows);

// 공통 목표 대신 영지 목표를 따르는 영지 하나
struct RegionOverride {
    std::string key;
    std::string name;   // 영지 이름. 모르면(게임 밖) 키
    int target = 0;

    bool operator==(const RegionOverride&) const = default;
};
// 자원마다, 영지 목표가 따로 있는 영지들. 영지 목표는 공통 목표보다 우선하므로 그 영지에서는 공통 목표가 쓰이지 않는다.
// 지금 게임에 있는 영지만 본다(status.regions 의 순서). 게임 밖이면 설정에 있는 영지를 모두 본다
std::map<std::string, std::vector<RegionOverride>> regionOverrides(const StatusDoc* status, const ResourcesSettings& settings);
// 한 줄에 영지 하나: "Altbruch (sel): 1000"
std::string regionOverrideText(const std::vector<RegionOverride>& overrides);
// 보이는 줄(shown)의 목표를 지운 것. filtered 가 거짓이면(추리지 않았으면) 모두 지운다(표에 보이지 않는 옛 자원의 목표 포함)
std::map<std::string, int> clearedTargets(std::map<std::string, int> targets, const std::vector<ResourceRow>& shown, bool filtered);

// 표에 보일 줄을 고르는 조건
struct ResourceFilter {
    std::string category;   // 비어 있으면 모든 분류
    std::string search;     // 이름·id·분류·묶음에 들어 있는 글. 앞뒤 공백은 빼고, 영문 대소문자는 가리지 않는다. 비어 있으면 전부
};
std::vector<ResourceRow> filterResourceRows(const std::vector<ResourceRow>& rows, const ResourceFilter& filter);

// 표의 열. 번호는 화면의 열 순서와 같다
enum class ResourceColumn { Name = 0, Category = 1, Current = 2, Target = 3 };
// 값이 없는 줄(현재 "-", 목표 빈칸)은 오름차순에서도 내림차순에서도 뒤에 둔다. 같은 값끼리는 게임의 순서
void sortResourceRows(std::vector<ResourceRow>& rows, ResourceColumn column, bool descending);

// 헤더를 눌러 정렬한 순서를 기억한다. 현재 값이나 목표가 바뀔 때마다 줄이 자리를 옮기면 누르려던 칸이 달아난다.
// 그래서 정렬 기준·방향, 보는 조건(context), 줄의 구성이 바뀔 때만 다시 정렬한다
class ResourceOrder {
public:
    void arrange(std::vector<ResourceRow>& rows, ResourceColumn column, bool descending, const std::string& context);

private:
    bool has_ = false;
    ResourceColumn column_ = ResourceColumn::Category;
    bool descending_ = false;
    std::string context_;
    std::vector<std::string> ids_;
};

// 열 너비를 표 너비에 대한 백분율(소수 한 자리)로 바꾼다. 아직 그려지지 않은 표(0 이하의 너비)면 빈 목록
std::vector<float> columnShares(const std::vector<float>& widths);
// 사용자가 열 너비를 바꿨는가(창 크기를 바꿀 때의 반올림 차이는 바뀐 것으로 보지 않는다)
bool columnSharesDiffer(const std::vector<float>& saved, const std::vector<float>& now);
}
