#include "test.h"
#include "overlay/core/bridge.h"
#include "overlay/core/commands.h"
#include "runtime.h"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <windows.h>

using namespace mlt::ov;
namespace fs = std::filesystem;

static fs::path freshDir(const char* tag) {
    static int n = 0;
    fs::path dir = fs::temp_directory_path() / ("mltb-overlay-" + std::to_string(GetCurrentProcessId()) + "-" + tag + std::to_string(++n));
    fs::remove_all(dir);
    return dir;
}

static void writeText(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    f << text;
}

static std::string normalized(std::string text) {
    text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
    while (!text.empty() && text.back() == '\n') text.pop_back();
    return text;
}

TEST(overlay_bridge_missing_files_give_empty_doc_and_no_status) {
    Bridge b(freshDir("missing"));
    CHECK(b.loadControl().seq() == 0);
    CHECK(!b.peekSeq().has_value());
    CHECK(!b.readStatus().has_value());
}

TEST(overlay_bridge_save_increments_seq_and_creates_the_folder) {
    Bridge b(freshDir("save"));
    ControlDoc doc;
    auto bs = doc.build();
    bs.enabled = true;
    doc.setBuild(bs);
    CHECK(b.saveControl(doc) == 1);
    CHECK(doc.seq() == 1 && b.peekSeq() == 1);
    CHECK(b.saveControl(doc) == 2);
    auto loaded = b.loadControl();
    CHECK(loaded.seq() == 2 && loaded.build().enabled);
    CHECK(!fs::exists(b.controlPath().wstring() + L".tmp"));
}

TEST(overlay_bridge_save_goes_past_a_newer_seq_in_the_file) {
    Bridge b(freshDir("newer"));
    writeText(b.controlPath(), R"({"version":1,"seq":40,"features":{}})");
    ControlDoc doc;                       // 메모리의 seq 는 0, 파일은 40 (패널이 그 사이에 저장한 경우)
    CHECK(b.saveControl(doc) == 41);
    ControlDoc ahead;
    ahead.setSeq(100);                    // 메모리가 더 큰 경우
    CHECK(b.saveControl(ahead) == 101);
}

TEST(overlay_bridge_commands_travel_with_one_save_only) {
    Bridge b(freshDir("commands"));
    ControlDoc doc;
    doc.setCommands({ makeSetLord("treasury", 5000, 1790000000) });
    CHECK(b.saveControl(doc).has_value());
    CHECK(b.loadControl().raw()["commands"].size() == 1);
    doc.setCommands({});
    CHECK(b.saveControl(doc).has_value());
    CHECK(b.loadControl().raw()["commands"].empty());
}

TEST(overlay_bridge_korean_names_survive_the_file) {
    Bridge b(freshDir("korean"));
    ControlDoc doc;
    doc.feature("mercenaries")["companies"] = Json::array({ Json::object({ { "name", "토이박스 용병단" } }) });
    CHECK(b.saveControl(doc).has_value());
    CHECK(b.loadControl().raw()["features"]["mercenaries"]["companies"][0]["name"] == "토이박스 용병단");
}

TEST(overlay_bridge_reads_status_and_ignores_broken_files) {
    Bridge b(freshDir("status"));
    writeText(b.statusPath(), R"({"version":1,"heartbeat":50,"inGame":true,"appliedSeq":7})");
    auto s = b.readStatus();
    CHECK(s.has_value() && s->heartbeat == 50 && s->appliedSeq == 7);
    writeText(b.statusPath(), "{\"heartbeat\":");
    CHECK(!b.readStatus().has_value());
}

TEST(overlay_bridge_state_follows_heartbeat_menu_and_applied_seq) {
    StatusDoc s;
    s.heartbeat = 1000;
    s.inGame = true;
    s.appliedSeq = 5;
    CHECK(Bridge::evaluate(nullptr, 5, 1000) == BridgeState::Disconnected);
    CHECK(Bridge::evaluate(&s, 5, 1006) == BridgeState::Disconnected);     // 5초 넘게 조용함
    CHECK(Bridge::evaluate(&s, 5, 1005) == BridgeState::Applied);
    CHECK(Bridge::evaluate(&s, 6, 1001) == BridgeState::Pending);
    s.appliedSeq.reset();
    CHECK(Bridge::evaluate(&s, 0, 1001) == BridgeState::Pending);          // 아직 아무것도 적용하지 않음
    s.inGame = false;
    CHECK(Bridge::evaluate(&s, 0, 1001) == BridgeState::MainMenu);
}

TEST(overlay_command_set_lord_has_the_fields_the_mod_reads) {
    Json c = makeSetLord("kingsFavour", 50000, 1790000123);
    CHECK(c["type"] == "setLord" && c["key"] == "kingsFavour" && c["value"] == 50000 && c["issuedAt"] == 1790000123);
    std::string id = c["id"];
    CHECK(id.size() == 32 && id.find_first_not_of("0123456789abcdef") == std::string::npos);
    CHECK(newCommandId() != newCommandId());
}

// Lua 스펙(overlay_fixture_spec.lua)이 읽는 견본. 코어의 출력이 바뀌면 이 테스트가 실패한다.
// 견본을 다시 만들려면 환경 변수 MLT_WRITE_FIXTURES=1 로 테스트를 한 번 돌린다.
TEST(overlay_fixture_for_the_lua_spec_matches_core_output) {
    ControlDoc doc;
    doc.setSeq(7);
    BuildSettings build;
    build.enabled = true;
    build.instantRepair = false;
    doc.setBuild(build);
    UpgradeSettings upgrade;
    upgrade.enabled = true;
    doc.setUpgrade(upgrade);
    LordSettings lord;
    lord.enabled = true;
    lord.treasury = 150000;
    lord.kingsFavour = 0;
    doc.setLord(lord);
    Json command = makeSetLord("influence", 20000, 1790000000);
    command["id"] = "0123456789abcdef0123456789abcdef";
    doc.setCommands({ command });
    const std::string text = doc.dump();

    const fs::path path = fs::path(MLT_LUA_FIXTURES_DIR) / "control_from_overlay.json";
    char* write = nullptr;
    size_t len = 0;
    _dupenv_s(&write, &len, "MLT_WRITE_FIXTURES");
    const bool regenerate = write && std::string(write) == "1";
    free(write);
    if (regenerate) writeText(path, text + "\n");
    auto saved = mlt::readFileUtf8(path);
    CHECK(saved.has_value());
    CHECK(normalized(*saved) == normalized(text));
}
