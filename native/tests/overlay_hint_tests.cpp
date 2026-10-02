#include "test.h"
#include "overlay/core/hint.h"

using namespace mlt::ov;

TEST(overlay_hint_shows_for_eight_seconds_after_ready) {
    HintTimer h;
    CHECK(!h.active(0) && !h.active(5000));        // 준비되기 전에는 띄우지 않는다
    h.onReady(1000);
    CHECK(h.active(1000) && h.active(8999));
    CHECK(!h.active(9000));
}

TEST(overlay_hint_shows_again_each_time_the_player_enters_a_map) {
    HintTimer h;
    h.onReady(0);
    h.update(true, false, 20000);                  // 메인 메뉴
    CHECK(!h.active(20000));
    h.update(true, true, 30000);                   // 맵에 들어갔다
    CHECK(h.active(30000) && h.active(37999) && !h.active(38000));
    h.update(true, true, 36000);                   // 맵 안에 계속 있는 동안에는 늘리지 않는다
    CHECK(!h.active(38000));
    h.update(true, false, 50000);                  // 메뉴로 나갔다가
    CHECK(!h.active(50000));
    h.update(true, true, 60000);                   // 다시 들어가면 또 띄운다
    CHECK(h.active(67999) && !h.active(68000));
}

// 모드의 heartbeat 가 잠깐 끊겼다 돌아온 것은 맵에 들어간 것이 아니다(게임이 오래 멈췄다 풀릴 때)
TEST(overlay_hint_stays_quiet_when_the_mod_reconnects_inside_a_map) {
    HintTimer h;
    h.onReady(0);
    h.update(true, true, 20000);
    CHECK(h.active(20000) && !h.active(28000));
    h.update(false, false, 40000);                 // 연결이 끊겼다(상태를 알 수 없다)
    h.update(true, true, 50000);                   // 같은 맵에서 돌아왔다
    CHECK(!h.active(50000));
    h.update(false, false, 60000);
    h.update(true, false, 70000);                  // 끊긴 사이 메뉴로 나갔다
    h.update(true, true, 80000);                   // 다시 들어가면 띄운다
    CHECK(h.active(80000));
}
