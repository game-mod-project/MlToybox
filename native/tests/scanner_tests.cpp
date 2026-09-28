#include "test.h"
#include "scanner.h"
#include <array>

using namespace mlt;

TEST(parse_pattern_with_wildcards) {
    auto p = parsePattern("48 8B ?? 05 ? ff");
    CHECK(p.has_value());
    CHECK(p->bytes.size() == 6);
    CHECK(p->bytes[0] == 0x48 && p->bytes[2] == -1 && p->bytes[4] == -1 && p->bytes[5] == 0xFF);
}

TEST(parse_pattern_rejects_bad_input) {
    CHECK(!parsePattern("").has_value());
    CHECK(!parsePattern("?? 48").has_value());   // 첫 바이트 와일드카드 금지
    CHECK(!parsePattern("4G").has_value());
    CHECK(!parsePattern("488B").has_value());    // 공백 필수
    CHECK(!parsePattern("4").has_value());
}

TEST(scan_unique_finds_single_match) {
    std::array<uint8_t, 10> hay{ 0x90, 0x48, 0x8B, 0x05, 0x11, 0x22, 0xC3, 0x90, 0x90, 0x90 };
    auto r = scanUnique(hay, "48 8B 05 ?? ?? C3");
    CHECK(r.result == ScanResult::Unique);
    CHECK(r.offset == 1);
}

TEST(scan_unique_reports_not_found_and_ambiguous) {
    std::array<uint8_t, 8> hay{ 0x48, 0x8B, 0xC3, 0x90, 0x48, 0x8B, 0xC3, 0x90 };
    CHECK(scanUnique(hay, "48 8B C3").result == ScanResult::Ambiguous);
    CHECK(scanUnique(hay, "48 8B C4").result == ScanResult::NotFound);
    CHECK(scanUnique(hay, "zz").result == ScanResult::BadPattern);
}

TEST(scan_handles_match_at_end_and_short_haystack) {
    std::array<uint8_t, 4> hay{ 0x90, 0x90, 0xAB, 0xCD };
    CHECK(scanUnique(hay, "AB CD").offset == 2);
    CHECK(scanUnique(std::span<const uint8_t>(hay.data(), 1), "AB CD").result == ScanResult::NotFound);
}
