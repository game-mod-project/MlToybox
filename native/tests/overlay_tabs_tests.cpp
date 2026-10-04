#include "test.h"
#include "overlay/ui/app.h"
#include "overlay/ui/tabs.h"
#include "tab_frames.h"
#include <imgui.h>
#include <string>
#include <vector>

using namespace mlt::ov;

// 업그레이드(건물 업그레이드의 조건·비용·해금)는 건물 기능이라 건설 탭에 있다. 따로 탭을 두지 않는다
TEST(overlay_tabs_upgrade_lives_in_the_build_tab) {
    CHECK(tabNames() == (std::vector<std::string>{ "자원", "영주", "건설", "군사", "용병", "인구", "자격·질서", "영지", "상태", "로그" }));

    TabFrames f;
    f.frame(drawBuildTab);
    CHECK(!f.a.control.upgrade().enabled);
    // 건설 탭의 왼쪽 가장자리를 위에서 아래로 눌러 본다. 어느 한 줄은 업그레이드 설정만 바꿔야 한다
    bool found = false;
    for (float y = 12.0f; y < 590.0f && !found; y += 6.0f) {
        const BuildSettings before = f.a.control.build();
        f.click(drawBuildTab, 20.0f, y);
        if (!f.a.control.upgrade().enabled) continue;
        found = true;
        const BuildSettings after = f.a.control.build();
        CHECK(after.enabled == before.enabled && after.ignorePlacement == before.ignorePlacement && after.instantBuild == before.instantBuild);
        CHECK(after.instantRepair == before.instantRepair && after.noMaterials == before.noMaterials && after.noRegionLimit == before.noRegionLimit);
        CHECK(f.a.dirty);                                 // 바꾸면 바로 저장 대상이 된다
        f.click(drawBuildTab, 20.0f, y);                  // 다시 누르면 꺼진다
        CHECK(!f.a.control.upgrade().enabled);
    }
    CHECK(found);
}
