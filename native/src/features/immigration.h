#pragma once
#include "hooks.h"
#include <cstddef>
#include <cstdint>

// 자연 이민의 가족 수와 속도를 내 영지에서만 (buildid 24905706, findings "자연 이민 — 게임의 방식").
// 게임은 영지마다 월간 인구 변화 n 을 구해, 그 달 1일부터 (그 달의 일수 / n) 간격의 날에 가족 하나씩을 들인다(n < 0 이면 내보낸다).
// n 을 돌려주는 함수를 후킹해, 주인이 플레이어인 영지의 값만 바꾼다. 영지 창의 숫자도 같은 함수로 읽으므로 따라 바뀐다.
namespace mlt::immigration {
// int32 ARegion::GetMonthlyPopChange() (구현 0x144BCAFC0)
constexpr const char* kMonthlyChangePattern =
    "48 89 5C 24 10 48 89 74 24 18 48 89 7C 24 20 55 48 8D 6C 24 A9 48 81 EC F0 00 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 45 47 48 8D 55 F7 33 DB";
// FLivingSpaceData* ARegion::getAvailableLivingSpace(FLivingSpaceData* out) (구현 0x144BE7EC0). 후킹하지 않고 주소만 찾는다.
// out 의 첫 int32 가 지어진 구획의 빈 주거 공간이다. 게임은 이것이 0 이하이면 월간 인구 변화를 0 으로 한다.
constexpr const char* kLivingSpacePattern =
    "40 53 48 83 EC 40 33 C0 48 89 74 24 58 48 8B B1 58 06 00 00 48 8B DA 4C 89 74 24 38 48 89 02 48 63 81 60 06 00 00";
// 영지의 주인과 그 주인이 플레이어인지를 읽는 영지 함수(0x144BC2ED0). 두 오프셋을 확인하려고 주소만 찾는다.
constexpr const char* kOwnerCheckPattern =
    "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 48 89 7C 24 20 41 54 41 56 41 57 48 83 EC 20 4C 8D A1 68 0D 00 00";

constexpr std::ptrdiff_t kRegionOwnerOffset = 0x350;        // APawnCPP* ARegion::ownerPawn
constexpr std::ptrdiff_t kPawnIsMainPlayerOffset = 0x34C;   // bool APawnCPP::isMainPlayer
constexpr int32_t kMaxPerMonth = 31;      // 게임은 하루에 한 가족까지만 들인다
constexpr int32_t kMaxMultiplier = 10;

// 영지의 주인이 플레이어인가.
bool ownedByMainPlayer(const uint8_t* region);

// 내 영지의 월간 인구 변화. vanilla 는 게임이 구한 값이다.
//  monthly > 0: 지지율과 무관하게 매달 그만큼(상한 kMaxPerMonth). 빈 주거 공간이 없으면 게임 값을 그대로 둔다.
//  monthly 가 없고 multiplier > 1: 오는 가족(vanilla > 0)에만 곱한다(배율 상한 kMaxMultiplier, 결과 상한 kMaxPerMonth).
//  둘 다 없으면 게임 값.
int32_t adjust(int32_t vanilla, int32_t monthly, int32_t multiplier, bool hasLivingSpace);

void registerHook(HookManager& manager);
}
