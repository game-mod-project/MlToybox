#include "test.h"
#include "features/upgrade_scope.h"
#include "control.h"
#include <cstring>

using namespace mlt;

namespace {
template <class T> void put(uint8_t* base, std::ptrdiff_t off, T v) { std::memcpy(base + off, &v, sizeof v); }
template <class T> T get(const uint8_t* base, std::ptrdiff_t off) { T v; std::memcpy(&v, base + off, sizeof v); return v; }

struct FakePawn { alignas(8) uint8_t bytes[0x350]{}; };
struct FakeBuilding {
    alignas(8) uint8_t bytes[0x400]{};
    void setOwner(FakePawn* p) { put(bytes, upgrade_scope::kBuildingOwnerOffset, reinterpret_cast<std::uintptr_t>(p)); }
};
// 업그레이드 표의 행(FUpgrade): 자재 2종, 지역 자산 25, 금고 10, 정착지 레벨 2, 집 레벨 2. 화면(블루프린트)만 읽는 값도 넣어 둔다
struct FakeRow {
    alignas(8) uint8_t bytes[0x138]{};
    FakeRow() {
        put<int32_t>(bytes, upgrade_scope::kRowCostNumOffset, 2);
        put<int32_t>(bytes, upgrade_scope::kRowRegionalWealthOffset, 25);
        put<int32_t>(bytes, upgrade_scope::kRowTreasuryOffset, 10);
        put<int32_t>(bytes, upgrade_scope::kRowMinSettlementLevelOffset, 2);
        bytes[upgrade_scope::kRowMinHouseLvOffset] = 2;
        put<int32_t>(bytes, 0x60, 1);    // requiresBuilding 의 Num
        bytes[0x7C] = 3;                 // minimumProsperity
        bytes[0x7F] = 1;                 // bUseOnlyOnce
    }
    bool original() const {
        return get<int32_t>(bytes, upgrade_scope::kRowCostNumOffset) == 2 && get<int32_t>(bytes, upgrade_scope::kRowRegionalWealthOffset) == 25
            && get<int32_t>(bytes, upgrade_scope::kRowTreasuryOffset) == 10 && get<int32_t>(bytes, upgrade_scope::kRowMinSettlementLevelOffset) == 2
            && bytes[upgrade_scope::kRowMinHouseLvOffset] == 2;
    }
    bool relaxed() const {
        return get<int32_t>(bytes, upgrade_scope::kRowCostNumOffset) == 0 && get<int32_t>(bytes, upgrade_scope::kRowRegionalWealthOffset) == 0
            && get<int32_t>(bytes, upgrade_scope::kRowTreasuryOffset) == 0 && get<int32_t>(bytes, upgrade_scope::kRowMinSettlementLevelOffset) == 0
            && bytes[upgrade_scope::kRowMinHouseLvOffset] == 0;
    }
    bool othersUntouched() const { return get<int32_t>(bytes, 0x60) == 1 && bytes[0x7C] == 3 && bytes[0x7F] == 1; }
};
}

TEST(upgrade_scope_only_counts_buildings_of_the_main_player) {
    // AI 영주도 같은 판정·비용 함수를 부른다(AI 작업 실행 함수가 canUpgrade 뒤에 useUpgrade 를 부른다). 주인으로 가른다
    FakePawn me, lord;
    me.bytes[upgrade_scope::kPawnIsMainPlayerOffset] = 1;
    FakeBuilding b;
    b.setOwner(&me);
    CHECK(upgrade_scope::ownedByMainPlayer(b.bytes));
    b.setOwner(&lord);
    CHECK(!upgrade_scope::ownedByMainPlayer(b.bytes));
    b.setOwner(nullptr);
    CHECK(!upgrade_scope::ownedByMainPlayer(b.bytes));
    CHECK(!upgrade_scope::ownedByMainPlayer(nullptr));
}

TEST(upgrade_relax_row_zeroes_what_the_game_code_reads_and_restore_puts_it_back) {
    FakeRow r;
    auto edit = upgrade_scope::relaxRow(r.bytes);
    CHECK(r.relaxed());
    CHECK(r.othersUntouched());      // 화면만 읽는 값과 한 번만 쓰는 업그레이드 표시는 건드리지 않는다
    upgrade_scope::restoreRow(edit);
    CHECK(r.original());
    CHECK(r.othersUntouched());
}

TEST(upgrade_relax_row_nests) {
    // 판정 함수가 안에서 비용 함수를 부른다. 안쪽이 되돌려도 바깥이 끝날 때까지 0 이어야 하고, 끝나면 원래 값이어야 한다
    FakeRow r;
    auto outer = upgrade_scope::relaxRow(r.bytes);
    auto inner = upgrade_scope::relaxRow(r.bytes);
    upgrade_scope::restoreRow(inner);
    CHECK(r.relaxed());
    upgrade_scope::restoreRow(outer);
    CHECK(r.original());
}

TEST(upgrade_relax_row_ignores_a_missing_row) {
    auto edit = upgrade_scope::relaxRow(nullptr);
    upgrade_scope::restoreRow(edit);
    upgrade_scope::restoreRow(upgrade_scope::RowEdit{});
}

TEST(upgrade_scope_registers_the_row_function_and_five_hooks) {
    HookManager m;
    upgrade_scope::registerHook(m);
    auto s = m.states();
    CHECK(s.size() == 6);
    CHECK(s[0].name == "upgrade_row");
    CHECK(s[1].name == "upgrade_can");
    CHECK(s[2].name == "upgrade_pay");
    CHECK(s[3].name == "upgrade_cost");
    CHECK(s[4].name == "upgrade_wealth");
    CHECK(s[5].name == "upgrade_residential");
}

TEST(control_reads_the_upgrade_feature) {
    auto c = parseControl(R"({"version":1,"seq":4,"features":{"upgrade":{"enabled":true}}})");
    CHECK(c.has_value() && c->upgradeFree);
    c = parseControl(R"({"version":1,"seq":5,"features":{"upgrade":{"enabled":false}}})");
    CHECK(c.has_value() && !c->upgradeFree);
    c = parseControl(R"({"version":1,"seq":6,"features":{}})");
    CHECK(c.has_value() && !c->upgradeFree);
}
