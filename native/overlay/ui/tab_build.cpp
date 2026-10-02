#include "tabs.h"
#include <imgui.h>

namespace mlt::ov {

void drawBuildTab(TabContext& ctx) {
    App& a = ctx.app;
    BuildSettings b = a.control.build();
    bool changed = false;
    changed |= ImGui::Checkbox("건설 기능 사용", &b.enabled);
    changed |= ImGui::Checkbox("배치 제한 무시 (영지 경계 안, 네이티브 DLL)", &b.ignorePlacement);
    changed |= ImGui::Checkbox("즉시 완공 (네이티브 DLL)", &b.instantBuild);
    changed |= ImGui::Checkbox("즉시 수리", &b.instantRepair);
    changed |= ImGui::Checkbox("자재 불필요 (건설 자재 없이 공사)", &b.noMaterials);
    changed |= ImGui::Checkbox("지역당 개수 제한 해제", &b.noRegionLimit);
    ImGui::Indent();
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("영주 저택 모듈·세금 징수소·장작/식량 수레. 끈 뒤에는 게임을 다시 켜야 원래대로");
    ImGui::PopTextWrapPos();
    ImGui::Unindent();
    if (changed) {
        a.control.setBuild(b);
        markDirty(a);
    }

    // 업그레이드: 건물 업그레이드 표(DT_Upgrades)와 주거지 레벨 조건을 고치는 기능이라 건설 탭에 둔다.
    // 설정은 따로다(features.upgrade). 위의 "건설 기능 사용"과 무관하게 켜고 끈다. 패널에는 따로 탭이 있다
    ImGui::SeparatorText("업그레이드");
    UpgradeSettings u = a.control.upgrade();
    if (ImGui::Checkbox("업그레이드 조건·비용·해금 무시", &u.enabled)) {
        a.control.setUpgrade(u);
        markDirty(a);
    }

    // 저장 용량: 건물의 저장 한도를 바꾸는 기능이라 건설 탭에 둔다. 설정은 따로다(features.storage)
    drawStorageSection(ctx);
}

}
