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
}
