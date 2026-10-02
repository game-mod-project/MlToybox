#include "test.h"
#include "overlay/ui/app.h"
#include "overlay/ui/tabs.h"
#include <imgui.h>
#include <string>
#include <vector>

using namespace mlt::ov;

// 탭을 실제 ImGui 프레임으로 그리고 마우스 클릭을 넣어 본다(그래픽 장치 없음).
namespace {
struct TabFrames {
    App& a = app();

    TabFrames() {
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
        a.control = ControlDoc();
        a.dirty = false;
    }
    ~TabFrames() {
        for (ImTextureData* tex : ImGui::GetPlatformIO().Textures) {
            tex->SetTexID(ImTextureID_Invalid);
            tex->SetStatus(ImTextureStatus_Destroyed);
        }
        ImGui::DestroyContext();
        a.control = ControlDoc();
        a.dirty = false;
    }
    void frame(void (*draw)(TabContext&)) {
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(800.0f, 600.0f);
        io.DeltaTime = 0.5f;   // 이어지는 클릭이 두 번 누르기로 묶이지 않게
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(ImVec2(800.0f, 600.0f));
        ImGui::Begin("test", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
        TabContext ctx{ a, nullptr, false, 0 };
        draw(ctx);
        ImGui::End();
        ImGui::Render();
        for (ImTextureData* tex : ImGui::GetPlatformIO().Textures) {
            if (tex->Status == ImTextureStatus_WantCreate) tex->SetTexID(static_cast<ImTextureID>(1));
            if (tex->Status == ImTextureStatus_WantCreate || tex->Status == ImTextureStatus_WantUpdates) tex->SetStatus(ImTextureStatus_OK);
        }
    }
    void click(void (*draw)(TabContext&), float x, float y) {
        ImGuiIO& io = ImGui::GetIO();
        io.AddMousePosEvent(x, y);
        frame(draw);
        io.AddMouseButtonEvent(0, true);
        frame(draw);
        io.AddMouseButtonEvent(0, false);
        frame(draw);
    }
};
}

// 업그레이드(건물 업그레이드의 조건·비용·해금)는 건물 기능이라 건설 탭에 있다. 따로 탭을 두지 않는다
TEST(overlay_tabs_upgrade_lives_in_the_build_tab) {
    CHECK(tabNames() == (std::vector<std::string>{ "자원", "영주", "건설", "군사", "용병", "인구", "상태", "로그" }));

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
