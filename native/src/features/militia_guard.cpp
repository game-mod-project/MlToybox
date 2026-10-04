#include "features/militia_guard.h"
#include "features/memory.h"

namespace mlt::militia_guard {

using mem::read;
using mem::readPtr;

bool isSafe(const uint8_t* pawn, const uint8_t* region) {
    if (!pawn) return false;
    const int32_t commandedNum = read<int32_t>(pawn, kPawnCommandedNumOffset);
    if (commandedNum == 0) return true;
    if (commandedNum < 0 || commandedNum > kMaxCount) return false;
    const uint8_t* commanded = readPtr(pawn, kPawnCommandedOffset);
    const uint8_t* engine = readPtr(pawn, kPawnEngineOffset);
    if (!commanded || !engine) return false;
    const uint8_t* squads = readPtr(engine, kEngineSquadsOffset);
    const int32_t squadsNum = read<int32_t>(engine, kEngineSquadsNumOffset);
    if (!squads || squadsNum <= 0 || squadsNum > kMaxCount) return false;

    for (int32_t i = 0; i < commandedNum; ++i) {
        const int32_t index = read<int32_t>(commanded, i * static_cast<std::ptrdiff_t>(sizeof(int32_t)));
        if (index < 0 || index >= squadsNum) return false;
        const uint8_t* squad = squads + index * kSquadSize;
        if (read<uint8_t>(squad, kSquadTypeOffset) != kMilitiaType) continue;
        if (readPtr(squad, kSquadOriginOffset) != region) continue;
        const int32_t recruitsNum = read<int32_t>(squad, kSquadRecruitsNumOffset);
        if (recruitsNum == 0) continue;
        const uint8_t* recruits = readPtr(squad, kSquadRecruitsOffset);
        if (!recruits || recruitsNum < 0 || recruitsNum > kMaxCount) return false;
        for (int32_t k = 0; k < recruitsNum; ++k) {
            const uint8_t* unit = readPtr(recruits, k * static_cast<std::ptrdiff_t>(sizeof(void*)));
            if (!unit || !readPtr(unit, kUnitHomeOffset)) return false;
        }
    }
    return true;
}

namespace {
using EvaluateFn = void(__fastcall*)(void* pawn, void* region);
EvaluateFn g_original = nullptr;
EvaluateFn g_original2 = nullptr;

// 게임 스레드에서 호출된다. 원본 반환값은 호출자가 쓰지 않는다(void).
void __fastcall Detour(void* pawn, void* region) {
    if (!isSafe(static_cast<const uint8_t*>(pawn), static_cast<const uint8_t*>(region))) return;
    g_original(pawn, region);
}

void __fastcall Detour2(void* pawn, void* region) {
    if (!isSafe(static_cast<const uint8_t*>(pawn), static_cast<const uint8_t*>(region))) return;
    g_original2(pawn, region);
}

// 치트가 아닌 크래시 방어이므로 항상 켠다
bool wanted(const NativeControl&) { return true; }
}

void registerHook(HookManager& manager) {
    HookSpec spec{ "militia_guard", kPattern, reinterpret_cast<void*>(&Detour), reinterpret_cast<void**>(&g_original), &wanted };
    spec.bodyChecks = {
        "4C 8B B1 10 0A 00 00",   // mov r14,[rcx+0A10h]    commandedSquads
        "48 8B 8B 40 03 00 00",   // mov rcx,[rbx+340h]     masterPtr
        "48 69 F0 E8 03 00 00",   // imul rsi,rax,3E8h      sizeof(FSquad)
        "48 03 B1 B0 05 00 00",   // add rsi,[rcx+5B0h]     squads
        "80 7E 10 01",            // cmp byte [rsi+10h],1   squadType == Militia
        "4C 39 86 F8 01 00 00",   // cmp [rsi+1F8h],r8      originRegion
        "48 8B BE 48 03 00 00",   // mov rdi,[rsi+348h]     assignedRecruits
        "48 8B 88 40 03 00 00",   // mov rcx,[rax+340h]     unit.Home
    };
    spec.bodyWindow = 0x290;
    manager.add(spec);

    HookSpec spec2{ "militia_guard_2", kPattern2, reinterpret_cast<void*>(&Detour2), reinterpret_cast<void**>(&g_original2), &wanted };
    spec2.bodyChecks = {
        "44 39 82 CC 07 00 00",   // cmp [rdx+7CCh],r8d     쿨다운
        "4C 8B BE 10 0A 00 00",   // mov r15,[rsi+0A10h]    commandedSquads
        "49 8B 8E 40 03 00 00",   // mov rcx,[r14+340h]     masterPtr
        "48 69 F0 E8 03 00 00",   // imul rsi,rax,3E8h      sizeof(FSquad)
        "48 03 B1 B0 05 00 00",   // add rsi,[rcx+5B0h]     squads
        "80 7E 10 01",            // cmp byte [rsi+10h],1   squadType == Militia
        "48 39 BE F8 01 00 00",   // cmp [rsi+1F8h],rdi     originRegion
        "48 8B BE 48 03 00 00",   // mov rdi,[rsi+348h]     assignedRecruits
        "48 8B 88 40 03 00 00",   // mov rcx,[rax+340h]     unit.Home
    };
    spec2.bodyWindow = 0x560;
    manager.add(spec2);
}

}
