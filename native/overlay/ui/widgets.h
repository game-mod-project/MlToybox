#pragma once

namespace mlt::ov {
// 숫자 칸. 포커스가 없을 때는 value 를 보여 준다. 편집을 끝내면(Enter 또는 포커스 이동) 입력을 해석해
// [min, max] 로 맞춘 값을 value 에 넣고, 값이 바뀌었으면 true 를 돌려준다. 해석할 수 없는 입력은 버린다.
// 전각 숫자·공백·쉼표를 허용한다(core/number_input).
bool numberField(const char* id, int& value, int min, int max, float width);

// 게임 상태가 필요한 부분이 비어 있을 때의 안내 문구
void needGameText();
}
