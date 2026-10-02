#include "test.h"
#include "overlay/core/control_doc.h"
#include "overlay/core/storage.h"
#include <map>
#include <optional>
#include <string>
#include <vector>

using namespace mlt::ov;

namespace {
// 모드가 bridge/catalog.json 에 쓰는 모양(features/storage_catalog.lua 의 describe). 값은 게임의 건물 표에서 잰 것이다
const char* kCatalog = R"({"version":1,"resources":[{"id":"Timber","name":"목재","category":"건설"}],"buildings":[
    {"id":"72","name":"창고","generic":250,"large":0,"pantry":0},
    {"id":"99","name":"대형 창고","generic":2500,"large":0,"pantry":0},
    {"id":"68","name":"대형 식량 비축고","generic":0,"large":0,"pantry":2500},
    {"id":"4","name":"벌목장","generic":0,"large":28,"pantry":0},
    {"id":"69","name":"농가","generic":1200,"large":0,"pantry":1200},
    {"id":"119","name":"자재 적치장","generic":800,"large":80,"pantry":0}]})";

std::vector<std::string> ids(const std::vector<StorageRow>& rows) {
    std::vector<std::string> out;
    for (const StorageRow& row : rows) out.push_back(row.id);
    return out;
}

std::vector<StorageRow> rowsOf(const StorageSettings& settings) {
    const BuildingCatalog catalog = BuildingCatalog::parse(kCatalog);
    return buildStorageRows(&catalog, settings);
}
}

TEST(overlay_storage_settings_are_off_and_empty_by_default) {
    const StorageSettings s = ControlDoc().storage();
    CHECK(!s.enabled && s.intervalSec == 5 && s.limits.empty());
}

// 값이 없는 분류와 빈 항목은 쓰지 않는다(모드는 없는 키를 "게임의 값 그대로"로 읽는다)
TEST(overlay_storage_settings_round_trip) {
    ControlDoc doc;
    StorageSettings s;
    s.enabled = true;
    s.limits["99"].generic = 5000;
    s.limits["69"].generic = 3000;
    s.limits["69"].pantry = 6000;
    s.limits["4"];   // 값이 하나도 없는 항목
    doc.setStorage(s);
    const Json& f = doc.raw()["features"]["storage"];
    CHECK(f["enabled"] == true && f["intervalSec"] == 5);
    CHECK(f["limits"].size() == 2 && f["limits"]["99"] == Json({ { "generic", 5000 } }));
    CHECK(f["limits"]["69"]["generic"] == 3000 && f["limits"]["69"]["pantry"] == 6000 && !f["limits"]["69"].contains("large"));

    const auto again = ControlDoc::tryParse(doc.dump());
    CHECK(again.has_value());
    const StorageSettings back = again->storage();
    CHECK(back.enabled && back.intervalSec == 5 && back.limits.size() == 2);
    CHECK(back.limits.at("99") == s.limits.at("99") && back.limits.at("69") == s.limits.at("69"));
}

// 손으로 고친 파일: 쓸 수 없는 값은 건너뛴다. 다른 기능과 모르는 키는 저장해도 남는다
TEST(overlay_storage_settings_skip_bad_values_and_keep_the_rest_of_the_file) {
    auto doc = ControlDoc::tryParse(R"({"version":1,"seq":4,"features":{"build":{"enabled":true},
        "storage":{"enabled":true,"intervalSec":9,"note":"mine","limits":{
            "99":{"generic":5000,"large":-3,"pantry":"lots"},"68":"all","4":{"large":100.0},"72":{}}}},"commands":[]})");
    CHECK(doc.has_value());
    StorageSettings s = doc->storage();
    CHECK(s.enabled && s.intervalSec == 9 && s.limits.size() == 2);
    CHECK(s.limits.at("99").generic == 5000 && !s.limits.at("99").large && !s.limits.at("99").pantry);
    CHECK(s.limits.at("4").large == 100);
    s.limits["68"].pantry = 9000;
    doc->setStorage(s);
    CHECK(doc->build().enabled);
    CHECK(doc->raw()["features"]["storage"]["note"] == "mine");
    CHECK(doc->storage().limits.at("68").pantry == 9000);
}

TEST(overlay_storage_catalog_reads_the_buildings_the_mod_listed) {
    const BuildingCatalog catalog = BuildingCatalog::parse(kCatalog);
    CHECK(!catalog.empty() && catalog.buildings().size() == 6);
    const BuildingInfo& farm = catalog.buildings()[4];
    CHECK(farm.id == "69" && farm.name == "농가" && farm.defaults == (std::array<int, 3>{ 1200, 0, 1200 }));
    CHECK(catalog.buildings()[3].defaults == (std::array<int, 3>{ 0, 28, 0 }));
    // 깨졌거나 건물 목록이 없는 파일(예전 판의 모드가 쓴 것)
    CHECK(BuildingCatalog::parse("{").empty());
    CHECK(BuildingCatalog::parse(R"({"version":1,"resources":[]})").empty());
    // id 가 없는 항목은 건너뛰고, 이름이 없으면 id 를 보여 준다. 음수 한도는 0 으로 본다
    const BuildingCatalog odd = BuildingCatalog::parse(R"({"buildings":[{"name":"?"},7,{"id":"5","generic":-4,"pantry":30}]})");
    CHECK(odd.buildings().size() == 1 && odd.buildings()[0].name == "5" && odd.buildings()[0].defaults == (std::array<int, 3>{ 0, 0, 30 }));
}

// 표의 줄은 모드가 적은 순서(저장 건물이 먼저)이고, 설정에 있는 값이 칸에 들어간다
TEST(overlay_storage_rows_follow_the_catalog_and_carry_the_settings) {
    StorageSettings s;
    s.limits["99"].generic = 5000;
    s.limits["69"].pantry = 6000;
    s.limits["777"].generic = 1;   // 목록에 없는 건물(게임이 더 쓰지 않는 종류): 줄로 보이지 않는다
    const std::vector<StorageRow> rows = rowsOf(s);
    CHECK(ids(rows) == (std::vector<std::string>{ "72", "99", "68", "4", "69", "119" }));
    CHECK(rows[1].name == "대형 창고" && rows[1].values[0] == 5000 && !rows[1].values[1] && !rows[1].values[2]);
    CHECK(rows[4].defaults == (std::array<int, 3>{ 1200, 0, 1200 }) && !rows[4].values[0] && rows[4].values[2] == 6000);
    CHECK(buildStorageRows(nullptr, s).empty());   // 목록을 아직 읽지 못했다
}

TEST(overlay_storage_rows_are_found_by_name_or_number) {
    const std::vector<StorageRow> rows = rowsOf({});
    CHECK(ids(filterStorageRows(rows, "창고")) == (std::vector<std::string>{ "72", "99" }));
    CHECK(ids(filterStorageRows(rows, "  비축고 ")) == (std::vector<std::string>{ "68" }));
    CHECK(ids(filterStorageRows(rows, "119")) == (std::vector<std::string>{ "119" }));
    CHECK(filterStorageRows(rows, "").size() == 6 && filterStorageRows(rows, "성").empty());
}

// 정렬하지 않으면 모드가 적은 순서. 숫자 열은 기본 한도로 정렬하고, 그 저장실이 없는 건물은 어느 방향에서도 뒤에 둔다
TEST(overlay_storage_rows_sort_by_name_or_by_default_limit) {
    std::vector<StorageRow> rows = rowsOf({});
    sortStorageRows(rows, StorageColumn::Generic, true);
    CHECK(ids(rows) == (std::vector<std::string>{ "99", "69", "119", "72", "68", "4" }));
    sortStorageRows(rows, StorageColumn::Generic, false);
    CHECK(ids(rows) == (std::vector<std::string>{ "72", "119", "69", "99", "68", "4" }));
    sortStorageRows(rows, StorageColumn::Large, true);
    CHECK(ids(rows) == (std::vector<std::string>{ "119", "4", "72", "99", "68", "69" }));
    sortStorageRows(rows, StorageColumn::Pantry, false);
    CHECK(ids(rows) == (std::vector<std::string>{ "69", "68", "72", "99", "4", "119" }));
    sortStorageRows(rows, StorageColumn::Name, false);   // 가나다순: 농가, 대형 식량 비축고, 대형 창고, 벌목장, 자재 적치장, 창고
    CHECK(ids(rows) == (std::vector<std::string>{ "69", "68", "99", "4", "119", "72" }));
    sortStorageRows(rows, std::nullopt, false);
    CHECK(ids(rows) == (std::vector<std::string>{ "72", "99", "68", "4", "69", "119" }));
}

// 칸 하나를 고친다. 값을 모두 지운 건물은 설정에서 사라진다
TEST(overlay_storage_one_cell_changes_one_limit) {
    StorageSettings s;
    setStorageLimit(s, "69", StorageKind::Pantry, 6000);
    setStorageLimit(s, "69", StorageKind::Generic, 3000);
    CHECK(s.limits.size() == 1 && s.limits.at("69").generic == 3000 && s.limits.at("69").pantry == 6000 && !s.limits.at("69").large);
    setStorageLimit(s, "69", StorageKind::Generic, std::nullopt);
    CHECK(s.limits.at("69").pantry == 6000 && !s.limits.at("69").generic);
    setStorageLimit(s, "69", StorageKind::Pantry, std::nullopt);
    CHECK(s.limits.empty());
    setStorageLimit(s, "4", StorageKind::Large, std::nullopt);   // 없던 것을 지워도 항목이 생기지 않는다
    CHECK(s.limits.empty());
}

// "N배로": 보이는 줄의, 그 건물에 있는 저장실마다 기본 한도의 N배를 넣는다
TEST(overlay_storage_fill_multiplies_the_default_of_each_storage_the_building_has) {
    StorageSettings s;
    s.limits["72"].generic = 999;
    const std::vector<StorageRow> all = rowsOf(s);
    fillStorageLimits(s, filterStorageRows(all, "적치장"), 4);
    CHECK(s.limits.size() == 2 && s.limits.at("72").generic == 999);   // 보이지 않는 줄은 그대로
    CHECK(s.limits.at("119").generic == 3200 && s.limits.at("119").large == 320 && !s.limits.at("119").pantry);
    fillStorageLimits(s, all, 10);
    CHECK(s.limits.size() == 6 && s.limits.at("72").generic == 2500 && s.limits.at("4").large == 280 && !s.limits.at("4").generic);
    CHECK(s.limits.at("69").generic == 12000 && s.limits.at("69").pantry == 12000);
    fillStorageLimits(s, all, 1000);
    CHECK(s.limits.at("99").generic == kStorageLimitMax && s.limits.at("4").large == 28000);   // 상한을 넘지 않는다
}

// 추렸으면 보이는 줄의 값만, 아니면 전부(표에 보이지 않는 건물의 값 포함) 지운다
TEST(overlay_storage_clear_follows_the_filter) {
    StorageSettings s;
    s.limits["72"].generic = 500;
    s.limits["99"].generic = 5000;
    s.limits["68"].pantry = 9000;
    s.limits["777"].generic = 1;
    const std::vector<StorageRow> all = rowsOf(s);
    clearStorageLimits(s, filterStorageRows(all, "창고"), true);
    CHECK(s.limits.size() == 2 && s.limits.count("68") == 1 && s.limits.count("777") == 1);
    clearStorageLimits(s, all, false);
    CHECK(s.limits.empty());
}
