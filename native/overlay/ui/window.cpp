#include "window.h"
#include "overlay/core/hint.h"
#include "runtime.h"
#include "tabs.h"
#include <algorithm>
#include <cmath>
#include <imgui.h>
#include <windows.h>

namespace mlt::ov {

namespace {
HintTimer g_hint;   // 안내를 띄울 때(core/hint)

struct Tab {
    const char* name;
    void (*draw)(TabContext&);
};
// 패널과 같은 이름·순서. 다만 패널의 "업그레이드" 탭은 건설 탭 안에 있다(건물 기능이라 합쳤다). "자격·질서", "영지", "로그"는 오버레이에만 있다
const Tab kTabs[] = {
    { "자원", drawResourcesTab },
    { "영주", drawLordTab },
    { "건설", drawBuildTab },
    { "군사", drawMilitaryTab },
    { "용병", drawMercenariesTab },
    { "인구", drawPopulationTab },
    { "자격·질서", drawMoodTab },
    { "영지", drawRegionTab },
    { "상태", drawStatusTab },
    { "로그", drawLogTab },
};

const ImVec4 kGreen(0.35f, 0.80f, 0.45f, 1.0f);
const ImVec4 kOrange(1.00f, 0.65f, 0.20f, 1.0f);
const ImVec4 kBlue(0.45f, 0.65f, 0.95f, 1.0f);
const ImVec4 kRed(0.95f, 0.40f, 0.35f, 1.0f);

bool hintActive() {
    return g_hint.active(GetTickCount64());
}

void drawHint(App& a) {
    ImGui::SetNextWindowPos(ImVec2(16.0f, 16.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.6f);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize
        | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
    if (ImGui::Begin("##mltoybox_hint", nullptr, flags)) ImGui::Text("%s: MLToybox", a.settings.toggleKey.c_str());
    ImGui::End();
}

void drawStateLine(App& a, BridgeState state) {
    if (a.saveFailed) {
        ImGui::TextColored(kRed, "● 저장 실패 (다시 시도 중)");
        return;
    }
    switch (state) {
        case BridgeState::Applied: ImGui::TextColored(kGreen, "● 적용됨"); break;
        case BridgeState::Pending: ImGui::TextColored(kOrange, "● 적용 대기 중"); break;
        case BridgeState::MainMenu: ImGui::TextColored(kBlue, "● 메인 메뉴"); break;
        case BridgeState::Disconnected: ImGui::TextColored(kRed, "● 모드 응답 없음"); break;
    }
}

// 설정에 적힌 창 위치·크기를 화면 안으로 맞춰 적용한다
void applyWindowRect(App& a) {
    const ImVec2 display = ImGui::GetMainViewport()->Size;
    OverlaySettings& s = a.settings;
    const float w = std::min(static_cast<float>(s.w), std::max(320.0f, display.x));
    const float h = std::min(static_cast<float>(s.h), std::max(240.0f, display.y));
    const float x = std::clamp(static_cast<float>(s.x), 0.0f, std::max(0.0f, display.x - w));
    const float y = std::clamp(static_cast<float>(s.y), 0.0f, std::max(0.0f, display.y - h));
    ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(w, h), ImGuiCond_Always);
}

// 창을 옮기거나 크기를 바꿨으면 설정에 적는다(작업 스레드가 1초 안에 저장한다)
void rememberWindowRect(App& a) {
    const ImVec2 pos = ImGui::GetWindowPos();
    const ImVec2 size = ImGui::GetWindowSize();
    const int x = static_cast<int>(std::lround(pos.x)), y = static_cast<int>(std::lround(pos.y));
    const int w = static_cast<int>(std::lround(size.x)), h = static_cast<int>(std::lround(size.y));
    OverlaySettings& s = a.settings;
    if (x == s.x && y == s.y && w == s.w && h == s.h) return;
    s.x = x;
    s.y = y;
    s.w = w;
    s.h = h;
    a.settingsDirty = true;
}

void drawMainWindow(App& a) {
    if (a.applyWindowRect.exchange(false)) applyWindowRect(a);
    bool open = true;
    if (ImGui::Begin("MLToybox", &open, ImGuiWindowFlags_NoCollapse)) {
        const long long now = nowEpochSeconds();
        const StatusDoc* status = a.status.get();
        const BridgeState state = Bridge::evaluate(status, a.lastSentSeq, now);
        drawStateLine(a, state);
        ImGui::Separator();
        TabContext ctx{ a, status, state == BridgeState::Applied || state == BridgeState::Pending, now };
        if (ImGui::BeginTabBar("##mltoybox_tabs")) {
            for (const Tab& tab : kTabs) {
                // 개발용 설정 devTab 이 있으면 그 탭을 연다(마우스 없이 화면을 캡처해 확인하기 위한 것)
                const ImGuiTabItemFlags flags = a.settings.devTab == tab.name ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
                if (ImGui::BeginTabItem(tab.name, nullptr, flags)) {
                    ImGui::BeginChild("##body");
                    tab.draw(ctx);
                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }
            }
            ImGui::EndTabBar();
        }
    }
    rememberWindowRect(a);
    ImGui::End();
    if (!open) a.visible = false;
}
}

std::vector<std::string> tabNames() {
    std::vector<std::string> names;
    for (const Tab& tab : kTabs) names.emplace_back(tab.name);
    return names;
}

void configureOverlayInput() {
    ImGuiIO& io = ImGui::GetIO();
    // 창은 제목 줄을 끌어서만 옮긴다. 기본값대로면 창 안의 빈 곳이나 글 위를 눌러 끌어도 창이 옮겨져,
    // 표의 열 경계(폭 8픽셀)를 잡으려다 조금만 빗나가도 창이 따라 움직인다
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    // 게임이 자기 커서를 쓴다. ImGui 백엔드가 Windows 커서를 바꾸거나 숨기지 않게 한다(바꿔도 게임이 곧 되돌린다)
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
}

void notifyOverlayReady() {
    g_hint.onReady(GetTickCount64());
}

bool overlayWantsFrame(App& a) {
    // 게임(맵)에 들어간 순간에도 안내를 띄운다. 그리기 시작할 때의 안내는 검은 시작 화면에 떠서 보기 어렵다
    const BridgeState state = Bridge::evaluate(a.status.get(), a.lastSentSeq, nowEpochSeconds());
    g_hint.update(state != BridgeState::Disconnected, state == BridgeState::Applied || state == BridgeState::Pending, GetTickCount64());
    return a.visible.load() || hintActive();
}

void drawOverlay(App& a) {
    if (a.visible.load()) drawMainWindow(a);
    else if (hintActive()) drawHint(a);
    // 열 경계나 창 가장자리처럼 끌어서 크기를 바꾸는 자리 위에서는 크기 조절 화살표를 직접 그린다.
    // 게임의 커서는 모양이 바뀌지 않아, 이것이 없으면 잡을 수 있는 자리인지 알 수 없다
    const ImGuiMouseCursor cursor = ImGui::GetMouseCursor();
    ImGui::GetIO().MouseDrawCursor = a.visible.load() && (cursor == ImGuiMouseCursor_ResizeEW || cursor == ImGuiMouseCursor_ResizeNS
        || cursor == ImGuiMouseCursor_ResizeNESW || cursor == ImGuiMouseCursor_ResizeNWSE);
}

}
