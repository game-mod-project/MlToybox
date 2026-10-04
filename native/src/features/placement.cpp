#include "features/placement.h"
#include "features/memory.h"
#include <atomic>

namespace mlt::placement {

using mem::read;
using mem::write;

bool allowPlacement(uint8_t* pawn) {
    if (!pawn) return false;
    // 다른 영주의 폰도 이 후킹을 지난다(원본은 AI 폰이면 바로 돌아간다). 그 폰의 판정은 건드리지 않는다
    if (pawn[kPawnIsAiOffset] != 0) return false;
    // 경계 밖 배치는 어느 지역에도 속하지 않는 건물이 되므로 허용하지 않는다
    if (pawn[kInsideBordersOffset] == 0) return false;
    // 도로·성벽 배치 함수(0x144B03460)도 이 플래그로 "지점이 어느 영지에도 없음"을 표시하고 뒤 처리를 건너뛴다.
    // 지우면 null 영지의 +0x738 을 읽어 크래시한다(실측 2026-09-30). 도로 모드에서는 건드리지 않는다
    if (pawn[kRoadModeOffset] != 0) return false;
    pawn[kInvalidFlagOffset] = 0;
    return true;
}

bool isPlayerPlacing(const uint8_t* pawn) {
    if (!pawn) return false;
    if (pawn[kPawnIsMainPlayerOffset] == 0 || pawn[kPawnIsAiOffset] != 0) return false;
    return read<int32_t>(pawn, kPlaceBuildingOffset) > 0;
}

RowEdit relaxRow(uint8_t* row, bool noRegionLimit, bool noMaterials) {
    RowEdit edit;
    if (!row) return edit;
    edit.row = row;
    if (noRegionLimit) {
        edit.limit = true;
        edit.maxInRegion = read<int32_t>(row, kRowMaxInRegionOffset);
        write<int32_t>(row, kRowMaxInRegionOffset, 0);
    }
    if (noMaterials) {
        edit.goods = true;
        edit.goodsNum = read<int32_t>(row, kRowGoodsNumOffset);
        write<int32_t>(row, kRowGoodsNumOffset, 0);
    }
    return edit;
}

void restoreRow(const RowEdit& edit) {
    if (!edit.row) return;
    if (edit.limit) write<int32_t>(edit.row, kRowMaxInRegionOffset, edit.maxInRegion);
    if (edit.goods) write<int32_t>(edit.row, kRowGoodsNumOffset, edit.goodsNum);
}

namespace {
using UpdateFn = void(__fastcall*)(void* self);
using RowFn = uint8_t*(__fastcall*)(int32_t type);
UpdateFn g_original = nullptr;
RowFn g_rowOf = nullptr;              // 찾지 못했으면 nullptr: 행을 고치지 않는다(Lua 가 표를 바꾸는 이전 방식으로 돌아간다)
void* g_rowLayout = nullptr;          // 배치 갱신 함수가 행의 그 오프셋을 읽는 것을 확인했으면 그 함수의 주소
std::atomic<bool> g_ignorePlacement{ false }, g_noRegionLimit{ false }, g_noMaterials{ false };

// 게임 스레드에서 매 틱 호출된다. 게임 논리는 이 스레드에서만 돌므로 원본이 도는 동안 고친 행을 AI 가 읽지 않는다.
void __fastcall Detour(void* self) {
    auto pawn = static_cast<uint8_t*>(self);
    const bool limit = g_noRegionLimit.load(std::memory_order_relaxed);
    const bool goods = g_noMaterials.load(std::memory_order_relaxed);
    RowEdit edit;
    if ((limit || goods) && g_rowOf && g_rowLayout && isPlayerPlacing(pawn))
        edit = relaxRow(g_rowOf(read<int32_t>(pawn, kPlaceBuildingOffset)), limit, goods);
    g_original(self);
    restoreRow(edit);
    // 원본이 판정을 쓴 뒤 결과만 덮어쓴다
    if (g_ignorePlacement.load(std::memory_order_relaxed)) allowPlacement(pawn);
}

bool wantsRows(const NativeControl& c) { return c.noRegionLimit || c.noMaterials; }
bool wanted(const NativeControl& c) { return c.ignorePlacement || wantsRows(c); }
void configure(const NativeControl& c) {
    g_ignorePlacement.store(c.ignorePlacement, std::memory_order_relaxed);
    g_noRegionLimit.store(c.noRegionLimit, std::memory_order_relaxed);
    g_noMaterials.store(c.noMaterials, std::memory_order_relaxed);
}
}

void registerHook(HookManager& manager) {
    HookSpec spec{ "placement", kPattern, reinterpret_cast<void*>(&Detour), reinterpret_cast<void**>(&g_original), &wanted };
    spec.configure = &configure;
    spec.bodyChecks = {
        "44 38 B9 4D 03 00 00",      // cmp [rcx+34Dh],r15b   isAI
        "8B 89 08 06 00 00",         // mov ecx,[rcx+608h]    placeBuilding
        "44 88 A7 0C 06 00 00",      // mov [rdi+60Ch],r12b   배치 불가 플래그
    };
    spec.bodyWindow = 0x200;
    manager.add(spec);

    // 건물 표의 행 함수: 후킹하지 않고 주소만 찾는다. id -> 행 포인터 맵을 뒤지는 본문인지 확인한다
    HookSpec row{ "building_row", kRowFunctionPattern, nullptr, reinterpret_cast<void**>(&g_rowOf), &wantsRows };
    row.bodyChecks = {
        "39 1C CA",                  // cmp [rdx+rcx*8],ebx   맵 항목의 키(건물 종류)
        "48 8D 48 08",               // lea rcx,[rax+8]       맵 항목의 값
        "48 8B 01",                  // mov rax,[rcx]         행 포인터
    };
    row.bodyWindow = 0x100;
    manager.add(row);

    // relaxRow 가 쓰는 오프셋을 배치 갱신 함수의 본문으로 확인한다. 같은 함수를 한 번 더 찾을 뿐 후킹하지 않는다.
    // "placement" 와 따로 두어, 이 검사가 실패해도 배치 제한 무시는 그대로 쓸 수 있다
    HookSpec layout{ "placement_rows", kPattern, nullptr, &g_rowLayout, &wantsRows };
    layout.bodyChecks = {
        "41 83 BD D8 02 00 00 00",   // cmp dword [r13+2D8h],0  maxInRegion
        "41 8B 85 D8 02 00 00",      // mov eax,[r13+2D8h]      maxInRegion (개수와 비교)
        "49 8B 95 90 02 00 00",      // mov rdx,[r13+290h]      constructionGoods 의 Data
        "49 63 85 98 02 00 00",      // movsxd rax,[r13+298h]   constructionGoods 의 Num
        "80 BF 4C 03 00 00 00",      // cmp byte [rdi+34Ch],0   isMainPlayer
    };
    layout.bodyWindow = 0x2800;
    manager.add(layout);
}

}
