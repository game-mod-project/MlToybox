#pragma once
#include <d3d12.h>
#include <dxgiformat.h>
#include <string>
#include <windows.h>

namespace mlt::ov {
// ImGui 컨텍스트, 글꼴(맑은 고딕), 스타일, win32 백엔드. 한 번만 부른다
bool initImGuiContext(HWND hwnd, std::string& err);
// DX12 백엔드와 그 SRV 힙. 백버퍼 형식이나 개수가 바뀌면 shutdown 뒤 다시 부른다
bool initImGuiDx12(ID3D12Device* device, ID3D12CommandQueue* queue, int framesInFlight, DXGI_FORMAT format, std::string& err);
void shutdownImGuiDx12();
ID3D12DescriptorHeap* imguiSrvHeap();
}
