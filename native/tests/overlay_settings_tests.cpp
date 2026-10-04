#include "test.h"
#include "overlay/core/control_doc.h"
#include "overlay/core/settings.h"
#include "overlay/core/status_file.h"

using namespace mlt::ov;

TEST(overlay_settings_defaults_when_missing_or_broken) {
    const OverlaySettings d;
    CHECK(d.toggleKey == "Insert" && d.scale == 1.0f && d.x == 80 && d.y == 80 && d.w == 640 && d.h == 720 && !d.startOpen && d.devTab.empty());
    CHECK(parseSettings("") == d);
    CHECK(parseSettings("{") == d);
    CHECK(parseSettings("[1]") == d);
}

// 게임 중에 밖에서 고친 overlay.json 을 다시 읽을 때: 읽을 수 없는 내용(쓰는 도중이거나 문법이 틀렸다)이면
// 기본값으로 바꾸지 않고 지금 설정을 그대로 둔다. 그러려면 "읽지 못했다"와 "빈 설정"을 가를 수 있어야 한다
TEST(overlay_settings_tells_unreadable_text_from_empty_settings) {
    CHECK(!tryParseSettings("").has_value());
    CHECK(!tryParseSettings("{\"toggleKey\":\"F8\",").has_value());
    CHECK(!tryParseSettings("[1]").has_value());
    const auto empty = tryParseSettings("{}");
    CHECK(empty.has_value() && *empty == OverlaySettings());
    const auto set = tryParseSettings(R"({"toggleKey":"F8"})");
    CHECK(set.has_value() && set->toggleKey == "F8");
}

TEST(overlay_settings_reads_values_and_repairs_bad_ones) {
    auto s = parseSettings(R"({"toggleKey":"F8","scale":1.3,"window":{"x":10,"y":20,"w":900,"h":800},"startOpen":true,"devTab":"영주"})");
    CHECK(s.toggleKey == "F8" && s.scale > 1.29f && s.scale < 1.31f && s.x == 10 && s.y == 20 && s.w == 900 && s.h == 800 && s.startOpen && s.devTab == "영주");
    s = parseSettings(R"({"toggleKey":"Escape","scale":9,"window":{"w":10,"h":99999},"devTab":null})");
    CHECK(s.toggleKey == "Insert");                 // 모르는 키
    CHECK(s.scale == kScaleMax);                    // 범위 밖
    CHECK(s.w == 320 && s.h == 4000 && s.x == 80);  // 너무 작거나 큰 창, 빠진 값
    CHECK(s.devTab.empty());
    CHECK(parseSettings(R"({"scale":0.1})").scale == kScaleMin);
    CHECK(parseSettings(R"({"scale":"big"})").scale == 1.0f);
}

TEST(overlay_settings_round_trip) {
    OverlaySettings s;
    s.toggleKey = "Home";
    s.scale = 1.2f;
    s.x = 300;
    s.w = 700;
    s.devTab = "상태";
    auto back = parseSettings(dumpSettings(s));
    CHECK(back.toggleKey == "Home" && back.x == 300 && back.w == 700 && back.devTab == "상태");
    CHECK(back.scale > 1.19f && back.scale < 1.21f);
    CHECK(Json::parse(dumpSettings(OverlaySettings()))["devTab"].is_null());
}

TEST(overlay_settings_toggle_keys) {
    CHECK(toggleKeyNames().size() == 6 && toggleKeyNames().front() == "Insert");
    CHECK(toggleKeyCode("Insert") == 0x2D && toggleKeyCode("Home") == 0x24 && toggleKeyCode("End") == 0x23);
    CHECK(toggleKeyCode("F7") == 0x76 && toggleKeyCode("F8") == 0x77 && toggleKeyCode("F9") == 0x78);
    CHECK(toggleKeyCode("nope") == 0x2D);
}

TEST(overlay_status_file_reports_state_and_reason) {
    OverlayStatus s;
    s.heartbeat = 1790000000;
    s.state = OverlayState::Ready;
    s.visible = true;
    s.frames = 12345;
    s.font = "malgun";
    auto j = Json::parse(renderOverlayStatus(s));
    CHECK(j["heartbeat"] == 1790000000 && j["state"] == "ready" && j["reason"].is_null() && j["visible"] == true && j["frames"] == 12345 && j["font"] == "malgun");
    s.state = OverlayState::Disabled;
    s.reason = "present queue not found";
    j = Json::parse(renderOverlayStatus(s));
    CHECK(j["state"] == "disabled" && j["reason"] == "present queue not found");
    CHECK(std::string(overlayStateName(OverlayState::Starting)) == "starting" && std::string(overlayStateName(OverlayState::Waiting)) == "waiting");
}

// 개발·검증용: 창이 열려 있는 동안의 글쇠 메시지를 bridge/overlay_input.log 에 적는다. 평소에는 꺼져 있고 파일에도 없다
TEST(overlay_settings_input_log_is_off_and_absent_unless_asked_for) {
    CHECK(!parseSettings("{}").inputLog);
    CHECK(parseSettings(R"({"inputLog":true})").inputLog);
    CHECK(!Json::parse(dumpSettings(OverlaySettings())).contains("inputLog"));
    OverlaySettings s;
    s.inputLog = true;
    CHECK(parseSettings(dumpSettings(s)).inputLog);
}

// 자원 표의 열 너비(표 너비에 대한 백분율 넷). 사용자가 열을 끌어 바꾸면 적힌다. 없거나 이상하면 기본 너비를 쓴다
TEST(overlay_settings_resource_column_shares) {
    CHECK(parseSettings("{}").resourceColumns.empty());
    CHECK(!Json::parse(dumpSettings(OverlaySettings())).contains("resourceColumns"));
    OverlaySettings s;
    s.resourceColumns = { 30.0f, 25.5f, 14.5f, 30.0f };
    CHECK(parseSettings(dumpSettings(s)).resourceColumns == s.resourceColumns);
    CHECK(parseSettings(R"({"resourceColumns":[30,20,50]})").resourceColumns.empty());            // 열 수가 다르다
    CHECK(parseSettings(R"({"resourceColumns":[30,0,20,50]})").resourceColumns.empty());          // 0 이하
    CHECK(parseSettings(R"({"resourceColumns":[30,"a",20,50]})").resourceColumns.empty());
    CHECK(parseSettings(R"({"resourceColumns":"wide"})").resourceColumns.empty());
}
