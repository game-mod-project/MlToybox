#include "test.h"
#include "features/mood.h"
#include "control.h"
#include <cstring>
#include <vector>

using namespace mlt;

namespace {
template <class T> void put(uint8_t* base, std::ptrdiff_t off, T v) { std::memcpy(base + off, &v, sizeof v); }
template <class T> T get(const uint8_t* base, std::ptrdiff_t off) { T v; std::memcpy(&v, base + off, sizeof v); return v; }

// 영지: 자격·공공질서 값과, 요인 목록 둘(원소 0x18, 효과는 +0x14), 정책 효과 목록(원소 0xC, 효과는 +4)
struct FakeRegion {
    alignas(8) uint8_t bytes[0x1100]{};
    std::vector<uint8_t> approvalFactors, orderFactors, policies;

    void setArray(std::ptrdiff_t off, std::vector<uint8_t>& store, std::ptrdiff_t elem, std::ptrdiff_t valueOff, std::initializer_list<float> values) {
        store.assign(values.size() * elem, 0);
        size_t i = 0;
        for (float v : values) put(store.data(), static_cast<std::ptrdiff_t>(i++ * elem) + valueOff, v);
        put(bytes, off, reinterpret_cast<std::uintptr_t>(store.empty() ? nullptr : store.data()));
        put(bytes, off + 8, static_cast<int32_t>(values.size()));
    }
    void approval(int v, std::initializer_list<float> factors, std::initializer_list<float> policyEffects = {}) {
        put(bytes, mood::kApproval.value, static_cast<int32_t>(v));
        setArray(mood::kApproval.factors, approvalFactors, mood::kFactorSize, mood::kFactorEffectOffset, factors);
        setArray(mood::kRegionPoliciesOffset, policies, mood::kPolicySize, mood::kPolicyEffectOffset, policyEffects);
    }
    void order(int v, std::initializer_list<float> factors) {
        put(bytes, mood::kOrder.value, static_cast<int32_t>(v));
        setArray(mood::kOrder.factors, orderFactors, mood::kFactorSize, mood::kFactorEffectOffset, factors);
    }
    int approvalValue() const { return get<int32_t>(bytes, mood::kApproval.value); }
    int orderValue() const { return get<int32_t>(bytes, mood::kOrder.value); }
};

// 게임의 갱신 함수 흉내: 미리 정해 둔 값을 영지에 쓴다(g_gameWrites 가 음수면 이번에는 계산하지 않는다)
int g_gameWrites = -1;
int g_seenBefore = 0;
void __fastcall fakeApprovalUpdate(void* region) {
    auto* r = static_cast<uint8_t*>(region);
    g_seenBefore = get<int32_t>(r, mood::kApproval.value);
    if (g_gameWrites >= 0) put(r, mood::kApproval.value, static_cast<int32_t>(g_gameWrites));
}
void __fastcall fakeOrderUpdate(void* region) {
    if (g_gameWrites >= 0) put(static_cast<uint8_t*>(region), mood::kOrder.value, static_cast<int32_t>(g_gameWrites));
}
}

TEST(mood_rounds_up_the_way_the_game_does) {
    CHECK(mood::ceilToInt(54.0f) == 54);
    CHECK(mood::ceilToInt(53.2f) == 54);
    CHECK(mood::ceilToInt(0.0f) == 0);
    CHECK(mood::ceilToInt(-0.5f) == 0);
    CHECK(mood::ceilToInt(-1.2f) == -1);
}

TEST(mood_game_value_is_the_base_plus_the_factors_kept_between_0_and_100) {
    // 실측(saveGame_1): 요인 9, 11, 17, 4, 5, 8 인 영지는 100(104 를 자른 값), 9, 8, 12, 4, 3, 5 인 영지는 91
    const float a[] = { 9, 11, 17, 4, 5, 8 }, b[] = { 9, 8, 12, 4, 3, 5 }, order[] = { -10, 10 }, low[] = { -70 };
    CHECK(mood::vanilla(mood::kApproval.base, a) == 100);
    CHECK(mood::vanilla(mood::kApproval.base, b) == 91);
    CHECK(mood::vanilla(mood::kOrder.base, order) == 100);
    CHECK(mood::vanilla(mood::kApproval.base, low) == 0);
    CHECK(mood::vanilla(mood::kApproval.base, {}) == 50);
}

TEST(mood_multipliers_scale_gains_and_losses_separately) {
    const float f[] = { 10, -20 };
    CHECK(mood::adjusted(50.0f, f, MoodStat{ 0, 1, 100 }) == 40);    // 그대로
    CHECK(mood::adjusted(50.0f, f, MoodStat{ 0, 2, 100 }) == 50);    // 오르는 요인 2배: 50 + 20 - 20
    CHECK(mood::adjusted(50.0f, f, MoodStat{ 0, 1, 50 }) == 50);     // 깎이는 요인 절반: 50 + 10 - 10
    CHECK(mood::adjusted(50.0f, f, MoodStat{ 0, 1, 0 }) == 60);      // 깎이는 요인 없음
    CHECK(mood::adjusted(50.0f, f, MoodStat{ 0, 10, 0 }) == 100);    // 100 에서 자른다
    const float bad[] = { -80 };
    CHECK(mood::adjusted(50.0f, bad, MoodStat{ 0, 3, 100 }) == 0);   // 0 아래로 내려가지 않는다
    // 배율이 범위를 벗어나면 범위 끝으로 본다
    CHECK(mood::adjusted(50.0f, f, MoodStat{ 0, 99, 100 }) == mood::adjusted(50.0f, f, MoodStat{ 0, mood::kMaxGood, 100 }));
    CHECK(mood::adjusted(50.0f, f, MoodStat{ 0, 0, 500 }) == 40);
}

TEST(mood_fixed_value_wins_over_the_multipliers) {
    const float f[] = { -40 };
    CHECK(mood::decide(10, 50.0f, f, true, MoodStat{ 80, 5, 0 }) == 80);
    CHECK(mood::decide(10, 50.0f, f, true, MoodStat{ 250, 1, 100 }) == 100);
    CHECK(mood::decide(10, 50.0f, f, false, MoodStat{ 80, 1, 100 }) == 80);   // 요인 목록을 못 읽어도 고정값은 쓴다
}

TEST(mood_multipliers_apply_only_when_our_sum_matches_the_games_value) {
    const float f[] = { 10, -20 };
    CHECK(mood::decide(40, 50.0f, f, true, MoodStat{ 0, 1, 0 }) == 60);
    // 게임이 쓴 값이 우리 계산(40)과 다르면 모르는 요인이 있다는 뜻이다. 게임 값을 그대로 둔다
    CHECK(mood::decide(47, 50.0f, f, true, MoodStat{ 0, 1, 0 }) == 47);
    CHECK(mood::decide(40, 50.0f, f, false, MoodStat{ 0, 1, 0 }) == 40);   // 요인 목록을 읽지 못했다
}

TEST(mood_neutral_settings_keep_the_games_value) {
    const float f[] = { 10, -20 };
    CHECK(MoodStat{}.neutral());
    CHECK(!(MoodStat{ 1, 1, 100 }.neutral()) && !(MoodStat{ 0, 2, 100 }.neutral()) && !(MoodStat{ 0, 1, 99 }.neutral()));
    CHECK(mood::decide(47, 50.0f, f, true, MoodStat{}) == 47);
}

TEST(mood_reads_the_factor_effects_from_the_region) {
    FakeRegion r;
    r.approval(91, { 9, 8, 12, 4, 3, 5 }, { -2.5f, 1.0f });
    std::vector<float> effects;
    CHECK(mood::readEffects(r.bytes, mood::kApproval, effects));
    CHECK(effects.size() == 8 && effects[0] == 9 && effects[5] == 5 && effects[6] == -2.5f && effects[7] == 1.0f);
    r.order(100, { -10, 10 });
    CHECK(mood::readEffects(r.bytes, mood::kOrder, effects));
    CHECK(effects.size() == 2 && effects[0] == -10 && effects[1] == 10);   // 공공질서에는 정책 목록이 없다
    // 빈 목록
    r.order(100, {});
    CHECK(mood::readEffects(r.bytes, mood::kOrder, effects) && effects.empty());
    // 말이 안 되는 개수(레이아웃이 바뀌었다)는 읽지 않는다
    put(r.bytes, mood::kOrder.factors + 8, static_cast<int32_t>(-3));
    CHECK(!mood::readEffects(r.bytes, mood::kOrder, effects));
    put(r.bytes, mood::kOrder.factors + 8, static_cast<int32_t>(100000));
    CHECK(!mood::readEffects(r.bytes, mood::kOrder, effects));
    put(r.bytes, mood::kOrder.factors + 8, static_cast<int32_t>(2));
    put(r.bytes, mood::kOrder.factors, std::uintptr_t{ 0 });               // 개수는 있는데 포인터가 없다
    CHECK(!mood::readEffects(r.bytes, mood::kOrder, effects));
}

TEST(mood_update_changes_the_value_right_after_the_game_computes_it) {
    FakeRegion r;
    r.approval(100, { 30, -60 });   // 지난번에 모드가 100 으로 올려 둔 영지. 게임 계산은 50 + 30 - 60 = 20
    g_gameWrites = 20;
    auto out = mood::runUpdate(r.bytes, mood::kApproval, MoodStat{ 0, 1, 0 }, &fakeApprovalUpdate);
    CHECK(out.updated && out.game == 20 && out.value == 80);
    CHECK(r.approvalValue() == 80);
    // 게임의 함수는 계산 전의 값을 "지난 값"으로 읽어, 20 을 넘다가 20 아래로 떨어지면 알림을 띄운다.
    // 모드가 올려 둔 값을 지난 값으로 보면 날마다 알림이 뜬다. 그래서 부르기 전에 20 을 넘지 않는 값을 넣어 둔다
    CHECK(g_seenBefore <= 20);
}

TEST(mood_update_restores_the_value_when_the_game_skips_the_calculation) {
    // 가족이 없는 영지 등은 게임이 값을 쓰지 않고 돌아간다. 부르기 전에 넣어 둔 표시가 남지 않아야 한다
    FakeRegion r;
    r.approval(64, { 30 });
    g_gameWrites = -1;
    auto out = mood::runUpdate(r.bytes, mood::kApproval, MoodStat{ 90, 1, 100 }, &fakeApprovalUpdate);
    CHECK(!out.updated);
    CHECK(r.approvalValue() == 64);
}

TEST(mood_update_fixes_public_order_too) {
    FakeRegion r;
    r.order(100, { -10, 5, -35 });   // 게임 계산은 100 - 10 + 5 - 35 = 60
    g_gameWrites = 60;
    auto out = mood::runUpdate(r.bytes, mood::kOrder, MoodStat{ 0, 1, 20 }, &fakeOrderUpdate);
    CHECK(out.updated && out.game == 60 && out.value == 96);   // 깎이는 요인을 20% 로: 100 - 2 + 5 - 7
    CHECK(r.orderValue() == 96);
    g_gameWrites = 30;
    out = mood::runUpdate(r.bytes, mood::kOrder, MoodStat{ 75, 1, 100 }, &fakeOrderUpdate);
    CHECK(out.updated && out.value == 75 && r.orderValue() == 75);
}

TEST(mood_low_approval_problem_follows_the_changed_value) {
    // 게임은 계산한 값이 25 아래면 "자격 낮음" 문제를 영지에 넣고, 아니면 뺀다. 모드가 값을 바꿔 25 를 넘나들면 같은 일을 다시 해 준다
    CHECK(mood::problemAction(10, 80) == mood::ProblemAction::Remove);
    CHECK(mood::problemAction(40, 10) == mood::ProblemAction::Add);
    CHECK(mood::problemAction(10, 20) == mood::ProblemAction::None);
    CHECK(mood::problemAction(40, 90) == mood::ProblemAction::None);
    CHECK(mood::problemAction(24, 25) == mood::ProblemAction::Remove);
}

TEST(mood_region_settings_replace_the_common_ones_for_that_region) {
    MoodControl c;
    c.common.approval = MoodStat{ 0, 2, 50 };
    c.regions.push_back({ "eich", MoodSet{ MoodStat{ 100, 1, 100 }, MoodStat{ 0, 1, 0 } } });
    c.regions.push_back({ "imm", MoodSet{} });
    auto isRegion = [](std::string_view want) { return [want](const std::string& key) { return key == want; }; };
    CHECK(mood::pick(c, isRegion("eich")).approval.fixed == 100);
    CHECK(mood::pick(c, isRegion("eich")).order.bad == 0);
    CHECK(mood::pick(c, isRegion("imm")).approval.neutral());     // 영지에 따로 둔 "게임 그대로"가 공통 설정을 이긴다
    CHECK(mood::pick(c, isRegion("hof")).approval.good == 2);     // 따로 둔 것이 없는 영지는 공통
    CHECK(mood::pick(c, nullptr).approval.good == 2);             // 영지를 가릴 수 없으면 공통
}

TEST(mood_registers_two_hooks_and_two_address_only_functions) {
    // 영지를 설정의 영지 키와 견주는 항목(region_name, region_tag)은 features/region_scope 가 등록한다
    HookManager m;
    mood::registerHook(m);
    auto s = m.states();
    CHECK(s.size() == 4);
    CHECK(s[0].name == "mood_approval");
    CHECK(s[1].name == "mood_order");
    CHECK(s[2].name == "mood_problem_add");
    CHECK(s[3].name == "mood_problem_remove");
}

TEST(control_reads_the_mood_settings) {
    auto c = parseControl(R"({"version":1,"seq":4,"features":{"mood":{"enabled":true,
        "approval":{"fixed":0,"good":3,"bad":25},"order":{"fixed":90,"good":1,"bad":100},
        "regions":{"eich":{"approval":{"fixed":100,"good":1,"bad":100},"order":{"fixed":0,"good":1,"bad":0}},"imm":{}}}}})");
    CHECK(c.has_value());
    CHECK(c->mood.common.approval.fixed == 0 && c->mood.common.approval.good == 3 && c->mood.common.approval.bad == 25);
    CHECK(c->mood.common.order.fixed == 90);
    CHECK(c->mood.regions.size() == 2);
    CHECK(c->mood.regions[0].first == "eich" && c->mood.regions[0].second.approval.fixed == 100 && c->mood.regions[0].second.order.bad == 0);
    CHECK(c->mood.regions[1].first == "imm" && c->mood.regions[1].second.neutral());
    CHECK(!c->mood.neutral());
}

TEST(control_mood_is_neutral_when_off_missing_or_malformed) {
    auto c = parseControl(R"({"version":1,"seq":5,"features":{"mood":{"enabled":false,"approval":{"fixed":100},"regions":{"eich":{"order":{"bad":0}}}}}})");
    CHECK(c.has_value() && c->mood.neutral() && c->mood.regions.empty());
    c = parseControl(R"({"version":1,"seq":6,"features":{}})");
    CHECK(c.has_value() && c->mood.neutral());
    c = parseControl(R"({"version":1,"seq":7,"features":{"mood":{"enabled":true,"approval":"x","order":[1],"regions":7}}})");
    CHECK(c.has_value() && c->mood.neutral());
    // 범위를 벗어난 값과 글자
    c = parseControl(R"({"version":1,"seq":8,"features":{"mood":{"enabled":true,"approval":{"fixed":500,"good":99,"bad":-4},"order":{"fixed":-3,"good":"x","bad":900}}}})");
    CHECK(c.has_value());
    CHECK(c->mood.common.approval.fixed == 100 && c->mood.common.approval.good == 10 && c->mood.common.approval.bad == 0);
    CHECK(c->mood.common.order.fixed == 0 && c->mood.common.order.good == 1 && c->mood.common.order.bad == 100);
}

TEST(control_mood_skips_region_entries_that_are_not_objects) {
    // Lua 와 오버레이는 객체가 아닌 영지 항목을 없는 것으로 본다(그 영지는 공통 설정을 따른다). 네이티브가 그것을
    // "따로 지정한 게임 그대로"로 읽으면 그 영지에만 공통 설정이 듣지 않는다
    auto c = parseControl(R"({"version":1,"seq":9,"features":{"mood":{"enabled":true,"approval":{"good":3},
        "regions":{"eich":7,"imm":"x","hof":{}}}}})");
    CHECK(c.has_value());
    CHECK(c->mood.regions.size() == 1 && c->mood.regions[0].first == "hof");
}

TEST(control_mood_settings_compare_equal_only_when_the_same) {
    auto a = parseControl(R"({"version":1,"seq":1,"features":{"mood":{"enabled":true,"approval":{"good":3},"regions":{"eich":{"order":{"bad":0}}}}}})");
    auto b = parseControl(R"({"version":1,"seq":2,"features":{"mood":{"enabled":true,"approval":{"good":3},"regions":{"eich":{"order":{"bad":0}}}}}})");
    auto d = parseControl(R"({"version":1,"seq":3,"features":{"mood":{"enabled":true,"approval":{"good":3},"regions":{"eich":{"order":{"bad":10}}}}}})");
    CHECK(a.has_value() && b.has_value() && d.has_value());
    CHECK(a->mood == b->mood);
    CHECK(!(a->mood == d->mood));
}
