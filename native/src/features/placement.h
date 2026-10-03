#pragma once
#include "hooks.h"
#include <cstddef>
#include <cstdint>

// APawnCPP 건물 배치 갱신 함수 후킹 (Plan 3 부록 A.3, buildid 24905706). 이 함수가 배치 가능 여부를 정한다. AI 폰이면 바로 돌아간다.
//  배치 제한 무시: 원본이 쓴 판정(+0x60C)을 지운다.
//  지역당 개수 제한 해제, 자재 불필요: 표를 바꾸면 AI 영주도 읽으므로, 플레이어의 이 호출 동안만 놓으려는 건물의 행을 고쳤다가 되돌린다
//    (findings "표를 바꾸는 기능을 플레이어에게만 — 방법 조사").
namespace mlt::placement {
constexpr const char* kPattern = "4C 8B DC 55 57 41 54 41 57 49 8D AB 88 FC FF FF";  // 0x144AC61E0
constexpr std::ptrdiff_t kInvalidFlagOffset = 0x60C;    // 숨은 bool: 1 = 배치 불가 (메모리 비교로 확인)
constexpr std::ptrdiff_t kInsideBordersOffset = 0xFE0;  // APawnCPP::isInsideBorders (리플렉션)
constexpr std::ptrdiff_t kRoadModeOffset = 0x7B0;       // APawnCPP::roadmode (리플렉션). 도로·성벽 배치도 +0x60C 를 쓴다
constexpr std::ptrdiff_t kPawnIsMainPlayerOffset = 0x34C;  // bool APawnCPP::isMainPlayer
constexpr std::ptrdiff_t kPawnIsAiOffset = 0x34D;          // bool APawnCPP::isAI
constexpr std::ptrdiff_t kPlaceBuildingOffset = 0x608;     // int32 APawnCPP::placeBuilding (놓으려는 건물 종류, 0 = 배치 중 아님)

// 건물 표(buildingStats)의 행을 돌려주는 게임 함수 FStat*(int32 종류) (구현 0x144C48480). 후킹하지 않고 주소만 찾는다.
// 배치 갱신 함수가 폰의 placeBuilding 으로 이 함수를 불러 얻은 행을 읽는다. 없는 종류면 정적 기본 행을 돌려준다.
constexpr const char* kRowFunctionPattern =
    "40 53 48 83 EC 20 8B 15 ?? ?? ?? ?? 65 48 8B 04 25 58 00 00 00 8B D9 B9 64 11 00 00 48 8B 04 D0 8B 04 01 39 05 ?? ?? ?? ?? 0F 8F ?? ?? ?? ??";
constexpr std::ptrdiff_t kRowGoodsNumOffset = 0x298;       // FStat::constructionGoods(+0x290, TArray<FGood>)::Num
constexpr std::ptrdiff_t kRowMaxInRegionOffset = 0x2D8;    // int32 FStat::maxInRegion (0 = 제한 없음)

// 건물 배치 중이고 영지 경계 안이면 배치 불가 플래그를 지운다. 지웠으면 true.
bool allowPlacement(uint8_t* pawn);

// 플레이어가 건물을 배치하는 중인가(행을 고쳐도 되는 호출인가).
bool isPlayerPlacing(const uint8_t* pawn);

// 원본을 부르는 동안만 행에서 개수 제한을 0 으로, 건설 자재의 개수를 0 으로 둔다(배열의 내용은 건드리지 않는다).
// restoreRow 가 고친 값만 되돌린다. row 가 없으면 아무것도 하지 않는다.
struct RowEdit { uint8_t* row = nullptr; bool limit = false; bool goods = false; int32_t maxInRegion = 0; int32_t goodsNum = 0; };
RowEdit relaxRow(uint8_t* row, bool noRegionLimit, bool noMaterials);
void restoreRow(const RowEdit& edit);

void registerHook(HookManager& manager);
}
