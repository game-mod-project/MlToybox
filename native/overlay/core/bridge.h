#pragma once
#include "control_doc.h"
#include "status_doc.h"
#include <filesystem>
#include <optional>
#include <string>

// bridge 폴더의 control.json / status.json 읽고 쓰기 (패널의 BridgeClient 와 같은 동작)
namespace mlt::ov {

enum class BridgeState { Disconnected, MainMenu, Pending, Applied };

// 다른 프로그램이 그 파일을 지우거나 이름을 바꾸는 것을 막지 않고 읽는다(패널도 이렇게 읽는다).
// 모드는 status.json 을 "지우고 이름 바꾸기"로 쓴다. 삭제 공유 없이 열고 있으면 그 순간 모드의 지우기가 실패한다.
// (읽는 동안 파일을 잡는 시간은 아주 짧다. 이 성질은 단위 테스트로 재현하지 못해 테스트가 없다)
std::optional<std::string> readFileShared(const std::filesystem::path& path);

struct LoadedControl {
    ControlDoc doc;
    bool unreadable = false;   // 파일은 있는데 해석하지 못했다(문서는 기본값)
};

class Bridge {
public:
    static constexpr long long kHeartbeatTimeoutSec = 5;

    explicit Bridge(std::filesystem::path dir) : dir_(std::move(dir)) {}

    std::filesystem::path controlPath() const { return dir_ / L"control.json"; }
    std::filesystem::path controlBackupPath() const { return dir_ / L"control.json.bak"; }
    std::filesystem::path statusPath() const { return dir_ / L"status.json"; }
    std::filesystem::path settingsPath() const { return dir_ / L"overlay.json"; }
    std::filesystem::path overlayStatusPath() const { return dir_ / L"overlay_status.json"; }

    // 파일이 없거나 깨졌으면 빈 문서
    ControlDoc loadControl() const;
    // 위와 같되, 파일이 있는데 해석하지 못한 경우를 알려 준다
    LoadedControl loadControlChecked() const;
    // control.json 을 control.json.bak 으로 복사한다(있던 사본은 덮는다). 파일이 없거나 복사하지 못하면 false
    bool backupControl() const;
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
