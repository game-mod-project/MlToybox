#include "test.h"
#include "features/deposits.h"
#include "control.h"
#include "json.h"
#include "status.h"
#include <cstring>
#include <vector>

using namespace mlt;

namespace {
template <class T> void put(uint8_t* base, std::ptrdiff_t off, T v) { std::memcpy(base + off, &v, sizeof v); }
template <class T> T get(const uint8_t* base, std::ptrdiff_t off) { T v; std::memcpy(&v, base + off, sizeof v); return v; }

struct FakePawn { alignas(8) uint8_t bytes[0x350]{}; };
struct FakeRegion {
    alignas(8) uint8_t bytes[0x400]{};
    std::string key;
    FakeRegion(std::string k, FakePawn* owner) : key(std::move(k)) { put(bytes, deposits::kRegionOwnerOffset, reinterpret_cast<std::uintptr_t>(owner)); }
    std::uintptr_t address() const { return reinterpret_cast<std::uintptr_t>(bytes); }
};
struct FakeNode {
    alignas(8) uint8_t bytes[0x318]{};
    FakeNode(int32_t type, FakeRegion* region, int32_t amount, int32_t squad = -1, int32_t clumps = 0) {
        put(bytes, deposits::kNodeTypeOffset, type);
        put(bytes, deposits::kNodeRegionOffset, reinterpret_cast<std::uintptr_t>(region ? region->bytes : nullptr));
        put(bytes, deposits::kNodeSquadOffset, squad);
        put(bytes, deposits::kNodeClumpCountOffset, clumps);
        put(bytes, deposits::kNodeAmountOffset, amount);
    }
    int32_t amount() const { return get<int32_t>(bytes, deposits::kNodeAmountOffset); }
};
struct FakeEngine {
    alignas(8) uint8_t bytes[0x800]{};
    std::vector<std::uintptr_t> list;
    void setNodes(std::initializer_list<FakeNode*> nodes) {
        list.clear();
        for (FakeNode* n : nodes) list.push_back(reinterpret_cast<std::uintptr_t>(n ? n->bytes : nullptr));
        put(bytes, deposits::kEngineNodesOffset, reinterpret_cast<std::uintptr_t>(list.empty() ? nullptr : list.data()));
        put(bytes, deposits::kEngineNodesOffset + 8, static_cast<int32_t>(list.size()));
    }
};

// 테스트에서는 영지의 키를 가짜 영지에 적어 두고 견준다(게임에서는 영지의 태그를 게임의 이름 비교 함수로 견준다)
std::vector<FakeRegion*> g_regions;
bool isRegion(const uint8_t* region, const std::string& key) {
    for (FakeRegion* r : g_regions) if (r->bytes == region) return r->key == key;
    return false;
}
}

TEST(deposits_minerals_are_salt_iron_and_clay) {
    CHECK(deposits::isMineral(deposits::kSalt) && deposits::isMineral(deposits::kIron) && deposits::isMineral(deposits::kClay));
    CHECK(!deposits::isMineral(0) && !deposits::isMineral(4) && !deposits::isMineral(7) && !deposits::isMineral(9));   // 사슴, 돌, 버섯은 여기서 다루지 않는다
}

TEST(deposits_top_up_only_raises_an_amount_below_the_target) {
    CHECK(deposits::topUp(25, 500) == 500);
    CHECK(deposits::topUp(1154, 500) == 1154);   // 이미 많으면 그대로
    CHECK(deposits::topUp(25, 0) == 25);         // 목표 없음
    CHECK(deposits::topUp(25, -3) == 25);
    CHECK(deposits::topUp(25, 9000000) == deposits::kMaxTarget);
}

TEST(deposits_region_target_wins_per_kind_and_falls_back_to_the_common_one) {
    MineralTargets common{ 100, 200, 300 };
    MineralTargets own{ -1, 5000, 0 };   // 소금은 정하지 않음, 철 5000, 점토는 이 영지에서 끔
    CHECK(deposits::targetOf(common, nullptr, deposits::kIron) == 200);
    CHECK(deposits::targetOf(common, &own, deposits::kSalt) == 100);
    CHECK(deposits::targetOf(common, &own, deposits::kIron) == 5000);
    CHECK(deposits::targetOf(common, &own, deposits::kClay) == 0);
    CHECK(deposits::targetOf(common, &own, 7) == 0);
}

TEST(deposits_fill_the_mineral_nodes_of_my_regions_and_report_them) {
    FakePawn me, lord;
    me.bytes[deposits::kPawnIsMainPlayerOffset] = 1;
    FakeRegion eich("eich", &me), imm("imm", &me), gold("gold", &lord), wild("none", nullptr);
    g_regions = { &eich, &imm, &gold, &wild };
    FakeNode clay(deposits::kClay, &eich, 25), salt(deposits::kSalt, &eich, 119), iron(deposits::kIron, &imm, 118);
    FakeNode aiIron(deposits::kIron, &gold, 40), freeIron(deposits::kIron, &wild, 40), orphan(deposits::kIron, nullptr, 40);
    FakeNode mushrooms(9, &eich, 0, -1, 16), game(10, &eich, 0, 3, 0);
    FakeEngine engine;
    engine.setNodes({ &clay, &salt, &iron, &aiIron, &freeIron, &orphan, &mushrooms, &game, nullptr });

    RegionControl c;
    c.enabled = true;
    c.common = MineralTargets{ 0, 1000, 500 };
    c.regions.push_back({ "imm", MineralTargets{ -1, 3000, -1 } });
    std::vector<deposits::NodeInfo> out;
    CHECK(deposits::process(engine.bytes, c, &isRegion, out));
    CHECK(clay.amount() == 500);       // 공통 목표
    CHECK(salt.amount() == 119);       // 소금은 목표가 없다
    CHECK(iron.amount() == 3000);      // 영지 목표가 공통을 이긴다
    CHECK(aiIron.amount() == 40);      // 다른 영주의 영지
    CHECK(freeIron.amount() == 40);    // 주인 없는 영지
    CHECK(orphan.amount() == 40);
    CHECK(mushrooms.amount() == 0 && game.amount() == 0);
    // 내 영지의 광물 매장지 셋만, 채운 뒤의 값으로
    CHECK(out.size() == 3);
    CHECK((out[0] == deposits::NodeInfo{ eich.address(), deposits::kClay, 500 }));
    CHECK((out[1] == deposits::NodeInfo{ eich.address(), deposits::kSalt, 119 }));
    CHECK((out[2] == deposits::NodeInfo{ imm.address(), deposits::kIron, 3000 }));
}

TEST(deposits_without_the_name_function_use_the_common_targets) {
    FakePawn me;
    me.bytes[deposits::kPawnIsMainPlayerOffset] = 1;
    FakeRegion imm("imm", &me);
    FakeNode iron(deposits::kIron, &imm, 118);
    FakeEngine engine;
    engine.setNodes({ &iron });
    RegionControl c;
    c.enabled = true;
    c.common.iron = 1000;
    c.regions.push_back({ "imm", MineralTargets{ -1, 3000, -1 } });
    std::vector<deposits::NodeInfo> out;
    CHECK(deposits::process(engine.bytes, c, nullptr, out));
    CHECK(iron.amount() == 1000);
}

TEST(deposits_leave_a_node_alone_when_it_holds_clumps_or_a_herd) {
    // 게임은 덩어리가 없고 무리가 아닌 매장지에만 남은 양을 둔다(세이브를 불러올 때의 조건과 같다)
    FakePawn me;
    me.bytes[deposits::kPawnIsMainPlayerOffset] = 1;
    FakeRegion eich("eich", &me);
    FakeNode withClumps(deposits::kIron, &eich, 7, -1, 4), withHerd(deposits::kIron, &eich, 7, 2, 0);
    FakeEngine engine;
    engine.setNodes({ &withClumps, &withHerd });
    RegionControl c;
    c.enabled = true;
    c.common.iron = 1000;
    std::vector<deposits::NodeInfo> out;
    CHECK(deposits::process(engine.bytes, c, nullptr, out));
    CHECK(withClumps.amount() == 7 && withHerd.amount() == 7 && out.empty());
}

TEST(deposits_refuse_a_node_list_that_makes_no_sense) {
    FakeEngine engine;
    RegionControl c;
    c.enabled = true;
    std::vector<deposits::NodeInfo> out{ { 1, 2, 3 } };
    CHECK(deposits::process(engine.bytes, c, nullptr, out) && out.empty());        // 빈 목록
    put(engine.bytes, deposits::kEngineNodesOffset + 8, static_cast<int32_t>(5));  // 개수는 있는데 포인터가 없다
    CHECK(!deposits::process(engine.bytes, c, nullptr, out));
    put(engine.bytes, deposits::kEngineNodesOffset + 8, static_cast<int32_t>(-1));
    CHECK(!deposits::process(engine.bytes, c, nullptr, out));
    put(engine.bytes, deposits::kEngineNodesOffset + 8, static_cast<int32_t>(1000000));
    CHECK(!deposits::process(engine.bytes, c, nullptr, out));
    CHECK(!deposits::process(nullptr, c, nullptr, out));
}

TEST(deposits_render_the_nodes_for_the_status_file) {
    CHECK(deposits::renderNodes({}, 4).empty());
    // nodesDay: 모을 때마다 오르는 번호. 영지가 주소로만 적혀서, 읽는 쪽이 "이 맵에서 모은 것인가"를 이 번호로 가린다
    const std::string text = deposits::renderNodes({ { 0x20E9D490010ull, deposits::kClay, 500 }, { 0x1EE18477A20ull, deposits::kIron, 3000 } }, 7);
    CHECK(text == R"("nodesDay":7,"nodes":[{"region":"20E9D490010","type":3,"amount":500},{"region":"1EE18477A20","type":2,"amount":3000}])");
    // 상태 파일에 그대로 끼워 넣을 수 있다
    auto j = parseJson(renderStatus(1, 2, {}, text));
    CHECK(j.has_value() && j->get("nodes") && j->get("nodes")->a && j->get("nodes")->a->size() == 2);
    CHECK(j->get("nodesDay")->asNumber(0) == 7);
    CHECK((*j->get("nodes")->a)[0].get("region")->s == "20E9D490010");
    CHECK((*j->get("nodes")->a)[1].get("amount")->asNumber(0) == 3000);
    CHECK(parseJson(renderStatus(1, 2, {}, "")).has_value());
}

TEST(deposits_register_one_hook_and_three_address_only_checks) {
    // 영지를 설정의 영지 키와 견주는 항목(region_name, region_tag)은 features/region_scope 가 등록한다
    HookManager m;
    deposits::registerHook(m);
    auto s = m.states();
    CHECK(s.size() == 4);
    CHECK(s[0].name == "deposits_day");
    CHECK(s[1].name == "deposits_nodes");
    CHECK(s[2].name == "deposits_amount");
    CHECK(s[3].name == "deposits_owner");
}

TEST(control_reads_the_mineral_targets_of_the_region_feature) {
    auto c = parseControl(R"({"version":1,"seq":4,"features":{"region":{"enabled":true,"noLivestockWait":true,
        "targets":{"Iron":1000,"Clay":500,"Fish":400},
        "regionTargets":{"imm":{"Iron":3000,"Mushrooms":90},"eich":{"Clay":0},"odd":7}}}})");
    CHECK(c.has_value() && c->region.enabled);
    CHECK((c->region.common == MineralTargets{ 0, 1000, 500 }));   // 덩어리형(물고기)은 Lua 가 맡는다
    CHECK(c->region.regions.size() == 2);
    CHECK(c->region.regions[0].first == "imm" && (c->region.regions[0].second == MineralTargets{ -1, 3000, -1 }));
    CHECK(c->region.regions[1].first == "eich" && (c->region.regions[1].second == MineralTargets{ -1, -1, 0 }));
}

TEST(control_region_feature_is_off_when_disabled_missing_or_malformed) {
    auto c = parseControl(R"({"version":1,"seq":5,"features":{"region":{"enabled":false,"targets":{"Iron":1000}}}})");
    CHECK(c.has_value() && !c->region.enabled && (c->region.common == MineralTargets{}) && c->region.regions.empty());
    c = parseControl(R"({"version":1,"seq":6,"features":{}})");
    CHECK(c.has_value() && !c->region.enabled);
    c = parseControl(R"({"version":1,"seq":7,"features":{"region":{"enabled":true,"targets":"x","regionTargets":[1]}}})");
    CHECK(c.has_value() && c->region.enabled && (c->region.common == MineralTargets{}) && c->region.regions.empty());
    c = parseControl(R"({"version":1,"seq":8,"features":{"region":{"enabled":true,"targets":{"Iron":-5,"Clay":"x","Salt":99999999}}}})");
    CHECK(c.has_value() && (c->region.common == MineralTargets{ 1000000, 0, 0 }));
}
