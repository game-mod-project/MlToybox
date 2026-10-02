#include "test.h"
#include "overlay/core/resources.h"
#include "overlay/ui/app.h"
#include "overlay/ui/clipboard_sync.h"
#include "overlay/ui/window.h"
#include "runtime.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <memory>
#include <string>

using namespace mlt::ov;

// 메인 창 전체를 실제 ImGui 프레임으로 그리고 마우스를 넣어 본다(그래픽 장치 없음). 창은 게임에서처럼 옮길 수 있는 창이다.
namespace {
struct OverlayFrames {
    App& a = app();

    OverlayFrames() {
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
        installClipboardCallbacks();
        configureOverlayInput();
        reset();
        a.settings.devTab = "자원";
        a.settings.x = 80;
        a.settings.y = 80;
        a.settings.w = 760;
        a.settings.h = 560;
        a.applyWindowRect = true;
        a.visible = true;
        a.catalog = std::make_shared<const ResourceCatalog>(ResourceCatalog::parse(R"({"resources":[
            {"id":"Timber","name":"목재","category":"건설","group":"목재 작업물"},
            {"id":"Beef","name":"소고기","category":"식량","group":"고기"},
            {"id":"Pork","name":"돼지고기","category":"식량","group":"고기"}]})"));
        const std::string status = R"({"heartbeat":)" + std::to_string(mlt::nowEpochSeconds())
            + R"(,"inGame":true,"appliedSeq":0,"resourceIds":["Timber","Beef","Pork"],"resources":{"Timber":740,"Beef":300,"Pork":20}})";
        a.status = std::make_shared<const StatusDoc>(*parseStatus(status));
        a.lastSentSeq = 0;
    }
    ~OverlayFrames() {
        for (ImTextureData* tex : ImGui::GetPlatformIO().Textures) {
            tex->SetTexID(ImTextureID_Invalid);
            tex->SetStatus(ImTextureStatus_Destroyed);
        }
        ImGui::DestroyContext();
        reset();
    }
    void reset() {
        a.control = ControlDoc();
        a.dirty = false;
        a.settings = OverlaySettings();
        a.settingsDirty = false;
        a.catalog.reset();
        a.status.reset();
        a.visible = false;
    }
    void frame() {
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(1200.0f, 800.0f);
        io.DeltaTime = 0.5f;
        ImGui::NewFrame();
        drawOverlay(a);
        ImGui::Render();
        for (ImTextureData* tex : ImGui::GetPlatformIO().Textures) {
            if (tex->Status == ImTextureStatus_WantCreate) tex->SetTexID(static_cast<ImTextureID>(1));
            if (tex->Status == ImTextureStatus_WantCreate || tex->Status == ImTextureStatus_WantUpdates) tex->SetStatus(ImTextureStatus_OK);
        }
    }
    void moveTo(float x, float y) {
        ImGui::GetIO().AddMousePosEvent(x, y);
        frame();
        frame();
    }
    ImVec2 windowPos() {
        return ImGui::FindWindowByName("MLToybox")->Pos;
    }
    // 자원 표의 첫 번째 열 경계(마우스 모양이 좌우 화살표가 되는 곳)
    bool findBorder(float& x, float& y) {
        for (y = 200.0f; y < 420.0f; y += 6.0f) {
            for (x = 120.0f; x < 800.0f; x += 2.0f) {
                moveTo(x, y);
                if (ImGui::GetMouseCursor() == ImGuiMouseCursor_ResizeEW) return true;
            }
        }
        return false;
    }
};
}

// 열 경계를 잡으려다 조금 빗나가 표의 글 위를 눌러 끌어도 창이 따라 움직이지 않는다. 창은 제목 줄을 끌어서만 옮긴다
// (사용자 확인 2026-10-02: 열 경계가 올바르게 잡히지 않는다)
TEST(overlay_window_does_not_move_when_a_drag_starts_inside_the_table) {
    OverlayFrames f;
    for (int i = 0; i < 4; ++i) f.frame();
    float borderX = 0.0f, borderY = 0.0f;
    CHECK(f.findBorder(borderX, borderY));
    const ImVec2 before = f.windowPos();
    ImGuiIO& io = ImGui::GetIO();
    // 경계에서 왼쪽으로 12픽셀 벗어난, 첫 열의 글이 있는 자리
    f.moveTo(borderX - 12.0f, borderY + 30.0f);
    io.AddMouseButtonEvent(0, true);
    f.frame();
    f.moveTo(borderX + 60.0f, borderY + 70.0f);
    io.AddMouseButtonEvent(0, false);
    f.frame();
    f.frame();
    const ImVec2 after = f.windowPos();
    CHECK(after.x == before.x && after.y == before.y);
    // 제목 줄을 끌면 옮겨진다
    f.moveTo(before.x + 200.0f, before.y + 8.0f);
    io.AddMouseButtonEvent(0, true);
    f.frame();
    f.moveTo(before.x + 240.0f, before.y + 38.0f);
    io.AddMouseButtonEvent(0, false);
    f.frame();
    f.frame();
    CHECK(f.windowPos().x == before.x + 40.0f && f.windowPos().y == before.y + 30.0f);
}

// 게임은 자기 커서를 그리므로 ImGui 가 바꾸려는 커서 모양은 보이지 않는다. 열 경계나 창 가장자리 위에서는
// 오버레이가 크기 조절 화살표를 직접 그려, 잡을 수 있는 자리임을 알린다. 그 밖에서는 그리지 않는다
TEST(overlay_window_draws_a_resize_cursor_over_a_column_border) {
    OverlayFrames f;
    for (int i = 0; i < 4; ++i) f.frame();
    ImGuiIO& io = ImGui::GetIO();
    CHECK(io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange);   // 게임의 커서는 건드리지 않는다
    CHECK(!io.MouseDrawCursor);
    float borderX = 0.0f, borderY = 0.0f;
    CHECK(f.findBorder(borderX, borderY));
    CHECK(io.MouseDrawCursor);
    f.moveTo(borderX - 40.0f, borderY + 30.0f);
    CHECK(!io.MouseDrawCursor);
}
