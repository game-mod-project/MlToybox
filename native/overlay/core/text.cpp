#include "text.h"

namespace mlt::ov {

static bool isSpace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}

std::string trim(std::string_view s) {
    size_t b = 0, e = s.size();
    while (b < e && isSpace(s[b])) ++b;
    while (e > b && isSpace(s[e - 1])) --e;
    return std::string(s.substr(b, e - b));
}

// 이어지는 바이트(10xxxxxx)가 아닌 바이트를 센다. 깨진 UTF-8 도 멈추지 않고 센다
size_t codePointCount(std::string_view utf8) {
    size_t n = 0;
    for (unsigned char c : utf8) {
        if ((c & 0xC0) != 0x80) ++n;
    }
    return n;
}

std::string lowerAscii(std::string_view s) {
    std::string out(s);
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return out;
}

bool equalsIgnoreCaseAscii(std::string_view a, std::string_view b) {
    return lowerAscii(a) == lowerAscii(b);
}

}
