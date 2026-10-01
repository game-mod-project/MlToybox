#include "overlay/core/commands.h"
#include "tabs.h"
#include "widgets.h"
#include <cstdio>
#include <imgui.h>
#include <optional>
#include <string>

namespace mlt::ov {

namespace {
constexpr int kLordMax = 10000000;

// 체크를 꺼도 입력해 둔 값은 남긴다(다시 켜면 그 값, '지금 설정'도 그 값을 쓴다)
struct Row {
    const char* label;
    const char* key;
    int remembered = 0;
    bool seeded = false;
};

std::string currentText(const std::optional<double>& value) {
    if (!value) return "현재: -";
    char buf[48];
    std::snprintf(buf, sizeof(buf), "현재: %.0f", *value);
    return buf;
}

// 줄 하나: [체크] 이름  목표 [값] [지금 설정]  현재: N. 설정이 바뀌었으면 true
bool drawRow(TabContext& ctx, Row& row, std::optional<int>& target, const std::optional<double>& current) {
    bool changed = false;
    if (!row.seeded || target) {   // 저장된 목표가 있으면 그 값을 보여 준다
        if (target) row.remembered = *target;
        row.seeded = true;
    }
    ImGui::PushID(row.key);
    bool use = target.has_value();
    if (ImGui::Checkbox(row.label, &use)) {
        if (use) target = row.remembered;
        else target.reset();
        changed = true;
    }
    ImGui::SameLine(130.0f * ImGui::GetStyle().FontScaleMain);
    ImGui::TextUnformatted("목표");
    ImGui::SameLine();
    int value = row.remembered;
    if (numberField("##target", value, 0, kLordMax, 120.0f * ImGui::GetStyle().FontScaleMain)) {
        row.remembered = value;
        if (target) {
            target = value;
            changed = true;
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!ctx.inGame);
    if (ImGui::Button("지금 설정")) sendCommand(ctx.app, makeSetLord(row.key, row.remembered, ctx.now));
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextUnformatted(currentText(current).c_str());
    ImGui::PopID();
    return changed;
}
}

void drawLordTab(TabContext& ctx) {
    static Row treasury{ "국고", "treasury" };
    static Row influence{ "영향력", "influence" };
    static Row favour{ "왕의 총애", "kingsFavour" };

    App& a = ctx.app;
    LordSettings l = a.control.lord();
    const std::optional<LordStatus> status = (ctx.inGame && ctx.status) ? ctx.status->lord : std::nullopt;
    auto asDouble = [](const std::optional<int>& v) { return v ? std::optional<double>(*v) : std::nullopt; };

    bool changed = ImGui::Checkbox("영주 자원 목표값 유지 (영지와 무관한 전체 값)", &l.enabled);
    changed |= drawRow(ctx, treasury, l.treasury, status ? status->treasury : std::nullopt);
    changed |= drawRow(ctx, influence, l.influence, status ? asDouble(status->influence) : std::nullopt);
    changed |= drawRow(ctx, favour, l.kingsFavour, status ? asDouble(status->kingsFavour) : std::nullopt);
    if (changed) {
        a.control.setLord(l);
        markDirty(a);
    }

    ImGui::Spacing();
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("체크한 항목은 목표값 아래로 내려가면 목표까지 채웁니다(더 많으면 그대로). 체크 해제 = 관리 안 함.");
    ImGui::TextDisabled("'지금 설정'은 체크와 상관없이 입력한 값으로 한 번 정확히 맞춥니다(내리기도 가능).");
    ImGui::PopTextWrapPos();
    if (!ctx.inGame) needGameText();
}

}
