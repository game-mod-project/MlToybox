#include "features/instant_build.h"
#include <cstring>

namespace mlt::instant_build {

int completeParts(uint8_t* master) {
    if (!master) return 0;
    std::uintptr_t dataAddress = 0;
    int32_t num = 0;
    std::memcpy(&dataAddress, master + kPartsOffset, sizeof dataAddress);
    std::memcpy(&num, master + kPartsNumOffset, sizeof num);
    if (dataAddress == 0 || num <= 0 || num > kMaxParts) return 0;
    auto parts = reinterpret_cast<uint8_t* const*>(dataAddress);
    int changed = 0;
    for (int32_t i = 0; i < num; ++i) {
        uint8_t* part = parts[i];
        if (!part) continue;
        float hp, maxHp;
        std::memcpy(&hp, part + kPartHpOffset, sizeof hp);
        std::memcpy(&maxHp, part + kPartMaxHpOffset, sizeof maxHp);
        if (hp < maxHp) {
            std::memcpy(part + kPartHpOffset, &maxHp, sizeof maxHp);
            ++changed;
        }
    }
    return changed;
}

namespace {
using GetProgressFn = float(__fastcall*)(void* self);
GetProgressFn g_original = nullptr;

// 게임 스레드에서 호출된다. 예외를 던지지 않고 항상 원본을 호출한다.
float __fastcall Detour(void* self) {
    completeParts(static_cast<uint8_t*>(self));
    return g_original(self);
}

bool wanted(const NativeControl& c) { return c.instantBuild; }
}

void registerHook(HookManager& manager) {
    HookSpec spec{ "instant_build", kPattern, reinterpret_cast<void*>(&Detour), reinterpret_cast<void**>(&g_original), &wanted };
    spec.bodyChecks = {
        "48 8D 96 C0 03 00 00",      // lea rdx,[rsi+3C0h]    constructionGoods
        "48 8B 8E F8 02 00 00",      // mov rcx,[rsi+2F8h]    파츠 배열 Data
        "F3 0F 58 80 14 03 00 00",   // addss xmm0,[rax+314h] 파츠 hp
        "F3 0F 58 88 18 03 00 00",   // addss xmm1,[rax+318h] 파츠 maxHp
    };
    spec.bodyWindow = 0x160;
    manager.add(spec);
}

}
