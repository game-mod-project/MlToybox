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

// 파츠마다 hp 를 maxHp 로 채운다. 바꾼 파츠 수를 돌려준다.
int completeParts(uint8_t* master);
// 완공 처리 함수를 불러도 되는가: 아직 미완공이고, 주인이 플레이어이고, 낼 자재가 없고, 파츠가 있고 모두 hp 가 찼다.
// 게임의 인부 쪽 경로와 같은 조건이다(남은 작업 0, 자재 충족). 주인 조건은 AI 영주의 건물을 건드리지 않기 위한 것이다.
bool readyToFinish(const uint8_t* master);
void registerHook(HookManager& manager);
}
