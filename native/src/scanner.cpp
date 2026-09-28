#include "scanner.h"
#include <cctype>

namespace mlt {

static int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::optional<Pattern> parsePattern(std::string_view text) {
    Pattern p;
    size_t i = 0;
    while (i < text.size()) {
        if (text[i] == ' ') { ++i; continue; }
        if (text[i] == '?') {
            p.bytes.push_back(-1);
            ++i;
            if (i < text.size() && text[i] == '?') ++i;
        } else {
            if (i + 1 >= text.size()) return std::nullopt;
            int hi = hexValue(text[i]), lo = hexValue(text[i + 1]);
            if (hi < 0 || lo < 0) return std::nullopt;
            p.bytes.push_back(static_cast<int16_t>(hi * 16 + lo));
            i += 2;
        }
        if (i < text.size() && text[i] != ' ') return std::nullopt;
    }
    if (p.bytes.empty() || p.bytes.front() < 0) return std::nullopt;
    return p;
}

std::vector<size_t> findAll(std::span<const uint8_t> hay, const Pattern& p, size_t maxHits) {
    std::vector<size_t> hits;
    const size_t n = p.bytes.size();
    if (n == 0 || hay.size() < n) return hits;
    const auto first = static_cast<uint8_t>(p.bytes[0]);
    for (size_t i = 0; i + n <= hay.size(); ++i) {
        if (hay[i] != first) continue;
        bool ok = true;
        for (size_t j = 1; j < n; ++j) {
            const int16_t b = p.bytes[j];
            if (b >= 0 && hay[i + j] != static_cast<uint8_t>(b)) { ok = false; break; }
        }
        if (ok) {
            hits.push_back(i);
            if (hits.size() >= maxHits) break;
        }
    }
    return hits;
}

ScanOutcome scanUnique(std::span<const uint8_t> hay, std::string_view text) {
    auto p = parsePattern(text);
    if (!p) return { ScanResult::BadPattern, 0 };
    auto hits = findAll(hay, *p, 2);
    if (hits.empty()) return { ScanResult::NotFound, 0 };
    if (hits.size() > 1) return { ScanResult::Ambiguous, 0 };
    return { ScanResult::Unique, hits[0] };
}

}
