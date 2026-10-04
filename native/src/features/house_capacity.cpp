#include "features/house_capacity.h"
#include "features/memory.h"
#include "features/upgrade_scope.h"
#include <algorithm>
#include <atomic>

namespace mlt::house_capacity {

bool ownedByMainPlayer(const uint8_t* building) {
    return mem::flagBehindPointer(building, kBuildingOwnerOffset, kPawnIsMainPlayerOffset);
}

int32_t scale(int32_t vanilla, int32_t multiplier) {
    if (multiplier <= 1 || vanilla <= 0) return vanilla;
    return vanilla * std::min(multiplier, kMaxMultiplier);
}

namespace {
using MaxFamiliesFn = int32_t(__fastcall*)(void* building);
using MaxOccupantsOfRoleFn = int32_t(__fastcall*)(void* building, uint8_t role);
MaxFamiliesFn g_maxFamilies = nullptr;
MaxOccupantsOfRoleFn g_maxOccupantsOfRole = nullptr;
void* g_ownerCheck = nullptr;
std::atomic<int32_t> g_multiplier{ 1 };

// 건물의 주인 오프셋(+0x2E0)과 폰의 isMainPlayer(+0x34C)는 업그레이드 비용 지불 함수의 본문으로 확인한다. 그 함수를 찾았을 때만 값을 바꾼다.
// 못 찾으면 게임 값 그대로다.
bool ready() { return g_ownerCheck != nullptr; }

int32_t scaled(void* building, int32_t vanilla) {
    const int32_t multiplier = g_multiplier.load(std::memory_order_relaxed);
    if (multiplier <= 1 || !ready() || !ownedByMainPlayer(static_cast<const uint8_t*>(building))) return vanilla;
    return scale(vanilla, multiplier);
}

int32_t __fastcall MaxFamiliesDetour(void* building) {
    return scaled(building, g_maxFamilies(building));
}

int32_t __fastcall MaxOccupantsOfRoleDetour(void* building, uint8_t role) {
    return scaled(building, g_maxOccupantsOfRole(building, role));
}

bool wanted(const NativeControl& c) { return c.houseCapacity > 1; }

void configure(const NativeControl& c) { g_multiplier.store(c.houseCapacity, std::memory_order_relaxed); }
}

void registerHook(HookManager& manager) {
    // 두 함수의 패턴은 함수 본문 전체라 레벨(+0xCA8)·건물 기능(+0x3BC)·확장(+0x430)을 읽는 명령이 패턴에 들어 있다
    HookSpec families{ "house_capacity", kMaxFamiliesPattern, reinterpret_cast<void*>(&MaxFamiliesDetour), reinterpret_cast<void**>(&g_maxFamilies), &wanted };
    families.configure = &configure;
    manager.add(families);

    HookSpec role{ "house_capacity_role", kMaxOccupantsOfRolePattern, reinterpret_cast<void*>(&MaxOccupantsOfRoleDetour), reinterpret_cast<void**>(&g_maxOccupantsOfRole), &wanted };
    role.bodyChecks = {
        "8B 91 A8 0C 00 00",         // mov edx,[rcx+0CA8h]        레벨
        "0F B6 81 30 04 00 00",      // movzx eax,byte [rcx+430h]  주거 공간 확장
    };
    role.bodyWindow = 0x50;
    manager.add(role);

    HookSpec owner{ "house_capacity_owner", upgrade_scope::kPayPattern, nullptr, &g_ownerCheck, &wanted };
    owner.bodyChecks = {
        "48 8B 83 E0 02 00 00",      // mov rax,[rbx+2E0h]        ownerPawn
        "80 BF 4C 03 00 00 00",      // cmp byte [rdi+34Ch],0     isMainPlayer
    };
    owner.bodyWindow = 0x160;
    manager.add(owner);
}

}
