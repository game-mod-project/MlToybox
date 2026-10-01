#pragma once
#include "control_doc.h"
#include "status_doc.h"
#include <filesystem>
#include <optional>

// bridge 폴더의 control.json / status.json 읽고 쓰기 (패널의 BridgeClient 와 같은 동작)
namespace mlt::ov {

enum class BridgeState { Disconnected, MainMenu, Pending, Applied };

class Bridge {
public:
    static constexpr long long kHeartbeatTimeoutSec = 5;

    explicit Bridge(std::filesystem::path dir) : dir_(std::move(dir)) {}

    std::filesystem::path controlPath() const { return dir_ / L"control.json"; }
    std::filesystem::path statusPath() const { return dir_ / L"status.json"; }
    std::filesystem::path settingsPath() const { return dir_ / L"overlay.json"; }
    std::filesystem::path overlayStatusPath() const { return dir_ / L"overlay_status.json"; }

    // 파일이 없거나 깨졌으면 빈 문서
    ControlDoc loadControl() const;
    // 파일에 적힌 seq. 파일이 없거나 깨졌으면 값 없음
    std::optional<long long> peekSeq() const;
    // seq = max(파일의 seq, 문서의 seq) + 1 로 저장하고 그 seq 를 돌려준다. 문서의 seq 도 바꾼다. 실패하면 값 없음(문서는 그대로)
    std::optional<long long> saveControl(ControlDoc& doc) const;
    std::optional<StatusDoc> readStatus() const;

    static BridgeState evaluate(const StatusDoc* status, long long lastSentSeq, long long nowEpochSeconds);

private:
    std::filesystem::path dir_;
};
}
