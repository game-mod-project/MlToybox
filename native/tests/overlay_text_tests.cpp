#include "test.h"
#include "overlay/core/number_input.h"
#include "overlay/core/text.h"

using namespace mlt::ov;

TEST(overlay_trim_removes_ascii_space_at_both_ends_only) {
    CHECK(trim("  \t토이박스 용병단\r\n ") == "토이박스 용병단");
    CHECK(trim("   ") == "");
    CHECK(trim("") == "");
    CHECK(trim("a  b") == "a  b");
}

TEST(overlay_code_point_count_counts_characters_not_bytes) {
    CHECK(codePointCount("") == 0);
    CHECK(codePointCount("abc") == 3);
    CHECK(codePointCount("검사대") == 3);                 // 9 바이트
    CHECK(codePointCount("\xF0\x9F\x98\x80" "a") == 2);    // 4 바이트 문자 하나 + a
    std::string forty;
    for (int i = 0; i < 40; ++i) forty += "가";
    CHECK(codePointCount(forty) == 40);
}

TEST(overlay_lower_ascii_leaves_other_bytes_alone) {
    CHECK(lowerAscii("Greencaps") == "greencaps");
    CHECK(lowerAscii("검사대 A") == "검사대 a");
    CHECK(equalsIgnoreCaseAscii("HILDEBOLTS_ARMY", "hildebolts_army"));
    CHECK(!equalsIgnoreCaseAscii("alpha", "alpha "));
}

TEST(overlay_parse_number_accepts_fullwidth_digits_spaces_and_commas) {
    CHECK(parseNumber("90000") == 90000);
    CHECK(parseNumber("９００００") == 90000);
    CHECK(parseNumber(" 1,000 ") == 1000);
    CHECK(parseNumber("１，０００") == 1000);
    CHECK(parseNumber("1　000") == 1000);                 // 전각 공백
    CHECK(parseNumber("0") == 0);
    CHECK(parseNumber("2147483647") == 2147483647);
}

TEST(overlay_parse_number_rejects_everything_else) {
    CHECK(!parseNumber(""));
    CHECK(!parseNumber("   "));
    CHECK(!parseNumber("-5"));
    CHECK(!parseNumber("12a"));
    CHECK(!parseNumber("1.5"));
    CHECK(!parseNumber("2147483648"));
    CHECK(!parseNumber("99999999999"));
    CHECK(!parseNumber("가"));
}
