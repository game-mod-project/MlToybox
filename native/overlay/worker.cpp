#include "worker.h"
#include "overlay/ui/clipboard_sync.h"
#include "overlay/render/dx12_hook.h"
#include "overlay/ui/app.h"
#include "runtime.h"
#include <windows.h>
#include <cstdio>
#include <vector>

namespace mlt::ov {

static std::filesystem::path bridgeDirFor(void* selfModule) {
    wchar_t buf[MAX_PATH]{};
    GetModuleFileNameW(static_cast<HMODULE>(selfModule), buf, MAX_PATH);
    // <mod>\native\mltoybox_overlay.dll -> <mod>\bridge
    return std::filesystem::path(buf).parent_path().parent_path() / L"bridge";
}

// 바뀐 설정을 저장한다(규칙은 core/session). 파일 쓰기는 잠금 밖에서 한다. 저장에 실패했으면 false
static bool saveControlIfDirty(App& a, const Bridge& bridge) {
    std::optional<PendingSave> pending;
    {
        std::lock_guard<std::mutex> lock(a.mutex);
        pending = beginSave(a);
    }
    if (!pending) return true;
    const auto seq = bridge.saveControl(pending->doc);
    std::lock_guard<std::mutex> lock(a.mutex);
    finishSave(a, *pending, seq);
    return seq.has_value();
}

// 패널 등 밖에서 control.json 을 바꿨으면 다시 읽는다. 저장 대기 중인 변경이 있으면 건드리지 않는다(우리 쪽을 저장한다)
static void reloadControlIfChangedOutside(App& a, const Bridge& bridge) {
    const auto fileSeq = bridge.peekSeq();
    if (!fileSeq) return;
    {
        std::lock_guard<std::mutex> lock(a.mutex);
        if (!wantsReload(a, *fileSeq)) return;
    }
    ControlDoc loaded = bridge.loadControl();
    std::lock_guard<std::mutex> lock(a.mutex);
    adoptReloaded(a, std::move(loaded));
}

static void readStatus(App& a, const Bridge& bridge) {
    auto status = bridge.readStatus();
    std::shared_ptr<const StatusDoc> shared;
    if (status) shared = std::make_shared<const StatusDoc>(std::move(*status));
    // 모드가 실행했다고 알린 명령은 더 싣지 않는다(core/session)
    std::vector<std::string> reported;
    if (shared && shared->commands) {
        for (const auto& entry : *shared->commands) reported.push_back(entry.first);
    }
    std::lock_guard<std::mutex> lock(a.mutex);
    dropFinishedCommands(a, reported, nowEpochSeconds());
    if (shared || !a.status) a.status = std::move(shared);
    // 읽기에 실패하면(모드가 쓰는 순간) 직전 값을 둔다. heartbeat 로 오래된 것을 가려낸다
}

static void saveSettingsIfDirty(App& a, const Bridge& bridge, std::filesystem::file_time_type& known) {
    OverlaySettings copy;
    {
        std::lock_guard<std::mutex> lock(a.mutex);
        if (!a.settingsDirty) return;
        copy = a.settings;
        a.settingsDirty = false;
    }
    if (!writeFileAtomic(bridge.settingsPath(), dumpSettings(copy))) {
        std::lock_guard<std::mutex> lock(a.mutex);
        a.settingsDirty = true;   // 다음 회차에 다시 쓴다
        return;
    }
    std::error_code ec;
    known = std::filesystem::last_write_time(bridge.settingsPath(), ec);   // 우리가 쓴 것은 밖에서 바뀐 것으로 보지 않는다
}

// overlay.json 을 밖에서 고쳤으면 다시 읽는다(개발·검증용 설정을 게임을 끄지 않고 바꾸기 위한 것)
static void reloadSettingsIfChangedOutside(App& a, const Bridge& bridge, std::filesystem::file_time_type& known) {
    std::error_code ec;
    const auto written = std::filesystem::last_write_time(bridge.settingsPath(), ec);
    if (ec || written == known) return;
    known = written;
    auto text = readFileShared(bridge.settingsPath());
    if (!text) return;
    std::lock_guard<std::mutex> lock(a.mutex);
    if (a.settingsDirty) return;
    a.settings = parseSettings(*text);
    syncSettingsAtoms(a);
    a.applyWindowRect = true;
    if (a.settings.startOpen) a.visible = true;
}

// 개발·검증용(overlay.json 의 inputLog): 창 스레드가 적어 둔 글쇠 메시지를 bridge/overlay_input.log 에 덧붙인다
static void flushInputLog(App& a, const Bridge& bridge) {
    std::vector<std::string> lines;
    {
        std::lock_guard<std::mutex> lock(a.mutex);
        lines.swap(a.inputLines);
    }
    if (lines.empty()) return;
    const auto path = bridge.settingsPath().parent_path() / L"overlay_input.log";
    if (FILE* file = _wfopen(path.c_str(), L"ab")) {
        for (const std::string& line : lines) std::fprintf(file, "%s\n", line.c_str());
        std::fclose(file);
    }
}

static void writeOverlayStatus(App& a, const Bridge& bridge) {
    OverlayStatus s;
    s.heartbeat = nowEpochSeconds();
    s.state = a.state.load();
    s.visible = a.visible.load();
    s.frames = a.frames.load();
    s.font = a.koreanFont.load() ? "malgun" : "default";
    {
        std::lock_guard<std::mutex> lock(a.mutex);
        s.reason = a.reason;
    }
    writeFileAtomic(bridge.overlayStatusPath(), renderOverlayStatus(s));
}

void runOverlayWorker(void* selfModule) {
    const Bridge bridge(bridgeDirFor(selfModule));
    App& a = app();
    {
        std::lock_guard<std::mutex> lock(a.mutex);
        a.settings = parseSettings(readFileShared(bridge.settingsPath()).value_or(""));
        syncSettingsAtoms(a);
        a.visible = a.settings.startOpen;
        // control.json 이 있는데 읽지 못했으면(손으로 고치다 문법을 틀린 경우) 기본값으로 덮기 전에 사본을 남긴다
        LoadedControl loaded = bridge.loadControlChecked();
        if (loaded.unreadable) bridge.backupControl();
        a.control = std::move(loaded.doc);
        a.lastSentSeq = a.control.seq();
    }
    std::error_code ec;
    auto settingsWritten = std::filesystem::last_write_time(bridge.settingsPath(), ec);
    writeOverlayStatus(a, bridge);

    std::string err;
    if (installRenderHooks(err)) {
        OverlayState expected = OverlayState::Starting;
        a.state.compare_exchange_strong(expected, OverlayState::Waiting);
    } else {
        disableOverlay("hook setup failed: " + err);
    }

    ClipboardSync clipboard(systemClipboard());   // 복사한 글을 쓰고, 붙여넣을 글을 읽어 둔다
    unsigned saveRetryAt = 0;
    for (unsigned tick = 0;; ++tick) {
        // 이 스레드가 예외로 끝나면 저장이 조용히 멈춘다. 그 회차만 건너뛰고 계속한다
        try {
            clipboard.tick(a);
            flushInputLog(a, bridge);
            if (tick % 2 == 0 && tick >= saveRetryAt) {            // 0.2초마다. 실패했으면 1초 뒤에 다시 한다
                if (!saveControlIfDirty(a, bridge)) saveRetryAt = tick + 10;
            }
            if (tick % 10 == 0) {                                  // 1초마다
                readStatus(a, bridge);
                reloadControlIfChangedOutside(a, bridge);
                saveSettingsIfDirty(a, bridge, settingsWritten);
                reloadSettingsIfChangedOutside(a, bridge, settingsWritten);
                writeOverlayStatus(a, bridge);
            }
        } catch (...) {
            ++a.workerErrors;
        }
        Sleep(100);
    }
}

}
