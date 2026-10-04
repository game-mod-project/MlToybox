#include "overlay/core/commands.h"
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
int g_addCount = 3;

// 목표 가족 수. 공통이면 "영지마다 최소 가족 수", 영지를 골랐으면 그 영지 값(따로 지정했을 때만 고칠 수 있다).
// 설정이 바뀌었으면 true
bool drawTarget(PopulationSettings& p) {
    bool changed = false;
    ImGui::AlignTextToFramePadding();
    if (!g_scope) {
        ImGui::TextUnformatted("영지마다 최소 가족 수(0 = 끔, 부족분만 채움):");
        ImGui::SameLine();
        return numberField("##target", p.targetFamilies, 0, 1000, scaled(70.0f));
    }
    const auto own = p.regionTargets.find(*g_scope);
    bool separate = own != p.regionTargets.end();
    int value = separate ? own->second : p.targetFamilies;   // 따로 지정하지 않았으면 공통 값을 보여 준다
    ImGui::TextUnformatted("이 영지 최소 가족 수(0 = 끔):");
    ImGui::SameLine();
    ImGui::PushID(g_scope->c_str());   // 영지마다 다른 칸이다(편집 중인 글이 섞이지 않게)
    ImGui::BeginDisabled(!separate);
    if (numberField("##regionTarget", value, 0, 1000, scaled(70.0f)) && separate) {
        p.regionTargets[*g_scope] = value;
        changed = true;
    }
    ImGui::EndDisabled();
    ImGui::PopID();
    ImGui::SameLine();
    if (ImGui::Checkbox("이 영지만 따로 지정", &separate)) {
        if (separate) p.regionTargets[*g_scope] = value;
        else p.regionTargets.erase(*g_scope);   // 끄면 공통 값을 따른다
        changed = true;
    }
    return changed;
}
}

void drawPopulationTab(TabContext& ctx) {
    App& a = ctx.app;
    PopulationSettings p = a.control.population();
    const PopulationStatus* live = (ctx.inGame && ctx.status && ctx.status->population) ? &*ctx.status->population : nullptr;

    bool changed = ImGui::Checkbox("인구 기능 사용", &p.enabled);

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("영지:");
    ImGui::SameLine();
    const std::vector<ScopeOption> scopes = populationScopeOptions(live);
    int scope = indexOfKey(scopes, g_scope);
    comboOptions("##scope", scopes, scope, scaled(230.0f));
    g_scope = scopes[static_cast<size_t>(scope)].key;   // 골라 둔 영지가 사라졌으면 공통으로 돌아간다

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("월 자연 이민 가족 수(0 = 게임 그대로):");
    ImGui::SameLine();
    changed |= numberField("##monthly", p.monthlyFamilies, 0, 31, scaled(50.0f));
    ImGui::SameLine();
    ImGui::TextUnformatted("이민 속도 배율(배):");
    ImGui::SameLine();
    changed |= numberField("##multiplier", p.multiplier, 1, 10, scaled(50.0f));

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("집의 수용 가족 수 배율(배):");
    ImGui::SameLine();
    changed |= numberField("##houseCapacity", p.houseCapacity, 1, 10, scaled(50.0f));

    changed |= drawTarget(p);
    if (changed) {
        a.control.setPopulation(p);
        markDirty(a);
    }

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("지금 바로 들일 가족 수:");
    ImGui::SameLine();
    numberField("##add", g_addCount, 1, 20, scaled(50.0f));
    ImGui::SameLine();
    ImGui::BeginDisabled(!ctx.inGame);
    if (ImGui::Button("가족 추가")) sendCommand(a, makeAddFamilies(g_addCount, ctx.now, g_scope));
    ImGui::EndDisabled();

    ImGui::Spacing();
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("월 가족 수와 배율은 내 영지의 자연 이민을 바꿉니다(영지 창의 인구 증가 숫자도 따라 바뀝니다). 월 가족 수는 지지율과 무관하게 매달 그만큼, 배율은 게임의 값에 곱합니다. 월 가족 수를 넣으면 배율은 쓰이지 않습니다.");
    ImGui::TextDisabled("자연 이민은 그 달에 고르게 나눠 하루 한 가족까지 오고, 빈 주거 공간이 없거나 집 없는 가족이 있으면 오지 않습니다(게임의 규칙).");
    ImGui::TextDisabled("수용 배율은 내 집이 받을 수 있는 가족 수(1·2레벨 1, 3레벨 2, 4레벨 3, 확장이 있으면 +1)에 곱합니다. 늘어난 자리는 자연 이민으로 찹니다. 가족 추가와 최소 가족 수는 집당 2가족까지만 들입니다.");
    ImGui::TextDisabled("배율을 낮추거나 꺼도 이미 들어온 가족은 그 집에 그대로 삽니다(세이브를 불러와도 같습니다). 새 가족만 더 들어오지 않습니다.");
    ImGui::TextDisabled("가족 추가는 선택한 영지에, 공통이면 빈 자리가 많은 영지부터 들입니다.");
    ImGui::TextDisabled("빈 집(가족 0 → 1 → 2 순)에 들어오며 미배치 가족으로 들어옵니다. 빈 자리가 없으면 들어오지 않습니다.");
    ImGui::Spacing();
    for (const std::string& line : populationInfo(live, g_scope)) ImGui::TextUnformatted(line.c_str());
    ImGui::PopTextWrapPos();
}

}
