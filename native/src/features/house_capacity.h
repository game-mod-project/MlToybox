#pragma once
#include "hooks.h"
#include <cstddef>
#include <cstdint>

// 주거 건물의 수용 가족 수 배율을 내 건물에만 (buildid 24905706, findings "주거 건물의 수용 가족 수 — 게임의 방식").
// 게임은 집이 받을 수 있는 가족 수를 함수 하나에 고정해 두었다: 1·2레벨 1, 3레벨 2, 4레벨 3, 주거 공간 확장이 있으면 +1, 일꾼 야영지 5.
// 자연 이민의 도착, 영지의 주거 공간 숫자, 집 없는 가족의 수용, 세이브를 불러올 때의 집 배정이 모두 이 값을 읽는다.
// 그 함수를 후킹해, 주인이 플레이어인 건물의 값에만 배율을 곱한다.
namespace mlt::house_capacity {
// int32 ASMBuildingMaster::getMaxResidingFamilies() (구현 0x144CBF040). 함수 전체(61바이트)와 뒤따르는 패딩·다음 함수의 첫 바이트까지다:
// 같은 계산이 아래 함수 안에 복사돼 있어서 함수만으로는 두 곳에서 찾아진다.
constexpr const char* kMaxFamiliesPattern =
    "83 B9 A8 03 00 00 6F 75 06 B8 05 00 00 00 C3 80 B9 BC 03 00 00 01 75 22 8B 91 A8 0C 00 00 0F B6 81 30 04 00 00 83 FA 04 75 04 83 C0 03 C3 "
    "83 FA 03 75 04 83 C0 02 C3 FF C0 C3 33 C0 C3 CC CC CC 48 83 EC 28 8B 89 A8";
// int32 ASMBuildingMaster::getMaxOccupantsOfRole(EUnitRole) (구현 0x144CBEFD0). 화면이 읽는 함수이고 같은 계산이 들어 있다.
constexpr const char* kMaxOccupantsOfRolePattern = "44 0F B6 C2 84 D2 74 0C 41 83 E8 01 74 06 41 83 E8 01 75 3A 83 B9 A8 03 00 00 6F";

constexpr std::ptrdiff_t kBuildingOwnerOffset = 0x2E0;      // APawnCPP* ASMBuildingMaster::ownerPawn
constexpr std::ptrdiff_t kPawnIsMainPlayerOffset = 0x34C;   // bool APawnCPP::isMainPlayer
constexpr int32_t kMaxMultiplier = 10;

// 건물의 주인이 플레이어인가.
bool ownedByMainPlayer(const uint8_t* building);

// 내 건물의 수용 가족 수: 게임 값에 배율(1 이하면 그대로, 상한 kMaxMultiplier)을 곱한다. 주거 건물이 아니면 게임 값이 0 이라 0 그대로다.
int32_t scale(int32_t vanilla, int32_t multiplier);

void registerHook(HookManager& manager);
}
