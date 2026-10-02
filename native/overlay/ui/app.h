#pragma once
#include "overlay/core/bridge.h"
#include "overlay/core/log_tail.h"
#include "overlay/core/resources.h"
#include "overlay/core/session.h"
#include "overlay/core/settings.h"
#include "overlay/core/status_file.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

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
    std::string clipboardOut;                  // 화면에서 복사한 글. 작업 스레드가 Windows 클립보드에 쓴다
    bool clipboardPending = false;
    std::string clipboardIn;                   // 붙여넣을 글. 작업 스레드가 시스템 클립보드에서 읽어 둔다(글자 칸에 커서가 있는 동안)
    std::shared_ptr<const ResourceCatalog> catalog;   // 자원 이름 표(bridge/catalog.json). 아직 읽지 못했으면 비어 있다
    std::vector<LogLine> logLines;             // UE4SS.log 의 끝부분(작업 스레드가 1초마다 이어 읽는다)
    std::vector<std::string> inputLines;       // 개발·검증용(inputLog): 창 스레드가 적어 둔 글쇠 메시지. 작업 스레드가 파일에 쓴다

    // --- 잠금 없이 읽고 쓴다 ---
    std::atomic<OverlayState> state{OverlayState::Starting};
    std::atomic<bool> visible{false};
    std::atomic<bool> wantMouse{false};        // 직전 프레임에 ImGui 가 마우스를 원했는가
    std::atomic<bool> wantKeyboard{false};
    std::atomic<bool> wantText{false};         // 직전 프레임에 글자 입력 칸에 커서가 있었는가(그동안 한/영 글쇠는 오버레이의 것이다)
    std::atomic<bool> inputLog{false};         // 개발·검증용: 글쇠 메시지를 적는가(settings.inputLog)
    std::atomic<int> toggleVk{0x2D};
    std::atomic<float> scale{1.0f};
    std::atomic<long long> frames{0};
    std::atomic<bool> koreanFont{false};
    std::atomic<bool> applyWindowRect{true};   // 설정의 창 위치·크기를 다음 프레임에 적용한다
    std::atomic<unsigned> hangulKeys{0};       // 글자 칸에 커서가 있을 때 한/영 글쇠를 누른 횟수(창 스레드가 센다. 한글 조합기를 켜고 끈다)
    std::atomic<long long> workerErrors{0};    // 작업 스레드가 예외로 건너뛴 회차 수(0 이 아니면 상태 탭에 보인다)
};

App& app();

// 오버레이를 끈다(그 세션에서 다시 그리지 않는다). 이유는 overlay_status.json 에 적힌다
void disableOverlay(const std::string& reason);

// 설정을 바꾼 뒤 원자 값(토글 키, 배율)을 맞춘다. mutex 를 잡은 채로 부른다
void syncSettingsAtoms(App& a);
}
