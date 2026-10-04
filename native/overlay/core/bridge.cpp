#include "bridge.h"
#include "runtime.h"
#include <algorithm>
#include <chrono>
#include <system_error>
#include <thread>
#include <windows.h>

namespace mlt::ov {

std::optional<std::string> readFileShared(const std::filesystem::path& path) {
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return std::nullopt;
    std::string out;
    char buf[16384];
    for (;;) {
        DWORD got = 0;
        if (!ReadFile(file, buf, sizeof(buf), &got, nullptr)) {
            CloseHandle(file);
            return std::nullopt;
        }
        if (got == 0) break;
        out.append(buf, got);
    }
    CloseHandle(file);
    return out;
}

std::optional<std::string> readFileFrom(const std::filesystem::path& path, unsigned long long offset, size_t maxBytes) {
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return std::nullopt;
    LARGE_INTEGER to{};
    to.QuadPart = static_cast<LONGLONG>(offset);
    bool ok = SetFilePointerEx(file, to, nullptr, FILE_BEGIN) != 0;
    std::string out;
    char buf[16384];
    while (ok && out.size() < maxBytes) {
        DWORD got = 0;
        const DWORD want = static_cast<DWORD>(std::min(sizeof(buf), maxBytes - out.size()));
        ok = ReadFile(file, buf, want, &got, nullptr) != 0;
        if (!ok || got == 0) break;
        out.append(buf, got);
    }
    CloseHandle(file);
    if (!ok) return std::nullopt;
    return out;
}

ControlDoc Bridge::loadControl() const {
    return loadControlChecked().doc;
}

LoadedControl Bridge::loadControlChecked() const {
    LoadedControl loaded;
    auto text = readFileShared(controlPath());
    if (!text) return loaded;
    if (auto doc = ControlDoc::tryParse(*text)) loaded.doc = std::move(*doc);
    else loaded.unreadable = true;
    return loaded;
}

bool Bridge::backupControl() const {
    std::error_code ec;
    return std::filesystem::copy_file(controlPath(), controlBackupPath(), std::filesystem::copy_options::overwrite_existing, ec);
}

std::optional<long long> Bridge::peekSeq() const {
    auto text = readFileShared(controlPath());
    if (!text) return std::nullopt;
    Json root = Json::parse(text->begin(), text->end(), nullptr, false);
    if (root.is_discarded() || !root.is_object()) return std::nullopt;
    auto it = root.find("seq");
    if (it == root.end() || !it->is_number()) return std::nullopt;
    return static_cast<long long>(it->get<double>());
}

std::optional<long long> Bridge::saveControl(ControlDoc& doc) const {
    std::error_code ec;
    std::filesystem::create_directories(dir_, ec);
    // 읽을 수 없는 파일(손으로 고치다 문법을 틀린 것)은 사본을 남긴 뒤에만 덮는다. 사본을 못 남기면 저장하지 않는다.
    // 시작할 때뿐 아니라 게임 중에 밖에서 깨진 경우에도 여기서 걸린다
    if (loadControlChecked().unreadable && !backupControl()) return std::nullopt;
    const long long previous = doc.seq();
    const long long next = std::max(peekSeq().value_or(0), previous) + 1;
    doc.setSeq(next);
    const std::string text = doc.dump();
    // 모드가 파일을 읽는 순간과 겹치면 이름 바꾸기가 실패할 수 있다. 잠깐 쉬고 다시 한다
    for (int attempt = 1; attempt <= 5; ++attempt) {
        if (writeFileAtomic(controlPath(), text)) return next;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    doc.setSeq(previous);
    return std::nullopt;
}

LoadedSettings Bridge::loadSettingsChecked() const {
    LoadedSettings loaded;
    auto text = readFileShared(settingsPath());
    if (!text) return loaded;
    if (auto settings = tryParseSettings(*text)) loaded.settings = std::move(*settings);
    else loaded.unreadable = true;
    return loaded;
}

bool Bridge::backupSettings() const {
    std::error_code ec;
    return std::filesystem::copy_file(settingsPath(), settingsBackupPath(), std::filesystem::copy_options::overwrite_existing, ec);
}

std::optional<StatusDoc> Bridge::readStatus() const {
    auto text = readFileShared(statusPath());
    if (!text) return std::nullopt;
    return parseStatus(*text);
}

BridgeState Bridge::evaluate(const StatusDoc* status, long long lastSentSeq, long long nowEpochSeconds) {
    if (!status) return BridgeState::Disconnected;
    if (nowEpochSeconds - status->heartbeat > kHeartbeatTimeoutSec) return BridgeState::Disconnected;
    if (!status->inGame) return BridgeState::MainMenu;
    return status->appliedSeq.value_or(-1) >= lastSentSeq ? BridgeState::Applied : BridgeState::Pending;
}

}
