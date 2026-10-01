#include "bridge.h"
#include "runtime.h"
#include <algorithm>
#include <chrono>
#include <system_error>
#include <thread>

namespace mlt::ov {

ControlDoc Bridge::loadControl() const {
    auto text = readFileUtf8(controlPath());
    if (!text) return ControlDoc();
    return ControlDoc::parse(*text);
}

std::optional<long long> Bridge::peekSeq() const {
    auto text = readFileUtf8(controlPath());
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

std::optional<StatusDoc> Bridge::readStatus() const {
    auto text = readFileUtf8(statusPath());
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
