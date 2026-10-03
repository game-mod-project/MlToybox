#include "features/instant_build.h"
#include <cstring>

namespace mlt::instant_build {

namespace {
template <class T> T read(const uint8_t* base, std::ptrdiff_t off) { T v; std::memcpy(&v, base + off, sizeof v); return v; }
}

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
bool ownedByMainPlayer(const uint8_t* master) {
    const auto owner = reinterpret_cast<const uint8_t*>(read<std::uintptr_t>(master, kOwnerPawnOffset));
    return owner && read<uint8_t>(owner, kPawnIsMainPlayerOffset) != 0;
}
}

int fillParts(uint8_t* master, bool ownerOffsetsVerified) {
    if (!master) return 0;
    if (ownerOffsetsVerified && !ownedByMainPlayer(master)) return 0;
    return completeParts(master);
}

bool readyToFinish(const uint8_t* master) {
    if (!master || read<uint8_t>(master, kConstructedOffset) != 0) return false;
    if (!ownedByMainPlayer(master)) return false;
    if (read<int32_t>(master, kGoodsNumOffset) != 0) return false;
    const auto parts = reinterpret_cast<const uint8_t* const*>(read<std::uintptr_t>(master, kPartsOffset));
    const int32_t num = read<int32_t>(master, kPartsNumOffset);
    if (!parts || num <= 0 || num > kMaxParts) return false;
    for (int32_t i = 0; i < num; ++i) {
        const uint8_t* part = parts[i];
        if (!part || read<float>(part, kPartHpOffset) < read<float>(part, kPartMaxHpOffset)) return false;
    }
    return true;
}

namespace {
using GetProgressFn = float(__fastcall*)(void* self);
using FinishFn = void(__fastcall*)(void* self);
GetProgressFn g_original = nullptr;
FinishFn g_finish = nullptr;          // 찾지 못했으면 nullptr: hp 만 채우고 완공 처리는 게임에 맡긴다(이전 동작)
thread_local bool t_finishing = false;

// 게임 스레드에서 호출된다. 예외를 던지지 않고 항상 원본을 호출한다.
float __fastcall Detour(void* self) {
    auto master = static_cast<uint8_t*>(self);
    // 주인 오프셋은 완공 처리 함수의 본문 검사로 확인한 것이다. 그 함수를 찾았을 때만 주인을 가린다
    fillParts(master, g_finish != nullptr);
    // 완공 처리 함수가 다시 진행도를 읽어도 두 번 부르지 않는다
    if (g_finish && !t_finishing && readyToFinish(master)) {
        t_finishing = true;
        g_finish(self);
        t_finishing = false;
    }
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

    // 완공 처리 함수: 후킹하지 않고 주소만 찾는다. readyToFinish 가 읽는 오프셋을 이 함수의 본문으로 확인한다
    HookSpec finish{ "instant_finish", kFinishPattern, nullptr, reinterpret_cast<void**>(&g_finish), &wanted };
    finish.bodyChecks = {
        "48 8B 86 E0 02 00 00",      // mov rax,[rsi+2E0h]      ownerPawn
        "80 B8 4C 03 00 00 00",      // cmp byte [rax+34Ch],0   isMainPlayer
        "80 BE 91 03 00 00 00",      // cmp byte [rsi+391h],0   isBeingUpgraded
        "C6 86 B1 03 00 00 01",      // mov byte [rsi+3B1h],1   Data.constructed
        "48 8D 96 C0 03 00 00",      // lea rdx,[rsi+3C0h]      constructionGoods
    };
    finish.bodyWindow = 0x400;
    manager.add(finish);
}

}
