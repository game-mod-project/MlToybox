#include "test.h"
#include "features/region_scope.h"
#include <cstring>

using namespace mlt;

namespace {
template <class T> void put(uint8_t* base, std::ptrdiff_t off, T v) { std::memcpy(base + off, &v, sizeof v); }
struct FakePawn { alignas(8) uint8_t bytes[0x350]{}; };
struct FakeRegion { alignas(8) uint8_t bytes[0x400]{}; };
}

TEST(region_scope_only_regions_of_the_main_player_count_as_mine) {
    FakePawn me, lord;
    me.bytes[region_scope::kPawnIsMainPlayerOffset] = 1;
    FakeRegion r;
    put(r.bytes, region_scope::kRegionOwnerOffset, reinterpret_cast<std::uintptr_t>(&me));
    CHECK(region_scope::ownedByMainPlayer(r.bytes));
    put(r.bytes, region_scope::kRegionOwnerOffset, reinterpret_cast<std::uintptr_t>(&lord));
    CHECK(!region_scope::ownedByMainPlayer(r.bytes));
    put(r.bytes, region_scope::kRegionOwnerOffset, std::uintptr_t{ 0 });
    CHECK(!region_scope::ownedByMainPlayer(r.bytes));
    CHECK(!region_scope::ownedByMainPlayer(nullptr));
}

TEST(region_scope_refuses_name_values_that_cannot_be_names) {
    // 레이아웃이 바뀌어 다른 필드를 태그로 읽었을 때, 그 값을 게임의 이름 표 함수에 넘기지 않는다
    CHECK(!region_scope::plausibleName(0));                          // None
    CHECK(region_scope::plausibleName(0x000199E60));                 // 이름 표의 번호
    CHECK(region_scope::plausibleName(0x0000000300019E60ull));       // 숫자 꼬리가 붙은 이름
    CHECK(!region_scope::plausibleName(0x0000020E9D490010ull));      // 포인터의 아래 32비트(0x9D490010)는 블록 범위 밖
    CHECK(!region_scope::plausibleName(0xFFFFFFFFull));
}

TEST(region_scope_cannot_tell_regions_until_both_addresses_are_found) {
    // 테스트 프로세스에는 게임 코드가 없다: 이름 비교 함수도 태그 자리 확인도 찾지 못한 상태다
    FakeRegion r;
    put(r.bytes, region_scope::kRegionTagOffset, std::uint64_t{ 0x199E60 });
    CHECK(!region_scope::canTellRegions());
    CHECK(!region_scope::tagIs(r.bytes, "eich"));   // 게임 함수를 부르지 않고 "아니다"
    CHECK(!region_scope::tagIs(nullptr, "eich"));
}

TEST(region_scope_registers_two_address_only_checks) {
    HookManager m;
    region_scope::registerHook(m);
    auto s = m.states();
    CHECK(s.size() == 2);
    CHECK(s[0].name == "region_name");
    CHECK(s[1].name == "region_tag");
}
