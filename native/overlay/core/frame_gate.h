#pragma once
#include <cstddef>
#include <vector>

// 명령 할당자를 다시 써도 되는지 가린다. 큐에 실행을 맡길 때마다 펜스 값이 1씩 늘고, 칸(할당자)마다 마지막으로 쓴 값을 적는다.
// GPU 가 그 칸의 지난 명령을 아직 실행 중일 때 할당자를 Reset 하면 D3D12 규약 위반이다(GPU 크래시로 이어질 수 있다).
namespace mlt::ov {

class FrameGate {
public:
    static constexpr size_t npos = static_cast<size_t>(-1);

    // 칸 수를 정하고 처음부터 다시 센다(펜스를 새로 만들었을 때)
    void reset(size_t slots);
    size_t size() const { return lastUse_.size(); }
    // 칸을 하나 늘리고 그 번호를 돌려준다
    size_t add();
    // 완료된 펜스 값이 이 칸을 마지막으로 쓴 값 이상이면 참. 없는 칸은 거짓
    bool ready(size_t slot, unsigned long long completed) const;
    // 다시 써도 되는 첫 칸. 없으면 npos
    size_t firstReady(unsigned long long completed) const;
    // 이 칸으로 실행을 맡겼다. 큐에 Signal 할 펜스 값을 돌려준다(1부터)
    unsigned long long submitted(size_t slot);

private:
    std::vector<unsigned long long> lastUse_;
    unsigned long long next_ = 0;
};
}
