#include "features/placement.h"

namespace mlt::placement {

bool allowPlacement(uint8_t* pawn) {
    if (!pawn) return false;
    // 경계 밖 배치는 어느 지역에도 속하지 않는 건물이 되므로 허용하지 않는다
    if (pawn[kInsideBordersOffset] == 0) return false;
    pawn[kInvalidFlagOffset] = 0;
    return true;
}

namespace {
using UpdateFn = void(__fastcall*)(void* self);
UpdateFn g_original = nullptr;

// 게임 스레드에서 매 틱 호출된다. 원본이 판정을 쓴 뒤 결과만 덮어쓴다.
void __fastcall Detour(void* self) {
    g_original(self);
    allowPlacement(static_cast<uint8_t*>(self));
}

bool wanted(const NativeControl& c) { return c.ignorePlacement; }
}

void registerHook(HookManager& manager) {
    HookSpec spec{ "placement", kPattern, reinterpret_cast<void*>(&Detour), reinterpret_cast<void**>(&g_original), &wanted };
    spec.bodyChecks = {
        "44 38 B9 4D 03 00 00",      // cmp [rcx+34Dh],r15b   isAI
        "8B 89 08 06 00 00",         // mov ecx,[rcx+608h]    placeBuilding
        "44 88 A7 0C 06 00 00",      // mov [rdi+60Ch],r12b   배치 불가 플래그
    };
    spec.bodyWindow = 0x200;
    manager.add(spec);
}

}
