#include "test.h"
#include "features/instant_build.h"
#include <cstring>
#include <vector>

using namespace mlt;

namespace {
struct FakePart { alignas(8) uint8_t bytes[0x320]{}; };
void setF(uint8_t* base, std::ptrdiff_t off, float v) { std::memcpy(base + off, &v, sizeof v); }
float getF(const uint8_t* base, std::ptrdiff_t off) { float v; std::memcpy(&v, base + off, sizeof v); return v; }

struct FakeMaster {
    alignas(8) uint8_t bytes[0x400]{};
    void setParts(void** data, int32_t num) {
        const auto address = reinterpret_cast<std::uintptr_t>(data);   // TArray.Data 필드에 포인터 값을 기록
        std::memcpy(bytes + instant_build::kPartsOffset, &address, sizeof address);
        std::memcpy(bytes + instant_build::kPartsNumOffset, &num, sizeof num);
    }
};
}

TEST(complete_parts_fills_hp_to_max) {
    FakePart a, b;
    setF(a.bytes, instant_build::kPartHpOffset, 10.f);  setF(a.bytes, instant_build::kPartMaxHpOffset, 100.f);
    setF(b.bytes, instant_build::kPartHpOffset, 100.f); setF(b.bytes, instant_build::kPartMaxHpOffset, 50.f);
    void* parts[] = { &a, nullptr, &b };
    FakeMaster m; m.setParts(parts, 3);
    int changed = instant_build::completeParts(m.bytes);
    CHECK(changed == 1);
    CHECK(getF(a.bytes, instant_build::kPartHpOffset) == 100.f);
    CHECK(getF(b.bytes, instant_build::kPartHpOffset) == 100.f);   // 이미 max 이상이면 그대로
}

namespace {
struct FakePawn { alignas(8) uint8_t bytes[0x350]{}; };
// 완공 직전의 공사 현장: 파츠 hp 가 가득 찼고, 낼 자재가 없고, 주인은 플레이어다(실측: saveGame_8 의 Lei 영지 미완공 건물)
struct Site {
    FakePart a, b;
    void* parts[2]{ &a, &b };
    FakeMaster m;
    FakePawn owner;
    Site() {
        setF(a.bytes, instant_build::kPartHpOffset, 100.f); setF(a.bytes, instant_build::kPartMaxHpOffset, 100.f);
        setF(b.bytes, instant_build::kPartHpOffset, 5.f);   setF(b.bytes, instant_build::kPartMaxHpOffset, 5.f);
        m.setParts(parts, 2);
        owner.bytes[instant_build::kPawnIsMainPlayerOffset] = 1;
        setOwner(&owner);
    }
    void setOwner(FakePawn* p) {
        const auto address = reinterpret_cast<std::uintptr_t>(p);
        std::memcpy(m.bytes + instant_build::kOwnerPawnOffset, &address, sizeof address);
    }
    void setGoodsNum(int32_t n) { std::memcpy(m.bytes + instant_build::kGoodsNumOffset, &n, sizeof n); }
};
}

TEST(ready_to_finish_when_parts_are_full_no_goods_are_due_and_the_main_player_owns_it) {
    Site s;
    CHECK(instant_build::readyToFinish(s.m.bytes));
}

TEST(not_ready_when_already_constructed) {
    Site s;
    s.m.bytes[instant_build::kConstructedOffset] = 1;
    CHECK(!instant_build::readyToFinish(s.m.bytes));
}

TEST(not_ready_while_a_part_is_below_its_max_hp) {
    Site s;
    setF(s.b.bytes, instant_build::kPartHpOffset, 4.f);
    CHECK(!instant_build::readyToFinish(s.m.bytes));
}

TEST(not_ready_while_construction_goods_are_due) {
    // 게임도 자재가 다 들어온 뒤에만 완공 처리한다. 자재 목록은 Lua 가 비운다
    Site s;
    s.setGoodsNum(1);
    CHECK(!instant_build::readyToFinish(s.m.bytes));
}

TEST(not_ready_for_another_lords_building_or_one_without_an_owner) {
    // 게임이 스스로 진행도를 읽을 때(화면 표시)도 detour 를 지난다. AI 영주의 건물은 완공 처리하지 않는다
    Site s;
    s.owner.bytes[instant_build::kPawnIsMainPlayerOffset] = 0;
    CHECK(!instant_build::readyToFinish(s.m.bytes));
    s.setOwner(nullptr);
    CHECK(!instant_build::readyToFinish(s.m.bytes));
}

TEST(not_ready_without_parts_or_with_bad_part_data) {
    CHECK(!instant_build::readyToFinish(nullptr));
    Site s;
    s.m.setParts(s.parts, 0);                     // 막 생긴 건물: 파츠가 아직 없다
    CHECK(!instant_build::readyToFinish(s.m.bytes));
    s.m.setParts(nullptr, 2);
    CHECK(!instant_build::readyToFinish(s.m.bytes));
    s.m.setParts(s.parts, 100000);
    CHECK(!instant_build::readyToFinish(s.m.bytes));
    void* withNull[] = { &s.a, nullptr };
    s.m.setParts(withNull, 2);
    CHECK(!instant_build::readyToFinish(s.m.bytes));
}

TEST(fill_leaves_another_lords_building_alone) {
    // 게임이 스스로 진행도를 읽을 때도 detour 를 지난다. 다른 영주의 공사 현장은 hp 를 채우지 않는다
    Site s;
    setF(s.a.bytes, instant_build::kPartHpOffset, 10.f);
    s.owner.bytes[instant_build::kPawnIsMainPlayerOffset] = 0;
    CHECK(instant_build::fillParts(s.m.bytes, true) == 0);
    CHECK(getF(s.a.bytes, instant_build::kPartHpOffset) == 10.f);
    s.setOwner(nullptr);
    CHECK(instant_build::fillParts(s.m.bytes, true) == 0);
    CHECK(getF(s.a.bytes, instant_build::kPartHpOffset) == 10.f);
}

TEST(fill_completes_the_main_players_building) {
    Site s;
    setF(s.a.bytes, instant_build::kPartHpOffset, 10.f);
    CHECK(instant_build::fillParts(s.m.bytes, true) == 1);
    CHECK(getF(s.a.bytes, instant_build::kPartHpOffset) == 100.f);
}

TEST(fill_does_not_read_the_owner_when_its_offsets_are_not_verified) {
    // 주인 오프셋은 완공 처리 함수의 본문으로 확인한다. 그 함수를 찾지 못했으면 주인을 읽지 않고 이전처럼 채운다
    Site s;
    setF(s.a.bytes, instant_build::kPartHpOffset, 10.f);
    const auto garbage = static_cast<std::uintptr_t>(0x10);            // 읽으면 튕길 주소
    std::memcpy(s.m.bytes + instant_build::kOwnerPawnOffset, &garbage, sizeof garbage);
    CHECK(instant_build::fillParts(s.m.bytes, false) == 1);
    CHECK(getF(s.a.bytes, instant_build::kPartHpOffset) == 100.f);
}

TEST(instant_build_registers_the_hook_the_finish_function_and_the_two_setup_hooks) {
    HookManager m;
    instant_build::registerHook(m);
    auto s = m.states();
    CHECK(s.size() == 4);
    CHECK(s[0].name == "instant_build");
    CHECK(s[1].name == "instant_finish");
    CHECK(s[2].name == "instant_setup");
    CHECK(s[3].name == "instant_convert");
}

namespace {
// 엔진의 디버그 플래그 목록(TArray<FName>): 개수만 본다
struct FakeEngine {
    alignas(8) uint8_t bytes[0x10D0]{};
    void setFlags(int32_t n) { std::memcpy(bytes + instant_build::kEngineFlagsNumOffset, &n, sizeof n); }
    int32_t flags() const { int32_t n; std::memcpy(&n, bytes + instant_build::kEngineFlagsNumOffset, sizeof n); return n; }
};
}

TEST(another_lords_building_is_one_whose_owner_is_set_and_is_not_the_main_player) {
    // 플래그(instaBuild)는 게임 전체에 하나라, 내가 배치하는 동안 AI 영주가 놓는 건물도 완공 상태로 생긴다. 주인으로 가른다
    Site s;
    CHECK(!instant_build::ownedByAnotherLord(s.m.bytes));        // 내 건물
    s.owner.bytes[instant_build::kPawnIsMainPlayerOffset] = 0;
    CHECK(instant_build::ownedByAnotherLord(s.m.bytes));
    s.setOwner(nullptr);
    CHECK(!instant_build::ownedByAnotherLord(s.m.bytes));        // 주인이 아직 없으면 건드리지 않는다
    CHECK(!instant_build::ownedByAnotherLord(nullptr));
}

TEST(hide_flags_empties_the_list_and_restore_puts_the_count_back) {
    FakeEngine e;
    e.setFlags(2);
    const int32_t saved = instant_build::hideFlags(e.bytes);
    CHECK(saved == 2 && e.flags() == 0);
    instant_build::restoreFlags(e.bytes, saved);
    CHECK(e.flags() == 2);
}

TEST(hide_flags_changes_nothing_when_the_list_is_empty_or_already_hidden) {
    // 플래그가 없을 때(대부분의 시간)와, 안쪽 호출이 겹쳤을 때: 바꾸지 않았으면 되돌리지도 않는다
    FakeEngine e;
    const int32_t outer = instant_build::hideFlags(e.bytes);
    CHECK(outer < 0 && e.flags() == 0);
    e.setFlags(1);
    const int32_t first = instant_build::hideFlags(e.bytes);
    const int32_t nested = instant_build::hideFlags(e.bytes);
    CHECK(first == 1 && nested < 0);
    instant_build::restoreFlags(e.bytes, nested);
    CHECK(e.flags() == 0);                                       // 안쪽이 끝나도 바깥이 끝날 때까지 숨겨져 있다
    instant_build::restoreFlags(e.bytes, first);
    CHECK(e.flags() == 1);
    CHECK(instant_build::hideFlags(nullptr) < 0);
    instant_build::restoreFlags(nullptr, 3);
}

TEST(complete_parts_ignores_null_and_bad_counts) {
    FakeMaster m;
    CHECK(instant_build::completeParts(nullptr) == 0);
    m.setParts(nullptr, 5);
    CHECK(instant_build::completeParts(m.bytes) == 0);
    void* parts[] = { nullptr };
    m.setParts(parts, -1);
    CHECK(instant_build::completeParts(m.bytes) == 0);
    m.setParts(parts, 100000);                                      // 비정상적으로 큰 Num 은 거부
    CHECK(instant_build::completeParts(m.bytes) == 0);
}
