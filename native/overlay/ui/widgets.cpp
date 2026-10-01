#include "widgets.h"
#include "overlay/core/number_input.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <imgui.h>
#include <unordered_map>

namespace mlt::ov {

bool numberField(const char* id, int& value, int min, int max, float width) {
    static std::unordered_map<ImGuiID, std::array<char, 32>> buffers;
    auto& buf = buffers[ImGui::GetID(id)];
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

void needGameText() {
    ImGui::TextDisabled("게임에 들어가면 표시됩니다");
}

}
