#pragma once
#include <string>
#include <string_view>
#include <vector>

// bridge/overlay.json: 오버레이 자체 설정(토글 키, 글자 배율, 창 위치·크기)과 개발용 설정
namespace mlt::ov {

struct OverlaySettings {
    std::string toggleKey = "Insert";
    float scale = 1.0f;
    int x = 80, y = 80, w = 640, h = 720;
    bool startOpen = false;   // 개발·검증용: 열린 채로 시작
    std::string devTab;       // 개발·검증용: 이 이름의 탭을 연다(비어 있으면 없음)
    bool inputLog = false;    // 개발·검증용: 창이 열려 있는 동안의 글쇠 메시지를 bridge/overlay_input.log 에 적는다
    std::vector<float> resourceColumns;   // 자원 표의 열 너비(표 너비에 대한 백분율, kResourceColumnCount 개). 비어 있으면 기본 너비

    bool operator==(const OverlaySettings&) const = default;
};

inline constexpr size_t kResourceColumnCount = 4;   // 자원, 분류, 현재, 목표

inline constexpr float kScaleMin = 0.8f;
inline constexpr float kScaleMax = 1.5f;

// 파일이 없거나 깨졌으면 기본값. 모르는 토글 키는 Insert, 범위를 벗어난 값은 가까운 끝값
OverlaySettings parseSettings(std::string_view text);
std::string dumpSettings(const OverlaySettings& s);

// 고를 수 있는 토글 키 이름(화면 표시 순서)
const std::vector<std::string>& toggleKeyNames();
// 키 이름의 가상 키 코드. 모르는 이름이면 Insert 의 코드
int toggleKeyCode(const std::string& name);
}
