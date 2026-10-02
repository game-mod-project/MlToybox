#include "widgets.h"
#include "overlay/core/number_input.h"
#include "overlay/core/text.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <imgui.h>
#include <unordered_map>

namespace mlt::ov {

float scaled(float px) {
    return px * ImGui::GetStyle().FontScaleMain;
}

namespace {
// 숫자 칸마다 편집 중인 글을 들고 있는다(ImGui 는 칸의 글을 호출하는 쪽이 들고 있어야 한다)
std::array<char, 32>& numberBuffer(const char* id) {
    static std::unordered_map<ImGuiID, std::array<char, 32>> buffers;
    return buffers[ImGui::GetID(id)];
}
}

bool numberField(const char* id, int& value, int min, int max, float width) {
    auto& buf = numberBuffer(id);
    ImGui::SetNextItemWidth(width);
    ImGui::InputText(id, buf.data(), buf.size(), ImGuiInputTextFlags_AutoSelectAll);
    bool changed = false;
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        if (auto parsed = parseNumber(buf.data())) {
            const int clamped = std::clamp(*parsed, min, max);
            if (clamped != value) {
                value = clamped;
                changed = true;
            }
        }
    }
    if (!ImGui::IsItemActive()) std::snprintf(buf.data(), buf.size(), "%d", value);
    return changed;
}

bool optionalNumberField(const char* id, std::optional<int>& value, int min, int max, float width) {
    auto& buf = numberBuffer(id);
    ImGui::SetNextItemWidth(width);
    ImGui::InputText(id, buf.data(), buf.size(), ImGuiInputTextFlags_AutoSelectAll);
    bool changed = false;
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        if (trim(buf.data()).empty()) {
            changed = value.has_value();
            value.reset();
        } else if (auto parsed = parseNumber(buf.data())) {
            const int clamped = std::clamp(*parsed, min, max);
            changed = value != clamped;
            value = clamped;
        }
    }
    if (!ImGui::IsItemActive()) {
        if (value) std::snprintf(buf.data(), buf.size(), "%d", *value);
        else buf[0] = '\0';
    }
    return changed;
}

bool comboOptions(const char* id, const std::vector<ScopeOption>& options, int& index, float width) {
    if (options.empty()) return false;
    if (index < 0 || index >= static_cast<int>(options.size())) index = 0;
    bool changed = false;
    ImGui::SetNextItemWidth(width);
    if (ImGui::BeginCombo(id, options[static_cast<size_t>(index)].label.c_str())) {
        for (int i = 0; i < static_cast<int>(options.size()); ++i) {
            ImGui::PushID(i);   // 같은 이름의 줄이 있어도 구별한다
            const bool selected = i == index;
            if (ImGui::Selectable(options[static_cast<size_t>(i)].label.c_str(), selected) && !selected) {
                index = i;
                changed = true;
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    return changed;
}

bool textField(const char* id, std::string& value, size_t maxBytes, float width) {
    char buf[256];
    const size_t capacity = std::min(maxBytes + 1, sizeof(buf));
    std::snprintf(buf, capacity, "%s", value.c_str());
    ImGui::SetNextItemWidth(width);
    if (!ImGui::InputText(id, buf, capacity)) return false;
    value = buf;
    return true;
}

void needGameText() {
    ImGui::TextDisabled("게임에 들어가면 표시됩니다");
}

}
