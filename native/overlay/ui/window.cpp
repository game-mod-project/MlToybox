#include "window.h"
#include "runtime.h"
#include "tabs.h"
#include <algorithm>
#include <cmath>
#include <imgui.h>
#include <windows.h>

namespace mlt::ov {

namespace {
constexpr ULONGLONG kHintMs = 8000;
ULONGLONG g_readyTick = 0;

struct Tab {
    const char* name;
    void (*draw)(TabContext&);
};
// 패널과 같은 이름. 탭은 뒤 태스크에서 더한다
const Tab kTabs[] = {
    { "상태", drawStatusTab },
};

const ImVec4 kGreen(0.35f, 0.80f, 0.45f, 1.0f);
const ImVec4 kOrange(1.00f, 0.65f, 0.20f, 1.0f);
const ImVec4 kBlue(0.45f, 0.65f, 0.95f, 1.0f);
const ImVec4 kRed(0.95f, 0.40f, 0.35f, 1.0f);

bool hintActive() {
    return g_readyTick != 0 && GetTickCount64() - g_readyTick < kHintMs;
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

void notifyOverlayReady() {
    g_readyTick = GetTickCount64();
}

bool overlayWantsFrame(App& a) {
    return a.visible.load() || hintActive();
}

void drawOverlay(App& a) {
    if (a.visible.load()) drawMainWindow(a);
    else if (hintActive()) drawHint(a);
}

}
