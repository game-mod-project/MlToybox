#pragma once
#include "control_doc.h"
#include "status_doc.h"
#include "units.h"
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// 커스텀 용병단 정의의 화면 쪽 검증과 선택지. 패널의 MercCompanyRules 와 같은 규칙이고,
// 모드도 같은 규칙으로 다시 검증한다(mod/MLToybox/Scripts/features/merc_plan.lua).
// 이름 길이는 모드처럼 글자(코드 포인트) 수로 센다.
namespace mlt::ov {

inline constexpr int kMercMaxSquads = 10;
inline constexpr int kMercNameMax = 40;
inline constexpr int kMercMaxEnabled = 3;
inline constexpr const char* kFirstRegionLabel = "내 첫 영지";
inline constexpr const char* kInheritBannerLabel = "칸의 것 그대로";

// 용병 표(DT_MercenaryCompanies)의 순정 용병단 이름 11개, 이름순. 깃발 선택지이기도 하다
const std::vector<std::string>& vanillaMercNames();
// 대소문자를 가리지 않는다
bool isVanillaMercName(std::string_view name);
// 깃발 입력을 표의 이름으로 맞춘다. 모르는 이름이나 빈 값은 없음(칸의 깃발 그대로)
std::optional<std::string> normalizeBanner(const std::optional<std::string>& banner);
// 깃발 선택지: "칸의 것 그대로" + 순정 용병단 11개
std::vector<ScopeOption> bannerOptions();
// 도착 영지 선택지: "내 첫 영지", 내 영지들, 그리고 keep 의 키 가운데 영지 목록에 없는 것.
// 게임 밖이거나 그 영지를 잃어 목록에 없는 키도 남겨야, 용병단을 고쳐 등록할 때 저장된 도착 영지가 사라지지 않는다
std::vector<ScopeOption> mercRegionOptions(const std::vector<RegionInfo>& regions, const std::vector<std::optional<std::string>>& keep);

// 문제가 없으면 값 없음, 있으면 사용자에게 보여 줄 이유. self 는 all 안에서 c 자신의 자리(새 용병단이면 -1)
std::optional<std::string> validateCompany(const MercCompany& c, const std::vector<MercCompany>& all, int self);
// "용병 - 보병 × 2, 용병 - 석궁병 × 1" (처음 나온 순서). 모르는 병종 id 는 그대로 보여 준다
std::string unitSummary(const std::vector<std::string>& units);
// self 를 "사용"으로 바꿔도 되는가(self 를 뺀 사용 수가 3 미만). 새 용병단이면 self = -1
bool canEnableCompany(const std::vector<MercCompany>& all, int self);

struct MercRegistration {
    bool ok = false;
    int index = -1;         // 등록된 자리
    std::string message;    // 사용자에게 보여 줄 결과(실패하면 이유)
};
// 편집한 내용(draft)을 등록한다. editing 이 -1 이면 새로 넣고, 아니면 그 자리를 고친다.
// 이름은 앞뒤 공백을 떼고, 깃발은 표의 이름으로 맞춘다. 새 용병단은 사용 중인 것이 3개 미만일 때만 켠 채로 넣고,
// 고칠 때는 "사용"을 그대로 둔다(draft.enabled 는 보지 않는다). 검증에 실패하면 all 을 바꾸지 않는다
MercRegistration registerCompany(std::vector<MercCompany>& all, int editing, MercCompany draft);
// "사용" 체크를 바꾼다. 켤 수 없으면(이미 3개가 사용 중) 바꾸지 않고 이유를 돌려준다
std::optional<std::string> setCompanyEnabled(std::vector<MercCompany>& all, int index, bool enabled);
// 편집 영역에 실었던 용병단(loaded, 그때의 자리 index)이 지금 목록의 어디에 있는가.
// 목록은 패널이나 손 편집으로 밖에서 바뀔 수 있다. 그 자리에 같은 내용이 있으면 그 자리, 다른 자리로 옮겨졌으면 그 자리,
// 없어졌거나 내용이 바뀌었으면 -1. index 가 -1(새 용병단)이면 -1
int locateCompany(const std::vector<MercCompany>& all, int index, const MercCompany& loaded);
}
