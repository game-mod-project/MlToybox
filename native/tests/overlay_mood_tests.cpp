#include "test.h"
#include "control.h"
#include "overlay/core/control_doc.h"
#include "overlay/core/status_doc.h"
#include "overlay/core/view.h"
#include "overlay/ui/app.h"
#include "overlay/ui/tabs.h"
#include <imgui.h>

using namespace mlt::ov;

TEST(overlay_mood_defaults_change_nothing) {
    const MoodSettings m = ControlDoc().mood();
    CHECK(!m.enabled && m.regions.empty());
    CHECK(m.common.approval.fixed == 0 && m.common.approval.good == 1 && m.common.approval.bad == 100);
    CHECK(m.common.order.fixed == 0 && m.common.order.good == 1 && m.common.order.bad == 100);
    CHECK(m.common.approval.neutral() && m.common.order.neutral());
}

TEST(overlay_mood_round_trips_common_and_region_settings) {
    ControlDoc doc;
    MoodSettings m;
    m.enabled = true;
    m.common.approval = { 0, 3, 25 };
    m.common.order = { 90, 1, 100 };
    m.regions["eich"].approval = { 100, 1, 100 };
    m.regions["eich"].order = { 0, 1, 0 };
    m.regions["imm"] = MoodPair{};   // 영지에 따로 둔 "게임 그대로"도 값으로 남는다(공통 설정을 이긴다)
    doc.setMood(m);
    const MoodSettings back = ControlDoc::parse(doc.dump()).mood();
    CHECK(back.enabled && back.common == m.common && back.regions == m.regions);
    // 영지 설정을 지우면 키도 사라진다
    m.regions.erase("imm");
    doc.setMood(m);
    const Json features = Json::parse(doc.dump())["features"];
    CHECK(features["mood"]["regions"].size() == 1 && features["mood"]["regions"].contains("eich"));
}

TEST(overlay_mood_writes_what_the_native_dll_reads) {
    // 네이티브 DLL(src/control.cpp)이 같은 파일을 읽는다. 오버레이가 쓴 글을 네이티브가 같은 뜻으로 읽어야 한다
    ControlDoc doc;
    doc.setSeq(7);
    MoodSettings m;
    m.enabled = true;
    m.common.approval = { 0, 4, 10 };
    m.regions["eich"].order = { 55, 2, 0 };
    doc.setMood(m);
    const auto native = mlt::parseControl(doc.dump());
    CHECK(native.has_value());
    CHECK(native->mood.common.approval.good == 4 && native->mood.common.approval.bad == 10 && native->mood.common.order.neutral());
    CHECK(native->mood.regions.size() == 1 && native->mood.regions[0].first == "eich");
    CHECK(native->mood.regions[0].second.order.fixed == 55 && native->mood.regions[0].second.order.good == 2 && native->mood.regions[0].second.order.bad == 0);
    m.enabled = false;
    doc.setMood(m);
    CHECK(mlt::parseControl(doc.dump())->mood.neutral());
}

TEST(overlay_mood_reads_hand_edited_values_into_range) {
    const ControlDoc doc = ControlDoc::parse(R"({"version":1,"seq":3,"features":{"mood":{"enabled":true,
        "approval":{"fixed":500,"good":99,"bad":-4},"order":"x",
        "regions":{"eich":{"approval":{"fixed":-3,"good":0,"bad":900}},"odd":7,"":{"order":{"fixed":5}}}}}})");
    const MoodSettings m = doc.mood();
    CHECK(m.enabled);
    CHECK(m.common.approval.fixed == 100 && m.common.approval.good == 10 && m.common.approval.bad == 0);
    CHECK(m.common.order.neutral());
    CHECK(m.regions.size() == 1 && m.regions.count("eich") == 1);   // 객체가 아닌 항목과 이름 없는 항목은 건너뛴다
    CHECK(m.regions.at("eich").approval.fixed == 0 && m.regions.at("eich").approval.good == 1 && m.regions.at("eich").approval.bad == 100);
}

TEST(overlay_mood_setter_keeps_unknown_keys) {
    ControlDoc doc = ControlDoc::parse(R"({"version":1,"seq":3,"features":{"mood":{"enabled":true,"future":1,
        "regions":{"eich":{"approval":{"fixed":80},"note":"keep"}}},"storage":{"enabled":true}}})");
    MoodSettings m = doc.mood();
    m.common.order.bad = 50;
    doc.setMood(m);
    const Json features = Json::parse(doc.dump())["features"];
    CHECK(features["mood"]["future"] == 1);
    CHECK(features["mood"]["regions"]["eich"]["note"] == "keep");
    CHECK(features["mood"]["regions"]["eich"]["approval"]["fixed"] == 80);
    CHECK(features["mood"]["order"]["bad"] == 50);
    CHECK(features["storage"]["enabled"] == true);
}

TEST(overlay_mood_status_lists_my_regions) {
    const auto s = parseStatus(R"({"heartbeat":5,"inGame":true,"mood":{"regions":[
        {"key":"eich","name":"Wilde Wand","approval":100,"order":96},{"key":"imm","name":"Krumme Leite","approval":91.0,"order":100},7]}})");
    CHECK(s.has_value() && s->mood.has_value());
    CHECK(s->mood->regions.size() == 2);
    CHECK(s->mood->regions[0].key == "eich" && s->mood->regions[0].name == "Wilde Wand" && s->mood->regions[0].approval == 100 && s->mood->regions[0].order == 96);
    CHECK(s->mood->regions[1].approval == 91);
    // Lua 의 빈 표는 [] 로 온다
    const auto empty = parseStatus(R"({"heartbeat":5,"inGame":true,"mood":{"regions":[]}})");
    CHECK(empty.has_value() && empty->mood.has_value() && empty->mood->regions.empty());
    CHECK(!parseStatus(R"({"heartbeat":5,"inGame":true})")->mood.has_value());
}

TEST(overlay_mood_scope_options_and_current_lines) {
    CHECK(moodScopeOptions(nullptr).size() == 1 && moodScopeOptions(nullptr)[0].label == "공통 (모든 내 영지)");
    CHECK(moodLines(nullptr) == std::vector<std::string>{ "현재: - (게임에 들어가면 표시됩니다)" });
    MoodStatus m;
    m.regions.push_back({ "eich", "Wilde Wand", 100, 96 });
    m.regions.push_back({ "imm", "Krumme Leite", 91, 100 });
    const std::vector<ScopeOption> options = moodScopeOptions(&m);
    CHECK(options.size() == 3 && options[1].key == "eich" && options[1].label == "Wilde Wand (eich)");
    CHECK(moodLines(&m) == (std::vector<std::string>{
        "현재 Wilde Wand: 자격 100 · 공공질서 96",
        "현재 Krumme Leite: 자격 91 · 공공질서 100",
    }));
    m.regions.clear();
    CHECK(moodLines(&m) == std::vector<std::string>{ "현재: 내 영지가 없습니다" });
}

TEST(overlay_mood_native_note_says_what_works_without_the_hooks) {
    CHECK(moodNativeNote(nullptr).empty());   // 아직 모른다(게임 밖)
    NativeStatus n;
    n.loaded = true;
    for (const char* name : { "mood_approval", "mood_order", "mood_problem_add", "mood_problem_remove", "mood_region_name" }) n.features[name].installed = true;
    CHECK(moodNativeNote(&n).empty());
    n.features["mood_region_name"].installed = false;
    CHECK(moodNativeNote(&n).find("영지별 설정") != std::string::npos);
    n.features["mood_order"].installed = false;
    CHECK(moodNativeNote(&n).find("배율") != std::string::npos);
    n.loaded = false;
    CHECK(moodNativeNote(&n).find("배율") != std::string::npos);
}

namespace {
// 탭을 실제 ImGui 프레임으로 그리고 마우스 클릭을 넣어 본다(그래픽 장치 없음)
struct MoodFrames {
    App& a = app();

    MoodFrames() {
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
        a.control = ControlDoc();
        a.dirty = false;
    }
    ~MoodFrames() {
        for (ImTextureData* tex : ImGui::GetPlatformIO().Textures) {
            tex->SetTexID(ImTextureID_Invalid);
            tex->SetStatus(ImTextureStatus_Destroyed);
        }
        ImGui::DestroyContext();
        a.control = ControlDoc();
        a.dirty = false;
    }
    void frame(const StatusDoc* status) {
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(800.0f, 600.0f);
        io.DeltaTime = 0.5f;
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(ImVec2(800.0f, 600.0f));
        ImGui::Begin("test", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
        TabContext ctx{ a, status, status != nullptr, 0 };
        drawMoodTab(ctx);
        ImGui::End();
        ImGui::Render();
        for (ImTextureData* tex : ImGui::GetPlatformIO().Textures) {
            if (tex->Status == ImTextureStatus_WantCreate) tex->SetTexID(static_cast<ImTextureID>(1));
            if (tex->Status == ImTextureStatus_WantCreate || tex->Status == ImTextureStatus_WantUpdates) tex->SetStatus(ImTextureStatus_OK);
        }
    }
    void click(const StatusDoc* status, float x, float y) {
        ImGuiIO& io = ImGui::GetIO();
        io.AddMousePosEvent(x, y);
        frame(status);
        io.AddMouseButtonEvent(0, true);
        frame(status);
        io.AddMouseButtonEvent(0, false);
        frame(status);
    }
};
}

TEST(overlay_mood_tab_sits_after_population_and_its_first_checkbox_turns_the_feature_on) {
    CHECK(tabNames() == (std::vector<std::string>{ "자원", "영주", "건설", "군사", "용병", "인구", "자격·질서", "영지", "상태", "로그" }));
    MoodFrames f;
    f.frame(nullptr);
    CHECK(!f.a.control.mood().enabled && !f.a.dirty);
    f.click(nullptr, 20.0f, 18.0f);   // 맨 위 줄의 체크 상자
    CHECK(f.a.control.mood().enabled);
    CHECK(f.a.dirty);                 // 바꾸면 바로 저장 대상이 된다
    CHECK(f.a.control.mood().common.approval.neutral() && f.a.control.mood().regions.empty());   // 다른 값은 그대로다
}

TEST(overlay_mood_tab_draws_with_regions_from_the_game) {
    const auto status = parseStatus(R"({"heartbeat":5,"inGame":true,"mood":{"regions":[
        {"key":"eich","name":"Wilde Wand","approval":100,"order":96},{"key":"imm","name":"Krumme Leite","approval":91,"order":100}]}})");
    CHECK(status.has_value());
    MoodFrames f;
    MoodSettings m;
    m.enabled = true;
    m.regions["eich"].approval.fixed = 80;
    f.a.control.setMood(m);
    f.frame(&*status);
    f.frame(&*status);
    // 그리기만 해서는 설정이 바뀌지 않는다
    CHECK(!f.a.dirty);
    CHECK(f.a.control.mood().regions.at("eich").approval.fixed == 80);
}
