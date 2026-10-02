#include "overlay/core/resources.h"
#include "tabs.h"
#include "widgets.h"
#include <algorithm>
#include <cfloat>
#include <imgui.h>
#include <imgui_internal.h>   // 표의 열 너비를 읽는다(공개 API 에는 없다)
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace mlt::ov {

namespace {
constexpr int kTargetMax = 1000000000;
// 열 너비의 기본값(표 너비에 대한 백분율): 자원, 분류, 현재, 목표
const std::vector<float> kDefaultShares = { 30.0f, 26.0f, 16.0f, 28.0f };
const ImVec4 kOrange(1.00f, 0.65f, 0.20f, 1.0f);
constexpr double kClearConfirmSec = 4.0;   // "지우기"를 한 번 누른 뒤 이 시간 안에 다시 눌러야 지운다

// 화면에서 고른 것(문서에 저장하지 않는다)
std::optional<std::string> g_scope;   // 없음 = 공통(모든 내 영지의 합계와 공통 목표), 있으면 영지 키
int g_fill = 500;
std::string g_category;               // 비어 있으면 모든 분류
std::string g_search;
ResourceOrder g_order;                // 헤더를 눌러 정한 줄 순서
double g_clearArmedUntil = -1.0;      // "지우기"를 한 번 눌렀다. 이 시각(ImGui 시각)까지 다시 누르면 지운다

using Overrides = std::map<std::string, std::vector<RegionOverride>>;

// 사용자가 열 경계를 끌어 너비를 바꿨으면 설정에 적는다(작업 스레드가 1초 안에 저장한다). 끄는 동안에는 적지 않는다
void rememberColumnWidths(App& a) {
    const ImGuiTable* table = ImGui::GetCurrentTable();
    if (!table || ImGui::IsMouseDown(ImGuiMouseButton_Left)) return;
    std::vector<float> weights;
    for (int i = 0; i < table->ColumnsCount; ++i) weights.push_back(table->Columns[i].StretchWeight);
    const std::vector<float> now = columnShares(weights);
    if (now.empty()) return;   // 아직 너비가 정해지지 않았다
    const std::vector<float>& saved = a.settings.resourceColumns.empty() ? kDefaultShares : a.settings.resourceColumns;
    if (!columnSharesDiffer(saved, now)) return;
    a.settings.resourceColumns = now;
    a.settingsDirty = true;
}

// 표: 자원 / 분류 / 현재 / 목표. 헤더를 누르면 그 열로 정렬하고, 열 경계를 끌면 너비가 바뀐다.
// overrides: 공통 범위에서, 자원마다 영지 목표가 따로 있는 영지들(그 영지는 공통 목표를 따르지 않는다. 목표 칸 옆에 표시한다).
// 목표 칸을 고쳤으면 targets 를 바꾸고 true
bool drawTable(App& a, std::vector<ResourceRow>& rows, std::map<std::string, int>& targets, const std::string& context, const Overrides& overrides) {
    const ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp
        | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_NoSavedSettings;
    const float height = std::max(scaled(120.0f), ImGui::GetContentRegionAvail().y);
    if (!ImGui::BeginTable("##resources", static_cast<int>(kResourceColumnCount), flags, ImVec2(0.0f, height))) return false;
    bool changed = false;
    // 처음 그릴 때의 너비. 설정에 적힌 것이 있으면 그것을 쓴다(그 뒤에는 표가 기억한다)
    const std::vector<float>& shares = a.settings.resourceColumns.size() == kResourceColumnCount ? a.settings.resourceColumns : kDefaultShares;
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("자원", ImGuiTableColumnFlags_WidthStretch, shares[0]);
    ImGui::TableSetupColumn("분류", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_DefaultSort, shares[1]);   // 처음에는 게임의 순서
    ImGui::TableSetupColumn("현재", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_PreferSortDescending, shares[2]);
    ImGui::TableSetupColumn(g_scope ? "영지 목표" : "목표", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_PreferSortDescending, shares[3]);
    ImGui::TableHeadersRow();

    ResourceColumn column = ResourceColumn::Category;
    bool descending = false;
    if (const ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs(); specs && specs->SpecsCount > 0) {
        column = static_cast<ResourceColumn>(std::clamp<int>(specs->Specs[0].ColumnIndex, 0, static_cast<int>(kResourceColumnCount) - 1));
        descending = specs->Specs[0].SortDirection == ImGuiSortDirection_Descending;
    }
    g_order.arrange(rows, column, descending, context);

    const char* mark = "영지별";
    const float markWidth = ImGui::CalcTextSize(mark).x + ImGui::GetStyle().ItemInnerSpacing.x;
    ImGui::PushID(g_scope ? g_scope->c_str() : "##common");   // 범위마다 다른 칸이다(편집 중인 글이 섞이지 않게)
    for (const ResourceRow& row : rows) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(row.name.c_str());
        if (row.name != row.id && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", row.id.c_str());   // 설정 파일에 적히는 이름
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", resourceClassLabel(row).c_str());
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        if (row.current) ImGui::Text("%.0f", *row.current);
        else ImGui::TextDisabled("-");
        ImGui::TableNextColumn();
        ImGui::PushID(row.id.c_str());
        const auto overridden = overrides.find(row.id);
        const bool marked = overridden != overrides.end();
        std::optional<int> target;
        if (auto it = targets.find(row.id); it != targets.end()) target = it->second;
        if (optionalNumberField("##target", target, 0, kTargetMax, marked ? -markWidth : -FLT_MIN)) {
            if (target) targets[row.id] = *target;
            else targets.erase(row.id);   // 빈칸 = 관리하지 않는다(영지 범위에서는 공통 목표를 따른다)
            changed = true;
        }
        if (marked) {
            // 영지 목표는 공통 목표보다 우선한다. 표시가 없으면 공통 목표를 바꿔도 그 영지가 왜 그대로인지 알 수 없다
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            ImGui::TextColored(kOrange, "%s", mark);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("영지 목표가 따로 있는 영지는 이 공통 목표를 따르지 않습니다:\n%s\n위의 \"영지\"에서 그 영지를 골라 바꾸거나 지웁니다.",
                                  regionOverrideText(overridden->second).c_str());
            }
        }
        ImGui::PopID();
    }
    ImGui::PopID();
    rememberColumnWidths(a);
    ImGui::EndTable();
    return changed;
}

// 분류 고르기와 검색. 골라 둔 분류가 사라졌으면 "전체"로 돌아간다
void drawFilter(const std::vector<ResourceRow>& all, size_t shown) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("분류");
    ImGui::SameLine();
    std::vector<ScopeOption> categories = { { std::nullopt, "전체" } };
    for (const std::string& category : resourceCategories(all)) categories.push_back({ category, category });
    int index = indexOfKey(categories, g_category.empty() ? std::nullopt : std::optional<std::string>(g_category));
    comboOptions("##category", categories, index, scaled(120.0f));
    g_category = categories[static_cast<size_t>(index)].key.value_or("");
    ImGui::SameLine();
    ImGui::TextUnformatted("검색");
    ImGui::SameLine();
    textField("##search", g_search, 60, scaled(150.0f));
    ImGui::SameLine();
    hangulModeButton();
    if (!g_search.empty()) {
        ImGui::SameLine();
        if (ImGui::Button("지우기##search")) g_search.clear();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%zu / %zu", shown, all.size());
}

// "이 값으로"와 "목표 지우기". 분류나 검색으로 줄을 추렸으면 보이는 줄에만 적용한다. 목표를 바꿨으면 true
bool drawBulk(const std::vector<ResourceRow>& rows, bool filtered, std::map<std::string, int>& targets) {
    bool changed = false;
    const std::string count = std::to_string(rows.size());
    numberField("##fill", g_fill, 0, 1000000, scaled(90.0f));
    ImGui::SameLine();
    const std::string fillLabel = filtered ? "보이는 " + count + "줄을 이 값으로###fill" : std::string("모두 이 값으로###fill");
    if (ImGui::Button(fillLabel.c_str()) && !rows.empty()) {
        for (const ResourceRow& row : rows) targets[row.id] = g_fill;
        changed = true;
    }
    ImGui::SameLine();
    // 한 번에 여러 목표가 사라지므로 두 번 눌러야 지운다
    const double now = ImGui::GetTime();
    const bool armed = now < g_clearArmedUntil;
    std::string clearLabel = filtered ? "보이는 " + count + "줄의 목표 지우기" : std::string(g_scope ? "영지 목표 모두 지우기" : "목표 모두 지우기");
    if (armed) clearLabel = "한 번 더 누르면 지웁니다";
    if (ImGui::Button((clearLabel + "###clear").c_str())) {
        if (armed) {
            targets = clearedTargets(std::move(targets), rows, filtered);
            g_clearArmedUntil = -1.0;
            changed = true;
        } else {
            g_clearArmedUntil = now + kClearConfirmSec;
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(g_scope ? "영지 목표를 지우면 그 자원은 공통 목표를 따릅니다" : "목표를 지우면 그 자원은 관리하지 않습니다");
    }
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
    ImGui::SameLine();
    ImGui::TextUnformatted("영지");
    ImGui::SameLine();
    const std::vector<ScopeOption> scopes = resourceScopeOptions(live);
    int scope = indexOfKey(scopes, g_scope);
    comboOptions("##scope", scopes, scope, scaled(250.0f));
    g_scope = scopes[static_cast<size_t>(scope)].key;   // 골라 둔 영지가 사라졌으면 공통으로 돌아간다

    const std::vector<ResourceRow> all = buildResourceRows(live, r, g_scope, a.catalog.get());
    std::vector<ResourceRow> rows = filterResourceRows(all, { g_category, g_search });
    const bool filtered = rows.size() != all.size();
    std::map<std::string, int> targets = resourceTargets(r, g_scope);
    bool targetsChanged = false;

    if (all.empty()) {
        if (ctx.inGame) ImGui::TextDisabled("자원이 없습니다");
        else ImGui::TextDisabled("게임에 들어가면 자원 목록이 표시됩니다");
    } else {
        drawFilter(all, rows.size());
        targetsChanged |= drawBulk(rows, filtered, targets);
        ImGui::TextDisabled(g_scope ? "목표 빈칸 = 공통 목표를 따름 · 헤더를 누르면 정렬 · 열 경계를 끌면 너비 조정"
                                    : "목표 빈칸 = 관리 안 함 · 헤더를 누르면 정렬 · 열 경계를 끌면 너비 조정");
        if (rows.empty()) {
            ImGui::TextDisabled("조건에 맞는 자원이 없습니다");
        } else {
            // 이 중 하나라도 바뀌면 지금 값으로 다시 정렬한다
            const std::string context = (g_scope ? *g_scope : std::string()) + "\n" + g_category + "\n" + g_search + "\n" + (a.catalog ? "named" : "plain");
            const Overrides overrides = g_scope ? Overrides() : regionOverrides(live, r);
            targetsChanged |= drawTable(a, rows, targets, context, overrides);
        }
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
