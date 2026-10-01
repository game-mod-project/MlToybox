#pragma once
#include <optional>
#include <string_view>

namespace mlt::ov {
// 숫자 칸 입력 해석: 전각 숫자(０-９), 공백, 쉼표(, ，)를 허용한다. 0 이상의 int 가 아니면 값 없음 (패널의 NumberInput.Parse 와 같다)
std::optional<int> parseNumber(std::string_view utf8);
}
