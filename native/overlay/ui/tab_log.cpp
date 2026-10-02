#include "overlay/core/log_tail.h"
#include "tabs.h"
#include "widgets.h"
#include <imgui.h>
#include <string>
#include <vector>

namespace mlt::ov {

namespace {
const ImVec4 kErrorColor(0.95f, 0.40f, 0.35f, 1.0f);

// 화면에서 고른 것(저장하지 않는다)
bool g_mineOnly = true;      // MLToybox 가 남긴 줄만
std::string g_search;
bool g_follow = true;        // 새 줄이 오면 맨 아래로 간다
size_t g_lastShown = 0;
}

// UE4SS.log 의 끝부분. 모드는 그 파일에 시작·게임 상태·명령 결과·오류를 남긴다(core/log.lua)
void drawLogTab(TabContext& ctx) {
    App& a = ctx.app;
    ImGui::Checkbox("MLToybox 줄만", &g_mineOnly);
    ImGui::SameLine();
    ImGui::Checkbox("새 줄 따라가기", &g_follow);
    ImGui::SameLine();
    const bool copy = ImGui::Button("복사");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("보이는 줄을 클립보드에 복사합니다");
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("검색");
    ImGui::SameLine();
    textField("##logsearch", g_search, 60, scaled(200.0f));
    ImGui::SameLine();
    hangulModeButton();
    if (!g_search.empty()) {
        ImGui::SameLine();
        if (ImGui::Button("지우기##logsearch")) g_search.clear();
    }

    const std::vector<size_t> shown = filterLog(a.logLines, { g_mineOnly, g_search });
    ImGui::SameLine();
    ImGui::TextDisabled("%zu / %zu줄", shown.size(), a.logLines.size());
    if (copy) {
        std::string text;
        for (size_t i : shown) {
            const LogLine& line = a.logLines[i];
            if (!line.time.empty()) text += line.time + " ";
            text += line.text + "\r\n";
        }
        ImGui::SetClipboardText(text.c_str());   // 작업 스레드가 시스템 클립보드에 쓴다(ui/clipboard_sync)
    }

    if (a.logLines.empty()) ImGui::TextDisabled("UE4SS.log 를 아직 읽지 못했습니다");
    else if (shown.empty()) ImGui::TextDisabled("조건에 맞는 줄이 없습니다");

    ImGui::BeginChild("##loglines", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(shown.size()));
    while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
            const LogLine& line = a.logLines[shown[static_cast<size_t>(i)]];
            if (!line.time.empty()) {
                ImGui::TextDisabled("%s", line.time.c_str());
                ImGui::SameLine();
            }
            if (line.error) ImGui::TextColored(kErrorColor, "%s", line.text.c_str());
            else if (line.mine) ImGui::TextUnformatted(line.text.c_str());
            else ImGui::TextDisabled("%s", line.text.c_str());   // UE4SS 와 다른 모드의 줄은 흐리게
        }
    }
    clipper.End();
    if (g_follow && shown.size() != g_lastShown) ImGui::SetScrollHereY(1.0f);   // 커서는 마지막 줄 아래에 있다
    g_lastShown = shown.size();
    ImGui::EndChild();
}

}
