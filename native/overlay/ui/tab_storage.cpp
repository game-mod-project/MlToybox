#include "overlay/core/storage.h"
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
constexpr double kClearConfirmSec = 4.0;   // "지우기"를 한 번 누른 뒤 이 시간 안에 다시 눌러야 지운다
const char* const kKindNames[kStorageKinds] = { "일반", "목재", "식량" };

// 화면에서 고른 것(문서에 저장하지 않는다)
std::string g_search;
int g_multiplier = 2;
double g_clearArmedUntil = -1.0;   // "지우기"를 한 번 눌렀다. 이 시각(ImGui 시각)까지 다시 누르면 지운다

// 표: 건물 / 일반 / 목재 / 식량. 칸이 비어 있으면 게임의 값 그대로이고, 그 기본값이 회색으로 보인다.
// 그 저장실이 없는 건물의 칸은 "-" 다(원래 0 인 한도는 고치지 않는다). 칸을 고쳤으면 settings 를 바꾸고 true
bool drawTable(std::vector<StorageRow>& rows, StorageSettings& settings) {
    const ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp
        | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_SortTristate | ImGuiTableFlags_NoSavedSettings;
    const float height = std::max(scaled(120.0f), ImGui::GetContentRegionAvail().y);
    if (!ImGui::BeginTable("##storage", 1 + kStorageKinds, flags, ImVec2(0.0f, height))) return false;
    bool changed = false;
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("건물", ImGuiTableColumnFlags_WidthStretch, 34.0f);
    for (const char* name : kKindNames) ImGui::TableSetupColumn(name, ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_PreferSortDescending, 22.0f);
    ImGui::TableHeadersRow();

    // 정렬하지 않으면 모드가 적은 순서(저장 건물이 먼저). 머리줄을 세 번째 누르면 정렬이 풀린다
    std::optional<StorageColumn> column;
    bool descending = false;
    if (const ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs(); specs && specs->SpecsCount > 0) {
        column = static_cast<StorageColumn>(std::clamp<int>(specs->Specs[0].ColumnIndex, 0, kStorageKinds));
        descending = specs->Specs[0].SortDirection == ImGuiSortDirection_Descending;
    }
    sortStorageRows(rows, column, descending);

    for (const StorageRow& row : rows) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(row.name.c_str());
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("건물 종류 %s", row.id.c_str());   // 설정 파일에 적히는 번호
        ImGui::PushID(row.id.c_str());
        for (int kind = 0; kind < kStorageKinds; ++kind) {
            ImGui::TableNextColumn();
            const int base = row.defaults[static_cast<size_t>(kind)];
            if (base <= 0) {
                ImGui::AlignTextToFramePadding();
                ImGui::TextDisabled("-");
                continue;
            }
            ImGui::PushID(kind);
            std::optional<int> value = row.values[static_cast<size_t>(kind)];
            const std::string hint = std::to_string(base);
            if (optionalNumberField("##limit", value, 1, kStorageLimitMax, -FLT_MIN, hint.c_str())) {
                setStorageLimit(settings, row.id, static_cast<StorageKind>(kind), value);
                changed = true;
            }
            if (value && ImGui::IsItemHovered()) ImGui::SetTooltip("게임의 기본값 %d", base);
            ImGui::PopID();
        }
        ImGui::PopID();
    }
    ImGui::EndTable();
    return changed;
}

// "N배로"와 "값 지우기". 검색으로 줄을 추렸으면 보이는 줄에만 적용한다. 값을 바꿨으면 true
bool drawBulk(const std::vector<StorageRow>& rows, bool filtered, StorageSettings& settings) {
    bool changed = false;
    const std::string count = std::to_string(rows.size());
    numberField("##storagemultiplier", g_multiplier, 1, 1000, scaled(60.0f));
    ImGui::SameLine();
    const std::string fillLabel = filtered ? "보이는 " + count + "줄을 기본값의 이 배수로###storagefill" : std::string("모두 기본값의 이 배수로###storagefill");
    if (ImGui::Button(fillLabel.c_str()) && !rows.empty()) {
        fillStorageLimits(settings, rows, g_multiplier);
        changed = true;
    }
    ImGui::SameLine();
    // 한 번에 여러 값이 사라지므로 두 번 눌러야 지운다
    const double now = ImGui::GetTime();
    const bool armed = now < g_clearArmedUntil;
    std::string clearLabel = filtered ? "보이는 " + count + "줄의 값 지우기" : std::string("값 모두 지우기");
    if (armed) clearLabel = "한 번 더 누르면 지웁니다";
    if (ImGui::Button((clearLabel + "###storageclear").c_str())) {
        if (armed) {
            clearStorageLimits(settings, rows, filtered);
            g_clearArmedUntil = -1.0;
            changed = true;
        } else {
            g_clearArmedUntil = now + kClearConfirmSec;
        }
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("값을 지운 건물은 게임의 기본 용량으로 돌아갑니다");
    return changed;
}
}

void drawStorageSection(TabContext& ctx) {
    App& a = ctx.app;
    ImGui::SeparatorText("저장 용량");
    StorageSettings s = a.control.storage();
    bool changed = ImGui::Checkbox("건물 저장 용량 변경", &s.enabled);

    const std::vector<StorageRow> all = buildStorageRows(a.buildings.get(), s);
    if (all.empty()) {
        // 모드가 시작할 때 bridge/catalog.json 에 건물 목록을 쓴다. 예전 판의 모드에는 없다
        ImGui::TextDisabled("건물 목록을 읽지 못했습니다. 모드를 새 판으로 배포하고 게임을 다시 켜면 표가 나옵니다");
    } else {
        ImGui::SameLine();
        ImGui::TextUnformatted("검색");
        ImGui::SameLine();
        textField("##storagesearch", g_search, 60, scaled(150.0f));
        ImGui::SameLine();
        hangulModeButton();
        if (!g_search.empty()) {
            ImGui::SameLine();
            if (ImGui::Button("지우기##storagesearch")) g_search.clear();
        }
        std::vector<StorageRow> rows = filterStorageRows(all, g_search);
        const bool filtered = rows.size() != all.size();
        ImGui::SameLine();
        ImGui::TextDisabled("%zu / %zu", rows.size(), all.size());

        changed |= drawBulk(rows, filtered, s);
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("내 영지의 건물에만 적용 · 빈칸 = 게임의 값 그대로(회색 숫자가 기본값) · \"-\" = 그 저장실이 없는 건물 · "
                            "용량을 다시 줄이거나 끄면 한도를 넘는 자원이 날씨 피해로 사라질 수 있습니다");
        ImGui::PopTextWrapPos();
        if (rows.empty()) ImGui::TextDisabled("조건에 맞는 건물이 없습니다");
        else changed |= drawTable(rows, s);
    }
    if (changed) {
        a.control.setStorage(s);
        markDirty(a);
    }
}

}
