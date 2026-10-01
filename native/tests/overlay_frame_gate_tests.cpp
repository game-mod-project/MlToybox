#include "test.h"
#include "overlay/core/frame_gate.h"

using namespace mlt::ov;

// GPU 가 어떤 명령 할당자로 기록한 명령을 아직 실행 중이면 그 할당자를 다시 쓰면 안 된다.
// 쓸 수 있는 칸을 고르고, 없으면 칸을 늘리거나 그 프레임을 넘긴다.
TEST(overlay_frame_gate_blocks_a_slot_until_the_gpu_finished_its_last_use) {
    FrameGate gate;
    gate.reset(3);
    CHECK(gate.size() == 3);
    CHECK(gate.ready(0, 0) && gate.ready(1, 0) && gate.ready(2, 0));   // 한 번도 쓰지 않은 칸
    CHECK(gate.submitted(0) == 1);
    CHECK(!gate.ready(0, 0));            // GPU 가 아직 1번을 끝내지 않았다
    CHECK(gate.ready(1, 0));             // 다른 칸은 상관없다
    CHECK(gate.submitted(1) == 2);
    CHECK(gate.ready(0, 1) && !gate.ready(1, 1));
    CHECK(gate.ready(0, 2) && gate.ready(1, 2));
    CHECK(gate.submitted(0) == 3);       // 같은 칸을 다시 썼다
    CHECK(!gate.ready(0, 2) && gate.ready(0, 3));
}

TEST(overlay_frame_gate_picks_a_free_slot_or_none) {
    FrameGate gate;
    gate.reset(2);
    CHECK(gate.firstReady(0) == 0);
    gate.submitted(0);                   // 펜스 1
    CHECK(gate.firstReady(0) == 1);      // 0번은 GPU 가 쓰는 중
    gate.submitted(1);                   // 펜스 2
    CHECK(gate.firstReady(0) == FrameGate::npos);   // GPU 가 밀려 있다: 쓸 칸이 없다
    const size_t added = gate.add();     // 칸을 늘린다
    CHECK(added == 2 && gate.size() == 3 && gate.firstReady(0) == 2);
    CHECK(gate.firstReady(1) == 0);      // GPU 가 1번까지 끝냈다
}

TEST(overlay_frame_gate_rejects_unknown_slots_and_starts_over_on_reset) {
    FrameGate gate;
    CHECK(!gate.ready(0, 100));          // 칸이 없다
    CHECK(gate.firstReady(100) == FrameGate::npos);
    gate.reset(2);
    CHECK(!gate.ready(2, 100));
    CHECK(gate.submitted(1) == 1);
    gate.reset(3);                       // 백버퍼 수가 바뀌어 새 펜스로 다시 시작한다
    CHECK(gate.ready(1, 0));
    CHECK(gate.submitted(2) == 1);
}
