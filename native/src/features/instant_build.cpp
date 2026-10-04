#include "features/instant_build.h"
#include "features/memory.h"
#include <cstring>

namespace mlt::instant_build {

using mem::read;

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
    return mem::flagBehindPointer(master, kOwnerPawnOffset, kPawnIsMainPlayerOffset);
}
}

int fillParts(uint8_t* master, bool ownerOffsetsVerified) {
    if (!master) return 0;
    if (ownerOffsetsVerified && !ownedByMainPlayer(master)) return 0;
    return completeParts(master);
}

bool ownedByAnotherLord(const uint8_t* master) {
    if (!master) return false;
    const uint8_t* owner = mem::readPtr(master, kOwnerPawnOffset);
    return owner && owner[kPawnIsMainPlayerOffset] == 0;
}

int32_t hideFlags(uint8_t* engine) {
    if (!engine) return -1;
    const int32_t num = read<int32_t>(engine, kEngineFlagsNumOffset);
    if (num <= 0) return -1;
    const int32_t zero = 0;
    std::memcpy(engine + kEngineFlagsNumOffset, &zero, sizeof zero);
    return num;
}

void restoreFlags(uint8_t* engine, int32_t saved) {
    if (!engine || saved < 0) return;
    std::memcpy(engine + kEngineFlagsNumOffset, &saved, sizeof saved);
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

using SetupFn = void(__fastcall*)(void* self);
using ConvertFn = void(__fastcall*)(void* self, bool onlyFirst);
SetupFn g_setup = nullptr;
ConvertFn g_convert = nullptr;

// 다른 영주의 건물에 대한 호출 동안만 플래그 목록을 비워 보인다. 주인 오프셋은 완공 처리 함수의 본문으로 확인한 것이라
// 그 함수를 찾았을 때만 주인을 읽는다(못 찾았으면 아무것도 숨기지 않는다: 이전 동작).
struct HideFromOtherLords {
    uint8_t* engine = nullptr;
    int32_t saved = -1;
    explicit HideFromOtherLords(void* building) {
        const auto master = static_cast<const uint8_t*>(building);
        if (!g_finish || !ownedByAnotherLord(master)) return;
        engine = reinterpret_cast<uint8_t*>(read<std::uintptr_t>(master, kBuildingEngineOffset));
        saved = hideFlags(engine);
    }
    ~HideFromOtherLords() { restoreFlags(engine, saved); }
    HideFromOtherLords(const HideFromOtherLords&) = delete;
    HideFromOtherLords& operator=(const HideFromOtherLords&) = delete;
};

void __fastcall SetupDetour(void* self) {
    HideFromOtherLords hide(self);
    g_setup(self);
}

void __fastcall ConvertDetour(void* self, bool onlyFirst) {
    HideFromOtherLords hide(self);
    g_convert(self, onlyFirst);
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

    // 플래그를 읽는 두 함수: 다른 영주의 건물이면 호출 동안 플래그를 숨긴다
    HookSpec setup{ "instant_setup", kSetupPattern, reinterpret_cast<void*>(&SetupDetour), reinterpret_cast<void**>(&g_setup), &wanted };
    setup.bodyChecks = {
        "48 8B 89 E8 02 00 00",      // mov rcx,[rcx+2E8h]      엔진
        "48 81 C1 C0 10 00 00",      // add rcx,10C0h           디버그 플래그 목록
        "C6 86 B1 03 00 00 01",      // mov byte [rsi+3B1h],1   Data.constructed
    };
    setup.bodyWindow = 0x100;
    manager.add(setup);

    HookSpec convert{ "instant_convert", kConvertPattern, reinterpret_cast<void*>(&ConvertDetour), reinterpret_cast<void**>(&g_convert), &wanted };
    convert.bodyChecks = {
        "48 8B 98 C0 10 00 00",      // mov rbx,[rax+10C0h]     디버그 플래그 목록의 Data
        "48 63 80 C8 10 00 00",      // movsxd rax,[rax+10C8h]  디버그 플래그 목록의 Num
    };
    convert.bodyWindow = 0x140;
    manager.add(convert);
}

}
