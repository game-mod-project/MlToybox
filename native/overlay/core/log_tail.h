#pragma once
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

// [로그] 탭: UE4SS.log 의 끝을 따라 읽는다. 모드는 그 파일에 "[MLToybox] …" 줄을 남긴다(core/log.lua).
namespace mlt::ov {

struct LogLine {
    std::string time;     // "17:28:17". 시각이 없는 줄이면 빈 문자열
    std::string text;     // 시각을 뺀 나머지. 모드의 줄은 "[Lua] [MLToybox] " 도 뺀다
    bool mine = false;    // MLToybox 가 남겼거나 MLToybox 를 가리키는 줄
    bool error = false;

    bool operator==(const LogLine&) const = default;
};

// "[2026-10-02 17:28:17.2951894] [Lua] [MLToybox] native: loaded" -> { "17:28:17", "native: loaded", mine }
LogLine parseLogLine(std::string_view raw);

class LogTail {
public:
    // maxLines: 기억할 줄 수(오래된 줄부터 버린다). firstBytes: 처음 읽을 때 파일 끝에서 거슬러 올라갈 바이트 수
    explicit LogTail(size_t maxLines = 1000, size_t firstBytes = 256 * 1024) : maxLines_(maxLines), firstBytes_(firstBytes) {}

    // 파일에 새로 붙은 부분을 읽는다. 줄이 달라졌으면 true. 파일이 없으면 가진 줄을 그대로 둔다.
    // 파일이 줄어들었으면(새로 만들어졌으면) 처음부터 다시 읽는다. 끝나지 않은 줄은 줄바꿈이 올 때까지 두고, 빈 줄은 버린다
    bool poll(const std::filesystem::path& path);
    const std::vector<LogLine>& lines() const { return lines_; }

private:
    size_t maxLines_;
    size_t firstBytes_;
    bool started_ = false;
    unsigned long long offset_ = 0;
    bool skipFirst_ = false;      // 파일 중간에서 읽기 시작했다. 첫 줄은 잘린 줄이다
    std::string partial_;         // 아직 줄바꿈이 오지 않은 끝부분
    std::vector<LogLine> lines_;
};

struct LogFilter {
    bool mineOnly = false;
    std::string search;   // 시각과 글에서 찾는다. 앞뒤 공백은 빼고, 영문 대소문자는 가리지 않는다
};
// 조건에 맞는 줄의 번호(오래된 줄부터)
std::vector<size_t> filterLog(const std::vector<LogLine>& lines, const LogFilter& filter);
}
