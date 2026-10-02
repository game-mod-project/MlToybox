#include "test.h"
#include "overlay/core/resources.h"
#include <algorithm>

using namespace mlt::ov;

namespace {
// 모드가 시작할 때 bridge/catalog.json 에 쓰는 것과 같은 꼴
const char* kCatalog = R"({"version":1,"resources":[
    {"id":"RegionalWealth","name":"지역 자산","category":"영지"},
    {"id":"Timber","name":"목재","category":"건설","group":"목재 작업물"},
    {"id":"planks","name":"널빤지","category":"건설","group":"목재 작업물"},
    {"id":"Berries","name":"열매","category":"식량","group":"채집한 상품"},
    {"id":"Beef","name":"소고기","category":"식량","group":"고기"},
    {"id":"spears","name":"창","category":"군사","group":"근접 무기"}]})";

StatusDoc status() {
    auto s = parseStatus(R"({"heartbeat":1,"inGame":true,
        "resourceIds":["RegionalWealth","Timber","planks","Berries","Beef","spears","Mystery"],
        "resources":{"Timber":740,"planks":12,"Berries":0,"Beef":300,"RegionalWealth":55}})");
    CHECK(s.has_value());
    return *s;
}

std::vector<std::string> ids(const std::vector<ResourceRow>& rows) {
    std::vector<std::string> out;
    for (const ResourceRow& row : rows) out.push_back(row.id);
    return out;
}

std::vector<ResourceRow> rows() {
    const StatusDoc s = status();
    ResourcesSettings r;
    r.targets = { { "Timber", 500 }, { "Beef", 1000 }, { "spears", 20 } };
    const ResourceCatalog catalog = ResourceCatalog::parse(kCatalog);
    return buildResourceRows(&s, r, std::nullopt, &catalog);
}
}

TEST(overlay_resource_catalog_reads_names_and_keeps_the_mods_order) {
    const ResourceCatalog catalog = ResourceCatalog::parse(kCatalog);
    CHECK(!catalog.empty());
    const ResourceInfo* timber = catalog.find("Timber");
    CHECK(timber && timber->name == "목재" && timber->category == "건설" && timber->group == "목재 작업물" && timber->order == 1);
    CHECK(catalog.find("RegionalWealth")->group.empty() && catalog.find("spears")->order == 5);
    CHECK(catalog.find("Mystery") == nullptr);
}

TEST(overlay_resource_catalog_is_empty_when_the_file_is_missing_or_broken) {
    CHECK(ResourceCatalog::parse("").empty() && ResourceCatalog::parse("{").empty() && ResourceCatalog::parse("[]").empty());
    CHECK(ResourceCatalog::parse(R"({"resources":{}})").empty());
    // 이름이 없는 줄은 id 를 이름으로 쓰고, id 가 없는 줄은 버린다
    const ResourceCatalog c = ResourceCatalog::parse(R"({"resources":[{"id":"Timber"},{"name":"없음"},7]})");
    CHECK(c.find("Timber") && c.find("Timber")->name == "Timber" && c.find("Timber")->category.empty());
}

TEST(overlay_resource_rows_carry_the_name_and_category_from_the_catalog) {
    const std::vector<ResourceRow> all = rows();
    CHECK(all.size() == 7);
    const auto beef = std::find_if(all.begin(), all.end(), [](const ResourceRow& r) { return r.id == "Beef"; });
    CHECK(beef != all.end() && beef->name == "소고기" && beef->category == "식량" && beef->group == "고기" && beef->current == 300.0 && beef->target == 1000);
    // 모드의 목록에는 있는데 이름 표에 없는 자원: 이름은 id, 분류는 "기타", 맨 뒤
    const auto mystery = std::find_if(all.begin(), all.end(), [](const ResourceRow& r) { return r.id == "Mystery"; });
    CHECK(mystery != all.end() && mystery->name == "Mystery" && mystery->category == "기타" && mystery->order > beef->order);
    // 이름 표가 없으면(옛 모드, 파일을 못 읽음) 모든 줄이 id 로 보인다
    const StatusDoc s = status();
    const std::vector<ResourceRow> plain = buildResourceRows(&s, ResourcesSettings(), std::nullopt);
    CHECK(plain.size() == 7 && plain[0].name == plain[0].id && plain[0].category == "기타");
}

TEST(overlay_resource_label_joins_category_and_group) {
    ResourceRow row;
    row.category = "식량";
    row.group = "고기";
    CHECK(resourceClassLabel(row) == "식량 · 고기");
    row.group.clear();
    CHECK(resourceClassLabel(row) == "식량");
}

TEST(overlay_resource_categories_are_listed_in_the_catalogs_order) {
    CHECK(resourceCategories(rows()) == (std::vector<std::string>{ "영지", "건설", "식량", "군사", "기타" }));
    CHECK(resourceCategories({}).empty());
}

TEST(overlay_resource_filter_by_category) {
    ResourceFilter f;
    f.category = "식량";
    std::vector<std::string> got = ids(filterResourceRows(rows(), f));
    std::sort(got.begin(), got.end());
    CHECK(got == (std::vector<std::string>{ "Beef", "Berries" }));
    f.category = "없는 분류";
    CHECK(filterResourceRows(rows(), f).empty());
    CHECK(filterResourceRows(rows(), ResourceFilter()).size() == 7);   // 조건이 없으면 전부
}

TEST(overlay_resource_search_matches_korean_name_id_and_group) {
    auto found = [](const char* text) {
        ResourceFilter f;
        f.search = text;
        std::vector<std::string> got = ids(filterResourceRows(rows(), f));
        std::sort(got.begin(), got.end());
        return got;
    };
    CHECK(found("고기") == (std::vector<std::string>{ "Beef" }));            // 이름 "소고기"와 묶음 "고기"
    CHECK(found("목재") == (std::vector<std::string>{ "Timber", "planks" }));   // 이름 "목재"와 묶음 "목재 작업물"
    CHECK(found("TIMB") == (std::vector<std::string>{ "Timber" }));          // id 는 영문 대소문자를 가리지 않는다
    CHECK(found("  창 ") == (std::vector<std::string>{ "spears" }));          // 앞뒤 공백은 뺀다
    CHECK(found("군사") == (std::vector<std::string>{ "spears" }));          // 분류 이름
    CHECK(found("없는것").empty());
    CHECK(found("   ").size() == 7);
    // 분류와 검색을 함께 걸면 둘 다 맞는 줄만
    ResourceFilter both;
    both.category = "건설";
    both.search = "널";
    CHECK(ids(filterResourceRows(rows(), both)) == (std::vector<std::string>{ "planks" }));
}

TEST(overlay_resource_sort_by_category_is_the_games_order) {
    std::vector<ResourceRow> r = rows();
    sortResourceRows(r, ResourceColumn::Category, false);
    CHECK(ids(r) == (std::vector<std::string>{ "RegionalWealth", "Timber", "planks", "Berries", "Beef", "spears", "Mystery" }));
    sortResourceRows(r, ResourceColumn::Category, true);
    CHECK(ids(r) == (std::vector<std::string>{ "Mystery", "spears", "Beef", "Berries", "planks", "Timber", "RegionalWealth" }));
}

TEST(overlay_resource_sort_by_name_uses_the_shown_name) {
    std::vector<ResourceRow> r = rows();
    sortResourceRows(r, ResourceColumn::Name, false);
    // 영문(이름 표에 없는 자원)이 먼저, 한글은 가나다순
    CHECK(ids(r) == (std::vector<std::string>{ "Mystery", "planks", "Timber", "Beef", "Berries", "RegionalWealth", "spears" }));
    sortResourceRows(r, ResourceColumn::Name, true);
    CHECK(r.front().id == "spears" && r.back().id == "Mystery");
}

TEST(overlay_resource_sort_by_numbers_puts_missing_values_last_both_ways) {
    std::vector<ResourceRow> r = rows();
    sortResourceRows(r, ResourceColumn::Current, true);
    // 현재 값이 없는 줄(spears, Mystery)은 내림차순에서도 오름차순에서도 뒤다. 같은 값끼리는 게임의 순서
    CHECK(ids(r) == (std::vector<std::string>{ "Timber", "Beef", "RegionalWealth", "planks", "Berries", "spears", "Mystery" }));
    sortResourceRows(r, ResourceColumn::Current, false);
    CHECK(ids(r) == (std::vector<std::string>{ "Berries", "planks", "RegionalWealth", "Beef", "Timber", "spears", "Mystery" }));
    sortResourceRows(r, ResourceColumn::Target, true);
    CHECK(ids(r) == (std::vector<std::string>{ "Beef", "Timber", "spears", "RegionalWealth", "planks", "Berries", "Mystery" }));
    sortResourceRows(r, ResourceColumn::Target, false);
    CHECK(ids(r) == (std::vector<std::string>{ "spears", "Timber", "Beef", "RegionalWealth", "planks", "Berries", "Mystery" }));
}

TEST(overlay_resource_order_keeps_rows_in_place_while_values_change) {
    ResourceOrder order;
    std::vector<ResourceRow> r = rows();
    order.arrange(r, ResourceColumn::Current, true, "common");
    CHECK(r.front().id == "Timber" && r[1].id == "Beef");
    // 값이 바뀌어도(게임이 돌아가는 동안, 또는 목표를 입력하는 동안) 줄은 자리를 지킨다
    r = rows();
    for (ResourceRow& row : r) {
        if (row.id == "Beef") row.current = 5000.0;
    }
    order.arrange(r, ResourceColumn::Current, true, "common");
    CHECK(r.front().id == "Timber" && r[1].id == "Beef" && r[1].current == 5000.0);
    // 정렬 기준이나 방향을 바꾸면(헤더를 누르면) 지금 값으로 다시 정렬한다
    order.arrange(r, ResourceColumn::Current, false, "common");
    CHECK(r.front().id == "Berries");
    order.arrange(r, ResourceColumn::Current, true, "common");
    CHECK(r.front().id == "Beef");
    // 보는 범위·분류·검색이 바뀌어도 다시 정렬한다
    for (ResourceRow& row : r) {
        if (row.id == "planks") row.current = 9000.0;
    }
    order.arrange(r, ResourceColumn::Current, true, "region:hof");
    CHECK(r.front().id == "planks");
    // 줄의 구성이 바뀌어도(자원이 늘거나 줄면) 다시 정렬한다
    r.pop_back();
    for (ResourceRow& row : r) {
        if (row.id == "Berries") row.current = 99999.0;
    }
    order.arrange(r, ResourceColumn::Current, true, "region:hof");
    CHECK(r.front().id == "Berries" && r.size() == 6);
}

TEST(overlay_resource_column_shares_are_percentages_of_the_table_width) {
    CHECK(columnShares({ 300.0f, 200.0f, 100.0f, 400.0f }) == (std::vector<float>{ 30.0f, 20.0f, 10.0f, 40.0f }));
    CHECK(columnShares({ 0.0f, 0.0f }).empty() && columnShares({}).empty() && columnShares({ 100.0f, -1.0f }).empty());   // 아직 그려지지 않은 표
    // 창 크기를 바꿔 모든 열이 같은 비율로 늘면 바뀐 것이 아니다. 열 하나를 끌면 바뀐 것이다
    CHECK(!columnSharesDiffer({ 30.0f, 20.0f, 10.0f, 40.0f }, columnShares({ 601.0f, 400.0f, 199.0f, 800.0f })));
    CHECK(columnSharesDiffer({ 30.0f, 20.0f, 10.0f, 40.0f }, columnShares({ 400.0f, 100.0f, 100.0f, 400.0f })));
    CHECK(columnSharesDiffer({ 30.0f, 70.0f }, { 30.0f, 20.0f, 50.0f }));
}

// 영지 목표는 공통 목표보다 우선한다. 공통 범위의 표에서는 그 사실이 보이지 않아, 공통 목표를 바꿔도 왜 그대로인지 알 수 없었다
// (사용자 확인 2026-10-02: 지역 자산의 공통 목표를 5000 으로 올려도 그대로. 그 영지에 영지 목표 1000 이 있었다)
TEST(overlay_resource_region_overrides_name_the_regions_that_ignore_the_common_target) {
    auto s = parseStatus(R"({"heartbeat":1,"inGame":true,"resourceIds":["RegionalWealth","Timber"],
        "regions":[{"key":"sel","name":"Altbruch","values":{}},{"key":"gold","name":"Mandlach","values":{}}]})");
    CHECK(s.has_value());
    ResourcesSettings r;
    r.targets = { { "RegionalWealth", 5000 } };
    r.regionTargets["sel"] = { { "RegionalWealth", 1000 }, { "Timber", 1000 } };
    r.regionTargets["gold"] = { { "Timber", 0 } };
    r.regionTargets["hof"] = { { "RegionalWealth", 600 } };   // 이 세이브에 없는 영지
    const auto overrides = regionOverrides(&*s, r);
    CHECK(overrides.size() == 2);
    CHECK(overrides.at("RegionalWealth") == (std::vector<RegionOverride>{ { "sel", "Altbruch", 1000 } }));
    CHECK(overrides.at("Timber") == (std::vector<RegionOverride>{ { "sel", "Altbruch", 1000 }, { "gold", "Mandlach", 0 } }));
    CHECK(regionOverrideText(overrides.at("Timber")) == "Altbruch (sel): 1000\nMandlach (gold): 0");
    // 게임 밖(영지 목록을 모른다): 설정에 있는 영지를 키로 적는다
    const auto offline = regionOverrides(nullptr, r);
    CHECK(offline.at("RegionalWealth").size() == 2 && offline.at("RegionalWealth")[0].key == "hof" && offline.at("RegionalWealth")[0].name == "hof");
    CHECK(regionOverrides(&*s, ResourcesSettings()).empty());
}

// "지우기": 보이는 줄의 목표를 지운다. 추리지 않았으면 그 범위의 목표를 모두 지운다(표에 보이지 않는 옛 자원의 목표까지)
TEST(overlay_resource_clear_removes_shown_targets_or_everything_when_unfiltered) {
    const std::map<std::string, int> targets = { { "Timber", 500 }, { "Beef", 1000 }, { "Pork", 20 }, { "Beer", 600 } };
    std::vector<ResourceRow> shown(2);
    shown[0].id = "Beef";
    shown[1].id = "Pork";
    CHECK(clearedTargets(targets, shown, true) == (std::map<std::string, int>{ { "Timber", 500 }, { "Beer", 600 } }));
    CHECK(clearedTargets(targets, shown, false).empty());
}
