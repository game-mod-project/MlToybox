#include "test.h"
#include "overlay/core/log_tail.h"
#include <filesystem>
#include <fstream>

using namespace mlt::ov;

namespace {
std::filesystem::path tempLog(const char* name) {
    const auto dir = std::filesystem::temp_directory_path() / "mltoybox_log_tail_tests";
    std::filesystem::create_directories(dir);
    const auto path = dir / name;
    std::filesystem::remove(path);
    return path;
}

void append(const std::filesystem::path& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary | std::ios::app);
    out << text;
}

std::vector<std::string> texts(const std::vector<LogLine>& lines) {
    std::vector<std::string> out;
    for (const LogLine& line : lines) out.push_back(line.text);
    return out;
}
}

TEST(overlay_log_line_splits_the_time_and_marks_mod_lines) {
    LogLine line = parseLogLine("[2026-10-02 17:28:17.2951894] [Lua] [MLToybox] native: loaded");
    CHECK(line.time == "17:28:17" && line.text == "native: loaded" && line.mine && !line.error);
    line = parseLogLine("[2026-10-02 17:28:17.3071377] [Lua] [MLToybox] ERROR resources: attempt to index a nil value");
    CHECK(line.time == "17:28:17" && line.text == "ERROR resources: attempt to index a nil value" && line.mine && line.error);
    // 다른 줄은 시각만 떼고 그대로 둔다
    line = parseLogLine("[2026-10-02 17:28:08.3385472] UE4SS - v3.0.1 Beta #0 - Git SHA #f6d5f942");
    CHECK(line.time == "17:28:08" && line.text == "UE4SS - v3.0.1 Beta #0 - Git SHA #f6d5f942" && !line.mine && !line.error);
    line = parseLogLine("[2026-10-02 17:28:16.8493190] Starting Lua mod 'MLToybox'");
    CHECK(line.mine && line.text == "Starting Lua mod 'MLToybox'");
    // 이름이 MLToybox 로 시작하는 다른 모드(개발용 MLToyboxLab, MLToyboxDump)의 줄은 모드의 줄이 아니다
    CHECK(!parseLogLine("[2026-10-02 18:19:09.0081828] [Lua] [MLToyboxLab] loaded, lab=E:\\x").mine);
    CHECK(!parseLogLine("[2026-10-02 18:19:09.0056889] Starting Lua mod 'MLToyboxLab'").mine);
    CHECK(parseLogLine("[2026-10-02 18:19:09.0055536] [Lua] [MLToybox] loaded (scripts=E:\\Mods\\MLToybox\\Scripts)").mine);
    line = parseLogLine("[2026-10-02 17:30:00.0000001] [Lua] Error: [string \"main.lua\"]:12: boom");
    CHECK(line.error && !line.mine);
    // 이름에 Error 가 들어갈 뿐인 줄은 오류가 아니다
    CHECK(!parseLogLine("[2026-10-02 17:28:16.7471407] FArchiveState::ArIsError = 0x29").error);
    // 시각이 없는 줄(여러 줄짜리 메시지의 뒷줄)
    line = parseLogLine("##### MEMBER OFFSETS START (Coalesced) #####\r");
    CHECK(line.time.empty() && line.text == "##### MEMBER OFFSETS START (Coalesced) #####");
    CHECK(parseLogLine("").text.empty() && parseLogLine("[broken").text == "[broken");
}

TEST(overlay_log_tail_reads_only_what_was_appended) {
    const auto path = tempLog("grow.log");
    LogTail tail;
    CHECK(!tail.poll(path) && tail.lines().empty());   // 파일이 아직 없다
    append(path, "[2026-10-02 17:28:08.1] first\n[2026-10-02 17:28:09.1] [Lua] [MLToybox] second\n");
    CHECK(tail.poll(path));
    CHECK(texts(tail.lines()) == (std::vector<std::string>{ "first", "second" }));
    CHECK(!tail.poll(path));                           // 바뀐 것이 없다
    // 줄이 끝나지 않았으면(모드가 쓰는 중) 줄바꿈이 올 때까지 기다린다
    append(path, "[2026-10-02 17:28:10.1] thi");
    CHECK(!tail.poll(path) && tail.lines().size() == 2);
    append(path, "rd\n\n[2026-10-02 17:28:11.1] fourth\n");
    CHECK(tail.poll(path));
    CHECK(texts(tail.lines()) == (std::vector<std::string>{ "first", "second", "third", "fourth" }));   // 빈 줄은 버린다
}

TEST(overlay_log_tail_keeps_the_newest_lines_only) {
    const auto path = tempLog("many.log");
    for (int i = 0; i < 10; ++i) append(path, "line " + std::to_string(i) + "\n");
    LogTail tail(4);
    CHECK(tail.poll(path));
    CHECK(texts(tail.lines()) == (std::vector<std::string>{ "line 6", "line 7", "line 8", "line 9" }));
    append(path, "line 10\n");
    CHECK(tail.poll(path));
    CHECK(texts(tail.lines()) == (std::vector<std::string>{ "line 7", "line 8", "line 9", "line 10" }));
}

TEST(overlay_log_tail_starts_near_the_end_of_a_big_file) {
    const auto path = tempLog("big.log");
    for (int i = 0; i < 200; ++i) append(path, "0123456789 line " + std::to_string(i) + "\n");
    LogTail tail(1000, 64);   // 처음에는 끝의 64바이트만
    CHECK(tail.poll(path));
    // 중간에서 시작했으니 잘린 첫 줄은 버린다
    CHECK(!tail.lines().empty() && tail.lines().size() < 5);
    CHECK(tail.lines().front().text.rfind("0123456789 line 19", 0) == 0 && tail.lines().back().text == "0123456789 line 199");
}

TEST(overlay_log_tail_starts_over_when_the_file_was_replaced) {
    const auto path = tempLog("replaced.log");
    append(path, "old one\nold two\nold three\n");
    LogTail tail;
    CHECK(tail.poll(path) && tail.lines().size() == 3);
    std::filesystem::remove(path);
    append(path, "new one\n");   // 더 짧은 새 파일
    CHECK(tail.poll(path));
    CHECK(texts(tail.lines()) == (std::vector<std::string>{ "new one" }));
}

// 줄바꿈 없이 길게 이어지는 글은 줄로 보지 않고 버린다(끝나지 않는 줄을 한없이 쌓아 두지 않는다)
TEST(overlay_log_tail_drops_a_line_that_never_ends) {
    const auto path = tempLog("endless.log");
    append(path, "first\n" + std::string(70000, 'x'));
    LogTail tail;
    CHECK(tail.poll(path));
    CHECK(texts(tail.lines()) == (std::vector<std::string>{ "first" }));
    append(path, "tail of the long one\nnext\n");
    CHECK(tail.poll(path));
    CHECK(texts(tail.lines()) == (std::vector<std::string>{ "first", "next" }));
}

TEST(overlay_log_filter_by_owner_and_search) {
    const std::vector<LogLine> lines = {
        parseLogLine("[2026-10-02 17:28:08.1] UE4SS - v3.0.1"),
        parseLogLine("[2026-10-02 17:28:17.1] [Lua] [MLToybox] native: loaded"),
        parseLogLine("[2026-10-02 17:28:18.1] [Lua] [MLToybox] 명령 spawnSquads 성공"),
    };
    LogFilter f;
    f.mineOnly = true;
    CHECK(filterLog(lines, f) == (std::vector<size_t>{ 1, 2 }));
    f.search = "NATIVE";   // 영문 대소문자를 가리지 않는다
    CHECK(filterLog(lines, f) == (std::vector<size_t>{ 1 }));
    f.search = " 명령 ";
    CHECK(filterLog(lines, f) == (std::vector<size_t>{ 2 }));
    f.mineOnly = false;
    f.search.clear();
    CHECK(filterLog(lines, f).size() == 3);
    f.search = "17:28:08";   // 시각으로도 찾는다
    CHECK(filterLog(lines, f) == (std::vector<size_t>{ 0 }));
}

// [로그] 탭은 프레임마다 그려진다. 줄이나 조건이 바뀌었을 때만 다시 거른다(프레임마다 1000줄을 다시 거르지 않는다)
TEST(overlay_log_view_filters_again_only_when_the_lines_or_the_filter_change) {
    std::vector<LogLine> lines = { { "", "a", true, false }, { "", "b", false, false } };
    LogView view;
    CHECK(view.shown(lines, 1, { true, "" }) == std::vector<size_t>{ 0 });
    lines.push_back({ "", "c", true, false });
    CHECK(view.shown(lines, 1, { true, "" }) == std::vector<size_t>{ 0 });            // 줄 목록의 판이 같다: 다시 거르지 않는다
    CHECK(view.shown(lines, 2, { true, "" }) == (std::vector<size_t>{ 0, 2 }));       // 줄이 바뀌었다
    CHECK(view.shown(lines, 2, { false, "" }).size() == 3);                           // 조건이 바뀌었다
    CHECK(view.shown(lines, 2, { false, "B" }) == std::vector<size_t>{ 1 });
}