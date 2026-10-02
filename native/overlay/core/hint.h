#pragma once

// "Insert: MLToybox" 안내를 띄우는 때를 정한다.
// 오버레이가 그리기 시작한 직후와, 게임(맵)에 들어간 직후에 각각 8초 동안 띄운다.
// 그리기 시작은 게임의 검은 시작 화면일 때라(실측) 그것만으로는 사용자가 보기 어렵다.
namespace mlt::ov {

class HintTimer {
public:
    static constexpr unsigned long long kShowMs = 8000;

    // 오버레이가 그릴 준비를 마쳤다
    void onReady(unsigned long long nowMs);
    // 모드가 알린 게임 상태(맵 안인가). 프레임마다 불러도 된다. 밖에서 안으로 바뀔 때만 다시 띄운다
    void update(bool inGame, unsigned long long nowMs);
    bool active(unsigned long long nowMs) const;

private:
    unsigned long long until_ = 0;
    bool wasInGame_ = false;
};
}
