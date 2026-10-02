#pragma once
#include <optional>
#include <string>

namespace mlt::ov {
// 글(UTF-8)을 Windows 클립보드에 쓴다. 작업 스레드에서 부른다.
// ImGui 의 기본 처리는 프레임을 그리는 스레드에서 잠금을 쥔 채 클립보드를 비운다. 클립보드 주인이 게임 창이면
// Windows 가 그 창에 메시지를 보내고, 창 스레드가 그때 ImGui 잠금을 기다리고 있으면 서로 멈춘다.
bool writeClipboardText(const std::string& utf8);
// 시스템 클립보드의 글(UTF-8). 열지 못했으면 값 없음(잠깐 뒤에 다시 한다), 글이 아니면 빈 글.
// 클립보드를 가진 프로그램이 글을 그때 만들어 주는 방식이면 그 프로그램의 응답을 기다린다. 그래서 잠금을 쥔 스레드에서는 부르지 않는다
std::optional<std::string> readClipboardText();
// 클립보드가 바뀔 때마다 달라지는 번호(클립보드를 열지 않는다)
unsigned long clipboardSequence();
}
