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
