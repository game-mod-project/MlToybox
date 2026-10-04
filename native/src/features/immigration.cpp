#include "features/immigration.h"
#include "features/memory.h"
#include <algorithm>
#include <atomic>

namespace mlt::immigration {

bool ownedByMainPlayer(const uint8_t* region) {
    return mem::flagBehindPointer(region, kRegionOwnerOffset, kPawnIsMainPlayerOffset);
}

int32_t adjust(int32_t vanilla, int32_t monthly, int32_t multiplier, bool hasLivingSpace) {
    if (monthly > 0) return hasLivingSpace ? std::min(monthly, kMaxPerMonth) : vanilla;
    if (multiplier > 1 && vanilla > 0) return std::min(vanilla * std::min(multiplier, kMaxMultiplier), kMaxPerMonth);
    return vanilla;
}

namespace {
using MonthlyChangeFn = int32_t(__fastcall*)(void* region);
using LivingSpaceFn = const int32_t*(__fastcall*)(void* region, int32_t* out);
MonthlyChangeFn g_monthlyChange = nullptr;
LivingSpaceFn g_livingSpace = nullptr;
void* g_ownerCheck = nullptr;
std::atomic<int32_t> g_monthly{ 0 }, g_multiplier{ 1 };

// 영지의 주인 오프셋(+0x350)과 폰의 isMainPlayer(+0x34C)는 주인 확인 함수의 본문으로 확인한다. 그 함수와 주거 공간 함수를 찾았을 때만 값을 바꾼다.
// 하나라도 못 찾으면 게임 값 그대로다(Lua 가 예전 방식의 배율로 돌아간다).
bool ready() { return g_livingSpace != nullptr && g_ownerCheck != nullptr; }

// 게임이 월간 인구 변화를 0 으로 만드는 것과 같은 검사: 지어진 구획에 빈 주거 공간이 있는가
bool hasLivingSpace(void* region) {
    int32_t data[2] = { 0, 0 };   // FLivingSpaceData { constructed, unconstructed }
    const int32_t* out = g_livingSpace(region, data);
    return out && out[0] > 0;
}

int32_t __fastcall MonthlyChangeDetour(void* region) {
    const int32_t vanilla = g_monthlyChange(region);
    const int32_t monthly = g_monthly.load(std::memory_order_relaxed);
    const int32_t multiplier = g_multiplier.load(std::memory_order_relaxed);
    if (!ready() || (monthly <= 0 && multiplier <= 1) || !ownedByMainPlayer(static_cast<const uint8_t*>(region))) return vanilla;
    return adjust(vanilla, monthly, multiplier, monthly <= 0 || hasLivingSpace(region));
}

bool wanted(const NativeControl& c) { return c.immigrationMonthly > 0 || c.immigrationMultiplier > 1; }

void configure(const NativeControl& c) {
    g_monthly.store(c.immigrationMonthly, std::memory_order_relaxed);
    g_multiplier.store(c.immigrationMultiplier, std::memory_order_relaxed);
}
}

void registerHook(HookManager& manager) {
    HookSpec change{ "immigration", kMonthlyChangePattern, reinterpret_cast<void*>(&MonthlyChangeDetour), reinterpret_cast<void**>(&g_monthlyChange), &wanted };
    change.configure = &configure;
    change.bodyChecks = {
        "03 5C C8 04",               // add ebx,[rax+rcx*8+4]   증가 요인 표의 값을 더한다
    };
    change.bodyWindow = 0x1A0;
    manager.add(change);

    HookSpec space{ "immigration_space", kLivingSpacePattern, nullptr, reinterpret_cast<void**>(&g_livingSpace), &wanted };
    space.bodyChecks = {
        "80 BF B1 03 00 00 00",      // cmp byte [rdi+3B1h],0   건물의 Data.constructed
        "44 01 03",                  // add [rbx],r8d           out 의 constructed 에 더한다
    };
    space.bodyWindow = 0x100;
    manager.add(space);

    HookSpec owner{ "immigration_owner", kOwnerCheckPattern, nullptr, &g_ownerCheck, &wanted };
    owner.bodyChecks = {
        "48 8B 85 50 03 00 00 48 85 C0 74 10 80 B8 4C 03 00 00 00",   // mov rax,[rbp+350h]; test rax,rax; je; cmp byte [rax+34Ch],0   영지의 주인과 isMainPlayer
    };
    owner.bodyWindow = 0x140;
    manager.add(owner);
}

}
