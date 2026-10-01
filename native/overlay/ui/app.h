#pragma once
#include "overlay/core/bridge.h"
#include "overlay/core/session.h"
#include "overlay/core/settings.h"
#include "overlay/core/status_file.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <string>

// 화면(RHI 스레드), 창 프로시저(게임 스레드), 작업 스레드가 나눠 쓰는 상태.
// 잠금 순서: imguiMutex(render/input) -> App::mutex. 작업 스레드는 App::mutex 만 잡는다.
namespace mlt::ov {

// Session(설정 문서, 변경 표시, 보낼 명령, 마지막 seq, 저장 실패)도 mutex 로 보호한다
struct App : Session {
    std::mutex mutex;

    // --- mutex 로 보호 ---
    std::shared_ptr<const StatusDoc> status;   // 최신 status.json. 없으면 비어 있다
    OverlaySettings settings;
    bool settingsDirty = false;
    std::string reason;                        // state 가 Disabled 일 때의 이유

    // --- 잠금 없이 읽고 쓴다 ---
    std::atomic<OverlayState> state{OverlayState::Starting};
    std::atomic<bool> visible{false};
    std::atomic<bool> wantMouse{false};        // 직전 프레임에 ImGui 가 마우스를 원했는가
    std::atomic<bool> wantKeyboard{false};
    std::atomic<int> toggleVk{0x2D};
    std::atomic<float> scale{1.0f};
    std::atomic<long long> frames{0};
    std::atomic<bool> koreanFont{false};
    std::atomic<bool> applyWindowRect{true};   // 설정의 창 위치·크기를 다음 프레임에 적용한다
};

App& app();

// 오버레이를 끈다(그 세션에서 다시 그리지 않는다). 이유는 overlay_status.json 에 적힌다
void disableOverlay(const std::string& reason);

// 설정을 바꾼 뒤 원자 값(토글 키, 배율)을 맞춘다. mutex 를 잡은 채로 부른다
void syncSettingsAtoms(App& a);
}
