#include "test.h"
#include "overlay/render/dx12_hook.h"
#include "overlay/ui/app.h"
#include <d3d12.h>
#include <dxgi1_4.h>
#include <windows.h>

using namespace mlt::ov;

namespace {
// 큐에 맡긴 일이 끝날 때까지 기다린다(게임도 크기를 바꾸기 전에 이렇게 한다)
void waitForGpu(ID3D12Device* device, ID3D12CommandQueue* queue) {
    ID3D12Fence* fence = nullptr;
    if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)))) return;
    HANDLE done = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    queue->Signal(fence, 1);
    fence->SetEventOnCompletion(1, done);
    WaitForSingleObject(done, 5000);
    CloseHandle(done);
    fence->Release();
}
}

// 화면 출력 후킹을 실제 D3D12 스왑체인으로 시험한다(화면 밖에 둔 창, 게임 없음).
// 오버레이가 스스로 꺼진 뒤에도 백버퍼 참조를 쥐고 있으면 게임의 ResizeBuffers 가 DXGI_ERROR_INVALID_CALL 로 실패한다.
TEST(overlay_render_draws_and_never_blocks_the_games_resize) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"mltoybox_overlay_render_test";
    RegisterClassExW(&wc);
    // 후킹은 프로세스의 보이는 최상위 창 가운데 가장 큰 것에만 그린다. 보이게 만들되 화면 밖에 두고 포커스를 가져오지 않는다
    HWND hwnd = CreateWindowExW(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, wc.lpszClassName, L"", WS_POPUP, -32000, -32000, 640, 480,
                                nullptr, nullptr, wc.hInstance, nullptr);
    CHECK(hwnd != nullptr);
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);

    IDXGIFactory4* factory = nullptr;
    ID3D12Device* device = nullptr;
    CHECK(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))));
    if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)))) {
        std::printf("SKIP overlay_render: no D3D12 device on this machine\n");
        factory->Release();
        DestroyWindow(hwnd);
        return;
    }
    ID3D12CommandQueue* queue = nullptr;
    D3D12_COMMAND_QUEUE_DESC qd{};
    qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    CHECK(SUCCEEDED(device->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue))));
    DXGI_SWAP_CHAIN_DESC1 sd{};
    sd.BufferCount = 2;
    sd.Width = 640;
    sd.Height = 480;
    sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.SampleDesc.Count = 1;
    IDXGISwapChain1* swap = nullptr;
    CHECK(SUCCEEDED(factory->CreateSwapChainForHwnd(queue, hwnd, &sd, nullptr, nullptr, &swap)));

    std::string err;
    CHECK(installRenderHooks(err));
    App& a = app();
    a.state = OverlayState::Waiting;   // 후킹을 건 뒤 작업 스레드가 두는 상태
    a.visible = false;                 // 닫힌 채로 시작한다. 준비되면 8초 동안 안내를 그린다

    // 게임처럼 Present 를 부르는 스레드에서 큐를 한 번 실행한다(후킹이 그 큐를 화면 출력용으로 고른다)
    ID3D12CommandAllocator* allocator = nullptr;
    ID3D12GraphicsCommandList* list = nullptr;
    CHECK(SUCCEEDED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator))));
    CHECK(SUCCEEDED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator, nullptr, IID_PPV_ARGS(&list))));
    list->Close();
    ID3D12CommandList* lists[] = { list };
    queue->ExecuteCommandLists(1, lists);

    long long before = a.frames.load();
    for (int i = 0; i < 4; ++i) CHECK(SUCCEEDED(swap->Present(0, 0)));
    CHECK(a.state.load() == OverlayState::Ready);
    CHECK(a.frames.load() > before);                                                        // 안내를 그렸다
    before = a.frames.load();
    CHECK(SUCCEEDED(swap->Present(0, DXGI_PRESENT_TEST)));                                  // 화면에 내지 않는 확인용 호출
    CHECK(a.frames.load() == before);                                                       // 거기에는 그리지 않는다

    waitForGpu(device, queue);
    CHECK(SUCCEEDED(swap->ResizeBuffers(2, 800, 600, DXGI_FORMAT_R8G8B8A8_UNORM, 0)));      // 정상일 때의 크기 변경
    before = a.frames.load();
    for (int i = 0; i < 3; ++i) CHECK(SUCCEEDED(swap->Present(0, 0)));
    CHECK(a.frames.load() > before);                                                        // 바꾼 뒤에도 그린다

    disableOverlay("test: gave up");                                                        // 프레임 예외 등으로 물러난 상태
    CHECK(SUCCEEDED(swap->Present(0, 0)));
    waitForGpu(device, queue);
    const HRESULT resized = swap->ResizeBuffers(2, 640, 480, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
    CHECK(SUCCEEDED(resized));                                                              // 꺼진 뒤에도 게임의 크기 변경을 막지 않는다

    a.state = OverlayState::Starting;
    list->Release();
    allocator->Release();
    swap->Release();
    queue->Release();
    device->Release();
    factory->Release();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
}
