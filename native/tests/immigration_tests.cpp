#include "test.h"
#include "features/immigration.h"
#include "control.h"
#include <cstring>

using namespace mlt;

namespace {
template <class T> void put(uint8_t* base, std::ptrdiff_t off, T v) { std::memcpy(base + off, &v, sizeof v); }

struct FakePawn { alignas(8) uint8_t bytes[0x350]{}; };
struct FakeRegion {
    alignas(8) uint8_t bytes[0x400]{};
    void setOwner(FakePawn* p) { put(bytes, immigration::kRegionOwnerOffset, reinterpret_cast<std::uintptr_t>(p)); }
};
}

TEST(immigration_only_counts_regions_of_the_main_player) {
    // 게임은 다른 영주의 영지에도 같은 함수로 월간 인구 변화를 구한다. 영지의 주인으로 가른다
    FakePawn me, lord;
    me.bytes[immigration::kPawnIsMainPlayerOffset] = 1;
    FakeRegion r;
    r.setOwner(&me);
    CHECK(immigration::ownedByMainPlayer(r.bytes));
    r.setOwner(&lord);
    CHECK(!immigration::ownedByMainPlayer(r.bytes));
    r.setOwner(nullptr);                              // 주인 없는 영지
    CHECK(!immigration::ownedByMainPlayer(r.bytes));
    CHECK(!immigration::ownedByMainPlayer(nullptr));
}

TEST(immigration_monthly_families_replace_the_games_number_when_there_is_room) {
    CHECK(immigration::adjust(0, 6, 1, true) == 6);    // 지지율이 낮아 게임 값이 0 이어도 온다
    CHECK(immigration::adjust(-1, 6, 1, true) == 6);   // 떠나던 영지도 그 숫자대로 온다
    CHECK(immigration::adjust(4, 2, 1, true) == 2);    // 게임 값보다 작게 넣으면 그만큼만 온다
    CHECK(immigration::adjust(0, 99, 1, true) == immigration::kMaxPerMonth);   // 하루 한 가족이 상한
}

TEST(immigration_monthly_families_keep_the_games_number_without_living_space) {
    // 빈 주거 공간이 없으면 게임은 0 을 돌려준다. 그 규칙은 그대로 둔다(영지 창의 숫자도 0 으로 남는다)
    CHECK(immigration::adjust(0, 6, 1, false) == 0);
    CHECK(immigration::adjust(-1, 6, 1, false) == -1);
}

TEST(immigration_multiplier_scales_arrivals_only) {
    CHECK(immigration::adjust(2, 0, 3, true) == 6);
    CHECK(immigration::adjust(0, 0, 3, true) == 0);     // 게임 값이 0 이면 0
    CHECK(immigration::adjust(-3, 0, 3, true) == -3);   // 떠나는 수는 곱하지 않는다
    CHECK(immigration::adjust(4, 0, 10, true) == immigration::kMaxPerMonth);
    CHECK(immigration::adjust(1, 0, 50, true) == immigration::kMaxMultiplier);   // 배율은 10 까지
    CHECK(immigration::adjust(2, 0, 3, false) == 6);    // 배율은 게임 값에만 달렸다(게임 값에 주거 공간이 이미 반영돼 있다)
}

TEST(immigration_monthly_families_win_over_the_multiplier) {
    CHECK(immigration::adjust(2, 5, 3, true) == 5);
}

TEST(immigration_leaves_the_games_number_when_nothing_is_set) {
    CHECK(immigration::adjust(2, 0, 1, true) == 2);
    CHECK(immigration::adjust(2, -4, 0, false) == 2);
    CHECK(immigration::adjust(-2, 0, 1, true) == -2);
}

TEST(immigration_registers_the_hook_and_two_address_only_functions) {
    HookManager m;
    immigration::registerHook(m);
    auto s = m.states();
    CHECK(s.size() == 3);
    CHECK(s[0].name == "immigration");
    CHECK(s[1].name == "immigration_space");
    CHECK(s[2].name == "immigration_owner");
}

TEST(control_reads_the_immigration_settings_of_the_population_feature) {
    auto c = parseControl(R"({"version":1,"seq":4,"features":{"population":{"enabled":true,"monthlyFamilies":6,"multiplier":3}}})");
    CHECK(c.has_value() && c->immigrationMonthly == 6 && c->immigrationMultiplier == 3);
    c = parseControl(R"({"version":1,"seq":5,"features":{"population":{"enabled":false,"monthlyFamilies":6,"multiplier":3}}})");
    CHECK(c.has_value() && c->immigrationMonthly == 0 && c->immigrationMultiplier == 1);   // population.enabled=false
    c = parseControl(R"({"version":1,"seq":6,"features":{"population":{"enabled":true}}})");
    CHECK(c.has_value() && c->immigrationMonthly == 0 && c->immigrationMultiplier == 1);   // 값이 없으면 게임 그대로
    c = parseControl(R"({"version":1,"seq":7,"features":{}})");
    CHECK(c.has_value() && c->immigrationMonthly == 0 && c->immigrationMultiplier == 1);
    // 범위를 벗어난 값과 글자는 게임 그대로로 읽는다
    c = parseControl(R"({"version":1,"seq":8,"features":{"population":{"enabled":true,"monthlyFamilies":-5,"multiplier":"x"}}})");
    CHECK(c.has_value() && c->immigrationMonthly == 0 && c->immigrationMultiplier == 1);
    c = parseControl(R"({"version":1,"seq":9,"features":{"population":{"enabled":true,"monthlyFamilies":500,"multiplier":99}}})");
    CHECK(c.has_value() && c->immigrationMonthly == 31 && c->immigrationMultiplier == 10);
}
