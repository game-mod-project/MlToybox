#pragma once
#include "app.h"
#include <string>
#include <vector>

// 탭 하나가 파일 하나다. 모두 ImGui 프레임 안에서, App::mutex 를 잡은 채로 불린다.
// status 는 최신 status.json(없으면 nullptr), inGame 은 게임 안이고 모드가 응답 중일 때 참
namespace mlt::ov {

struct TabContext {
    App& app;
    const StatusDoc* status;   // nullptr 가능
    bool inGame;
    long long now;             // UTC 초
};

void drawLordTab(TabContext& ctx);
void drawBuildTab(TabContext& ctx);
void drawMilitaryTab(TabContext& ctx);
void drawPopulationTab(TabContext& ctx);
void drawResourcesTab(TabContext& ctx);
void drawMercenariesTab(TabContext& ctx);
void drawStatusTab(TabContext& ctx);
void drawLogTab(TabContext& ctx);
// 건설 탭의 아래쪽: 건물 종류별 저장 용량 표
void drawStorageSection(TabContext& ctx);

// 탭 막대의 이름들(왼쪽부터)
std::vector<std::string> tabNames();

// 설정 문서를 바꿨다고 표시한다(작업 스레드가 0.2초 안에 저장한다)
inline void markDirty(App& a) { a.dirty = true; }
// 일회성 명령을 다음 저장에 실어 보낸다
inline void sendCommand(App& a, Json command) {
    a.commands.push_back(std::move(command));
    a.dirty = true;
}
}
