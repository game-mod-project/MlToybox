#include "test.h"
#include "features/militia_guard.h"
#include <cstring>
#include <vector>

using namespace mlt;

namespace {
template <class T> void put(uint8_t* base, std::ptrdiff_t off, T v) { std::memcpy(base + off, &v, sizeof v); }
void putPtr(uint8_t* base, std::ptrdiff_t off, const void* p) { put(base, off, reinterpret_cast<std::uintptr_t>(p)); }

struct FakeUnit { alignas(8) uint8_t bytes[0x350]{}; void setHome(const void* h) { putPtr(bytes, militia_guard::kUnitHomeOffset, h); } };
struct FakeRegion { alignas(8) uint8_t bytes[0x800]{}; };

// 폰(commandedSquads, masterPtr) → 엔진(squads) → FSquad(type, originRegion, assignedRecruits) → 유닛(Home)
struct World {
    alignas(8) uint8_t pawn[0xA30]{};
    alignas(8) uint8_t engine[0x5C0]{};
    std::vector<uint8_t> squads;
    std::vector<int32_t> commanded;
    std::vector<std::vector<void*>> recruits;

    explicit World(int numSquads) : squads(static_cast<size_t>(numSquads) * militia_guard::kSquadSize), recruits(numSquads) {
        putPtr(pawn, militia_guard::kPawnEngineOffset, engine);
        putPtr(engine, militia_guard::kEngineSquadsOffset, squads.data());
        put<int32_t>(engine, militia_guard::kEngineSquadsNumOffset, numSquads);
    }
    uint8_t* squad(int i) { return squads.data() + static_cast<size_t>(i) * militia_guard::kSquadSize; }
    void setSquad(int i, uint8_t type, const void* origin, std::vector<void*> units) {
        recruits[i] = std::move(units);
        put<uint8_t>(squad(i), militia_guard::kSquadTypeOffset, type);
        putPtr(squad(i), militia_guard::kSquadOriginOffset, origin);
        putPtr(squad(i), militia_guard::kSquadRecruitsOffset, recruits[i].data());
        put<int32_t>(squad(i), militia_guard::kSquadRecruitsNumOffset, static_cast<int32_t>(recruits[i].size()));
    }
    void command(std::vector<int32_t> ids) {
        commanded = std::move(ids);
        putPtr(pawn, militia_guard::kPawnCommandedOffset, commanded.data());
        put<int32_t>(pawn, militia_guard::kPawnCommandedNumOffset, static_cast<int32_t>(commanded.size()));
    }
};
}

TEST(militia_guard_safe_when_all_recruits_have_home) {
    FakeRegion region, house;
    FakeUnit a, b; a.setHome(&house); b.setHome(&house);
    World w(2);
    w.setSquad(1, militia_guard::kMilitiaType, &region, { &a, &b });
    w.command({ 1 });
    CHECK(militia_guard::isSafe(w.pawn, region.bytes));
}

TEST(militia_guard_unsafe_when_militia_recruit_home_is_null) {
    // 크래시 재현 상태: 지역 민병대 가족이 집을 잠깐 비운 사이 (unit.Home == null)
    FakeRegion region, house;
    FakeUnit a, homeless; a.setHome(&house);
    World w(2);
    w.setSquad(1, militia_guard::kMilitiaType, &region, { &a, &homeless });
    w.command({ 1 });
    CHECK(!militia_guard::isSafe(w.pawn, region.bytes));
}

TEST(militia_guard_unsafe_when_recruit_pointer_is_null) {
    FakeRegion region;
    World w(1);
    w.setSquad(0, militia_guard::kMilitiaType, &region, { nullptr });
    w.command({ 0 });
    CHECK(!militia_guard::isSafe(w.pawn, region.bytes));
}

TEST(militia_guard_ignores_squads_the_original_skips) {
    // 원본은 민병대(type 1)이면서 originRegion == 이 지역인 분대만 순회한다
    FakeRegion region, other;
    FakeUnit homeless;
    World w(3);
    w.setSquad(0, 2, &region, { &homeless });                        // 용병
    w.setSquad(1, militia_guard::kMilitiaType, &other, { &homeless }); // 다른 지역 민병대
    w.setSquad(2, 0, nullptr, { &homeless });                         // 생성 분대
    w.command({ 0, 1, 2 });
    CHECK(militia_guard::isSafe(w.pawn, region.bytes));
}

TEST(militia_guard_unsafe_on_broken_layout) {
    FakeRegion region;
    World w(1);
    w.setSquad(0, 0, nullptr, {});
    w.command({ 5 });                                                 // 분대 배열 범위 밖 인덱스
    CHECK(!militia_guard::isSafe(w.pawn, region.bytes));
    CHECK(!militia_guard::isSafe(nullptr, region.bytes));
    putPtr(w.pawn, militia_guard::kPawnEngineOffset, nullptr);
    w.command({ 0 });
    CHECK(!militia_guard::isSafe(w.pawn, region.bytes));
}

TEST(militia_guard_safe_with_no_commanded_squads) {
    FakeRegion region;
    World w(1);
    w.command({});
    CHECK(militia_guard::isSafe(w.pawn, region.bytes));
}
