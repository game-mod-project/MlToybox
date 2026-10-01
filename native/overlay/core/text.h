#pragma once
#include <string>
#include <string_view>

// UTF-8 문자열 도우미. 모드(Lua)와 같은 기준을 쓴다: 공백은 ASCII 공백, 대소문자 변환은 ASCII 만, 길이는 코드 포인트 수
namespace mlt::ov {
std::string trim(std::string_view s);
size_t codePointCount(std::string_view utf8);
std::string lowerAscii(std::string_view s);
bool equalsIgnoreCaseAscii(std::string_view a, std::string_view b);
}
