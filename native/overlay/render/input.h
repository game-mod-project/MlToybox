#pragma once
#include <mutex>
#include <windows.h>

namespace mlt::ov {
// ImGui 는 스레드 안전하지 않다. 화면 스레드(프레임)와 창 스레드(입력)가 이 잠금 아래에서만 ImGui 를 부른다.
// 재진입 잠금이어야 한다: 창 프로시저는 Win32 호출(ReleaseCapture 등) 안에서 같은 스레드로 다시 불린다.
std::recursive_mutex& imguiMutex();
// 게임 창의 창 프로시저를 바꿔 토글 키와 입력을 가로챈다. 이전 프로시저는 이어 부른다
bool installWndProc(HWND hwnd);
}
