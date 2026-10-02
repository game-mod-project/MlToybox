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

// 저장 실패의 실제 방아쇠: 다른 프로그램이 control.json 을 잡고 있어 이름 바꾸기가 안 된다
TEST(overlay_bridge_failed_save_reports_and_leaves_the_seq_alone) {
    Bridge b(freshDir("locked"));
    ControlDoc doc;
    CHECK(b.saveControl(doc) == 1);
    HANDLE held = CreateFileW(b.controlPath().c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);   // 공유 없이 연다
    CHECK(held != INVALID_HANDLE_VALUE);
    CHECK(!b.saveControl(doc).has_value());
    CHECK(doc.seq() == 1);                 // 실패하면 문서의 seq 를 올리지 않는다
    CloseHandle(held);
    CHECK(b.saveControl(doc) == 2);        // 풀리면 다음 번호로 저장된다
    CHECK(b.peekSeq() == 2);
}

// 손으로 고치다 문법을 틀린 control.json 으로 시작하면 오버레이는 기본값으로 뜬다.
// 그 상태에서 무엇이든 바꾸면 파일을 기본값으로 덮으므로, 덮기 전에 사본을 남길 수 있어야 한다
TEST(overlay_bridge_tells_an_unreadable_control_file_from_a_missing_one) {
    Bridge b(freshDir("broken"));
    CHECK(!b.loadControlChecked().unreadable);                       // 파일이 없다: 첫 실행
    CHECK(!b.backupControl());                                       // 남길 것이 없다
    writeText(b.controlPath(), R"({"version":1,"seq":9,"features":{"build":{"enabled":true}})");   // 닫는 괄호가 없다
    const LoadedControl loaded = b.loadControlChecked();
    CHECK(loaded.unreadable && loaded.doc.seq() == 0 && !loaded.doc.build().enabled);
    CHECK(b.backupControl());
    auto kept = readFileShared(b.controlBackupPath());
    CHECK(kept.has_value() && kept->find("\"seq\":9") != std::string::npos);
    writeText(b.controlPath(), R"({"version":1,"seq":9,"features":{"build":{"enabled":true}}})");
    const LoadedControl good = b.loadControlChecked();
    CHECK(!good.unreadable && good.doc.seq() == 9 && good.doc.build().enabled);
}

// 저장은 읽을 수 없는 control.json 을 사본 없이 덮지 않는다. 시작할 때뿐 아니라 게임 중에 파일이 깨진 경우에도 같다
TEST(overlay_bridge_save_keeps_a_copy_of_an_unreadable_file_or_does_not_save) {
    Bridge b(freshDir("guard"));
    writeText(b.controlPath(), "{ broken");
    fs::create_directories(b.controlBackupPath());                   // 사본을 쓸 자리가 막혀 있다(같은 이름의 폴더)
    ControlDoc doc;
    CHECK(!b.saveControl(doc).has_value());                          // 사본을 못 남기면 덮지 않는다
    CHECK(readFileShared(b.controlPath()) == "{ broken" && doc.seq() == 0);
    fs::remove(b.controlBackupPath());
    CHECK(b.saveControl(doc) == 1);
    CHECK(readFileShared(b.controlBackupPath()) == "{ broken");      // 덮기 전의 내용이 사본에 있다
    CHECK(b.peekSeq() == 1);
    writeText(b.controlPath(), "broken again");                      // 게임 중에 밖에서 깨졌다
    CHECK(b.saveControl(doc) == 2);
    CHECK(readFileShared(b.controlBackupPath()) == "broken again");
    CHECK(b.saveControl(doc) == 3);                                  // 읽을 수 있는 파일은 사본을 건드리지 않는다
    CHECK(readFileShared(b.controlBackupPath()) == "broken again");
}

TEST(overlay_bridge_shared_read_returns_the_bytes_or_nothing) {
    Bridge b(freshDir("shared"));
    CHECK(!readFileShared(b.statusPath()).has_value());
    writeText(b.statusPath(), "{\"name\":\"검사대\"}");
    CHECK(readFileShared(b.statusPath()) == "{\"name\":\"검사대\"}");
    writeText(b.statusPath(), "");
    CHECK(readFileShared(b.statusPath()) == "");
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
    MilitarySettings military;
    military.enabled = true;
    military.zeroUpkeep = false;
    doc.setMilitary(military);
    PopulationSettings population;
    population.enabled = true;
    population.multiplier = 3;
    population.targetFamilies = 12;
    population.regionTargets["nus"] = 30;
    doc.setPopulation(population);
    ResourcesSettings resources;
    resources.enabled = true;
    resources.targets["Timber"] = 500;
    resources.regionTargets["gold"]["Timber"] = 2000;
    doc.setResources(resources);
    MercSettings mercenaries;
    mercenaries.enabled = true;
    std::string longest;                                       // 받아 주는 가장 긴 이름: 한글 40자(120바이트)
    for (int i = 0; i < 40; ++i) longest += "가";
    mercenaries.companies = {
        { "토이박스 용병단", { "mercenary_infantry", "mercenary_crossbowmen" }, 3000, "gold", "greencaps", true },
        { "예비대", { "militia" }, 0, std::nullopt, std::nullopt, false },
        { longest, { "militia" }, 10, std::nullopt, std::nullopt, true },
    };
    doc.setMercenaries(mercenaries);
    StorageSettings storage;
    storage.enabled = true;
    storage.limits["99"].generic = 5000;
    storage.limits["69"].generic = 3000;
    storage.limits["69"].pantry = 6000;
    doc.setStorage(storage);
    Json command = makeSetLord("influence", 20000, 1790000000);
    command["id"] = "0123456789abcdef0123456789abcdef";
    Json spawn = makeSpawnSquads("spearMilitia", 2, 1790000000, "nus");
    spawn["id"] = "fedcba9876543210fedcba9876543210";
    doc.setCommands({ command, spawn });
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
