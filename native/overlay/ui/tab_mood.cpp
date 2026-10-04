#include "overlay/core/view.h"
#include "tabs.h"
#include "widgets.h"
#include <imgui.h>
#include <optional>
#include <string>
#include <vector>

namespace mlt::ov {

namespace {
// 화면에서 고른 것(문서에 저장하지 않는다)
std::optional<std::string> g_scope;   // 없음 = 공통(모든 내 영지), 있으면 영지 키

// 범위 목록과 아래쪽의 글은 상태(1초에 한 번쯤 바뀐다)가 바뀔 때만 다시 만든다
struct LiveKey {
    const StatusDoc* status = nullptr;
    long long heartbeat = 0;   // 같은 주소에 새 상태가 놓여도 다르다
    bool inGame = false;
    bool operator==(const LiveKey&) const = default;
};
struct Shown {
    std::vector<ScopeOption> scopes;
    std::vector<std::string> lines;
    std::string note;
};
Memo<LiveKey, Shown> g_shown;

// 한 값의 세 칸: 고정값, 오르는 요인 배율, 깎이는 요인 비율. 바뀌었으면 true
bool drawValue(const char* label, MoodValue& v) {
    bool changed = false;
    ImGui::PushID(label);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(scaled(80.0f));
    ImGui::TextUnformatted("고정값(0 = 고정 안 함):");
    ImGui::SameLine();
    changed |= numberField("##fixed", v.fixed, 0, 100, scaled(50.0f));
    ImGui::SameLine();
    ImGui::TextUnformatted("오르는 요인(배):");
    ImGui::SameLine();
    changed |= numberField("##good", v.good, 1, 10, scaled(40.0f));
    ImGui::SameLine();
    ImGui::TextUnformatted("깎이는 요인(%):");
    ImGui::SameLine();
    changed |= numberField("##bad", v.bad, 0, 100, scaled(50.0f));
    ImGui::PopID();
    return changed;
}
}

void drawMoodTab(TabContext& ctx) {
    App& a = ctx.app;
    MoodSettings m = a.control.mood();
    const MoodStatus* live = (ctx.inGame && ctx.status && ctx.status->mood) ? &*ctx.status->mood : nullptr;
    const Shown& shown = g_shown.get(LiveKey{ ctx.status, ctx.status ? ctx.status->heartbeat : 0, ctx.inGame }, [&] {
        return Shown{ moodScopeOptions(live), moodLines(live), moodNativeNote(ctx.status && ctx.status->native ? &*ctx.status->native : nullptr) };
    });

    bool changed = ImGui::Checkbox("자격·공공질서 기능 사용", &m.enabled);

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("영지:");
    ImGui::SameLine();
    const std::vector<ScopeOption>& scopes = shown.scopes;
    int scope = indexOfKey(scopes, g_scope);
    comboOptions("##scope", scopes, scope, scaled(230.0f));
    g_scope = scopes[static_cast<size_t>(scope)].key;   // 골라 둔 영지가 사라졌으면 공통으로 돌아간다

    // 영지를 골랐으면 그 영지에 따로 둔 설정을 고친다. 따로 두지 않았으면 공통 값을 보여 주기만 한다
    bool separate = true;
    if (g_scope) {
        separate = m.regions.count(*g_scope) > 0;
        ImGui::SameLine();
        if (ImGui::Checkbox("이 영지만 따로 지정", &separate)) {
            if (separate) m.regions[*g_scope] = m.common;   // 공통 값에서 시작한다
            else m.regions.erase(*g_scope);                 // 끄면 공통 값을 따른다
            changed = true;
        }
    }
    MoodPair pair = (g_scope && separate) ? m.regions[*g_scope] : m.common;
    ImGui::PushID(g_scope ? g_scope->c_str() : "##common");   // 영지마다 다른 칸이다(편집 중인 글이 섞이지 않게)
    ImGui::BeginDisabled(!separate);
    bool edited = drawValue("자격", pair.approval);
    edited |= drawValue("공공질서", pair.order);
    ImGui::EndDisabled();
    ImGui::PopID();
    if (edited && separate) {
        if (g_scope) m.regions[*g_scope] = pair;
        else m.common = pair;
        changed = true;
    }
    if (changed) {
        a.control.setMood(m);
        markDirty(a);
    }

    ImGui::Spacing();
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("게임은 영지마다 하루에 한 번 자격(50 + 요인의 합)과 공공질서(100 + 요인의 합)를 다시 계산합니다. 여기의 설정은 그 직후에 내 영지에만 적용되고, 다른 영주에게는 닿지 않습니다.");
    ImGui::TextDisabled("고정값을 넣으면 그 값으로 유지합니다(배율은 쓰이지 않습니다). 넣는 즉시 반영됩니다.");
    ImGui::TextDisabled("배율은 오르는 요인에 배수를, 깎이는 요인에 비율을 곱합니다(0%% = 깎이지 않음, 100%% = 게임 그대로). 게임이 다음에 계산할 때(하루 안에) 반영됩니다.");
    ImGui::TextDisabled("영지를 고르고 \"이 영지만 따로 지정\"을 켜면 그 영지는 공통 대신 자기 설정을 씁니다.");
    ImGui::TextDisabled("영지 창의 요인 목록은 게임의 원래 숫자로 남고 합계만 달라집니다. 설정이 값을 바꾸는 영지에서는 게임의 \"자격 매우 낮음\" 알림을 막아 두었습니다(게임에서 확인하지는 못했습니다).");
    if (!shown.note.empty()) ImGui::TextColored(ImVec4(1.00f, 0.65f, 0.20f, 1.0f), "%s", shown.note.c_str());
    ImGui::Spacing();
    for (const std::string& line : shown.lines) ImGui::TextUnformatted(line.c_str());
    ImGui::PopTextWrapPos();
}

}
