#include "features/upgrade_scope.h"
#include <cstring>

namespace mlt::upgrade_scope {

namespace {
template <class T> T read(const uint8_t* base, std::ptrdiff_t off) { T v; std::memcpy(&v, base + off, sizeof v); return v; }
template <class T> void write(uint8_t* base, std::ptrdiff_t off, T v) { std::memcpy(base + off, &v, sizeof v); }
}

bool ownedByMainPlayer(const uint8_t* building) {
    if (!building) return false;
    const auto owner = reinterpret_cast<const uint8_t*>(read<std::uintptr_t>(building, kBuildingOwnerOffset));
    return owner && owner[kPawnIsMainPlayerOffset] != 0;
}

RowEdit relaxRow(uint8_t* row) {
    RowEdit edit;
    if (!row) return edit;
    edit.row = row;
    edit.costNum = read<int32_t>(row, kRowCostNumOffset);
    edit.regionalWealth = read<int32_t>(row, kRowRegionalWealthOffset);
    edit.treasury = read<int32_t>(row, kRowTreasuryOffset);
    edit.minSettlementLevel = read<int32_t>(row, kRowMinSettlementLevelOffset);
    edit.minHouseLv = row[kRowMinHouseLvOffset];
    write<int32_t>(row, kRowCostNumOffset, 0);
    write<int32_t>(row, kRowRegionalWealthOffset, 0);
    write<int32_t>(row, kRowTreasuryOffset, 0);
    write<int32_t>(row, kRowMinSettlementLevelOffset, 0);
    row[kRowMinHouseLvOffset] = 0;
    return edit;
}

void restoreRow(const RowEdit& edit) {
    if (!edit.row) return;
    write<int32_t>(edit.row, kRowCostNumOffset, edit.costNum);
    write<int32_t>(edit.row, kRowRegionalWealthOffset, edit.regionalWealth);
    write<int32_t>(edit.row, kRowTreasuryOffset, edit.treasury);
    write<int32_t>(edit.row, kRowMinSettlementLevelOffset, edit.minSettlementLevel);
    edit.row[kRowMinHouseLvOffset] = edit.minHouseLv;
}

namespace {
using RowFn = uint8_t*(__fastcall*)(void* engine, int32_t id);
using CanUpgradeFn = bool(__fastcall*)(void* building, int32_t id, void* reasons);
using PayFn = void(__fastcall*)(void* building, int32_t id, bool flag);
using ResourceCostFn = void*(__fastcall*)(void* building, void* out, int32_t id);
using WealthCostFn = int32_t(__fastcall*)(void* building, int32_t id);
using ResidentialFn = bool(__fastcall*)(void* building);
RowFn g_rowOf = nullptr;
CanUpgradeFn g_canUpgrade = nullptr;
PayFn g_pay = nullptr;
ResourceCostFn g_resourceCost = nullptr;
WealthCostFn g_wealthCost = nullptr;
ResidentialFn g_residential = nullptr;

// 주인과 엔진의 오프셋(+0x2E0, +0x2E8, 폰+0x34C)은 비용 지불 함수의 본문으로 확인한다. 그 함수와 행 함수를 찾았을 때만 주인을 읽는다.
// 하나라도 못 찾으면 아무것도 고치지 않는다(Lua 가 표를 바꾸는 이전 방식으로 돌아간다).
bool ready() { return g_rowOf != nullptr && g_pay != nullptr; }

// 내 건물에 대한 호출이면 그 업그레이드의 행을 고쳐 두고, 호출이 끝나면 되돌린다. 게임 논리는 게임 스레드에서만 돌므로 그 사이에 AI 가 읽지 않는다.
struct Scope {
    RowEdit edit;
    Scope(void* building, int32_t id) {
        const auto b = static_cast<const uint8_t*>(building);
        if (!ready() || !ownedByMainPlayer(b)) return;
        const auto engine = reinterpret_cast<void*>(read<std::uintptr_t>(b, kBuildingEngineOffset));
        if (engine) edit = relaxRow(g_rowOf(engine, id));
    }
    ~Scope() { restoreRow(edit); }
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
};

bool __fastcall CanUpgradeDetour(void* building, int32_t id, void* reasons) {
    Scope scope(building, id);
    return g_canUpgrade(building, id, reasons);
}

void __fastcall PayDetour(void* building, int32_t id, bool flag) {
    Scope scope(building, id);
    g_pay(building, id, flag);
}

void* __fastcall ResourceCostDetour(void* building, void* out, int32_t id) {
    Scope scope(building, id);
    return g_resourceCost(building, out, id);
}

int32_t __fastcall WealthCostDetour(void* building, int32_t id) {
    Scope scope(building, id);
    return g_wealthCost(building, id);
}

// 주거 요구: 원본은 설정 CDO 를 읽어 판정만 한다(부수 효과 없음). 내 건물이면 충족으로 본다
bool __fastcall ResidentialDetour(void* building) {
    if (ready() && ownedByMainPlayer(static_cast<const uint8_t*>(building))) return true;
    return g_residential(building);
}

bool wanted(const NativeControl& c) { return c.upgradeFree; }
}

void registerHook(HookManager& manager) {
    HookSpec row{ "upgrade_row", kRowFunctionPattern, nullptr, reinterpret_cast<void**>(&g_rowOf), &wanted };
    row.bodyChecks = {
        "48 8B 8B D8 0D 00 00",      // mov rcx,[rbx+0DD8h]   엔진의 업그레이드 표
    };
    row.bodyWindow = 0x130;
    manager.add(row);

    HookSpec can{ "upgrade_can", kCanUpgradePattern, reinterpret_cast<void*>(&CanUpgradeDetour), reinterpret_cast<void**>(&g_canUpgrade), &wanted };
    can.bodyChecks = {
        "48 8B 86 E0 02 00 00",      // mov rax,[rsi+2E0h]        ownerPawn
        "48 8B 8E E8 02 00 00",      // mov rcx,[rsi+2E8h]        엔진(행 함수의 인자)
        "41 8B 47 78",               // mov eax,[r15+78h]         minimumSettlementLevel
        "41 0F B6 47 7D",            // movzx eax,byte [r15+7Dh]  minimumHouseLv
        "41 8B 4F 54",               // mov ecx,[r15+54h]         treasury
    };
    can.bodyWindow = 0x800;
    manager.add(can);

    HookSpec pay{ "upgrade_pay", kPayPattern, reinterpret_cast<void*>(&PayDetour), reinterpret_cast<void**>(&g_pay), &wanted };
    pay.bodyChecks = {
        "48 8B 89 E8 02 00 00",      // mov rcx,[rcx+2E8h]        엔진
        "8B 4E 54",                  // mov ecx,[rsi+54h]         treasury
        "48 8B 83 E0 02 00 00",      // mov rax,[rbx+2E0h]        ownerPawn
        "29 88 40 0A 00 00",         // sub [rax+0A40h],ecx       폰의 금고에서 뺀다
        "80 BF 4C 03 00 00 00",      // cmp byte [rdi+34Ch],0     isMainPlayer
    };
    pay.bodyWindow = 0x160;
    manager.add(pay);

    HookSpec cost{ "upgrade_cost", kResourceCostPattern, reinterpret_cast<void*>(&ResourceCostDetour), reinterpret_cast<void**>(&g_resourceCost), &wanted };
    cost.bodyChecks = {
        "48 8B 89 E8 02 00 00",      // mov rcx,[rcx+2E8h]        엔진
        "48 63 7D 48",               // movsxd rdi,[rbp+48h]      cost 의 Num
        "48 8B 75 40",               // mov rsi,[rbp+40h]         cost 의 Data
    };
    cost.bodyWindow = 0x100;
    manager.add(cost);

    HookSpec wealth{ "upgrade_wealth", kWealthCostPattern, reinterpret_cast<void*>(&WealthCostDetour), reinterpret_cast<void**>(&g_wealthCost), &wanted };
    wealth.bodyChecks = {
        "48 8B 8D E8 02 00 00",      // mov rcx,[rbp+2E8h]        엔진
        "80 78 38 00",               // cmp byte [rax+38h],0      bIsCostScalable
        "8B 47 50",                  // mov eax,[rdi+50h]         regionalWealth
    };
    wealth.bodyWindow = 0x200;
    manager.add(wealth);

    HookSpec residential{ "upgrade_residential", kResidentialPattern, reinterpret_cast<void*>(&ResidentialDetour), reinterpret_cast<void**>(&g_residential), &wanted };
    residential.bodyChecks = {
        "48 63 8E A8 0C 00 00",      // movsxd rcx,[rsi+0CA8h]    집 레벨로 요구 목록을 고른다
    };
    residential.bodyWindow = 0x100;
    manager.add(residential);
}

}
