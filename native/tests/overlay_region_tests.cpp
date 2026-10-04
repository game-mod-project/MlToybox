#include "test.h"
#include "control.h"
#include "overlay/core/control_doc.h"
#include "overlay/core/region.h"
#include "overlay/core/status_doc.h"
#include "overlay/ui/app.h"
#include "overlay/ui/tabs.h"
#include "tab_frames.h"
#include <imgui.h>

using namespace mlt::ov;

TEST(overlay_region_defaults_change_nothing) {
    const RegionSettings r = ControlDoc().region();
    CHECK(!r.enabled && r.intervalSec == 5 && !r.noLivestockWait && r.targets.empty() && r.regionTargets.empty());
}

TEST(overlay_region_settings_round_trip_and_reach_the_native_dll) {
    ControlDoc doc;
    doc.setSeq(3);
    RegionSettings r;
    r.enabled = true;
    r.noLivestockWait = true;
    r.targets["Iron"] = 1000;
    r.targets["Fish"] = 400;
    r.regionTargets["imm"]["Iron"] = 3000;
    r.regionTargets["eich"]["Clay"] = 0;   // 0 = 그 영지는 채우지 않는다. 값으로 남는다
    doc.setRegion(r);
    const RegionSettings back = ControlDoc::parse(doc.dump()).region();
    CHECK(back.enabled && back.noLivestockWait && back.intervalSec == 5);
    CHECK(back.targets == r.targets && back.regionTargets == r.regionTargets);
    // 네이티브 DLL(src/control.cpp)은 같은 글에서 광물 목표만 읽는다
    const auto native = mlt::parseControl(doc.dump());
    CHECK(native.has_value() && native->region.enabled);
    CHECK((native->region.common == mlt::MineralTargets{ 0, 1000, 0 }));
    CHECK(native->region.regions.size() == 2);
    // 다른 기능과 모르는 키는 그대로
    ControlDoc other = ControlDoc::parse(R"({"version":1,"seq":3,"features":{"region":{"enabled":true,"future":1},"storage":{"enabled":true}}})");
    RegionSettings s = other.region();
    s.noLivestockWait = true;
    other.setRegion(s);
    const Json features = Json::parse(other.dump())["features"];
    CHECK(features["region"]["future"] == 1 && features["region"]["noLivestockWait"] == true && features["storage"]["enabled"] == true);
}

TEST(overlay_region_status_lists_wait_and_deposits) {
    const auto s = parseStatus(R"({"heartbeat":5,"inGame":true,"region":{"regions":[
        {"key":"eich","name":"Wilde Wand","livestockWait":7,"deposits":[{"kind":"Salt","amount":119},{"kind":"Mushrooms","amount":629,"capacity":640,"clumps":16}]},
        {"key":"hof","name":"Obere Wiese","livestockWait":0,"deposits":[]},7]}})");
    CHECK(s.has_value() && s->region.has_value() && s->region->regions.size() == 2);
    const RegionState& eich = s->region->regions[0];
    CHECK(eich.key == "eich" && eich.name == "Wilde Wand" && eich.livestockWait == 7 && eich.deposits.size() == 2);
    CHECK(eich.deposits[0].kind == "Salt" && eich.deposits[0].amount == 119 && !eich.deposits[0].capacity);
    CHECK(eich.deposits[1].kind == "Mushrooms" && eich.deposits[1].amount == 629 && eich.deposits[1].capacity == 640);
    CHECK(s->region->regions[1].deposits.empty());
    CHECK(!parseStatus(R"({"heartbeat":5,"inGame":true})")->region.has_value());
}

namespace {
RegionStatus sampleStatus() {
    RegionStatus st;
    RegionState eich{ "eich", "Wilde Wand", 7, {} };
    eich.deposits = { { "Salt", 119, std::nullopt }, { "Clay", 25, std::nullopt }, { "Stone", 39, 0 }, { "Mushrooms", 629, 640 } };
    RegionState imm{ "imm", "Krumme Leite", 0, {} };
    imm.deposits = { { "Iron", 1154, std::nullopt }, { "Stone", 8, 0 }, { "Stone", 500, 0 }, { "Fish", 3295, 3300 } };
    st.regions = { eich, imm };
    return st;
}

const DepositRow& rowOf(const std::vector<DepositRow>& rows, const std::string& key) {
    for (const DepositRow& r : rows) if (r.key == key) return r;
    static const DepositRow none;
    return none;
}
}

TEST(overlay_region_kinds_are_listed_in_table_order) {
    std::vector<std::string> keys, labels;
    for (const DepositKind& k : depositKinds()) { keys.emplace_back(k.key); labels.emplace_back(k.label); }
    CHECK(keys == (std::vector<std::string>{ "Salt", "Iron", "Clay", "Stone", "Fish", "Eel", "Berries", "Mushrooms" }));
    CHECK(labels == (std::vector<std::string>{ "소금", "철", "점토", "돌", "물고기", "장어", "열매", "버섯" }));
    CHECK(depositKinds()[1].mineral && !depositKinds()[3].mineral);
}

TEST(overlay_region_rows_show_the_current_amounts_for_the_chosen_scope) {
    const RegionStatus st = sampleStatus();
    RegionSettings s;
    s.targets["Iron"] = 1000;
    s.regionTargets["imm"]["Iron"] = 3000;
    s.regionTargets["imm"]["Fish"] = 0;

    // 공통: 내 영지 전체의 합과 매장지 수
    std::vector<DepositRow> rows = buildDepositRows(s, &st, std::nullopt);
    CHECK(rows.size() == 8 && rows[0].key == "Salt" && rows[0].label == "소금");
    CHECK(rowOf(rows, "Salt").current == "119 (1곳)");
    CHECK(rowOf(rows, "Stone").current == "547 (3곳)");
    CHECK(rowOf(rows, "Fish").current == "3,295 (1곳)");
    CHECK(rowOf(rows, "Eel").current == "-");
    CHECK(rowOf(rows, "Iron").target == 1000 && !rowOf(rows, "Iron").inherited);
    CHECK(!rowOf(rows, "Fish").target);

    // 영지: 그 영지의 매장지마다. 다시 차는 것은 "양 / 용량"
    rows = buildDepositRows(s, &st, "imm");
    CHECK(rowOf(rows, "Iron").current == "1,154");
    CHECK(rowOf(rows, "Stone").current == "8, 500");
    CHECK(rowOf(rows, "Fish").current == "3,295 / 3,300");
    CHECK(rowOf(rows, "Salt").current == "-");
    CHECK(rowOf(rows, "Iron").target == 3000 && rowOf(rows, "Iron").inherited == 1000);
    CHECK(rowOf(rows, "Fish").target == 0);   // 0 = 이 영지는 채우지 않는다
    rows = buildDepositRows(s, &st, "eich");
    CHECK(!rowOf(rows, "Iron").target && rowOf(rows, "Iron").inherited == 1000);   // 따로 정하지 않은 종류는 공통을 따른다
    CHECK(rowOf(rows, "Mushrooms").current == "629 / 640");

    // 게임 밖: 값은 없고 설정은 보인다
    rows = buildDepositRows(s, nullptr, std::nullopt);
    CHECK(rows.size() == 8 && rowOf(rows, "Iron").current == "-" && rowOf(rows, "Iron").target == 1000);
}

TEST(overlay_region_targets_are_set_per_scope_and_dropped_when_cleared) {
    RegionSettings s;
    setDepositTarget(s, std::nullopt, "Iron", 1000);
    setDepositTarget(s, "imm", "Iron", 3000);
    setDepositTarget(s, "imm", "Fish", 0);
    CHECK(s.targets.at("Iron") == 1000 && s.regionTargets.at("imm").at("Iron") == 3000 && s.regionTargets.at("imm").at("Fish") == 0);
    setDepositTarget(s, std::nullopt, "Iron", std::nullopt);
    CHECK(s.targets.empty());
    setDepositTarget(s, "imm", "Iron", std::nullopt);
    CHECK(s.regionTargets.at("imm").size() == 1);
    setDepositTarget(s, "imm", "Fish", std::nullopt);
    CHECK(s.regionTargets.empty());   // 비게 된 영지는 뺀다
    setDepositTarget(s, std::nullopt, "Iron", 99999999);
    CHECK(s.targets.at("Iron") == kDepositTargetMax);
    // 공통 목표에 0 을 넣으면 끈 것이다(1 로 맞춰져 "1 을 유지"가 되지 않는다). 영지에서는 0 이 값으로 남는다
    setDepositTarget(s, std::nullopt, "Iron", 0);
    CHECK(s.targets.empty());
    setDepositTarget(s, "imm", "Iron", 0);
    CHECK(s.regionTargets.at("imm").at("Iron") == 0);
}

TEST(overlay_region_scope_options_and_livestock_lines) {
    CHECK(regionScopeOptions(nullptr).size() == 1 && regionScopeOptions(nullptr)[0].label == "공통 (모든 내 영지)");
    const RegionStatus st = sampleStatus();
    const std::vector<ScopeOption> options = regionScopeOptions(&st);
    CHECK(options.size() == 3 && options[2].key == "imm" && options[2].label == "Krumme Leite (imm)");
    CHECK(livestockLine(nullptr) == "가축 상인: - (게임에 들어가면 표시됩니다)");
    CHECK(livestockLine(&st) == "가축 상인: Wilde Wand 방문까지 7일 · Krumme Leite 지금 주문 가능");
    RegionStatus none;
    CHECK(livestockLine(&none) == "가축 상인: 내 영지가 없습니다");
}

TEST(overlay_region_native_note_says_what_is_missing) {
    CHECK(depositNativeNote(nullptr).empty());
    NativeStatus n;
    n.loaded = true;
    CHECK(depositNativeNote(&n).empty());   // 항목 목록을 아직 못 받았다: 모르는 것을 "못 맡았다"고 하지 않는다
    for (const char* name : { "deposits_day", "deposits_nodes", "deposits_amount", "deposits_owner", "region_name", "region_tag" }) n.features[name].installed = true;
    CHECK(depositNativeNote(&n).empty());
    n.features["region_name"].installed = false;
    CHECK(depositNativeNote(&n).find("영지별 목표") != std::string::npos);
    n.features["deposits_amount"].installed = false;
    CHECK(depositNativeNote(&n).find("소금·철·점토") != std::string::npos);
    n.stale = true;
    CHECK(depositNativeNote(&n).empty());   // 오래된 상태로는 판단하지 않는다
    n.stale = false;
    n.loaded = false;
    CHECK(depositNativeNote(&n).find("소금·철·점토") != std::string::npos);
}

TEST(overlay_region_tab_sits_after_mood_and_its_two_checkboxes_work) {
    CHECK(tabNames() == (std::vector<std::string>{ "자원", "영주", "건설", "군사", "용병", "인구", "자격·질서", "영지", "상태", "로그" }));
    TabFrames f;
    f.frame(drawRegionTab);
    CHECK(!f.a.control.region().enabled && !f.a.dirty);
    f.click(drawRegionTab, 20.0f, 18.0f);   // 첫 줄: 기능 사용
    CHECK(f.a.control.region().enabled && !f.a.control.region().noLivestockWait && f.a.dirty);
    // 둘째 줄을 찾아 누른다: 가축 상인 대기 없음
    bool found = false;
    for (float y = 30.0f; y < 80.0f && !found; y += 4.0f) {
        f.click(drawRegionTab, 20.0f, y);
        found = f.a.control.region().noLivestockWait;
    }
    CHECK(found && f.a.control.region().enabled);
}

TEST(overlay_region_tab_draws_the_table_with_values_from_the_game) {
    const auto status = parseStatus(R"({"heartbeat":5,"inGame":true,"region":{"regions":[
        {"key":"eich","name":"Wilde Wand","livestockWait":7,"deposits":[{"kind":"Salt","amount":119},{"kind":"Mushrooms","amount":629,"capacity":640,"clumps":16}]}]}})");
    CHECK(status.has_value());
    TabFrames f;
    RegionSettings r;
    r.enabled = true;
    r.targets["Iron"] = 1000;
    f.a.control.setRegion(r);
    f.frame(drawRegionTab, &*status);
    f.frame(drawRegionTab, &*status);
    CHECK(!f.a.dirty);   // 그리기만 해서는 설정이 바뀌지 않는다
    CHECK(f.a.control.region().targets.at("Iron") == 1000);
}
