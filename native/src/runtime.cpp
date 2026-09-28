#include "runtime.h"
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

void runWorker(void* selfModule) {
    const auto bridge = bridgeDirFor(selfModule);
    for (;;) {
        std::string status = "{\"version\":1,\"heartbeat\":" + std::to_string(nowEpochSeconds()) + ",\"appliedSeq\":-1}";
        writeFileAtomic(bridge / L"native_status.json", status);
        Sleep(1000);
    }
}

}
