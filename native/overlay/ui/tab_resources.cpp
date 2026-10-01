#include "overlay/core/resources.h"
#include "tabs.h"
#include "widgets.h"
#include <algorithm>
#include <cfloat>
#include <imgui.h>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace mlt::ov {

namespace {
constexpr int kTargetMax = 1000000000;

// 화면에서 고른 것(문서에 저장하지 않는다)
std::optional<std::string> g_scope;   // 없음 = 공통(모든 내 영지의 합계와 공통 목표), 있으면 영지 키
int g_fill = 500;

// 표: 자원 / 현재 / 목표. 목표 칸을 고쳤으면 targets 를 바꾸고 true
bool drawTable(const std::vector<ResourceRow>& rows, std::map<std::string, int>& targets) {
    const ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp;
    const float height = std::max(scaled(120.0f), ImGui::GetContentRegionAvail().y);
    if (!ImGui::BeginTable("##resources", 3, flags, ImVec2(0.0f, height))) return false;
    bool changed = false;
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("자원", ImGuiTableColumnFlags_WidthStretch, 3.0f);
    ImGui::TableSetupColumn("현재", ImGuiTableColumnFlags_WidthStretch, 2.0f);
    ImGui::TableSetupColumn(g_scope ? "영지 목표 (빈칸=공통 목표 따름)" : "목표 (빈칸=관리 안 함)", ImGuiTableColumnFlags_WidthStretch, 3.0f);
    ImGui::TableHeadersRow();
    ImGui::PushID(g_scope ? g_scope->c_str() : "##common");   // 범위마다 다른 칸이다(편집 중인 글이 섞이지 않게)
    for (const ResourceRow& row : rows) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(row.id.c_str());
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        if (row.current) ImGui::Text("%.0f", *row.current);
        else ImGui::TextDisabled("-");
        ImGui::TableNextColumn();
        ImGui::PushID(row.id.c_str());
        std::optional<int> target;
        if (auto it = targets.find(row.id); it != targets.end()) target = it->second;
        if (optionalNumberField("##target", target, 0, kTargetMax, -FLT_MIN)) {
            if (target) targets[row.id] = *target;
            else targets.erase(row.id);   // 빈칸 = 관리하지 않는다(영지 범위에서는 공통 목표를 따른다)
            changed = true;
        }
        ImGui::PopID();
    }
    ImGui::PopID();
    ImGui::EndTable();
    return changed;
}
}

void drawResourcesTab(TabContext& ctx) {
    App& a = ctx.app;
    ResourcesSettings r = a.control.resources();
    const StatusDoc* live = ctx.inGame ? ctx.status : nullptr;   // 게임 안일 때만 게임 상태를 쓴다

    bool changed = ImGui::Checkbox("자원 목표값 유지", &r.enabled);
    ImGui::SameLine();
    ImGui::TextUnformatted("주기(초)");
    ImGui::SameLine();
    changed |= numberField("##interval", r.intervalSec, 1, 60, scaled(50.0f));

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("영지");
    ImGui::SameLine();
    const std::vector<ScopeOption> scopes = resourceScopeOptions(live);
    int scope = indexOfKey(scopes, g_scope);
    comboOptions("##scope", scopes, scope, scaled(260.0f));
    g_scope = scopes[static_cast<size_t>(scope)].key;   // 골라 둔 영지가 사라졌으면 공통으로 돌아간다
    ImGui::SameLine();
    numberField("##fill", g_fill, 0, 1000000, scaled(90.0f));
    ImGui::SameLine();
    const bool fill = ImGui::Button("모두 이 값으로");

    const std::vector<ResourceRow> rows = buildResourceRows(live, r, g_scope);
    std::map<std::string, int> targets = resourceTargets(r, g_scope);
    bool targetsChanged = false;
    if (fill && !rows.empty()) {   // 지금 보이는 범위의 모든 줄
        for (const ResourceRow& row : rows) targets[row.id] = g_fill;
        targetsChanged = true;
    }
    if (rows.empty()) {
        if (ctx.inGame) ImGui::TextDisabled("자원이 없습니다");
        else ImGui::TextDisabled("게임에 들어가면 자원 목록이 표시됩니다");
    } else {
        targetsChanged |= drawTable(rows, targets);
    }
    if (targetsChanged) {
        storeResourceTargets(r, g_scope, std::move(targets));   // 영지 목표가 하나도 없으면 그 영지 키를 지운다
        changed = true;
    }
    if (changed) {
        a.control.setResources(r);
        markDirty(a);
    }
}

}
