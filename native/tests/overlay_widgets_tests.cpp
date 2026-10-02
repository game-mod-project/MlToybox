#include "test.h"
#include "overlay/ui/app.h"
#include "overlay/ui/widgets.h"
#include <imgui.h>
#include <string>

using namespace mlt::ov;

// 글자 칸을 실제 ImGui 프레임으로 돌려 본다(그래픽 장치 없음). 게임에서는 글쇠를 자동으로 칠 수 없으므로,
// ImGui 가 글자와 지우기 글쇠를 처리하는 순서에 조합기가 맞게 붙어 있는지는 여기서 확인한다.
namespace {
struct Frames {
    std::string value;

    Frames() {
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;   // 글꼴 텍스처는 아래에서 만든 셈 친다
    }
    ~Frames() {
        for (ImTextureData* tex : ImGui::GetPlatformIO().Textures) {
            tex->SetTexID(ImTextureID_Invalid);
            tex->SetStatus(ImTextureStatus_Destroyed);
        }
        ImGui::DestroyContext();
    }
    // 한 프레임. 첫 프레임에는 글자 칸에 커서를 둔다
    void frame(bool focus = false) {
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(800.0f, 600.0f);
        io.DeltaTime = 1.0f / 60.0f;
        ImGui::NewFrame();
        ImGui::Begin("test");
        if (focus) ImGui::SetKeyboardFocusHere();
        textField("##name", value, 160, 200.0f);
        ImGui::End();
        ImGui::Render();
        for (ImTextureData* tex : ImGui::GetPlatformIO().Textures) {
            if (tex->Status == ImTextureStatus_WantCreate) tex->SetTexID(static_cast<ImTextureID>(1));
            if (tex->Status == ImTextureStatus_WantCreate || tex->Status == ImTextureStatus_WantUpdates) tex->SetStatus(ImTextureStatus_OK);
        }
    }
    void type(const char* keys) {
        for (const char* k = keys; *k; ++k) ImGui::GetIO().AddInputCharacter(static_cast<unsigned>(*k));
        frame();
    }
    void press(ImGuiKey key) {
        ImGui::GetIO().AddKeyEvent(key, true);
        frame();
        ImGui::GetIO().AddKeyEvent(key, false);
        frame();
    }
    void hangulKey() {                 // 한/영 글쇠(창 스레드가 세는 것을 흉내 낸다)
        ++app().hangulKeys;
        frame();
    }
};
}

TEST(overlay_widgets_text_field_composes_hangul_inside_imgui) {
    Frames f;
    f.frame(true);
    f.frame();
    f.frame();
    CHECK(ImGui::GetIO().WantTextInput);               // 글자 칸에 커서가 있다(ImGui 는 이 값을 한 프레임 뒤에 올린다)
    if (hangulModeOn()) f.hangulKey();                 // 앞 테스트가 남긴 상태와 무관하게 영문에서 시작한다
    f.type("ab");
    CHECK(f.value == "ab");                            // 영문 모드: 그대로

    f.hangulKey();
    CHECK(hangulModeOn());
    f.type("dkssud");
    CHECK(f.value == "ab안녕");
    f.type("g");
    f.type("k");                                       // 프레임을 나눠 쳐도 조합이 이어진다
    f.type("s");
    CHECK(f.value == "ab안녕한");

    f.press(ImGuiKey_Backspace);                       // 조합 중: 자모 하나만 지운다
    CHECK(f.value == "ab안녕하");
    f.press(ImGuiKey_Backspace);
    CHECK(f.value == "ab안녕ㅎ");
    f.press(ImGuiKey_Backspace);
    CHECK(f.value == "ab안녕");
    f.press(ImGuiKey_Backspace);                       // 조합이 끝난 글자는 통째로 지운다
    CHECK(f.value == "ab안");

    f.type("rk");
    f.press(ImGuiKey_LeftArrow);                       // 커서를 옮기면 조합이 끝난다
    f.type("s");
    CHECK(f.value == "ab안ㄴ가");

    ImGui::GetIO().AddKeyEvent(ImGuiMod_Shift, true);  // 윗글쇠를 누른 채: 쌍자음
    f.frame();
    f.type("R");
    ImGui::GetIO().AddKeyEvent(ImGuiMod_Shift, false);
    f.frame();
    f.type("K");                                       // Caps Lock 이 켜져 있어도(대문자로 와도) 윗글쇠 없이는 ㅏ
    CHECK(f.value == "ab안ㄴ까가");

    f.hangulKey();                                     // 다시 영문
    CHECK(!hangulModeOn());
    f.type("x 1");
    CHECK(f.value == "ab안ㄴ까x 1가");

    f.hangulKey();                                     // 한글 모드에서 전체를 고르고(Ctrl+A) 치면 고른 글이 바뀐다
    ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl, true);
    ImGui::GetIO().AddKeyEvent(ImGuiKey_A, true);
    f.frame();
    ImGui::GetIO().AddKeyEvent(ImGuiKey_A, false);
    ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl, false);
    f.frame();
    f.type("rk");
    CHECK(f.value == "가");
    f.hangulKey();
}
