#pragma once
#include "control_doc.h"
#include "status_doc.h"
#include "units.h"
#include <optional>
#include <string>
#include <vector>

// [영지] 탭: 가축 상인 대기와 매장량 표. 줄은 매장지의 종류, 칸은 지금 값(풍부한 매장지는 표시)과 최소 유지 목표다.
// 목표는 매장지 하나의 양이다. 덩어리형(돌·물고기·장어·열매·버섯)은 모드의 Lua 가(features/region.lua),
// 광물(소금·철·점토)은 네이티브 DLL 이 날짜가 넘어갈 때 채운다. ImGui 에 의존하지 않아 단위 테스트한다.
namespace mlt::ov {

struct DepositKind {
    const char* key;     // 설정과 상태의 종류 이름
    const char* label;
    bool mineral;        // 네이티브가 맡는 광물인가
};
// 표의 줄 순서
const std::vector<DepositKind>& depositKinds();
// 넣을 수 있는 가장 큰 목표(모드의 features/region.lua 의 MAX_TARGET, 네이티브의 kMaxTarget 과 같다)
inline constexpr int kDepositTargetMax = 1000000;

struct DepositRow {
    std::string key;
    std::string label;
    bool mineral = false;
    std::string current;            // 지금 값. 공통: "547 (3곳)" / "547 (3곳, 풍부 1)", 영지: "1,154 풍부" / "629 / 640" / "8, 500", 없으면 "-"
    std::optional<int> target;      // 이 범위(공통 또는 고른 영지)에 넣은 목표
    std::optional<int> inherited;   // 영지를 골랐을 때 그 종류의 공통 목표(영지 칸이 비어 있으면 이 값을 따른다)
};

// 범위: "공통 (모든 내 영지)"과 영지들
std::vector<ScopeOption> regionScopeOptions(const RegionStatus* status);
// 표의 줄. status 가 없으면(게임 밖) 지금 값은 "-"
std::vector<DepositRow> buildDepositRows(const RegionSettings& settings, const RegionStatus* status, const std::optional<std::string>& scope);
// 칸 하나를 고친다. 값이 없으면 지운다(공통: 채우지 않는다, 영지: 공통을 따른다). 비게 된 영지는 설정에서 뺀다.
// 0 은 공통에서는 "채우지 않는다"(지운다), 영지에서는 "이 영지는 채우지 않는다"(0 으로 남긴다)
void setDepositTarget(RegionSettings& settings, const std::optional<std::string>& scope, const std::string& kind, std::optional<int> value);
// "가축 상인: Wilde Wand 방문까지 7일 · Krumme Leite 지금 주문 가능"
std::string livestockLine(const RegionStatus* status);
// 네이티브 DLL 이 광물이나 풍부 표시를 맡지 못할 때의 안내. 다 맡았거나 아직 모르면 빈 글
std::string depositNativeNote(const NativeStatus* native);
}
