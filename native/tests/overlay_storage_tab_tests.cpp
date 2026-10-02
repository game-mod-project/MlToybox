#include "test.h"
#include "overlay/core/storage.h"
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

// 건설 탭의 저장 용량 구획을 실제 ImGui 프레임으로 그리고 마우스·글쇠를 넣어 본다(그래픽 장치 없음).
namespace {
const char* kCatalog = R"({"version":1,"buildings":[
    {"id":"72","name":"창고","generic":250,"large":0,"pantry":0},
    {"id":"99","name":"대형 창고","generic":2500,"large":0,"pantry":0},
    {"id":"68","name":"대형 식량 비축고","generic":0,"large":0,"pantry":2500},
    {"id":"4","name":"벌목장","generic":0,"large":28,"pantry":0},
    {"id":"69","name":"농가","generic":1200,"large":0,"pantry":1200}]})";

struct Frames {
    App& a = app();
    float width = 800.0f;
    float height = 700.0f;

    explicit Frames(bool withCatalog = true) {
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
        installClipboardCallbacks();
        reset();
        if (withCatalog) a.buildings = std::make_shared<const BuildingCatalog>(BuildingCatalog::parse(kCatalog));
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
        a.buildings.reset();
    }
    void frame() {
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(1200.0f, height);
        io.DeltaTime = 0.5f;   // 이어지는 클릭이 두 번 누르기로 묶이지 않게
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(ImVec2(width, height));
        ImGui::Begin("test", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
        TabContext ctx{ a, nullptr, false, 0 };
        drawBuildTab(ctx);
        ImGui::End();
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
    void click(float x, float y) {
        ImGuiIO& io = ImGui::GetIO();
        moveTo(x, y);
        io.AddMouseButtonEvent(0, true);
        frame();
        io.AddMouseButtonEvent(0, false);
        frame();
    }
    // 탭이 "test" 창에 바로 그리는 화면 조각 위로 마우스를 훑어 올려 둔다. 누르지는 않는다(다른 조각을 건드리지 않는다)
    bool hover(const char* label) {
        const ImGuiID id = ImGui::FindWindowByName("test")->GetID(label);
        for (float y = 150.0f; y < 420.0f; y += 5.0f) {
            for (float x = 8.0f; x < width - 4.0f; x += 8.0f) {
                moveTo(x, y);
                if (ImGui::GetCurrentContext()->HoveredId == id) return true;
            }
        }
        return false;
    }
    bool clickOn(const char* label) {
        if (!hover(label)) return false;
        const ImVec2 at = ImGui::GetIO().MousePos;
        click(at.x, at.y);
        return true;
    }
    void type(const char* text) {
        ImGui::GetIO().AddInputCharactersUTF8(text);
        frame();
        frame();
    }
    void enter() {
        ImGuiIO& io = ImGui::GetIO();
        io.AddKeyEvent(ImGuiKey_Enter, true);
        frame();
        io.AddKeyEvent(ImGuiKey_Enter, false);
        frame();
        frame();
    }
    // 표의 머리줄 높이와 열 경계(경계 위에서는 마우스 모양이 좌우 화살표가 된다)
    bool findHeader(float& headerY, std::vector<float>& borders) {
        for (float y = 200.0f; y < 460.0f; y += 6.0f) {
            for (float x = 20.0f; x < width - 10.0f; x += 2.0f) {
                moveTo(x, y);
                if (ImGui::GetMouseCursor() != ImGuiMouseCursor_ResizeEW) continue;
                headerY = y;
                if (borders.empty() || x - borders.back() > 12.0f) borders.push_back(x);
            }
            if (!borders.empty()) return true;
        }
        return false;
    }
    // 머리줄 아래로 내려가며 그 열에서 처음 만나는 글자 칸(마우스 모양이 글자 커서)의 높이. 없으면 음수
    float firstCell(float x, float fromY, float rows = 30.0f) {
        for (float y = fromY; y < fromY + rows; y += 3.0f) {
            moveTo(x, y);
            if (ImGui::GetMouseCursor() == ImGuiMouseCursor_TextInput) return y;
        }
        return -1.0f;
    }
};

std::map<std::string, StorageLimits> limits(App& a) {
    return a.control.storage().limits;
}
}

// 건물 목록(bridge/catalog.json)을 읽기 전에는 표가 없다. 체크는 쓸 수 있다
TEST(overlay_storage_section_has_no_table_until_the_building_list_arrives) {
    Frames f(false);
    for (int i = 0; i < 3; ++i) f.frame();
    CHECK(!f.hover("###storagefill"));
    CHECK(f.clickOn("건물 저장 용량 변경"));
    CHECK(f.a.control.storage().enabled && f.a.dirty);
}

TEST(overlay_storage_section_checkbox_turns_the_feature_on_and_off) {
    Frames f;
    for (int i = 0; i < 3; ++i) f.frame();
    const BuildSettings before = f.a.control.build();
    CHECK(f.clickOn("건물 저장 용량 변경"));
    CHECK(f.a.control.storage().enabled && f.a.dirty);
    CHECK(f.a.control.build().enabled == before.enabled && !f.a.control.upgrade().enabled);   // 건설·업그레이드 설정과는 따로다
    const ImVec2 at = ImGui::GetIO().MousePos;
    f.click(at.x, at.y);
    CHECK(!f.a.control.storage().enabled);
}

// "N배로"는 표에 보이는 줄에만 적용한다: 검색으로 창고 둘만 남기고 누르면 그 둘의 값만 생긴다
TEST(overlay_storage_section_fill_applies_to_the_rows_the_search_left) {
    Frames f;
    for (int i = 0; i < 3; ++i) f.frame();
    CHECK(f.clickOn("##storagesearch"));
    CHECK(ImGui::GetIO().WantTextInput);
    f.type("창고");
    CHECK(f.clickOn("###storagefill"));
    std::map<std::string, StorageLimits> got = limits(f.a);
    CHECK(got.size() == 2 && got.at("72").generic == 500 && got.at("99").generic == 5000);   // 기본 배수는 2
    CHECK(!got.at("99").large && !got.at("99").pantry && f.a.dirty);
    // 검색을 지우면 모든 줄에 적용한다
    CHECK(f.clickOn("지우기##storagesearch"));
    CHECK(f.clickOn("###storagefill"));
    got = limits(f.a);
    CHECK(got.size() == 5 && got.at("4").large == 56 && got.at("69").pantry == 2400 && got.at("69").generic == 2400);
}

// "지우기"는 두 번 눌러야 지운다(한 번에 여러 값이 사라진다)
TEST(overlay_storage_section_clear_needs_a_second_click) {
    Frames f;
    StorageSettings s = f.a.control.storage();
    s.limits["72"].generic = 500;
    s.limits["68"].pantry = 9000;
    f.a.control.setStorage(s);
    for (int i = 0; i < 3; ++i) f.frame();
    CHECK(f.clickOn("###storageclear"));
    CHECK(limits(f.a).size() == 2 && !f.a.dirty);
    const ImVec2 at = ImGui::GetIO().MousePos;
    for (int i = 0; i < 12; ++i) f.frame();   // 6초 뒤(한 프레임이 0.5초다): 처음부터 다시 물어본다
    f.click(at.x, at.y);
    CHECK(limits(f.a).size() == 2 && !f.a.dirty);
    f.click(at.x, at.y);                      // 곧바로 한 번 더
    CHECK(limits(f.a).empty() && f.a.dirty);
}

// 칸에 값을 넣으면 그 건물의 그 저장실 한도만 바뀐다. 그 저장실이 없는 건물에는 칸이 없다
TEST(overlay_storage_section_a_cell_sets_one_limit_and_missing_storages_have_no_cell) {
    Frames f;
    for (int i = 0; i < 4; ++i) f.frame();
    float headerY = -1.0f;
    std::vector<float> borders;
    CHECK(f.findHeader(headerY, borders));
    CHECK(borders.size() == 3);   // 열 넷(건물, 일반, 목재, 식량) 사이의 경계 셋
    const float genericX = (borders[0] + borders[1]) / 2.0f;
    const float largeX = (borders[1] + borders[2]) / 2.0f;
    const float pantryX = (borders[2] + f.width - 10.0f) / 2.0f;

    // 첫 줄은 창고(일반 250): 일반 칸만 있다
    const float rowY = f.firstCell(genericX, headerY + 12.0f);
    CHECK(rowY > 0.0f);
    f.moveTo(largeX, rowY);
    CHECK(ImGui::GetMouseCursor() != ImGuiMouseCursor_TextInput);
    f.moveTo(pantryX, rowY);
    CHECK(ImGui::GetMouseCursor() != ImGuiMouseCursor_TextInput);

    f.click(genericX, rowY);
    CHECK(ImGui::GetIO().WantTextInput);
    f.type("700");
    f.enter();
    std::map<std::string, StorageLimits> got = limits(f.a);
    CHECK(got.size() == 1 && got.at("72").generic == 700 && !got.at("72").large && !got.at("72").pantry);
    CHECK(f.a.dirty);

    // 빈칸으로 두면 값이 사라진다(게임의 값 그대로)
    f.click(genericX, rowY);
    ImGuiIO& io = ImGui::GetIO();
    io.AddKeyEvent(ImGuiKey_Delete, true);   // 칸에 들어가면 글이 모두 선택돼 있다
    f.frame();
    io.AddKeyEvent(ImGuiKey_Delete, false);
    f.frame();
    f.enter();
    CHECK(limits(f.a).empty());
}

// 머리줄을 누르면 그 열의 기본 한도로 정렬한다. 맨 윗줄의 칸에 값을 넣어, 어느 건물이 맨 위에 왔는지로 확인한다
TEST(overlay_storage_section_header_click_sorts_by_the_default_limit) {
    Frames f;
    for (int i = 0; i < 4; ++i) f.frame();
    float headerY = -1.0f;
    std::vector<float> borders;
    CHECK(f.findHeader(headerY, borders));
    CHECK(borders.size() == 3);
    const float pantryX = (borders[2] + f.width - 10.0f) / 2.0f;

    auto topPantryRow = [&]() -> std::string {
        f.a.control = ControlDoc();
        const float y = f.firstCell(pantryX, headerY + 12.0f, 120.0f);
        if (y < 0.0f) return "";
        f.click(pantryX, y);
        f.type("77");
        f.enter();
        const std::map<std::string, StorageLimits> got = limits(f.a);
        return got.size() == 1 && got.begin()->second.pantry == 77 ? got.begin()->first : std::string("?");
    };
    CHECK(topPantryRow() == "68");          // 모드가 적은 순서: 식량 저장실이 있는 첫 건물은 대형 식량 비축고
    f.click(pantryX, headerY);              // "식량": 큰 값부터(2500, 1200)
    CHECK(topPantryRow() == "68");
    f.click(pantryX, headerY);              // 다시 누르면 작은 값부터
    CHECK(topPantryRow() == "69");
    f.click(pantryX, headerY);              // 한 번 더 누르면 정렬을 푼다
    CHECK(topPantryRow() == "68");
}
