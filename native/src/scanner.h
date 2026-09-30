#pragma once
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace mlt {
struct Pattern { std::vector<int16_t> bytes; };
std::optional<Pattern> parsePattern(std::string_view text);
std::vector<size_t> findAll(std::span<const uint8_t> haystack, const Pattern& pattern, size_t maxHits);
enum class ScanResult { Unique, NotFound, Ambiguous, BadPattern };
struct ScanOutcome { ScanResult result; size_t offset; };
ScanOutcome scanUnique(std::span<const uint8_t> haystack, std::string_view pattern);
}
