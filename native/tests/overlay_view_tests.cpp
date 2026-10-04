#include "test.h"
#include "overlay/core/resources.h"
#include "overlay/core/view.h"

using namespace mlt::ov;

static StatusDoc resourceStatus() {
    auto s = parseStatus(R"({"heartbeat":1,"inGame":true,
        "resourceIds":["RegionalWealth","Influence","Treasury","Timber"],
        "resources":{"Timber":740,"Treasury":9000},
        "regions":[{"key":"hof","name":"Klainau","values":{"Timber":700}},{"key":"sel","name":"Furdau","values":{"Timber":40}}]})");
    CHECK(s.has_value());
    return *s;
}

TEST(overlay_resources_scope_options_are_common_first_then_regions) {
    const StatusDoc s = resourceStatus();
    const std::vector<ScopeOption> options = resourceScopeOptions(&s);
    CHECK(options.size() == 3 && !options[0].key && options[0].label == "공통 (모든 내 영지, 현재=합계)");
    CHECK(options[1].key == "hof" && options[1].label == "Klainau (hof)");
    CHECK(resourceScopeOptions(nullptr).size() == 1);              // 게임 밖: 공통만
}

TEST(overlay_resources_current_is_the_total_or_the_regions_own_stock) {
    const StatusDoc s = resourceStatus();
    CHECK(resourceCurrent(&s, std::nullopt)->at("Timber") == 740.0);
    CHECK(resourceCurrent(&s, "sel")->at("Timber") == 40.0);
    CHECK(resourceCurrent(&s, "gone") == nullptr && resourceCurrent(nullptr, std::nullopt) == nullptr);
}

TEST(overlay_resources_region_targets_are_separate_and_dropped_when_empty) {
    ResourcesSettings r;
    r.targets["Timber"] = 500;
    CHECK(resourceTargets(r, "hof").empty());
    storeResourceTargets(r, "hof", { { "Timber", 2000 } });
    CHECK(r.regionTargets.at("hof").at("Timber") == 2000 && resourceTargets(r, std::nullopt).at("Timber") == 500);
    storeResourceTargets(r, "hof", {});                            // 영지 목표가 하나도 없으면 영지 키를 지운다
    CHECK(r.regionTargets.find("hof") == r.regionTargets.end());
    storeResourceTargets(r, std::nullopt, { { "Timber", 7 } });
    CHECK(r.targets.at("Timber") == 7);
}

TEST(overlay_resources_rows_join_ids_current_and_targets_sorted_by_name) {
    const StatusDoc s = resourceStatus();
    ResourcesSettings r;
    r.targets = { { "Iron", 50 }, { "Timber", 500 } };
    const std::vector<ResourceRow> rows = buildResourceRows(&s, r, std::nullopt);
    // 모드가 자원 목록을 알려 주면 그 목록이 표다. 목표만 남아 있고 모드가 모르는 자원(게임이 더 쓰지 않아 목록에서 뺀
    // meat, Beer 같은 것)은 줄로 보이지 않는다. 설정에 남은 목표는 지우지 않는다
    CHECK(rows.size() == 4);
    CHECK(rows[0].id == "Influence" && !rows[0].current && !rows[0].target);
    CHECK(rows[1].id == "RegionalWealth");
    CHECK(rows[2].id == "Timber" && rows[2].current == 740.0 && rows[2].target == 500);
    CHECK(rows[3].id == "Treasury" && rows[3].current == 9000.0 && !rows[3].target);
    CHECK(resourceTargets(r, std::nullopt).at("Iron") == 50);
    // 영지 범위: 영주 전체 값(국고, 영향력)을 숨기고, 그 영지의 재고와 영지 목표를 쓴다
    r.regionTargets["hof"] = { { "Timber", 2000 }, { "Treasury", 5 } };
    const std::vector<ResourceRow> region = buildResourceRows(&s, r, "hof");
    CHECK(region.size() == 2 && region[0].id == "RegionalWealth" && region[1].id == "Timber" && region[1].current == 700.0 && region[1].target == 2000);
    // 게임 밖: 목표가 있는 자원만
    const std::vector<ResourceRow> offline = buildResourceRows(nullptr, r, std::nullopt);
    CHECK(offline.size() == 2 && offline[0].id == "Iron" && !offline[0].current);
    CHECK(buildResourceRows(nullptr, ResourcesSettings(), std::nullopt).empty());
}

TEST(overlay_view_thousands) {
    CHECK(formatThousands(0) == "0" && formatThousands(999) == "999" && formatThousands(1000) == "1,000");
    CHECK(formatThousands(10000000) == "10,000,000" && formatThousands(-1234567) == "-1,234,567");
}

TEST(overlay_view_spawn_regions_and_reform_text) {
    CHECK(spawnRegionOptions(nullptr).size() == 1 && !spawnRegionOptions(nullptr)[0].key && spawnRegionOptions(nullptr)[0].label == "내 첫 영지");
    const std::vector<RegionInfo> regions = { { "gold", "Mandlach" }, { "nus", "Haderwand" } };
    const std::vector<ScopeOption> options = spawnRegionOptions(&regions);
    CHECK(options.size() == 2 && options[0].key == "gold" && options[1].label == "Haderwand (nus)");
    const std::vector<RegionInfo> none;
    CHECK(spawnRegionOptions(&none).size() == 1);                  // 영지가 없으면 "내 첫 영지"

    CHECK(reformText(nullptr) == "해제된 생성 분대: -" && !canReform(nullptr));
    SpawnStatus spawn;
    CHECK(reformText(&spawn) == "해제된 생성 분대: 0개" && !canReform(&spawn));
    spawn.disbanded = 3;
    spawn.byUnit = { { "retinue_tier1", 2 }, { "militia", 1 } };
    CHECK(reformText(&spawn) == "해제된 생성 분대: 3개 (친위대 - 1단계 2, 민병대 - 농민 1)" && canReform(&spawn));
    spawn.pending = 1;
    CHECK(reformText(&spawn) == "해제된 생성 분대: 3개 (친위대 - 1단계 2, 민병대 - 농민 1) — 빈 카드 정리 중 1" && !canReform(&spawn));
}

TEST(overlay_view_retinue_label_names_the_unit_count_and_origin) {
    CHECK(retinueLabel({ 63, "retinue_tier1", 36, "spawned" }) == "#63 친위대 - 1단계 ×36 (생성)");
    CHECK(retinueLabel({ 64, "retinue_tier3", 12, "mercenary" }) == "#64 친위대 - 3단계 ×12 (용병)");
    CHECK(retinueLabel({ 65, "odd", 1, "odd" }) == "#65 odd ×1 (odd)");
}

TEST(overlay_view_population_scope_and_info) {
    CHECK(populationScopeOptions(nullptr).size() == 1 && populationScopeOptions(nullptr)[0].label == "공통 (모든 내 영지)");
    CHECK(populationInfo(nullptr, std::nullopt) == std::vector<std::string>{ "현재: - (게임에 들어가면 표시됩니다)" });
    PopulationStatus p;
    p.families = 14;
    p.population = 42;
    p.freeSlots = 3;
    p.unassigned = 2;
    p.natural = 1;
    p.multiplied = 2;
    p.regions.push_back({ "hof", "Klainau", 10, 30, 0, 3, 2 });
    const std::vector<ScopeOption> options = populationScopeOptions(&p);
    CHECK(options.size() == 2 && options[1].key == "hof" && options[1].label == "Klainau (hof)");
    std::vector<std::string> lines = populationInfo(&p, std::nullopt);
    CHECK(lines.size() == 2);
    CHECK(lines[0] == "현재(모든 내 영지 합계): 가족 14 · 인구 42 · 집 없는 가족 0 · 빈 자리 3 · 미배치 가족 2");
    CHECK(lines[1] == "이번 세션(전체): 자연 이민 1가족 → 배율로 추가 2가족");
    p.multiplied = 0;   // 네이티브가 배율을 맡으면 게임이 직접 들인다: 모드가 따로 들인 가족이 없으면 뒤의 말을 뺀다
    CHECK(populationInfo(&p, std::nullopt)[1] == "이번 세션(전체): 자연 이민 1가족");
    lines = populationInfo(&p, "hof");
    CHECK(lines[0] == "현재(Klainau): 가족 10 · 인구 30 · 집 없는 가족 0 · 빈 자리 3 · 미배치 가족 2");
    CHECK(populationInfo(&p, "gone")[0].find("모든 내 영지 합계") != std::string::npos);   // 사라진 영지는 합계로
}

TEST(overlay_view_mercenary_status_lines) {
    CHECK(mercStatusLines(nullptr) == std::vector<std::string>{ "고용 창: - (용병 기능이 꺼져 있거나 게임 밖)" });
    MercenaryStatus m;
    std::vector<std::string> lines = mercStatusLines(&m);
    CHECK(lines.size() == 2 && lines[0] == "고용 창: (비어 있음)" && lines[1] == "고용 중: 내 용병단 0개, AI 0개 · 맵을 불러온 뒤 환급 0");
    m.slots = { { "토이박스 용병단", 10000000, true }, { "wayward_sons", 90, false } };
    m.hiredMine = 2;
    m.hiredAi = 3;
    m.refunded = 6000;
    m.skipped = { { "궁수대", "unknown unit: foo" } };
    m.note = "rebuild produced 1 of 3 slots";
    lines = mercStatusLines(&m);
    CHECK(lines.size() == 4);
    CHECK(lines[0] == "고용 창: 토이박스 용병단(커스텀) 10,000,000, wayward_sons 90");
    CHECK(lines[1] == "고용 중: 내 용병단 2개, AI 3개 · 맵을 불러온 뒤 환급 6,000");
    CHECK(lines[2] == "띄우지 못함: 궁수대 — unknown unit: foo");
    CHECK(lines[3] == "참고: rebuild produced 1 of 3 slots");
}
