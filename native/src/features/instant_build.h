#pragma once
#include "hooks.h"
#include <cstddef>
#include <cstdint>

// 즉시 완공: ASMBuildingMaster::getConstructionProgress 후킹 (Plan 3 부록 A.1, buildid 24905706)
namespace mlt::instant_build {
constexpr const char* kPattern = "48 8B C4 48 89 58 08 48 89 70 10 57 48 83 EC 50 48 8B F1";
constexpr std::ptrdiff_t kPartsOffset = 0x2F8;      // TArray<ASMBuilding*>::Data
constexpr std::ptrdiff_t kPartsNumOffset = 0x300;   // TArray<ASMBuilding*>::Num
constexpr std::ptrdiff_t kPartHpOffset = 0x314;     // float, 공사로 쌓인 hp
constexpr std::ptrdiff_t kPartMaxHpOffset = 0x318;  // float
constexpr int32_t kMaxParts = 4096;                 // 비정상 Num 방어

// 파츠마다 hp 를 maxHp 로 채운다. 바꾼 파츠 수를 돌려준다.
int completeParts(uint8_t* master);
void registerHook(HookManager& manager);
}
