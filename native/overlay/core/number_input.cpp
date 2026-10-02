#include "number_input.h"
#include <string>

namespace mlt::ov {

std::optional<int> parseNumber(std::string_view s) {
    std::string digits;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c >= '0' && c <= '9') { digits.push_back(static_cast<char>(c)); ++i; continue; }
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f' || c == ',') { ++i; continue; }
        if (c == 0xEF && i + 2 < s.size() && static_cast<unsigned char>(s[i + 1]) == 0xBC) {
            unsigned char t = static_cast<unsigned char>(s[i + 2]);
            if (t >= 0x90 && t <= 0x99) { digits.push_back(static_cast<char>('0' + (t - 0x90))); i += 3; continue; }   // ０-９
            if (t == 0x8C) { i += 3; continue; }                                                                       // ，
        }
        if (c == 0xE3 && i + 2 < s.size() && static_cast<unsigned char>(s[i + 1]) == 0x80 && static_cast<unsigned char>(s[i + 2]) == 0x80) { i += 3; continue; }   // 전각 공백
        return std::nullopt;
    }
    if (digits.empty() || digits.size() > 10) return std::nullopt;
    long long v = 0;
    for (char d : digits) v = v * 10 + (d - '0');
    if (v > 2147483647LL) return std::nullopt;
    return static_cast<int>(v);
}

}
