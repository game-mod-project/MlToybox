#include "test.h"
#include "overlay/core/resources.h"
#include "overlay/ui/app.h"
#include "overlay/ui/clipboard_sync.h"
#include "overlay/ui/tabs.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace mlt::ov;

// 자원 탭과 로그 탭을 실제 ImGui 프레임으로 그리고 마우스·글쇠를 넣어 본다(그래픽 장치 없음).
namespace {
const char* kCatalog = R"({"version":1,"resources":[
    {"id":"RegionalWealth","name":"지역 자산","category":"영지"},
    {"id":"Timber","name":"목재","category":"건설","group":"목재 작업물"},
    {"id":"Berries","name":"열매","category":"식량","group":"채집한 상품"},
    {"id":"Beef","name":"소고기","category":"식량","group":"고기"},
    {"id":"Pork","name":"돼지고기","category":"식량","group":"고기"},
    {"id":"spears","name":"창","category":"군사","group":"근접 무기"}]})";

const char* kStatus = R"({"heartbeat":1,"inGame":true,
    "resourceIds":["RegionalWealth","Timber","Berries","Beef","Pork","spears"],
    "resources":{"Timber":740,"Berries":0,"Beef":300,"Pork":20,"RegionalWealth":55,"spears":4}})";

struct Frames {
    App& a = app();
    StatusDoc status = *parseStatus(kStatus);
    float width = 800.0f;

    Frames() {
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
        installClipboardCallbacks();
        reset();
        a.catalog = std::make_shared<const ResourceCatalog>(ResourceCatalog::parse(kCatalog));
    }
    ~Frames() {
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
        a.logLines.clear();
        a.clipboardOut.clear();
        a.clipboardPending = false;
    }
    void frame(void (*draw)(TabContext&)) {
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(1200.0f, 600.0f);
        io.DeltaTime = 0.5f;   // 이어지는 클릭이 두 번 누르기로 묶이지 않게
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(ImVec2(width, 600.0f));
        ImGui::Begin("test", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
        TabContext ctx{ a, &status, true, 0 };
        draw(ctx);
        ImGui::End();
        ImGui::Render();
        for (ImTextureData* tex : ImGui::GetPlatformIO().Textures) {
            if (tex->Status == ImTextureStatus_WantCreate) tex->SetTexID(static_cast<ImTextureID>(1));
            if (tex->Status == ImTextureStatus_WantCreate || tex->Status == ImTextureStatus_WantUpdates) tex->SetStatus(ImTextureStatus_OK);
        }
    }
    void moveTo(void (*draw)(TabContext&), float x, float y) {
        ImGui::GetIO().AddMousePosEvent(x, y);
        frame(draw);
        frame(draw);
    }
    void click(void (*draw)(TabContext&), float x, float y) {
        ImGuiIO& io = ImGui::GetIO();
        moveTo(draw, x, y);
        io.AddMouseButtonEvent(0, true);
        frame(draw);
        io.AddMouseButtonEvent(0, false);
        frame(draw);
    }
    // 탭이 "test" 창에 바로 그리는 화면 조각의 ID
    ImGuiID idOf(const char* label) {
        return ImGui::FindWindowByName("test")->GetID(label);
    }
    // 마우스를 훑어 그 화면 조각 위에 올려 둔다. 누르지는 않는다(다른 조각을 건드리지 않는다)
    bool hover(void (*draw)(TabContext&), ImGuiID id, float fromY, float toY) {
        for (float y = fromY; y < toY; y += 5.0f) {
            for (float x = 8.0f; x < width - 4.0f; x += 8.0f) {
                moveTo(draw, x, y);
                if (ImGui::GetCurrentContext()->HoveredId == id) return true;
            }
        }
        return false;
    }
    bool clickOn(void (*draw)(TabContext&), const char* label, float fromY = 8.0f, float toY = 140.0f) {
        if (!hover(draw, idOf(label), fromY, toY)) return false;
        const ImVec2 at = ImGui::GetIO().MousePos;
        click(draw, at.x, at.y);
        return true;
    }
};

std::map<std::string, int> targets(App& a) {
    return a.control.resources().targets;
}
}

// 탭을 그리기만 해서는(열을 끌지 않으면) 열 너비가 설정에 적히지 않는다. 창 크기를 바꿔도 마찬가지다
TEST(overlay_resource_tab_does_not_save_column_widths_unless_the_user_drags_one) {
    Frames f;
    for (int i = 0; i < 4; ++i) f.frame(drawResourcesTab);
    CHECK(f.a.settings.resourceColumns.empty() && !f.a.settingsDirty);
    f.width = 1100.0f;
    for (int i = 0; i < 4; ++i) f.frame(drawResourcesTab);
    f.width = 500.0f;
    for (int i = 0; i < 4; ++i) f.frame(drawResourcesTab);
    CHECK(f.a.settings.resourceColumns.empty() && !f.a.settingsDirty);
    CHECK(!f.a.dirty);   // 설정 문서도 건드리지 않는다
}

// 열 경계를 끌면 그 너비가 설정에 적힌다(다음 실행에 그대로 쓴다)
TEST(overlay_resource_tab_saves_column_widths_after_a_drag) {
    Frames f;
    for (int i = 0; i < 4; ++i) f.frame(drawResourcesTab);
    ImGuiIO& io = ImGui::GetIO();
    // 표를 가로로 훑어 첫 번째 열 경계를 찾는다: 경계 위에서는 마우스 모양이 좌우 화살표가 된다
    float borderX = -1.0f, borderY = -1.0f;
    for (float y = 60.0f; y < 260.0f && borderX < 0.0f; y += 6.0f) {
        for (float x = 60.0f; x < 700.0f; x += 2.0f) {
            f.moveTo(drawResourcesTab, x, y);
            if (ImGui::GetMouseCursor() == ImGuiMouseCursor_ResizeEW) {
                borderX = x;
                borderY = y;
                break;
            }
        }
    }
    CHECK(borderX > 0.0f);
    io.AddMouseButtonEvent(0, true);
    f.frame(drawResourcesTab);
    CHECK(!f.a.settingsDirty);                       // 끄는 동안에는 적지 않는다
    f.moveTo(drawResourcesTab, borderX + 90.0f, borderY);
    CHECK(!f.a.settingsDirty);
    io.AddMouseButtonEvent(0, false);
    f.frame(drawResourcesTab);
    f.frame(drawResourcesTab);
    CHECK(f.a.settingsDirty);
    const std::vector<float> shares = f.a.settings.resourceColumns;
    CHECK(shares.size() == kResourceColumnCount);
    CHECK(shares[0] > 35.0f);                         // 첫 열이 넓어졌다(기본 30%)
    // 적은 뒤에는 다시 적지 않는다
    f.a.settingsDirty = false;
    for (int i = 0; i < 4; ++i) f.frame(drawResourcesTab);
    CHECK(!f.a.settingsDirty && f.a.settings.resourceColumns == shares);
}

// 설정에 적힌 열 너비로 표를 연다
TEST(overlay_resource_tab_opens_with_the_saved_column_widths) {
    Frames f;
    f.a.settings.resourceColumns = { 55.0f, 15.0f, 15.0f, 15.0f };
    for (int i = 0; i < 4; ++i) f.frame(drawResourcesTab);
    CHECK(!f.a.settingsDirty && f.a.settings.resourceColumns == (std::vector<float>{ 55.0f, 15.0f, 15.0f, 15.0f }));
    // 첫 열 경계가 표 너비의 절반을 넘는 곳에 있다(기본 너비면 30% 근처다)
    float borderX = -1.0f;
    for (float y = 60.0f; y < 260.0f && borderX < 0.0f; y += 6.0f) {
        for (float x = 60.0f; x < 700.0f; x += 2.0f) {
            f.moveTo(drawResourcesTab, x, y);
            if (ImGui::GetMouseCursor() == ImGuiMouseCursor_ResizeEW) {
                borderX = x;
                break;
            }
        }
    }
    CHECK(borderX > 380.0f);
}

// "이 값으로"는 표에 보이는 줄에만 적용한다: 검색으로 고기 두 종만 남기고 누르면 그 둘의 목표만 생긴다
TEST(overlay_resource_tab_fill_applies_to_the_rows_the_search_left) {
    Frames f;
    for (int i = 0; i < 3; ++i) f.frame(drawResourcesTab);
    CHECK(f.clickOn(drawResourcesTab, "##search"));
    CHECK(ImGui::GetIO().WantTextInput);
    ImGui::GetIO().AddInputCharactersUTF8("고기");
    f.frame(drawResourcesTab);
    f.frame(drawResourcesTab);
    CHECK(f.clickOn(drawResourcesTab, "###fill"));
    const std::map<std::string, int> got = targets(f.a);
    CHECK(got.size() == 2 && got.count("Beef") == 1 && got.count("Pork") == 1 && got.at("Beef") == 500);
    CHECK(f.a.dirty);
    // 검색을 지우면 다시 모든 줄에 적용한다
    CHECK(f.clickOn(drawResourcesTab, "지우기##search"));
    CHECK(f.clickOn(drawResourcesTab, "###fill"));
    CHECK(targets(f.a).size() == 6);
}

// 헤더를 누르면 그 열로 정렬한다. 맨 윗줄의 목표 칸에 값을 넣어, 어느 자원이 맨 위에 왔는지로 확인한다
TEST(overlay_resource_tab_header_click_sorts_the_rows) {
    Frames f;
    for (int i = 0; i < 4; ++i) f.frame(drawResourcesTab);
    // 머리줄의 높이와 열 경계들을 찾는다(경계 위에서는 마우스 모양이 좌우 화살표가 된다)
    float headerY = -1.0f;
    std::vector<float> borders;
    for (float y = 60.0f; y < 260.0f && headerY < 0.0f; y += 6.0f) {
        for (float x = 20.0f; x < 790.0f; x += 2.0f) {
            f.moveTo(drawResourcesTab, x, y);
            if (ImGui::GetMouseCursor() != ImGuiMouseCursor_ResizeEW) continue;
            headerY = y;
            if (borders.empty() || x - borders.back() > 12.0f) borders.push_back(x);
        }
    }
    CHECK(headerY > 0.0f && borders.size() == 3);   // 열 넷 사이의 경계 셋
    const float currentX = (borders[1] + borders[2]) / 2.0f;
    const float targetX = (borders[2] + 790.0f) / 2.0f;

    // 맨 윗줄의 목표 칸에 77 을 넣고, 그 줄의 자원 id 를 돌려준다
    auto topRow = [&]() -> std::string {
        f.a.control = ControlDoc();
        ImGuiIO& io = ImGui::GetIO();
        for (float y = headerY + 4.0f; y < headerY + 80.0f; y += 3.0f) {
            // 헤더를 누르면 정렬이 바뀐다. 누르지 않고 내려가다가 글자 칸 위(마우스 모양이 글자 커서)에서 누른다
            f.moveTo(drawResourcesTab, targetX, y);
            if (ImGui::GetMouseCursor() != ImGuiMouseCursor_TextInput) continue;
            f.click(drawResourcesTab, targetX, y);
            if (!io.WantTextInput) continue;
            io.AddInputCharactersUTF8("77");
            f.frame(drawResourcesTab);
            io.AddKeyEvent(ImGuiKey_Enter, true);
            f.frame(drawResourcesTab);
            io.AddKeyEvent(ImGuiKey_Enter, false);
            f.frame(drawResourcesTab);
            f.frame(drawResourcesTab);
            const std::map<std::string, int> got = targets(f.a);
            return got.size() == 1 && got.begin()->second == 77 ? got.begin()->first : std::string("?");
        }
        return "";
    };
    CHECK(topRow() == "RegionalWealth");            // 처음에는 게임의 순서(분류 열 오름차순)
    f.click(drawResourcesTab, currentX, headerY);   // "현재": 큰 값부터
    CHECK(topRow() == "Timber");
    f.click(drawResourcesTab, currentX, headerY);   // 다시 누르면 작은 값부터
    CHECK(topRow() == "Berries");
    f.click(drawResourcesTab, (20.0f + borders[0]) / 2.0f, headerY);   // "자원": 이름 가나다순
    CHECK(topRow() == "Pork");                      // 돼지고기
}

// 로그 탭: 줄을 그리고, "복사"는 보이는 줄을 클립보드로 보낸다
TEST(overlay_log_tab_copies_the_shown_lines) {
    Frames f;
    f.a.logLines = {
        parseLogLine("[2026-10-02 17:28:08.1] UE4SS - v3.0.1"),
        parseLogLine("[2026-10-02 17:28:17.1] [Lua] [MLToybox] native: loaded"),
        parseLogLine("[2026-10-02 17:28:18.1] [Lua] [MLToybox] ERROR resources: boom"),
    };
    for (int i = 0; i < 3; ++i) f.frame(drawLogTab);
    CHECK(f.clickOn(drawLogTab, "복사"));
    CHECK(f.a.clipboardPending);
    // 처음에는 MLToybox 줄만 보인다
    CHECK(f.a.clipboardOut == "17:28:17 native: loaded\r\n17:28:18 ERROR resources: boom\r\n");
    CHECK(f.clickOn(drawLogTab, "MLToybox 줄만"));
    CHECK(f.clickOn(drawLogTab, "복사"));
    CHECK(f.a.clipboardOut.rfind("17:28:08 UE4SS - v3.0.1\r\n", 0) == 0);
    CHECK(f.clickOn(drawLogTab, "MLToybox 줄만"));   // 다른 테스트를 위해 되돌린다
    // 줄이 많아도 그린다
    for (int i = 0; i < 3000; ++i) f.a.logLines.push_back(parseLogLine("[2026-10-02 17:30:00.1] [Lua] [MLToybox] line"));
    for (int i = 0; i < 3; ++i) f.frame(drawLogTab);
}

TEST(overlay_tabs_include_the_log_tab) {
    CHECK(tabNames() == (std::vector<std::string>{ "자원", "영주", "건설", "군사", "용병", "인구", "상태", "로그" }));
}
