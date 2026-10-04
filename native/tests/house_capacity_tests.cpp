#include "test.h"
#include "features/house_capacity.h"
#include "control.h"
#include <cstring>

using namespace mlt;

namespace {
template <class T> void put(uint8_t* base, std::ptrdiff_t off, T v) { std::memcpy(base + off, &v, sizeof v); }

struct FakePawn { alignas(8) uint8_t bytes[0x350]{}; };
struct FakeBuilding {
    alignas(8) uint8_t bytes[0x400]{};
    void setOwner(FakePawn* p) { put(bytes, house_capacity::kBuildingOwnerOffset, reinterpret_cast<std::uintptr_t>(p)); }
};
}

TEST(house_capacity_only_counts_buildings_of_the_main_player) {
    // 게임은 다른 영주의 집에도 같은 함수로 수용 가족 수를 구한다. 건물의 주인으로 가른다
    FakePawn me, lord;
    me.bytes[house_capacity::kPawnIsMainPlayerOffset] = 1;
    FakeBuilding b;
    b.setOwner(&me);
    CHECK(house_capacity::ownedByMainPlayer(b.bytes));
    b.setOwner(&lord);
    CHECK(!house_capacity::ownedByMainPlayer(b.bytes));
    b.setOwner(nullptr);
    CHECK(!house_capacity::ownedByMainPlayer(b.bytes));
    CHECK(!house_capacity::ownedByMainPlayer(nullptr));
}

TEST(house_capacity_multiplies_the_games_number) {
    CHECK(house_capacity::scale(1, 3) == 3);     // 1·2레벨 집
    CHECK(house_capacity::scale(2, 3) == 6);     // 확장이 있는 1·2레벨, 또는 3레벨
    CHECK(house_capacity::scale(4, 10) == 40);   // 확장이 있는 4레벨
    CHECK(house_capacity::scale(5, 2) == 10);    // 일꾼 야영지
}

TEST(house_capacity_leaves_the_games_number_when_nothing_is_set) {
    CHECK(house_capacity::scale(3, 1) == 3);
    CHECK(house_capacity::scale(3, 0) == 3);
    CHECK(house_capacity::scale(3, -2) == 3);
    CHECK(house_capacity::scale(0, 5) == 0);     // 주거 건물이 아니면 0 그대로
}

TEST(house_capacity_multiplier_stops_at_ten) {
    CHECK(house_capacity::scale(1, 50) == house_capacity::kMaxMultiplier);
}

TEST(house_capacity_registers_two_hooks_and_the_owner_check) {
    HookManager m;
    house_capacity::registerHook(m);
    auto s = m.states();
    CHECK(s.size() == 3);
    CHECK(s[0].name == "house_capacity");
    CHECK(s[1].name == "house_capacity_role");
    CHECK(s[2].name == "house_capacity_owner");
}

TEST(control_reads_the_house_capacity_multiplier_of_the_population_feature) {
    auto c = parseControl(R"({"version":1,"seq":4,"features":{"population":{"enabled":true,"houseCapacity":3}}})");
    CHECK(c.has_value() && c->houseCapacity == 3);
    c = parseControl(R"({"version":1,"seq":5,"features":{"population":{"enabled":false,"houseCapacity":3}}})");
    CHECK(c.has_value() && c->houseCapacity == 1);   // population.enabled=false
    c = parseControl(R"({"version":1,"seq":6,"features":{"population":{"enabled":true}}})");
    CHECK(c.has_value() && c->houseCapacity == 1);   // 값이 없으면 게임 그대로
    c = parseControl(R"({"version":1,"seq":7,"features":{"population":{"enabled":true,"houseCapacity":0}}})");
    CHECK(c.has_value() && c->houseCapacity == 1);
    c = parseControl(R"({"version":1,"seq":8,"features":{"population":{"enabled":true,"houseCapacity":99}}})");
    CHECK(c.has_value() && c->houseCapacity == 10);
}
