#include "dx12_hook.h"
#include "guard.h"
#include "imgui_layer.h"
#include "input.h"
#include "overlay/core/frame_gate.h"
#include "overlay/ui/app.h"
#include "overlay/ui/window.h"
#include "runtime.h"
#include <MinHook.h>
#include <atomic>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <exception>
#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>
#include <stdexcept>
#include <string>
#include <vector>
#include <windows.h>

// findings "게임 안 오버레이 창 — DX12 후킹 + Dear ImGui (2026-10-01, 스파이크)" 를 따른다.
namespace mlt::ov {
namespace {

using PresentFn = HRESULT(WINAPI*)(IDXGISwapChain*, UINT, UINT);
using ResizeBuffersFn = HRESULT(WINAPI*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
using ExecuteFn = void(WINAPI*)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);

constexpr ULONGLONG kQueueTimeoutMs = 20000;

PresentFn g_origPresent = nullptr;
ResizeBuffersFn g_origResize = nullptr;
ExecuteFn g_origExecute = nullptr;

// 직접(DIRECT) 큐와 그 큐를 마지막으로 실행한 스레드. 이 게임에는 직접 큐가 둘 있고, 화면 출력용이 아닌 큐로 그리면 GPU 크래시가 난다
struct SeenQueue {
    ID3D12CommandQueue* queue;
    DWORD thread;
    unsigned long long order;   // 몇 번째 실행이었는가(클수록 최근)
};
std::mutex g_seenMutex;
SeenQueue g_seen[8] = {};
int g_seenCount = 0;
unsigned long long g_seenOrder = 0;
std::atomic<bool> g_recordQueues{true};

IDXGISwapChain* g_swap = nullptr;   // 우리가 그리는 스왑체인
HWND g_window = nullptr;            // 그 스왑체인의 창
ID3D12Device* g_device = nullptr;
ID3D12CommandQueue* g_queue = nullptr;
ID3D12DescriptorHeap* g_rtvHeap = nullptr;
ID3D12GraphicsCommandList* g_list = nullptr;
std::vector<ID3D12CommandAllocator*> g_allocators;
ID3D12Resource* g_frameBuffer = nullptr;   // 이번 프레임에 그리는 백버퍼. 프레임이 끝나면 놓는다
ID3D12Fence* g_fence = nullptr;            // 우리가 맡긴 명령을 GPU 가 끝냈는지 본다
FrameGate g_gate;                          // 다시 써도 되는 명령 할당자를 고른다(g_allocators 와 칸이 같다)
constexpr size_t kMaxAllocators = 8;
UINT g_bufferCount = 0;
DXGI_FORMAT g_format = DXGI_FORMAT_UNKNOWN;
bool g_contextReady = false;        // ImGui 컨텍스트와 창 프로시저
bool g_initialized = false;
ULONGLONG g_firstPresentTick = 0;

// 구조적 예외가 났을 때 처리기가 풀어야 하는 잠금(guard.h 의 TrackedLock)
// Present 와 ResizeBuffers 가 다른 스레드에서 불려도 남의 잠금을 풀지 않게 스레드마다 둔다
thread_local bool g_holdsImgui = false;
thread_local bool g_holdsApp = false;

// 이 프로세스의 보이는 최상위 창 가운데 가장 큰 것
HWND mainWindow() {
    struct Best {
        HWND hwnd = nullptr;
        long long area = 0;
    } best;
    EnumWindows([](HWND hwnd, LPARAM param) -> BOOL {
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid != GetCurrentProcessId() || !IsWindowVisible(hwnd)) return TRUE;
        RECT r{};
        GetWindowRect(hwnd, &r);
        const long long area = static_cast<long long>(r.right - r.left) * (r.bottom - r.top);
        auto* b = reinterpret_cast<Best*>(param);
        if (area > b->area) {
            b->area = area;
            b->hwnd = hwnd;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&best));
    return best.hwnd;
}

// 백버퍼 참조는 프레임 사이에 들고 있지 않는다. 남아 있으면 게임의 ResizeBuffers 가 실패하는데,
// 오버레이가 스스로 꺼진 뒤에는 놓아 줄 기회가 없다
void dropFrameBuffer() {
    if (g_frameBuffer) {
        g_frameBuffer->Release();
        g_frameBuffer = nullptr;
    }
}

void destroyFrameObjects() {
    dropFrameBuffer();
    if (g_list) { g_list->Release(); g_list = nullptr; }
    for (auto* a : g_allocators) a->Release();
    g_allocators.clear();
    if (g_fence) { g_fence->Release(); g_fence = nullptr; }
    if (g_rtvHeap) { g_rtvHeap->Release(); g_rtvHeap = nullptr; }
}

// 백버퍼 수에 맞춰 RTV 힙, 명령 할당자, 명령 목록을 만든다
bool createFrameObjects(std::string& err) {
    D3D12_DESCRIPTOR_HEAP_DESC rtv{};
    rtv.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtv.NumDescriptors = g_bufferCount;
    if (FAILED(g_device->CreateDescriptorHeap(&rtv, IID_PPV_ARGS(&g_rtvHeap)))) { err = "rtv heap"; return false; }
    for (UINT i = 0; i < g_bufferCount; ++i) {
        ID3D12CommandAllocator* allocator = nullptr;
        if (FAILED(g_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)))) { err = "command allocator"; return false; }
        g_allocators.push_back(allocator);
    }
    if (FAILED(g_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_allocators[0], nullptr, IID_PPV_ARGS(&g_list)))) { err = "command list"; return false; }
    g_list->Close();
    if (FAILED(g_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence)))) { err = "fence"; return false; }
    g_gate.reset(g_allocators.size());
    return true;
}

// Present 를 부르는 지금 스레드에서 실행된 직접 큐
ID3D12CommandQueue* queueForThisThread() {
    const DWORD tid = GetCurrentThreadId();
    std::lock_guard<std::mutex> lock(g_seenMutex);
    // 이 스레드에서 실행된 큐가 둘 이상이면 가장 최근에 실행된 것
    const SeenQueue* latest = nullptr;
    for (int i = 0; i < g_seenCount; ++i) {
        if (g_seen[i].thread == tid && (!latest || g_seen[i].order > latest->order)) latest = &g_seen[i];
    }
    return latest ? latest->queue : nullptr;
}

void tryInit(IDXGISwapChain* swap) {
    DXGI_SWAP_CHAIN_DESC desc{};
    if (FAILED(swap->GetDesc(&desc)) || !desc.OutputWindow) return;
    if (desc.OutputWindow != mainWindow()) return;   // 게임 메인 창의 스왑체인만
    if (!g_firstPresentTick) g_firstPresentTick = GetTickCount64();   // 큐 찾기 제한 시간은 메인 창에 처음 그릴 때부터 잰다

    ID3D12CommandQueue* queue = queueForThisThread();
    if (!queue) {
        if (GetTickCount64() - g_firstPresentTick > kQueueTimeoutMs) disableOverlay("present queue not found");
        return;
    }

    std::string err;
    g_queue = queue;
    if (FAILED(g_queue->GetDevice(IID_PPV_ARGS(&g_device)))) { disableOverlay("init failed: queue device"); return; }
    g_bufferCount = desc.BufferCount;
    g_format = desc.BufferDesc.Format;
    if (!createFrameObjects(err)) { disableOverlay("init failed: " + err); return; }
    if (!g_contextReady) {
        if (!initImGuiContext(desc.OutputWindow, err)) { disableOverlay("init failed: " + err); return; }
        if (!installWndProc(desc.OutputWindow)) { disableOverlay("init failed: window procedure"); return; }
        g_contextReady = true;
    }
    if (!initImGuiDx12(g_device, g_queue, static_cast<int>(g_bufferCount), g_format, err)) { disableOverlay("init failed: " + err); return; }

    g_swap = swap;
    g_window = desc.OutputWindow;
    g_initialized = true;
    g_recordQueues = false;
    notifyOverlayReady();
    // 꺼진 상태가 아니면 Ready 로 둔다(작업 스레드가 Starting 을 Waiting 으로 바꾸는 것과 겹쳐도 Ready 로 끝난다)
    OverlayState current = app().state.load();
    while (current != OverlayState::Disabled && !app().state.compare_exchange_weak(current, OverlayState::Ready)) {
    }
}

// 게임이 스왑체인을 새로 만들었다(화면 모드나 HDR 전환 등). 예전 것에 맞춰 만든 것을 놓고, 큐 찾기부터 다시 한다.
// ImGui 컨텍스트와 창 프로시저는 그대로 쓴다. ImGui 잠금을 쥔 채로 부른다
void forgetSwapChain() {
    shutdownImGuiDx12();
    destroyFrameObjects();
    if (g_device) { g_device->Release(); g_device = nullptr; }
    g_queue = nullptr;
    g_swap = nullptr;
    g_initialized = false;
    g_firstPresentTick = 0;
    {
        std::lock_guard<std::mutex> lock(g_seenMutex);
        g_seenCount = 0;
    }
    g_recordQueues = true;
}

// 이 스왑체인이 우리가 준비해 둔 것과 다른가: 우리 창에 그리는 다른 스왑체인이거나(한 창에는 스왑체인이 하나뿐이므로 게임이 새로 만든 것이다),
// 주소는 같은데 백버퍼 수나 형식이 다르다(예전 것이 사라진 자리에 새 것이 생겼다). 다른 창의 스왑체인은 우리 것이 아니다
enum class SwapChainIs { Ours, Replacement, Unrelated };
SwapChainIs classify(IDXGISwapChain* swap) {
    DXGI_SWAP_CHAIN_DESC desc{};
    if (FAILED(swap->GetDesc(&desc))) return swap == g_swap ? SwapChainIs::Ours : SwapChainIs::Unrelated;
    if (swap != g_swap) return desc.OutputWindow && desc.OutputWindow == g_window ? SwapChainIs::Replacement : SwapChainIs::Unrelated;
    return desc.BufferCount == g_bufferCount && desc.BufferDesc.Format == g_format ? SwapChainIs::Ours : SwapChainIs::Replacement;
}

void renderFrame(IDXGISwapChain* swap) {
    App& a = app();
    const bool openAtStart = a.visible.load();
    {
        TrackedLock imguiLock(imguiMutex(), g_holdsImgui);
        TrackedLock appLock(a.mutex, g_holdsApp);
        ImGui::GetStyle().FontScaleMain = a.scale.load();
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        drawOverlay(a);
        ImGui::Render();
        const ImGuiIO& io = ImGui::GetIO();
        const bool visible = a.visible.load();
        a.wantMouse = visible && io.WantCaptureMouse;
        a.wantKeyboard = visible && (io.WantCaptureKeyboard || io.WantTextInput);
        const bool text = visible && io.WantTextInput;
        a.wantText = text;
        // 화면에서 닫았다(닫기 버튼, 꾸미기 열기). 창 스레드가 눌린 입력을 정리한다
        if (openAtStart && !visible) wakeWindowThread();
    }

    UINT index = 0;
    IDXGISwapChain3* swap3 = nullptr;
    if (FAILED(swap->QueryInterface(IID_PPV_ARGS(&swap3)))) return;
    index = swap3->GetCurrentBackBufferIndex();
    swap3->Release();
    if (index >= g_bufferCount) return;

    // GPU 가 아직 실행 중인 할당자를 Reset 하면 안 된다. 다 쓴 칸을 고르고, 없으면 칸을 늘리고, 그래도 없으면 이번 프레임은 그리지 않는다
    size_t slot = g_gate.firstReady(g_fence->GetCompletedValue());
    if (slot == FrameGate::npos) {
        if (g_allocators.size() >= kMaxAllocators) return;
        ID3D12CommandAllocator* extra = nullptr;
        if (FAILED(g_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&extra)))) return;
        g_allocators.push_back(extra);
        slot = g_gate.add();
    }

    if (FAILED(swap->GetBuffer(index, IID_PPV_ARGS(&g_frameBuffer)))) {
        g_frameBuffer = nullptr;
        return;
    }
    // RTV 서술자는 명령을 기록할 때 읽히므로 같은 칸을 프레임마다 다시 써도 된다
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = g_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtv.ptr += static_cast<SIZE_T>(index) * g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    g_device->CreateRenderTargetView(g_frameBuffer, nullptr, rtv);

    ID3D12CommandAllocator* allocator = g_allocators[slot];
    if (FAILED(allocator->Reset()) || FAILED(g_list->Reset(allocator, nullptr))) {
        dropFrameBuffer();
        return;
    }
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = g_frameBuffer;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    g_list->ResourceBarrier(1, &barrier);
    g_list->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    ID3D12DescriptorHeap* heaps[] = { imguiSrvHeap() };
    g_list->SetDescriptorHeaps(1, heaps);
    {
        TrackedLock imguiLock(imguiMutex(), g_holdsImgui);   // 그리기 자료는 다음 NewFrame 전까지 유효하지만, 텍스처 갱신이 ImGui 상태를 만진다
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), g_list);
    }
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    g_list->ResourceBarrier(1, &barrier);
    if (SUCCEEDED(g_list->Close())) {   // 닫히지 않은 목록을 게임의 큐에 실행하면 장치가 제거된다
        ID3D12CommandList* lists[] = { g_list };
        g_queue->ExecuteCommandLists(1, lists);
        g_queue->Signal(g_fence, g_gate.submitted(slot));
        ++a.frames;
    }
    dropFrameBuffer();
}

void frame(IDXGISwapChain* swap) {
    App& a = app();
    if (!g_initialized) {
        tryInit(swap);
        if (!g_initialized) return;
    }
    const SwapChainIs which = classify(swap);
    if (which == SwapChainIs::Unrelated) return;
    if (which == SwapChainIs::Replacement) {
        // 예전 스왑체인의 마지막 프레임에서 남은 값으로 입력을 삼키지 않는다(그리지 못하는 동안 창은 보이지 않는다)
        a.wantMouse = false;
        a.wantKeyboard = false;
        a.wantText = false;
        {
            TrackedLock imguiLock(imguiMutex(), g_holdsImgui);
            forgetSwapChain();
        }
        tryInit(swap);   // 이 스레드가 큐를 실행한 뒤라야 된다. 안 되면 다음 Present 에서 다시 한다
        if (!g_initialized) return;
    }
    const HRESULT removed = g_device->GetDeviceRemovedReason();
    if (removed != S_OK) {
        disableOverlay("device removed " + hexCode(static_cast<unsigned long>(removed)));
        return;
    }
    bool wanted;
    {
        TrackedLock appLock(a.mutex, g_holdsApp);
        wanted = overlayWantsFrame(a);
    }
    if (!wanted) {
        a.wantMouse = false;
        a.wantKeyboard = false;
        a.wantText = false;
        return;
    }
    renderFrame(swap);
}

// C++ 예외는 여기서 잡는다(소멸자가 돌아 잠금이 풀린다). 구조적 예외는 runGuarded 가 잡는다
void frameThunk(void* swap) {
    try {
        frame(static_cast<IDXGISwapChain*>(swap));
    } catch (const std::exception& e) {
        disableOverlay(std::string("frame exception ") + e.what());
    } catch (...) {
        disableOverlay("frame exception (unknown)");
    }
}

// 구조적 예외 뒤처리: 프레임 코드가 쥐고 있던 잠금을 풀고 오버레이를 끈다
void afterCrash(unsigned long code) {
    if (g_holdsApp) { g_holdsApp = false; app().mutex.unlock(); }
    if (g_holdsImgui) { g_holdsImgui = false; imguiMutex().unlock(); }
    disableOverlay("frame exception " + hexCode(code));
}

HRESULT WINAPI HookedPresent(IDXGISwapChain* swap, UINT sync, UINT flags) {
    // DXGI_PRESENT_TEST 는 화면에 내지 않는 확인용 호출이다. 그리지 않는다
    if (!(flags & DXGI_PRESENT_TEST) && app().state.load() != OverlayState::Disabled) {
        unsigned long code = 0;
        if (!runGuarded(frameThunk, swap, &code)) afterCrash(code);
        dropFrameBuffer();   // 프레임이 예외로 끊겼어도 백버퍼 참조는 남기지 않는다
    }
    return g_origPresent(swap, sync, flags);
}

struct ResizeArgs {
    IDXGISwapChain* swap;
    bool after;
};

// 크기를 바꾼 뒤 백버퍼 개수나 형식이 달라졌으면 그에 맞춰 다시 만든다. 백버퍼 참조는 프레임 사이에 들고 있지 않아 놓을 것이 없다
void resizeThunk(void* p) {
    auto* args = static_cast<ResizeArgs*>(p);
    if (!g_initialized || args->swap != g_swap) return;
    TrackedLock imguiLock(imguiMutex(), g_holdsImgui);
    if (!args->after) {
        dropFrameBuffer();
        return;
    }
    app().applyWindowRect = true;   // 해상도가 줄었으면 창을 화면 안으로 옮긴다
    DXGI_SWAP_CHAIN_DESC desc{};
    if (FAILED(args->swap->GetDesc(&desc))) return;
    if (desc.BufferCount == g_bufferCount && desc.BufferDesc.Format == g_format) return;
    std::string err;
    shutdownImGuiDx12();
    destroyFrameObjects();
    g_bufferCount = desc.BufferCount;
    g_format = desc.BufferDesc.Format;
    if (!createFrameObjects(err) || !initImGuiDx12(g_device, g_queue, static_cast<int>(g_bufferCount), g_format, err)) {
        g_initialized = false;
        throw std::runtime_error(err);
    }
}

void guardedResize(IDXGISwapChain* swap, bool after) {
    if (app().state.load() == OverlayState::Disabled) return;
    ResizeArgs args{ swap, after };
    unsigned long code = 0;
    auto thunk = [](void* p) {
        try {
            resizeThunk(p);
        } catch (const std::exception& e) {
            disableOverlay(std::string("init failed: ") + e.what());
        }
    };
    if (!runGuarded(thunk, &args, &code)) afterCrash(code);
}

HRESULT WINAPI HookedResizeBuffers(IDXGISwapChain* swap, UINT count, UINT width, UINT height, DXGI_FORMAT format, UINT flags) {
    guardedResize(swap, false);
    const HRESULT hr = g_origResize(swap, count, width, height, format, flags);
    guardedResize(swap, true);
    return hr;
}

void WINAPI HookedExecute(ID3D12CommandQueue* queue, UINT count, ID3D12CommandList* const* lists) {
    // 큐를 고른 뒤나 오버레이가 꺼진 뒤에는 기록하지 않는다(게임의 모든 실행이 이 후킹을 지나간다)
    if (g_recordQueues.load() && app().state.load() != OverlayState::Disabled && queue->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT) {
        const DWORD tid = GetCurrentThreadId();
        std::lock_guard<std::mutex> lock(g_seenMutex);
        int slot = -1;
        for (int i = 0; i < g_seenCount; ++i) {
            if (g_seen[i].queue == queue) slot = i;
        }
        if (slot < 0 && g_seenCount < 8) {
            slot = g_seenCount++;
            g_seen[slot].queue = queue;
        }
        if (slot >= 0) {
            g_seen[slot].thread = tid;
            g_seen[slot].order = ++g_seenOrder;
        }
    }
    g_origExecute(queue, count, lists);
}

// 더미 창·장치·스왑체인을 만들어 가상 함수 표에서 함수 주소를 얻는다
bool findVTables(void** present, void** resize, void** execute, std::string& err) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"mltoybox_overlay_probe";
    RegisterClassExW(&wc);
    HWND dummy = CreateWindowExW(0, wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);
    if (!dummy) {
        err = "probe window";
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return false;
    }
    IDXGIFactory4* factory = nullptr;
    ID3D12Device* device = nullptr;
    ID3D12CommandQueue* queue = nullptr;
    IDXGISwapChain1* swap = nullptr;
    bool ok = false;
    do {
        if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) { err = "dxgi factory"; break; }
        if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)))) { err = "d3d12 device"; break; }
        D3D12_COMMAND_QUEUE_DESC qd{};
        qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        if (FAILED(device->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue)))) { err = "command queue"; break; }
        DXGI_SWAP_CHAIN_DESC1 sd{};
        sd.BufferCount = 2;
        sd.Width = 100;
        sd.Height = 100;
        sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        sd.SampleDesc.Count = 1;
        if (FAILED(factory->CreateSwapChainForHwnd(queue, dummy, &sd, nullptr, nullptr, &swap))) { err = "probe swapchain"; break; }
        void** swapTable = *reinterpret_cast<void***>(swap);
        void** queueTable = *reinterpret_cast<void***>(queue);
        *present = swapTable[8];     // IDXGISwapChain::Present
        *resize = swapTable[13];     // IDXGISwapChain::ResizeBuffers
        *execute = queueTable[10];   // ID3D12CommandQueue::ExecuteCommandLists
        ok = true;
    } while (false);
    if (swap) swap->Release();
    if (queue) queue->Release();
    if (device) device->Release();
    if (factory) factory->Release();
    DestroyWindow(dummy);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return ok;
}

bool hook(void* target, void* detour, void** original, const char* name, std::string& err) {
    const MH_STATUS created = MH_CreateHook(target, detour, original);
    if (created != MH_OK) {
        err = std::string(name) + ": " + MH_StatusToString(created);
        return false;
    }
    MH_STATUS enabled;
    {
        HookGate gate;   // 네이티브 DLL 의 MinHook 과 같은 때에 스레드를 멈추지 않는다(runtime.h)
        enabled = MH_EnableHook(target);
    }
    if (enabled != MH_OK) {
        err = std::string(name) + ": " + MH_StatusToString(enabled);
        return false;
    }
    return true;
}
}

bool installRenderHooks(std::string& err) {
    void* present = nullptr;
    void* resize = nullptr;
    void* execute = nullptr;
    if (!findVTables(&present, &resize, &execute, err)) return false;
    // 이 DLL 의 MinHook 은 네이티브 DLL 의 것과 다른 사본이다. 이미 초기화돼 있는 경우는 같은 프로세스에서 두 번 부르는 테스트뿐이다
    const MH_STATUS init = MH_Initialize();
    if (init != MH_OK && init != MH_ERROR_ALREADY_INITIALIZED) {
        err = std::string("minhook: ") + MH_StatusToString(init);
        return false;
    }
    // 큐 기록을 먼저 켜야 Present 가 처음 불릴 때 큐를 알 수 있다
    return hook(execute, reinterpret_cast<void*>(HookedExecute), reinterpret_cast<void**>(&g_origExecute), "ExecuteCommandLists", err)
        && hook(resize, reinterpret_cast<void*>(HookedResizeBuffers), reinterpret_cast<void**>(&g_origResize), "ResizeBuffers", err)
        && hook(present, reinterpret_cast<void*>(HookedPresent), reinterpret_cast<void**>(&g_origPresent), "Present", err);
}

}
