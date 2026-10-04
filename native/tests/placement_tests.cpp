#include "test.h"
#include "features/placement.h"
#include <cstring>

using namespace mlt;

namespace {
struct FakePawn { alignas(8) uint8_t bytes[0x1100]{}; };
// 건물 표의 행(FStat). 지역당 개수 제한 1, 건설 자재 2종(실측: 수비용 탑 등 9개 행이 제한 1)
struct FakeRow {
    alignas(8) uint8_t bytes[0x390]{};
    FakeRow() { set(placement::kRowMaxInRegionOffset, 1); set(placement::kRowGoodsNumOffset, 2); }
    void set(std::ptrdiff_t off, int32_t v) { std::memcpy(bytes + off, &v, sizeof v); }
    int32_t get(std::ptrdiff_t off) const { int32_t v; std::memcpy(&v, bytes + off, sizeof v); return v; }
    int32_t limit() const { return get(placement::kRowMaxInRegionOffset); }
    int32_t goods() const { return get(placement::kRowGoodsNumOffset); }
};
void setPlacing(FakePawn& p, int32_t type) { std::memcpy(p.bytes + placement::kPlaceBuildingOffset, &type, sizeof type); }
}

TEST(relax_row_lifts_the_region_limit_and_restore_puts_it_back) {
    FakeRow r;
    auto edit = placement::relaxRow(r.bytes, true, false);
    CHECK(r.limit() == 0 && r.goods() == 2);          // 자재는 그대로
    placement::restoreRow(edit);
    CHECK(r.limit() == 1 && r.goods() == 2);
}

TEST(relax_row_hides_the_construction_goods_and_restore_puts_them_back) {
    // 배열은 개수만 0 으로 둔다. 내용은 그대로라 되돌리면 AI 가 읽는 값이 원래와 같다
    FakeRow r;
    auto edit = placement::relaxRow(r.bytes, false, true);
    CHECK(r.limit() == 1 && r.goods() == 0);
    placement::restoreRow(edit);
    CHECK(r.limit() == 1 && r.goods() == 2);
}

TEST(relax_row_with_both_options_and_with_none) {
    FakeRow r;
    auto edit = placement::relaxRow(r.bytes, true, true);
    CHECK(r.limit() == 0 && r.goods() == 0);
    placement::restoreRow(edit);
    CHECK(r.limit() == 1 && r.goods() == 2);
    edit = placement::relaxRow(r.bytes, false, false);
    CHECK(r.limit() == 1 && r.goods() == 2);
    placement::restoreRow(edit);
    CHECK(r.limit() == 1 && r.goods() == 2);
}

TEST(relax_row_ignores_a_missing_row) {
    auto edit = placement::relaxRow(nullptr, true, true);
    placement::restoreRow(edit);                       // 튕기지 않는다
    placement::restoreRow(placement::RowEdit{});
}

TEST(only_the_main_player_placing_a_building_counts) {
    FakePawn p;
    setPlacing(p, 57);
    p.bytes[placement::kPawnIsMainPlayerOffset] = 1;
    CHECK(placement::isPlayerPlacing(p.bytes));
    p.bytes[placement::kPawnIsAiOffset] = 1;           // 다른 영주
    CHECK(!placement::isPlayerPlacing(p.bytes));
    p.bytes[placement::kPawnIsAiOffset] = 0;
    p.bytes[placement::kPawnIsMainPlayerOffset] = 0;
    CHECK(!placement::isPlayerPlacing(p.bytes));
    p.bytes[placement::kPawnIsMainPlayerOffset] = 1;
    setPlacing(p, 0);                                  // 배치 중이 아니다
    CHECK(!placement::isPlayerPlacing(p.bytes));
    CHECK(!placement::isPlayerPlacing(nullptr));
}

TEST(placement_registers_the_hook_the_row_function_and_the_row_layout) {
    HookManager m;
    placement::registerHook(m);
    auto s = m.states();
    CHECK(s.size() == 3);
    CHECK(s[0].name == "placement");
    CHECK(s[1].name == "building_row");
    CHECK(s[2].name == "placement_rows");
}

TEST(placement_clears_invalid_flag_inside_borders) {
    FakePawn p;
    p.bytes[placement::kInsideBordersOffset] = 1;
    p.bytes[placement::kInvalidFlagOffset] = 1;
    CHECK(placement::allowPlacement(p.bytes) == true);
    CHECK(p.bytes[placement::kInvalidFlagOffset] == 0);
}

TEST(placement_keeps_flag_outside_borders) {
    FakePawn p;
    p.bytes[placement::kInsideBordersOffset] = 0;
    p.bytes[placement::kInvalidFlagOffset] = 1;
    CHECK(placement::allowPlacement(p.bytes) == false);
    CHECK(p.bytes[placement::kInvalidFlagOffset] == 1);
}

// 다른 영주의 폰도 이 후킹을 지난다(원본은 AI 폰이면 바로 돌아간다). 그 폰의 판정은 건드리지 않는다
TEST(placement_leaves_the_pawn_of_another_lord_alone) {
    FakePawn p;
    p.bytes[placement::kInsideBordersOffset] = 1;
    p.bytes[placement::kPawnIsAiOffset] = 1;
    p.bytes[placement::kInvalidFlagOffset] = 1;
    CHECK(placement::allowPlacement(p.bytes) == false);
    CHECK(p.bytes[placement::kInvalidFlagOffset] == 1);
}

TEST(placement_ignores_null) {
    CHECK(placement::allowPlacement(nullptr) == false);
}

TEST(placement_keeps_flag_in_road_or_wall_mode) {
    // 실측 크래시(2026-09-30, reading 0x738): 도로/성벽 배치 함수는 같은 플래그(+0x60C)로 "지점이 어느 영지에도 없음"을 표시하고
    // 그 뒤 처리를 건너뛴다. 지우면 null 영지를 읽다가 튕긴다. 도로 모드에서는 건드리지 않는다
    FakePawn p;
    p.bytes[placement::kInsideBordersOffset] = 1;
    p.bytes[placement::kRoadModeOffset] = 1;
    p.bytes[placement::kInvalidFlagOffset] = 1;
    CHECK(placement::allowPlacement(p.bytes) == false);
    CHECK(p.bytes[placement::kInvalidFlagOffset] == 1);
}
