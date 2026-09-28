#include "runtime.h"
#include "control.h"
#include "hooks.h"
#include "probe.h"
#include "status.h"
#include <MinHook.h>
#include <chrono>
#include <cstring>
#include <fstream>
#include <sstream>
#include <windows.h>

namespace mlt {

long long nowEpochSeconds() {
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

std::optional<std::string> readFileUtf8(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return std::nullopt;
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

bool writeFileAtomic(const std::filesystem::path& path, std::string_view content) {
    auto tmp = path;
    tmp += L".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) return false;
        f.write(content.data(), static_cast<std::streamsize>(content.size()));
        if (!f) return false;
    }
    return MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
}

bool pinModuleContaining(const void* address) {
    if (!address) return false;
    HMODULE pinned = nullptr;
    return GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                              static_cast<LPCWSTR>(address), &pinned) != 0;
}

TextSection mainModuleText() {
    auto base = reinterpret_cast<const uint8_t*>(GetModuleHandleW(nullptr));
    auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    auto nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    auto sec = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec) {
        if (std::memcmp(sec->Name, ".text", 5) == 0) {
            return { base + sec->VirtualAddress, sec->Misc.VirtualSize, reinterpret_cast<uintptr_t>(base) };
        }
    }
    return {};
}

static std::filesystem::path bridgeDirFor(void* selfModule) {
    wchar_t buf[MAX_PATH]{};
    GetModuleFileNameW(static_cast<HMODULE>(selfModule), buf, MAX_PATH);
    // <mod>\native\mltoybox_native.dll -> <mod>\bridge
    return std::filesystem::path(buf).parent_path().parent_path() / L"bridge";
}

namespace {
struct MinHookBackend : HookBackend {
    bool create(void* target, void* detour, void** original, std::string& err) override {
        MH_STATUS s = MH_CreateHook(target, detour, original);
        if (s != MH_OK) { err = MH_StatusToString(s); return false; }
        return true;
    }
    bool setEnabled(void* target, bool on, std::string& err) override {
        MH_STATUS s = on ? MH_EnableHook(target) : MH_DisableHook(target);
        if (s != MH_OK) { err = MH_StatusToString(s); return false; }
        return true;
    }
};
}

void runWorker(void* selfModule) {
    const auto bridge = bridgeDirFor(selfModule);
    HookManager hooks;
    registerFeatures(hooks);
    MinHookBackend backend;
    if (MH_Initialize() == MH_OK) {
        auto text = mainModuleText();
        hooks.installAll(std::span<const uint8_t>(text.data, text.size), reinterpret_cast<uintptr_t>(text.data), backend);
    }
    NativeControl control;
    long long lastProbeSeq = -1;
    for (;;) {
        // 개발용 메모리 프로브: 새 seq 요청만 한 번 처리한다
        if (auto req = readFileUtf8(bridge / L"probe_request.txt")) {
            if (auto p = parseProbeRequest(*req); p && p->seq != lastProbeSeq) {
                lastProbeSeq = p->seq;
                std::string bytes;
                auto out = bridge / (L"probe_" + std::to_wstring(p->seq) + L".bin");
                if (copyProcessMemory(p->address, p->size, bytes)) writeFileAtomic(out, bytes);
                else writeFileAtomic(out.replace_extension(L".err"), "read failed");
            }
        }
        if (auto s = readFileUtf8(bridge / L"control.json")) {
            if (auto c = parseControl(*s)) control = *c;   // 깨진 파일이면 직전 값 유지
        }
        hooks.sync(control, backend);
        writeFileAtomic(bridge / L"native_status.json", renderStatus(nowEpochSeconds(), control.seq, hooks.states()));
        Sleep(1000);
    }
}

}
