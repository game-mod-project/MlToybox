#include "imgui_layer.h"
#include "input.h"
#include "overlay/ui/app.h"
#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>
#include <vector>

namespace mlt::ov {

namespace {
constexpr UINT kSrvCount = 64;
constexpr float kFontSize = 18.0f;

ID3D12Device* g_device = nullptr;
ID3D12DescriptorHeap* g_srvHeap = nullptr;
std::vector<UINT> g_free;

// ImGui 1.92 의 DX12 백엔드는 텍스처마다 SRV 서술자를 달라고 한다. 작은 자유 목록으로 내준다
void srvAlloc(ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* cpu, D3D12_GPU_DESCRIPTOR_HANDLE* gpu) {
    cpu->ptr = 0;
    gpu->ptr = 0;
    if (g_free.empty() || !g_srvHeap) return;
    const UINT index = g_free.back();
    g_free.pop_back();
    const UINT step = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    cpu->ptr = g_srvHeap->GetCPUDescriptorHandleForHeapStart().ptr + static_cast<SIZE_T>(index) * step;
    gpu->ptr = g_srvHeap->GetGPUDescriptorHandleForHeapStart().ptr + static_cast<UINT64>(index) * step;
}

void srvFree(ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_GPU_DESCRIPTOR_HANDLE) {
    if (!g_srvHeap || !cpu.ptr) return;
    const UINT step = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    g_free.push_back(static_cast<UINT>((cpu.ptr - g_srvHeap->GetCPUDescriptorHandleForHeapStart().ptr) / step));
}

std::string koreanFontPath() {
    char dir[MAX_PATH]{};
    if (!GetWindowsDirectoryA(dir, MAX_PATH)) return {};
    std::string path = std::string(dir) + "\\Fonts\\malgun.ttf";
    return GetFileAttributesA(path.c_str()) == INVALID_FILE_ATTRIBUTES ? std::string() : path;
}
}

bool initImGuiContext(HWND hwnd, std::string& err) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;   // 창 위치·크기는 overlay.json 에 우리가 저장한다
    io.LogFilename = nullptr;

    const std::string font = koreanFontPath();
    bool korean = false;
    if (!font.empty()) korean = io.Fonts->AddFontFromFileTTF(font.c_str()) != nullptr;
    if (!korean) io.Fonts->AddFontDefault();
    app().koreanFont = korean;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.FontSizeBase = kFontSize;
    style.WindowRounding = 4.0f;
    style.FrameRounding = 3.0f;
    style.WindowBorderSize = 1.0f;

    if (!ImGui_ImplWin32_Init(hwnd)) {
        err = "ImGui_ImplWin32_Init";
        return false;
    }

    // 프레임 안에서(화면 스레드가 ImGui 잠금과 App::mutex 를 쥔 채) 불린다. 여기서는 적어 두기만 하고,
    // Windows 의 클립보드 함수는 작업 스레드가 부른다.
    // 시스템 IME 는 쓰지 않는다(게임 창의 IME 는 꺼진 채로 둔다. 한글은 core/hangul 의 조합기가 만든다)
    ImGuiPlatformIO& platform = ImGui::GetPlatformIO();
    platform.Platform_SetImeDataFn = nullptr;
    platform.Platform_SetClipboardTextFn = [](ImGuiContext*, const char* text) {
        App& a = app();
        a.clipboardOut = text ? text : "";
        a.clipboardPending = true;
    };
    return true;
}

bool initImGuiDx12(ID3D12Device* device, ID3D12CommandQueue* queue, int framesInFlight, DXGI_FORMAT format, std::string& err) {
    g_device = device;
    D3D12_DESCRIPTOR_HEAP_DESC desc{};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = kSrvCount;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_srvHeap)))) {
        err = "srv heap";
        return false;
    }
    g_free.clear();
    for (UINT i = kSrvCount; i > 0; --i) g_free.push_back(i - 1);

    ImGui_ImplDX12_InitInfo info;
    info.Device = device;
    info.CommandQueue = queue;
    info.NumFramesInFlight = framesInFlight;
    info.RTVFormat = format;
    info.SrvDescriptorHeap = g_srvHeap;
    info.SrvDescriptorAllocFn = srvAlloc;
    info.SrvDescriptorFreeFn = srvFree;
    if (!ImGui_ImplDX12_Init(&info)) {
        err = "ImGui_ImplDX12_Init";
        g_srvHeap->Release();
        g_srvHeap = nullptr;
        return false;
    }
    return true;
}

void shutdownImGuiDx12() {
    ImGui_ImplDX12_Shutdown();
    if (g_srvHeap) {
        g_srvHeap->Release();
        g_srvHeap = nullptr;
    }
    g_free.clear();
}

ID3D12DescriptorHeap* imguiSrvHeap() {
    return g_srvHeap;
}

}
