#pragma once
#include "hooks.h"
#include <cstddef>
#include <cstdint>

// 업그레이드 조건·비용 무시를 플레이어의 건물에만 (buildid 24905706, findings "표를 바꾸는 기능을 플레이어에게만 — 방법 조사").
// 업그레이드 표(DT_Upgrades)는 게임 전체에 하나이고 AI 영주도 같은 판정·비용 함수로 읽는다. 표는 원래 값으로 두고,
// 주인이 플레이어인 건물에 대한 호출이 도는 동안만 그 행에서 게임 코드가 읽는 값을 0 으로 두었다가 되돌린다.
// 화면(블루프린트)만 읽는 값(requiresBuilding, requiredPerks, minimumProsperity, lockedInOutposts)은 AI 가 읽지 않으므로 Lua 가 표에서 바꾼다.
namespace mlt::upgrade_scope {
// 행을 돌려주는 게임 함수 FUpgrade*(engine, int32 id) (구현 0x144C50AD0). 후킹하지 않고 주소만 찾는다.
constexpr const char* kRowFunctionPattern = "48 89 5C 24 10 57 48 83 EC 30 44 8B 05 ?? ?? ?? ?? 48 8B D9";
// bool canUpgrade(building, int32 id, TArray<FName>* reasons) (구현 0x144CA8ED0)
constexpr const char* kCanUpgradePattern = "40 55 53 56 57 41 54 41 56 41 57 48 8D 6C 24 D9 48 81 EC F0 00 00 00 41 8B 40 0C";
// void 비용 지불(building, int32 id, bool) (구현 0x144C8F6D0). useUpgrade 가 끝에서 부른다: 지역 자산, 금고, 건설 자재 목록
constexpr const char* kPayPattern = "48 8B C4 48 89 58 20 55 48 8D 68 A1 48 81 EC A0 00 00 00";
// TArray<FGood>* GetUpgradeResourceCost(building, TArray<FGood>* out, int32 id) (구현 0x144C96510)
constexpr const char* kResourceCostPattern = "48 89 5C 24 10 48 89 74 24 18 57 48 83 EC 30 41 8B F8";
// int32 GetRegionalWealthCostForUpgrade(building, int32 id) (구현 0x144C94E40)
constexpr const char* kWealthCostPattern = "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 48 83 EC 30 8B DA";
// bool AllResidentialRequirementsSatisfied(building) (구현 0x144C82E40). 주거 요구(설정 CDO)를 읽는 곳
constexpr const char* kResidentialPattern = "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 48 83 EC 20 48 8B 81 E8 02 00 00";

constexpr std::ptrdiff_t kBuildingOwnerOffset = 0x2E0;       // APawnCPP* ASMBuildingMaster::ownerPawn
constexpr std::ptrdiff_t kBuildingEngineOffset = 0x2E8;      // ARTSMultiEngineCPP* (행 함수의 첫 인자)
constexpr std::ptrdiff_t kPawnIsMainPlayerOffset = 0x34C;    // bool APawnCPP::isMainPlayer
constexpr std::ptrdiff_t kRowCostNumOffset = 0x48;           // FUpgrade::cost(+0x40, TArray<FGood>)::Num
constexpr std::ptrdiff_t kRowRegionalWealthOffset = 0x50;    // int32 FUpgrade::regionalWealth
constexpr std::ptrdiff_t kRowTreasuryOffset = 0x54;          // int32 FUpgrade::treasury
constexpr std::ptrdiff_t kRowMinSettlementLevelOffset = 0x78;  // int32 FUpgrade::minimumSettlementLevel
constexpr std::ptrdiff_t kRowMinHouseLvOffset = 0x7D;        // uint8 FUpgrade::minimumHouseLv

// 건물의 주인이 플레이어인가.
bool ownedByMainPlayer(const uint8_t* building);

// 원본을 부르는 동안만 행에서 비용(자재 개수, 지역 자산, 금고)과 조건(정착지 레벨, 집 레벨)을 0 으로 둔다. 배열의 내용은 건드리지 않는다.
// 겹쳐 써도 된다(안쪽이 먼저 되돌리고 바깥이 원래 값을 되돌린다). row 가 없으면 아무것도 하지 않는다.
struct RowEdit {
    uint8_t* row = nullptr;
    int32_t costNum = 0, regionalWealth = 0, treasury = 0, minSettlementLevel = 0;
    uint8_t minHouseLv = 0;
};
RowEdit relaxRow(uint8_t* row);
void restoreRow(const RowEdit& edit);

void registerHook(HookManager& manager);
}
