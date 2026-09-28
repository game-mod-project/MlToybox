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
    manager.add({ "placement", kPattern, reinterpret_cast<void*>(&Detour), reinterpret_cast<void**>(&g_original), &wanted });
}

}
