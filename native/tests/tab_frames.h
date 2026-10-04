#pragma once
#include "overlay/ui/app.h"
#include "overlay/ui/tabs.h"
#include <imgui.h>

// 탭을 실제 ImGui 프레임으로 그리고 마우스 클릭을 넣어 본다(그래픽 장치 없음).
// 설정 문서는 시작할 때와 끝날 때 비운다. status 를 주면 게임 안(inGame)으로 그린다
struct TabFrames {
    mlt::ov::App& a = mlt::ov::app();

    TabFrames() {
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
        a.control = mlt::ov::ControlDoc();
        a.dirty = false;
    }
    ~TabFrames() {
        for (ImTextureData* tex : ImGui::GetPlatformIO().Textures) {
            tex->SetTexID(ImTextureID_Invalid);
            tex->SetStatus(ImTextureStatus_Destroyed);
        }
        ImGui::DestroyContext();
        a.control = mlt::ov::ControlDoc();
        a.dirty = false;
    }
    void frame(void (*draw)(mlt::ov::TabContext&), const mlt::ov::StatusDoc* status = nullptr) {
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(800.0f, 600.0f);
        io.DeltaTime = 0.5f;   // 이어지는 클릭이 두 번 누르기로 묶이지 않게
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(ImVec2(800.0f, 600.0f));
        ImGui::Begin("test", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
        mlt::ov::TabContext ctx{ a, status, status != nullptr, 0 };
        draw(ctx);
        ImGui::End();
        ImGui::Render();
        for (ImTextureData* tex : ImGui::GetPlatformIO().Textures) {
            if (tex->Status == ImTextureStatus_WantCreate) tex->SetTexID(static_cast<ImTextureID>(1));
            if (tex->Status == ImTextureStatus_WantCreate || tex->Status == ImTextureStatus_WantUpdates) tex->SetStatus(ImTextureStatus_OK);
        }
    }
    void click(void (*draw)(mlt::ov::TabContext&), float x, float y, const mlt::ov::StatusDoc* status = nullptr) {
        ImGuiIO& io = ImGui::GetIO();
        io.AddMousePosEvent(x, y);
        frame(draw, status);
        io.AddMouseButtonEvent(0, true);
        frame(draw, status);
        io.AddMouseButtonEvent(0, false);
        frame(draw, status);
    }
};
