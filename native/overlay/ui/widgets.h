#pragma once
#include "overlay/core/units.h"
#include <optional>
#include <string>
#include <vector>

// 여러 탭이 쓰는 화면 조각. 모두 ImGui 프레임 안에서 부른다
namespace mlt::ov {
// 글자 배율을 곱한 길이
float scaled(float px);

// 숫자 칸. 포커스가 없을 때는 value 를 보여 준다. 편집을 끝내면(Enter 또는 포커스 이동) 입력을 해석해
// [min, max] 로 맞춘 값을 value 에 넣고, 값이 바뀌었으면 true 를 돌려준다. 해석할 수 없는 입력은 버린다.
// 전각 숫자·공백·쉼표를 허용한다(core/number_input).
bool numberField(const char* id, int& value, int min, int max, float width);

// 빈칸을 허용하는 숫자 칸. 빈칸으로 두고 편집을 끝내면 값 없음이 된다(자원 목표: 빈칸 = 관리하지 않음)
bool optionalNumberField(const char* id, std::optional<int>& value, int min, int max, float width);

// 선택지에서 하나 고르기. 고른 줄이 바뀌면 true. index 가 범위를 벗어나 있으면 첫 줄로 맞춘다
bool comboOptions(const char* id, const std::vector<ScopeOption>& options, int& index, float width);

// 글자 칸(UTF-8). 내용이 바뀌면 true. maxBytes 는 받을 수 있는 가장 긴 바이트 수(최대 255).
// 한글 모드에서는 영문 글쇠를 두벌식으로 조합한다(core/hangul). 시스템 IME 는 쓰지 않는다
bool textField(const char* id, std::string& value, size_t maxBytes, float width);

// 글자 칸의 한/영 상태를 보여 주고, 누르면 바꾼다(한/영 글쇠로도 바뀐다). 누른 뒤에는 글자 칸으로 커서를 돌려준다
void hangulModeButton();
// 글자 칸이 한글 모드인가
bool hangulModeOn();

// 게임 상태가 필요한 부분이 비어 있을 때의 안내 문구
void needGameText();
}
