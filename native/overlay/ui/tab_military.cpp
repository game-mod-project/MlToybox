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
int g_unit = 1;                          // 병종 목록의 줄. 패널처럼 "민병대 - 창"에서 시작한다
int g_count = 1;
std::optional<std::string> g_region;     // 병력 생성·재구성·꾸미기의 위치(영지 키)
std::optional<std::string> g_squad;      // 고른 수행원 분대의 ID

std::vector<ScopeOption> unitChoices() {
    std::vector<ScopeOption> options;
    for (const UnitOption& u : units()) options.push_back({ u.id, u.label });
    return options;
}

void drawSpawn(TabContext& ctx, const StatusDoc* live) {
    static const std::vector<ScopeOption> unitOptions = unitChoices();
    ImGui::SeparatorText("병력 생성 (주민과 무관)");
    comboOptions("##unit", unitOptions, g_unit, scaled(180.0f));
    ImGui::SameLine();
    numberField("##count", g_count, 1, 5, scaled(50.0f));
    ImGui::SameLine();
    ImGui::TextUnformatted("개 분대, 위치:");
    ImGui::SameLine();
    const std::vector<ScopeOption> regions = spawnRegionOptions(live && live->playerRegions ? &*live->playerRegions : nullptr);
    int region = indexOfKey(regions, g_region);
    comboOptions("##region", regions, region, scaled(200.0f));
    g_region = regions[static_cast<size_t>(region)].key;   // 골라 둔 영지가 사라졌으면 첫 줄로 돌아간다
    ImGui::SameLine();
    ImGui::BeginDisabled(!ctx.inGame);
    if (ImGui::Button("분대 생성")) {
        sendCommand(ctx.app, makeSpawnSquads(units()[static_cast<size_t>(g_unit)].id, g_count, ctx.now, g_region));
    }
    ImGui::EndDisabled();

    // 생성 분대는 집이 없어 게임의 해제 → 집결이 안 된다(해제하면 0/N 빈 카드). 모드가 같은 병종으로 다시 만든다
    const SpawnStatus* spawn = live && live->spawn ? &*live->spawn : nullptr;
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(reformText(spawn).c_str());
    ImGui::SameLine();
    ImGui::BeginDisabled(!canReform(spawn));
    if (ImGui::Button("재구성")) sendCommand(ctx.app, makeReformSquads(ctx.now, g_region));
    ImGui::EndDisabled();
}

// 모드가 만든 수행원 분대에 게임의 꾸미기 화면을 연다(위 "위치" 영지의 영주 저택 기준)
void drawRetinue(TabContext& ctx, const StatusDoc* live) {
    ImGui::SeparatorText("수행원 꾸미기 (생성·고용한 친위대 분대)");
    const RetinueStatus* retinue = live && live->retinue ? &*live->retinue : nullptr;
    std::vector<ScopeOption> squads;
    if (retinue) {
        for (const RetinueSquad& s : retinue->squads) squads.push_back({ std::to_string(s.id), retinueLabel(s) });
    }
    if (squads.empty()) {
        ImGui::TextDisabled("꾸밀 수 있는 분대가 없습니다");
        return;
    }
    int squad = indexOfKey(squads, g_squad);
    comboOptions("##squad", squads, squad, scaled(260.0f));
    g_squad = squads[static_cast<size_t>(squad)].key;
    ImGui::SameLine();
    // 꾸미기 화면이 열려 있는 동안에는 다시 열 수 없다
    const std::string label = (retinue->editing ? "꾸미기 열림 (#" + std::to_string(*retinue->editing) + ")" : std::string("꾸미기 열기")) + "##retinue";
    ImGui::BeginDisabled(retinue->editing.has_value());
    if (ImGui::Button(label.c_str())) {
        sendCommand(ctx.app, makeCustomizeRetinue(retinue->squads[static_cast<size_t>(squad)].id, ctx.now, g_region));
        ctx.app.visible = false;   // 게임의 꾸미기 화면을 가리지 않게 창을 닫는다
    }
    ImGui::EndDisabled();
}
}

void drawMilitaryTab(TabContext& ctx) {
    App& a = ctx.app;
    MilitarySettings m = a.control.military();
    bool changed = false;
    changed |= ImGui::Checkbox("군사 기능 사용", &m.enabled);
    changed |= ImGui::Checkbox("민병대 장비 요구 무시", &m.ignoreEquipment);
    changed |= ImGui::Checkbox("징집 조건(집 레벨·훈련) 무시", &m.ignorePopulation);
    ImGui::Indent();
    ImGui::TextDisabled("주민 수보다 많은 병력은 아래 '병력 생성' 사용");
    ImGui::Unindent();
    changed |= ImGui::Checkbox("민병대 모집비 0", &m.zeroUpkeep);
    changed |= ImGui::Checkbox("부대 수 상한 해제", &m.unlimitedSquads);
    if (changed) {
        a.control.setMilitary(m);
        markDirty(a);
    }

    const StatusDoc* live = ctx.inGame ? ctx.status : nullptr;   // 게임 안일 때만 게임 상태를 쓴다
    drawSpawn(ctx, live);
    drawRetinue(ctx, live);
    if (!ctx.inGame) needGameText();
}

}
