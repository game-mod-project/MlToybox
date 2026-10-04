#include "features/mood.h"
#include "features/memory.h"
#include <algorithm>
#include <memory>
#include <mutex>
#include <xmmintrin.h>

namespace mlt::mood {

int ceilToInt(float v) {
    return -(_mm_cvt_ss2si(_mm_set_ss(-0.5f - (v + v))) >> 1);
}

int vanilla(float base, std::span<const float> effects) {
    float total = base;
    for (float e : effects) total += e;
    return std::clamp(ceilToInt(total), 0, 100);
}

int adjusted(float base, std::span<const float> effects, const MoodStat& s) {
    const float good = static_cast<float>(std::clamp(s.good, 1, kMaxGood));
    const float bad = static_cast<float>(std::clamp(s.bad, 0, 100)) / 100.0f;
    float total = base;
    for (float e : effects) total += e > 0 ? e * good : e * bad;
    return std::clamp(ceilToInt(total), 0, 100);
}

int decide(int game, float base, std::span<const float> effects, bool readable, const MoodStat& s) {
    if (s.fixed > 0) return std::min(s.fixed, 100);
    if (s.neutral() || !readable || vanilla(base, effects) != game) return game;
    return adjusted(base, effects, s);
}

namespace {
bool appendEffects(const uint8_t* region, std::ptrdiff_t arrayOffset, std::ptrdiff_t elementSize, std::ptrdiff_t effectOffset, std::vector<float>& out) {
    const uint8_t* data = mem::readPtr(region, arrayOffset);
    const int32_t count = mem::read<int32_t>(region, arrayOffset + 8);
    if (count < 0 || count > kMaxFactors) return false;
    if (count == 0) return true;
    if (!data) return false;
    for (int32_t i = 0; i < count; ++i) out.push_back(mem::read<float>(data, i * elementSize + effectOffset));
    return true;
}
}

bool readEffects(const uint8_t* region, const StatLayout& layout, std::vector<float>& out) {
    out.clear();
    if (!region) return false;
    if (!appendEffects(region, layout.factors, kFactorSize, kFactorEffectOffset, out)) return false;
    return !layout.policies || appendEffects(region, kRegionPoliciesOffset, kPolicySize, kPolicyEffectOffset, out);
}

bool ownedByMainPlayer(const uint8_t* region) {
    return mem::flagBehindPointer(region, kRegionOwnerOffset, kPawnIsMainPlayerOffset);
}

const MoodSet& pick(const MoodControl& control, const std::function<bool(const std::string&)>& isRegion) {
    if (isRegion) {
        for (const auto& [key, set] : control.regions) if (isRegion(key)) return set;
    }
    return control.common;
}

Outcome runUpdate(uint8_t* region, const StatLayout& layout, const MoodStat& stat, UpdateFn original) {
    constexpr int32_t kPending = -1;   // 게임은 0~100 만 쓴다
    const int32_t before = mem::read<int32_t>(region, layout.value);
    mem::write<int32_t>(region, layout.value, kPending);
    original(region);
    const int32_t game = mem::read<int32_t>(region, layout.value);
    if (game == kPending) {
        mem::write<int32_t>(region, layout.value, before);
        return {};
    }
    std::vector<float> effects;
    const bool readable = readEffects(region, layout, effects);
    const int value = decide(game, layout.base, effects, readable, stat);
    if (value != game) mem::write<int32_t>(region, layout.value, static_cast<int32_t>(value));
    return { true, game, value };
}

ProblemAction problemAction(int game, int value) {
    const bool gameLow = game < kLowApproval, valueLow = value < kLowApproval;
    if (gameLow == valueLow) return ProblemAction::None;
    return valueLow ? ProblemAction::Add : ProblemAction::Remove;
}

namespace {
using ProblemFn = void(__fastcall*)(void* region, uint8_t type, void* a, void* b);
using NameEqualsFn = bool(__fastcall*)(uint64_t name, const char* text);
UpdateFn g_updateApproval = nullptr, g_updateOrder = nullptr;
ProblemFn g_addProblem = nullptr, g_removeProblem = nullptr;
NameEqualsFn g_nameEquals = nullptr;

// 작업 스레드가 설정을 바꾸고 게임 스레드의 detour 가 읽는다. 하루에 영지마다 한 번 읽으므로 잠금으로 충분하다
std::mutex g_mutex;
std::shared_ptr<const MoodControl> g_control = std::make_shared<const MoodControl>();

MoodSet settingsFor(const uint8_t* region) {
    std::shared_ptr<const MoodControl> control;
    {
        std::lock_guard lock(g_mutex);
        control = g_control;
    }
    // 이름을 견주는 게임 함수를 못 찾았으면 영지를 가릴 수 없다. 공통 설정만 쓴다
    if (!g_nameEquals) return pick(*control, nullptr);
    const uint64_t tag = mem::read<uint64_t>(region, kRegionTagOffset);
    return pick(*control, [tag](const std::string& key) { return g_nameEquals(tag, key.c_str()); });
}

void __fastcall ApprovalDetour(void* region) {
    auto* r = static_cast<uint8_t*>(region);
    if (!ownedByMainPlayer(r)) { g_updateApproval(region); return; }
    const MoodStat stat = settingsFor(r).approval;
    if (stat.neutral()) { g_updateApproval(region); return; }
    const Outcome out = runUpdate(r, kApproval, stat, g_updateApproval);
    if (!out.updated) return;
    // 게임은 자기가 계산한 값으로 "자격 낮음" 문제를 넣고 뺐다. 바꾼 값이 경계의 다른 쪽이면 바꾼 값에 맞춘다
    switch (problemAction(out.game, out.value)) {
        case ProblemAction::Add: if (g_addProblem) g_addProblem(region, kProblemLowApproval, nullptr, nullptr); break;
        case ProblemAction::Remove: if (g_removeProblem) g_removeProblem(region, kProblemLowApproval, nullptr, nullptr); break;
        case ProblemAction::None: break;
    }
}

void __fastcall OrderDetour(void* region) {
    auto* r = static_cast<uint8_t*>(region);
    if (!ownedByMainPlayer(r)) { g_updateOrder(region); return; }
    const MoodStat stat = settingsFor(r).order;
    if (stat.neutral()) { g_updateOrder(region); return; }
    runUpdate(r, kOrder, stat, g_updateOrder);
}

bool wanted(const NativeControl& c) { return !c.mood.neutral(); }

void configure(const NativeControl& c) {
    std::lock_guard lock(g_mutex);
    if (*g_control == c.mood) return;
    g_control = std::make_shared<const MoodControl>(c.mood);
}
}

void registerHook(HookManager& manager) {
    HookSpec approval{ "mood_approval", kApprovalPattern, reinterpret_cast<void*>(&ApprovalDetour), reinterpret_cast<void**>(&g_updateApproval), &wanted };
    approval.configure = &configure;
    approval.bodyChecks = {
        "49 8D B5 70 10 00 00",                  // lea rsi,[r13+1070h]            요인 목록
        "F3 44 0F 58 42 14 48 83 C2 18",         // addss xmm8,[rdx+14h]; add rdx,18h   요인의 효과와 원소 크기
        "49 8B 95 F8 0C 00 00",                  // mov rdx,[r13+0CF8h]            정책 효과 목록
        "F3 44 0F 58 42 04",                     // addss xmm8,[rdx+4]             정책 효과
        "48 83 C2 0C",                           // add rdx,0Ch                    정책 원소 크기
        "45 8B 85 68 10 00 00",                  // mov r8d,[r13+1068h]            계산 전의 값
        "41 83 F8 14",                           // cmp r8d,14h                    그 값이 20 을 넘었을 때만 "자격 매우 낮음" 알림
        "41 89 8D 68 10 00 00",                  // mov [r13+1068h],ecx            계산한 값을 쓴다
        "49 8B BD 50 03 00 00",                  // mov rdi,[r13+350h]             영지의 주인
        "80 BF 4C 03 00 00 00",                  // cmp byte [rdi+34Ch],0          isMainPlayer
        "B2 26 41 83 BD 68 10 00 00 19",         // mov dl,26h; cmp dword [r13+1068h],19h   "자격 낮음" 문제와 경계 25
    };
    approval.bodyWindow = 0x750;
    manager.add(approval);

    HookSpec order{ "mood_order", kOrderPattern, reinterpret_cast<void*>(&OrderDetour), reinterpret_cast<void**>(&g_updateOrder), &wanted };
    order.configure = &configure;
    order.bodyChecks = {
        "49 8D BE 98 10 00 00",                  // lea rdi,[r14+1098h]            요인 목록
        "F3 44 0F 58 52 14 48 83 C2 18",         // addss xmm10,[rdx+14h]; add rdx,18h
        "45 89 A6 84 10 00 00",                  // mov [r14+1084h],r12d           계산한 값을 쓴다
        "49 8B 86 50 03 00 00",                  // mov rax,[r14+350h]             영지의 주인
        "80 B8 4C 03 00 00 00",                  // cmp byte [rax+34Ch],0          isMainPlayer
    };
    order.bodyWindow = 0x5E0;
    manager.add(order);

    HookSpec add{ "mood_problem_add", kAddProblemPattern, nullptr, reinterpret_cast<void**>(&g_addProblem), &wanted };
    add.bodyChecks = {
        "48 6B FA 38",                           // imul rdi,rdx,38h               문제 원소 크기
    };
    add.bodyWindow = 0x100;
    manager.add(add);

    HookSpec remove{ "mood_problem_remove", kRemoveProblemPattern, nullptr, reinterpret_cast<void**>(&g_removeProblem), &wanted };
    remove.bodyChecks = {
        "48 8D 99 68 0D 00 00",                  // lea rbx,[rcx+0D68h]            영지의 문제 목록
        "48 83 C0 38",                           // add rax,38h                    문제 원소 크기
    };
    remove.bodyWindow = 0x100;
    manager.add(remove);

    HookSpec name{ "mood_region_name", kNameEqualsPattern, nullptr, reinterpret_cast<void**>(&g_nameEquals), &wanted };
    name.bodyChecks = {
        "80 3A 5F",                              // cmp byte [rdx],5Fh             글자열의 첫 글자를 본다(두 번째 인수가 const char*)
    };
    name.bodyWindow = 0x60;
    manager.add(name);
}

}
