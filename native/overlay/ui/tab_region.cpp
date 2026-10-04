#include "overlay/core/region.h"
#include "overlay/core/view.h"
#include "tabs.h"
#include "widgets.h"
#include <algorithm>
#include <cfloat>
#include <imgui.h>
#include <optional>
#include <string>
#include <vector>

namespace mlt::ov {

namespace {
// 화면에서 고른 것(문서에 저장하지 않는다)
std::optional<std::string> g_scope;   // 없음 = 공통(모든 내 영지), 있으면 영지 키

// 화면에 보일 줄과 글은 상태(1초에 한 번쯤 바뀐다)나 설정, 고른 범위가 바뀔 때만 다시 만든다
struct LiveKey {
    const StatusDoc* status = nullptr;
    long long heartbeat = 0;   // 같은 주소에 새 상태가 놓여도 다르다
    bool inGame = false;
    bool operator==(const LiveKey&) const = default;
};
struct Header {
    std::vector<ScopeOption> scopes;
    std::string livestock;
    std::string note;
};
struct RowsKey {
    LiveKey live;
    std::optional<std::string> scope;
    RegionSettings settings;
    bool operator==(const RowsKey&) const = default;
};
Memo<LiveKey, Header> g_header;
Memo<RowsKey, std::vector<DepositRow>> g_rows;

// 표: 종류 / 지금 값 / 최소 유지. 칸을 고쳤으면 settings 를 바꾸고 true
bool drawTable(const std::vector<DepositRow>& rows, RegionSettings& settings) {
    const ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings;
    if (!ImGui::BeginTable("##deposits", 3, flags)) return false;
    bool changed = false;
    ImGui::TableSetupColumn("종류", ImGuiTableColumnFlags_WidthStretch, 22.0f);
    ImGui::TableSetupColumn(g_scope ? "지금(매장지마다)" : "지금(합계)", ImGuiTableColumnFlags_WidthStretch, 40.0f);
    ImGui::TableSetupColumn("최소 유지", ImGuiTableColumnFlags_WidthStretch, 38.0f);
    ImGui::TableHeadersRow();
    ImGui::PushID(g_scope ? g_scope->c_str() : "##common");   // 영지마다 다른 칸이다(편집 중인 글이 섞이지 않게)
    for (const DepositRow& row : rows) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(row.label.c_str());
        if (row.mineral && ImGui::IsItemHovered()) ImGui::SetTooltip("날짜가 넘어갈 때 채우고, 지금 값도 그때 갱신됩니다");
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(row.current.c_str());
        ImGui::TableNextColumn();
        ImGui::PushID(row.key.c_str());
        std::optional<int> value = row.target;
        // 영지를 골랐으면 빈칸은 공통 목표를 따른다는 뜻이다(그 값을 흐리게 보인다). 0 은 영지에서는 "이 영지는 채우지 않는다",
        // 공통에서는 빈칸과 같다(setDepositTarget 이 지운다)
        const std::string hint = row.inherited ? std::to_string(*row.inherited) : std::string();
        if (optionalNumberField("##target", value, 0, kDepositTargetMax, -FLT_MIN, hint.empty() ? nullptr : hint.c_str())) {
            setDepositTarget(settings, g_scope, row.key, value);
            changed = true;
        }
        ImGui::PopID();
    }
    ImGui::PopID();
    ImGui::EndTable();
    return changed;
}
}

void drawRegionTab(TabContext& ctx) {
    App& a = ctx.app;
    RegionSettings r = a.control.region();
    const RegionStatus* live = (ctx.inGame && ctx.status && ctx.status->region) ? &*ctx.status->region : nullptr;
    const LiveKey liveKey{ ctx.status, ctx.status ? ctx.status->heartbeat : 0, ctx.inGame };
    const Header& header = g_header.get(liveKey, [&] {
        return Header{ regionScopeOptions(live), livestockLine(live), depositNativeNote(ctx.status && ctx.status->native ? &*ctx.status->native : nullptr) };
    });

    bool changed = ImGui::Checkbox("영지 기능 사용", &r.enabled);
    changed |= ImGui::Checkbox("가축 상인 대기 없음", &r.noLivestockWait);
    ImGui::SameLine();
    ImGui::TextDisabled("(가축을 주문한 뒤의 \"상인 방문까지\" 30일을 없앱니다)");
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(header.livestock.c_str());
    ImGui::PopTextWrapPos();

    ImGui::SeparatorText("매장량");
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("영지:");
    ImGui::SameLine();
    const std::vector<ScopeOption>& scopes = header.scopes;
    int scope = indexOfKey(scopes, g_scope);
    comboOptions("##scope", scopes, scope, scaled(230.0f));
    g_scope = scopes[static_cast<size_t>(scope)].key;   // 골라 둔 영지가 사라졌으면 공통으로 돌아간다

    changed |= drawTable(g_rows.get(RowsKey{ liveKey, g_scope, r }, [&] { return buildDepositRows(r, live, g_scope); }), r);
    if (changed) {
        a.control.setRegion(r);
        markDirty(a);
    }

    ImGui::Spacing();
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("\"최소 유지\"는 매장지 하나의 양입니다. 내 영지의 매장지가 그보다 적으면 그 값까지 채웁니다(빈칸 = 채우지 않음). 다른 영주의 영지에는 닿지 않습니다.");
    ImGui::TextDisabled("영지를 고르면 그 영지의 목표를 따로 넣습니다. 빈칸이면 공통 목표(흐린 숫자)를 따르고, 0 이면 그 영지는 채우지 않습니다. 공통에서는 0 이 빈칸과 같습니다.");
    ImGui::TextDisabled("돌·물고기·장어·열매·버섯은 몇 초 안에 채웁니다. 소금·철·점토는 게임의 날짜가 넘어갈 때 채우고, 지금 값도 그때 갱신됩니다(게임에 들어온 뒤 하루가 지나야 보입니다).");
    ImGui::TextDisabled("목표를 낮추거나 꺼도 이미 채운 양은 줄지 않습니다. 동물(사슴, 작은 사냥감)과 새 매장지 추가는 다루지 않습니다.");
    if (!header.note.empty()) ImGui::TextColored(ImVec4(1.00f, 0.65f, 0.20f, 1.0f), "%s", header.note.c_str());
    ImGui::PopTextWrapPos();
}

}
