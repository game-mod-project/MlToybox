#include "widgets.h"
#include "app.h"
#include "overlay/core/hangul.h"
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

namespace {
// 글자 칸의 한글 조합 상태. 글자 칸은 한 번에 하나만 편집하므로 하나면 된다
struct HangulUi {
    HangulField field;
    bool on = false;          // 한글 모드(꺼져 있으면 친 글자가 그대로 들어간다)
    unsigned seenKeys = 0;    // App::hangulKeys 에서 마지막으로 본 값
    bool refocus = false;     // 단추로 한/영을 바꿨다. 다음 프레임에 글자 칸으로 커서를 돌려준다
};
HangulUi g_hangul;

void toggleHangul() {
    g_hangul.on = !g_hangul.on;
    g_hangul.field.reset();   // 조합 중이던 글자는 그대로 두고 조합만 끝낸다
}

int hangulCallback(ImGuiInputTextCallbackData* data) {
    HangulUi& h = g_hangul;
    const ImGuiIO& io = ImGui::GetIO();
    if (data->EventFlag == ImGuiInputTextFlags_CallbackCharFilter) {
        if (!h.on) return 0;
        // 붙여넣기(Ctrl+V, Shift+Insert)로 들어온 글자는 조합하지 않는다
        if (io.KeyCtrl || io.KeyAlt || io.KeySuper || ImGui::IsKeyDown(ImGuiKey_Insert)) return 0;
        h.field.push(data->EventChar, io.KeyShift);
        return 1;   // 글자 칸에는 넣지 않는다. 아래에서 조합한 글자를 넣는다
    }
    // 범위를 골라 둔 채 쳤으면 고른 글을 먼저 지운다(글자를 위에서 걸러 냈으므로 글자 칸이 대신 지워 주지 않는다)
    if (h.field.pending() && data->HasSelection()) {
        const int from = std::min(data->SelectionStart, data->SelectionEnd);
        const int to = std::max(data->SelectionStart, data->SelectionEnd);
        data->DeleteChars(from, to - from);
        data->CursorPos = from;
        data->SelectionStart = data->SelectionEnd = from;
    }
    // 프레임마다: 쌓인 글쇠를 조합해 넣고, 밖에서 글이나 커서가 바뀌었으면 조합을 끝낸다
    std::string text(data->Buf, static_cast<size_t>(data->BufTextLen));
    int cursor = data->CursorPos;
    const bool backspace = ImGui::IsKeyPressed(ImGuiKey_Backspace, true);
    if (h.field.apply(text, cursor, static_cast<size_t>(data->BufSize - 1), backspace, data->HasSelection())) {
        data->DeleteChars(0, data->BufTextLen);
        data->InsertChars(0, text.c_str(), text.c_str() + text.size());
        data->CursorPos = cursor;
        data->SelectionStart = data->SelectionEnd = cursor;
    }
    return 0;
}
}

bool textField(const char* id, std::string& value, size_t maxBytes, float width) {
    HangulUi& h = g_hangul;
    // 한/영 글쇠는 창 스레드가 센다. 홀수 번 눌렸으면 바꾼다
    const unsigned keys = app().hangulKeys.load();
    if (keys != h.seenKeys) {
        if ((keys - h.seenKeys) % 2 != 0) toggleHangul();
        h.seenKeys = keys;
    }
    char buf[256];
    const size_t capacity = std::min(maxBytes + 1, sizeof(buf));
    std::snprintf(buf, capacity, "%s", value.c_str());
    ImGui::SetNextItemWidth(width);
    if (h.refocus) {
        ImGui::SetKeyboardFocusHere();
        h.refocus = false;
    }
    const bool changed = ImGui::InputText(id, buf, capacity, ImGuiInputTextFlags_CallbackCharFilter | ImGuiInputTextFlags_CallbackAlways, hangulCallback);
    // 글자 칸을 떠나면 조합을 끝낸다. 들어올 때도 비운다(창을 닫아 떠난 경우에는 떠나는 것을 보지 못한다)
    if (ImGui::IsItemDeactivated() || ImGui::IsItemActivated()) h.field.reset();
    if (!changed) return false;
    value = buf;
    return true;
}

bool hangulModeOn() {
    return g_hangul.on;
}

void hangulModeButton() {
    if (ImGui::Button(g_hangul.on ? "한##hangul" : "A##hangul", ImVec2(ImGui::GetFrameHeight() * 1.4f, 0.0f))) {
        toggleHangul();
        g_hangul.refocus = true;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("한/영 (한/영 키로도 바꿉니다)");
}

void needGameText() {
    ImGui::TextDisabled("게임에 들어가면 표시됩니다");
}

}
