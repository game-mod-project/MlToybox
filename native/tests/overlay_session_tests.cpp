#include "test.h"
#include "overlay/core/commands.h"
#include "overlay/core/session.h"

using namespace mlt::ov;

TEST(overlay_session_nothing_to_save_when_clean) {
    Session s;
    CHECK(!beginSave(s).has_value());
}

// 모드는 control.json 을 1초마다 읽고, 오버레이는 바뀔 때마다(0.2초 안에) 저장한다.
// 명령을 한 번만 실으면 모드가 읽기 전에 나간 다음 저장이 그 명령을 지운다("지금 설정"을 연달아 누르면 앞의 것이 사라진다).
TEST(overlay_session_commands_ride_every_save_until_the_mod_reports_them) {
    Session s;
    const Json first = makeSetLord("treasury", 100, 1790000000);
    s.commands.push_back(first);
    s.dirty = true;
    auto p = beginSave(s);
    CHECK(p.has_value() && p->doc.raw()["commands"].size() == 1);
    CHECK(!s.dirty && s.control.raw()["commands"].empty());   // 메모리의 문서에는 명령을 두지 않는다
    finishSave(s, *p, 8);
    CHECK(s.lastSentSeq == 8 && s.control.seq() == 8 && !s.saveFailed && !s.dirty);
    CHECK(!beginSave(s).has_value());                         // 바뀐 것이 없으면 다시 저장하지 않는다

    const Json second = makeSetLord("influence", 7, 1790000001);   // 모드가 첫 저장을 읽기 전에 누른 두 번째 명령
    s.commands.push_back(second);
    s.dirty = true;
    p = beginSave(s);
    CHECK(p.has_value() && p->doc.raw()["commands"].size() == 2);
    CHECK(p->doc.raw()["commands"][0]["id"] == first["id"] && p->doc.raw()["commands"][1]["id"] == second["id"]);
    finishSave(s, *p, 9);

    // 모드가 첫 명령을 실행했다고 알렸다(status.json 의 commands). 그 명령만 뺀다. 빼는 것만으로는 저장하지 않는다
    dropFinishedCommands(s, { first["id"].get<std::string>() }, 1790000002);
    CHECK(s.commands.size() == 1 && s.commands[0]["id"] == second["id"] && !s.dirty);
    BuildSettings b = s.control.build();
    b.enabled = true;
    s.control.setBuild(b);
    s.dirty = true;
    p = beginSave(s);
    CHECK(p.has_value() && p->doc.raw()["commands"].size() == 1 && p->doc.raw()["commands"][0]["id"] == second["id"]);
}

TEST(overlay_session_gives_up_on_commands_the_mod_would_ignore) {
    Session s;
    s.commands.push_back(makeSetLord("treasury", 100, 1790000000));
    dropFinishedCommands(s, {}, 1790000000 + kCommandMaxAgeSec);
    CHECK(s.commands.size() == 1);                            // 아직 60초가 지나지 않았다
    dropFinishedCommands(s, {}, 1790000000 + kCommandMaxAgeSec + 1);
    CHECK(s.commands.empty());                                // 모드도 60초 넘은 명령은 버린다
    s.commands.push_back(Json::object({ { "id", "x" } }));    // issuedAt 이 없는 명령도 모드가 버린다
    dropFinishedCommands(s, {}, 1790000000);
    CHECK(s.commands.empty());
}

TEST(overlay_session_failed_save_keeps_changes_and_commands) {
    Session s;
    s.control.setSeq(5);
    s.lastSentSeq = 5;
    const Json first = makeSetLord("treasury", 100, 1790000000);
    s.commands.push_back(first);
    s.dirty = true;
    auto p = beginSave(s);
    CHECK(p.has_value());
    s.commands.push_back(makeSetLord("influence", 7, 1790000001));   // 저장하는 사이 화면에서 누른 명령
    finishSave(s, *p, std::nullopt);
    CHECK(s.saveFailed && s.dirty && s.lastSentSeq == 5 && s.control.seq() == 5);
    CHECK(s.commands.size() == 2 && s.commands[0]["id"] == first["id"]);   // 먼저 누른 것이 앞에 온다
    auto retry = beginSave(s);
    CHECK(retry.has_value() && retry->doc.raw()["commands"].size() == 2);
    finishSave(s, *retry, 6);
    CHECK(!s.saveFailed && s.lastSentSeq == 6);
    CHECK(s.commands.size() == 2);                            // 모드가 실행했다고 알릴 때까지 남는다
}

TEST(overlay_session_change_made_during_a_save_is_saved_next) {
    Session s;
    s.dirty = true;
    auto p = beginSave(s);
    CHECK(p.has_value() && !p->doc.build().enabled);
    BuildSettings b = s.control.build();   // 저장하는 사이 화면에서 바꿈
    b.enabled = true;
    s.control.setBuild(b);
    s.dirty = true;
    finishSave(s, *p, 1);
    CHECK(s.dirty && s.control.build().enabled && s.control.seq() == 1);
    auto next = beginSave(s);
    CHECK(next.has_value() && next->doc.build().enabled);
}

TEST(overlay_session_reloads_only_newer_files_and_never_over_pending_changes) {
    Session s;
    s.control.setSeq(10);
    s.lastSentSeq = 10;
    CHECK(!wantsReload(s, 10) && !wantsReload(s, 9) && wantsReload(s, 11));
    CHECK(adoptReloaded(s, ControlDoc::parse(R"({"version":1,"seq":11,"features":{"upgrade":{"enabled":true}},"commands":[{"id":"x","type":"setLord"}]})")));
    CHECK(s.control.upgrade().enabled && s.lastSentSeq == 11);
    CHECK(s.control.raw()["commands"].empty());      // 파일에 있던 남의 명령은 다시 보내지 않는다
    s.dirty = true;                                  // 저장 대기 중인 변경이 있으면 우리 쪽이 이긴다
    CHECK(!wantsReload(s, 12));
    CHECK(!adoptReloaded(s, ControlDoc::parse(R"({"version":1,"seq":12,"features":{"upgrade":{"enabled":false}}})")));
    CHECK(s.control.upgrade().enabled && s.control.seq() == 11);
    s.dirty = false;
    CHECK(!adoptReloaded(s, ControlDoc::parse(R"({"version":1,"seq":11,"features":{}})")));   // 더 새롭지 않다
}
