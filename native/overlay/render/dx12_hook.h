#pragma once
#include <string>

namespace mlt::ov {
// DXGI 스왑체인의 Present / ResizeBuffers 와 D3D12 명령 큐의 ExecuteCommandLists 를 후킹한다.
// 게임 코드의 주소가 아니라 DXGI·D3D12 의 가상 함수 표에 걸므로 게임이 업데이트돼도 그대로 쓴다.
bool installRenderHooks(std::string& err);
}
