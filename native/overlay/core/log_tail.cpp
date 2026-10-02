#include "log_tail.h"
#include "bridge.h"
#include "text.h"
#include <system_error>

namespace mlt::ov {

namespace {
constexpr size_t kChunk = 1024 * 1024;   // 한 번에 읽는 가장 큰 크기. 더 남았으면 다음 회차에 이어 읽는다
constexpr size_t kMaxLine = 64 * 1024;   // 이보다 길게 줄바꿈이 없으면 줄로 보지 않는다
constexpr std::string_view kModPrefix = "[Lua] [MLToybox] ";

bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

// "[2026-10-02 17:28:17.2951894] " 꼴이면 시각("17:28:17")과 그 뒤의 위치를 준다
bool splitTime(std::string_view raw, std::string& time, size_t& rest) {
    // [YYYY-MM-DD HH:MM:SS
    if (raw.size() < 21 || raw[0] != '[' || raw[5] != '-' || raw[8] != '-' || raw[11] != ' ' || raw[14] != ':' || raw[17] != ':') return false;
    for (size_t i : { 1, 2, 3, 4, 6, 7, 9, 10, 12, 13, 15, 16, 18, 19 }) {
        if (!isDigit(raw[i])) return false;
    }
    const size_t close = raw.find(']', 20);
    if (close == std::string_view::npos) return false;
    time = std::string(raw.substr(12, 8));
    rest = close + 1;
    if (rest < raw.size() && raw[rest] == ' ') ++rest;
    return true;
}
}

LogLine parseLogLine(std::string_view raw) {
    while (!raw.empty() && (raw.back() == '\r' || raw.back() == '\n')) raw.remove_suffix(1);
    LogLine line;
    size_t rest = 0;
    if (splitTime(raw, line.time, rest)) raw.remove_prefix(rest);
    // 모드가 남긴 줄("[MLToybox] …")과 UE4SS 가 모드를 가리켜 남긴 줄("… mod 'MLToybox'"). 이름이 같은 말로 시작하는 다른 모드는 아니다
    line.mine = raw.find("[MLToybox]") != std::string_view::npos || raw.find("'MLToybox'") != std::string_view::npos;
    if (raw.substr(0, kModPrefix.size()) == kModPrefix) raw.remove_prefix(kModPrefix.size());
    line.text = std::string(raw);
    // 모드는 "ERROR …"로, UE4SS 는 "Error: …"로 적는다. 이름에 Error 가 들어간 줄(ArIsError)은 오류가 아니다
    line.error = line.text.rfind("ERROR ", 0) == 0 || line.text.find("Error: ") != std::string::npos;
    return line;
}

bool LogTail::poll(const std::filesystem::path& path) {
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec) return false;
    bool changed = false;
    if (started_ && size < offset_) {   // 새 파일이다
        started_ = false;
        changed = !lines_.empty();
        lines_.clear();
        partial_.clear();
    }
    if (!started_) {
        started_ = true;
        offset_ = size > firstBytes_ ? size - firstBytes_ : 0;
        skipFirst_ = offset_ > 0;
    }
    if (size == offset_) return changed;
    const auto chunk = readFileFrom(path, offset_, kChunk);
    if (!chunk || chunk->empty()) return changed;
    offset_ += chunk->size();
    partial_ += *chunk;

    size_t begin = 0;
    for (;;) {
        const size_t end = partial_.find('\n', begin);
        if (end == std::string::npos) break;
        const std::string_view raw(partial_.data() + begin, end - begin);
        begin = end + 1;
        if (skipFirst_) {
            skipFirst_ = false;
            continue;
        }
        LogLine line = parseLogLine(raw);
        if (line.text.empty()) continue;
        lines_.push_back(std::move(line));
        changed = true;
    }
    partial_.erase(0, begin);
    if (partial_.size() > kMaxLine) {   // 줄바꿈 없이 이어지는 글. 버리고, 그 줄의 나머지도 줄바꿈까지 건너뛴다
        partial_.clear();
        skipFirst_ = true;
    }
    if (lines_.size() > maxLines_) lines_.erase(lines_.begin(), lines_.end() - static_cast<std::ptrdiff_t>(maxLines_));
    return changed;
}

std::vector<size_t> filterLog(const std::vector<LogLine>& lines, const LogFilter& filter) {
    const std::string needle = lowerAscii(trim(filter.search));
    std::vector<size_t> out;
    for (size_t i = 0; i < lines.size(); ++i) {
        const LogLine& line = lines[i];
        if (filter.mineOnly && !line.mine) continue;
        if (!needle.empty() && lowerAscii(line.text).find(needle) == std::string::npos && line.time.find(needle) == std::string::npos) continue;
        out.push_back(i);
    }
    return out;
}

}
