#pragma once

namespace mlt::ov {
struct App;

// 그릴 것이 있는가(창이 열려 있거나 안내가 떠 있다). App::mutex 를 잡은 채로 부른다
bool overlayWantsFrame(App& a);
// 메인 창과 안내를 그린다. ImGui 프레임 안에서, App::mutex 를 잡은 채로 부른다
void drawOverlay(App& a);
// ImGui 를 만든 뒤 한 번 부른다: 창은 제목 줄로만 옮기고, 게임의 마우스 커서는 건드리지 않는다
void configureOverlayInput();
// 그릴 준비가 끝났을 때 한 번 부른다(안내 표시 시작)
void notifyOverlayReady();
}
