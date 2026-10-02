#pragma once
#include <mutex>
#include <windows.h>

namespace mlt::ov {
// ImGui 는 스레드 안전하지 않다. 화면 스레드(프레임)와 창 스레드(입력)가 이 잠금 아래에서만 ImGui 를 부른다.
// 재진입 잠금이어야 한다: 창 프로시저는 Win32 호출(ReleaseCapture 등) 안에서 같은 스레드로 다시 불린다.
std::recursive_mutex& imguiMutex();
// 게임 창의 창 프로시저를 바꿔 토글 키와 입력을 가로챈다. 이전 프로시저는 이어 부른다.
// 이미 걸어 둔 창이 살아 있으면 아무것도 하지 않는다. 그 창이 없어졌으면 새 창에 다시 건다.
// 게임에서는 화면 출력 후킹이 처음 그릴 때 한 번만 부른다. 다시 거는 경로는 창을 여러 번 만드는 테스트가 쓴다
bool installWndProc(HWND hwnd);
// 창 스레드가 다음 메시지를 기다리지 않고 한 번 돌게 한다(빈 메시지를 보낸다). 어느 스레드에서 불러도 된다
void wakeWindowThread();
// 테스트용: 커서의 화면 좌표를 읽는 함수를 바꾼다(nullptr 이면 GetCursorPos)
void setCursorSource(BOOL(WINAPI* source)(POINT*));
}
