#pragma once
#include "hooks.h"
#include <cstddef>
#include <cstdint>

// 즉시 완공(이미 공사 중인 건물, 업그레이드 포함): ASMBuildingMaster::getConstructionProgress 후킹 (Plan 3 부록 A.1, buildid 24905706).
// Lua 가 내 영지의 미완공 건물마다 이 함수를 부르면, 파츠 hp 를 채우고 게임의 완공 처리 함수를 부른다(findings "즉시 완공 — 완공 처리 함수").
namespace mlt::instant_build {
constexpr const char* kPattern = "48 8B C4 48 89 58 08 48 89 70 10 57 48 83 EC 50 48 8B F1";
constexpr std::ptrdiff_t kPartsOffset = 0x2F8;      // TArray<ASMBuilding*>::Data
constexpr std::ptrdiff_t kPartsNumOffset = 0x300;   // TArray<ASMBuilding*>::Num
constexpr std::ptrdiff_t kPartHpOffset = 0x314;     // float, 공사로 쌓인 hp
constexpr std::ptrdiff_t kPartMaxHpOffset = 0x318;  // float
constexpr int32_t kMaxParts = 4096;                 // 비정상 Num 방어

// 게임의 완공 처리 함수 void(ASMBuildingMaster*) (구현 0x144CB7890): 인부가 일을 마치면 게임이 부른다.
// Data.constructed = 1, isBeingUpgraded = 0, 건설 자재 정산, 영지 갱신. 후킹하지 않고 주소만 찾아 detour 가 부른다.
constexpr const char* kFinishPattern =
    "48 8B C4 48 89 58 10 48 89 70 18 48 89 78 20 55 41 54 41 55 41 56 41 57 48 8D A8 08 FE FF FF 48 81 EC D0 02 00 00 44 0F 29 48 98";
constexpr std::ptrdiff_t kOwnerPawnOffset = 0x2E0;         // APawnCPP* ownerPawn
constexpr std::ptrdiff_t kPawnIsMainPlayerOffset = 0x34C;  // bool APawnCPP::isMainPlayer
constexpr std::ptrdiff_t kConstructedOffset = 0x3B1;       // bool Data.constructed
constexpr std::ptrdiff_t kGoodsNumOffset = 0x3C8;          // TArray<FGood> constructionGoods(+0x3C0)::Num

// 새로 놓는 건물의 즉시 완공은 엔진의 디버그 플래그(instaBuild)로 한다(Lua 가 내가 배치하는 동안 넣는다). 플래그는 게임 전체에 하나라
// 그동안 AI 영주가 놓는 건물도 완공 상태로 생긴다. 플래그를 읽는 두 함수를 후킹해, 주인이 다른 영주인 건물에 대한 호출 동안만 플래그 목록을 비워 보인다.
//  void ASMBuildingMaster::SetupBuilding() (구현 0x144C9A720): 플래그가 있으면 Data.constructed = 1
//  void ASMBuildingMaster::convertBlueprintsToBuildings(bool) (구현 0x144CADF50): AI 의 건물 생성 함수가 SetupBuilding 바로 뒤에 부른다
// 두 함수를 부르기 전에 게임이 건물을 영지에 붙이면서 주인(영지의 주인)을 정한다.
constexpr const char* kSetupPattern = "4C 8B DC 55 49 8D AB F8 FD FF FF 48 81 EC 00 03 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 85 30 01 00 00";
constexpr const char* kConvertPattern =
    "4C 8B DC 55 49 8D AB 38 FC FF FF 48 81 EC C0 04 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 85 E0 02 00 00 48 8B 81 E8 02 00 00";
constexpr std::ptrdiff_t kBuildingEngineOffset = 0x2E8;    // ARTSMultiEngineCPP* (건물이 든 엔진)
constexpr std::ptrdiff_t kEngineFlagsNumOffset = 0x10C8;   // TArray<FName> drawDebugFlags(+0x10C0)::Num

// 파츠마다 hp 를 maxHp 로 채운다. 바꾼 파츠 수를 돌려준다.
int completeParts(uint8_t* master);
// detour 가 하는 채우기: 주인이 플레이어인 건물만 채운다(게임이 스스로 진행도를 읽을 때 다른 영주의 공사 현장이 채워지지 않게).
// ownerOffsetsVerified 가 false 면(완공 처리 함수를 찾지 못해 주인 오프셋을 확인하지 못했다) 주인을 읽지 않고 채운다.
int fillParts(uint8_t* master, bool ownerOffsetsVerified);
// 완공 처리 함수를 불러도 되는가: 아직 미완공이고, 주인이 플레이어이고, 낼 자재가 없고, 파츠가 있고 모두 hp 가 찼다.
// 게임의 인부 쪽 경로와 같은 조건이다(남은 작업 0, 자재 충족). 주인 조건은 AI 영주의 건물을 건드리지 않기 위한 것이다.
bool readyToFinish(const uint8_t* master);
// 주인이 정해져 있고 플레이어가 아닌 건물인가. 주인이 아직 없으면 false(건드리지 않는다).
bool ownedByAnotherLord(const uint8_t* master);
// 플래그 목록을 비워 보이게 한다(개수만 0 으로. 내용은 그대로). 바꿨으면 원래 개수를, 바꾸지 않았으면(비어 있었다) 음수를 돌려준다.
// restoreFlags 는 hideFlags 가 돌려준 값이 음수가 아닐 때만 되돌린다. 겹쳐 불려도 바깥 호출이 끝날 때까지 숨겨져 있다.
int32_t hideFlags(uint8_t* engine);
void restoreFlags(uint8_t* engine, int32_t saved);
void registerHook(HookManager& manager);
}
