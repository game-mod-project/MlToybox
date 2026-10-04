#include "test.h"
#include "hooks.h"
#include <array>

using namespace mlt;

namespace {
struct FakeBackend : HookBackend {
    int created = 0; std::vector<std::pair<void*, bool>> toggles; bool failCreate = false;
    bool create(void*, void*, void** original, std::string& err) override {
        if (failCreate) { err = "MH_ERROR"; return false; }
        ++created; *original = reinterpret_cast<void*>(0x1234); return true;
    }
    bool setEnabled(void* target, bool on, std::string&) override { toggles.push_back({ target, on }); return true; }
};
void* g_orig = nullptr;
int detour() { return 0; }
bool wantsBuild(const NativeControl& c) { return c.instantBuild; }
}

TEST(install_only_on_unique_match_and_report_reasons) {
    std::array<uint8_t, 12> text{ 0x90, 0xAA, 0xBB, 0xCC, 0x90, 0x11, 0x22, 0x90, 0x11, 0x22, 0x90, 0x90 };
    HookManager m; FakeBackend be;
    m.add({ "unique", "AA BB CC", reinterpret_cast<void*>(&detour), &g_orig, &wantsBuild });
    m.add({ "missing", "DE AD", reinterpret_cast<void*>(&detour), &g_orig, &wantsBuild });
    m.add({ "ambiguous", "11 22", reinterpret_cast<void*>(&detour), &g_orig, &wantsBuild });
    m.installAll(text, 0x1000, be);
    auto s = m.states();
    CHECK(be.created == 1);
    CHECK(s[0].installed && s[0].error.empty());
    CHECK(!s[1].installed && s[1].error == "pattern not found");
    CHECK(!s[2].installed && s[2].error == "pattern ambiguous");
}

TEST(sync_toggles_only_on_change) {
    std::array<uint8_t, 4> text{ 0xAA, 0xBB, 0xCC, 0x90 };
    HookManager m; FakeBackend be;
    m.add({ "b", "AA BB CC", reinterpret_cast<void*>(&detour), &g_orig, &wantsBuild });
    m.installAll(text, 0x2000, be);
    NativeControl on; on.instantBuild = true;
    m.sync(on, be); m.sync(on, be);
    CHECK(be.toggles.size() == 1 && be.toggles[0].second == true);
    CHECK(be.toggles[0].first == reinterpret_cast<void*>(0x2000));
    m.sync(NativeControl{}, be);
    CHECK(be.toggles.size() == 2 && be.toggles[1].second == false);
    CHECK(m.states()[0].active == false);
}

TEST(body_check_rejects_same_prologue_with_wrong_layout) {
    // 같은 프롤로그(AA BB CC)지만 본문에 기대한 오프셋 명령(DE AD)이 없는 함수
    std::array<uint8_t, 12> text{ 0xAA, 0xBB, 0xCC, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0xDE };
    HookManager m; FakeBackend be;
    HookSpec s{ "b", "AA BB CC", reinterpret_cast<void*>(&detour), &g_orig, &wantsBuild };
    s.bodyChecks = { "DE AD" };
    s.bodyWindow = 12;
    m.add(s);
    m.installAll(text, 0, be);
    CHECK(be.created == 0);
    CHECK(!m.states()[0].installed && m.states()[0].error == "layout check failed: DE AD");
}

TEST(body_check_accepts_expected_layout_within_window) {
    std::array<uint8_t, 12> text{ 0xAA, 0xBB, 0xCC, 0x90, 0x90, 0xDE, 0xAD, 0x90, 0x90, 0x90, 0x90, 0x90 };
    HookManager m; FakeBackend be;
    HookSpec s{ "b", "AA BB CC", reinterpret_cast<void*>(&detour), &g_orig, &wantsBuild };
    s.bodyChecks = { "DE AD" };
    s.bodyWindow = 8;
    m.add(s);
    m.installAll(text, 0, be);
    CHECK(be.created == 1 && m.states()[0].installed);
}

TEST(body_check_outside_window_fails) {
    std::array<uint8_t, 12> text{ 0xAA, 0xBB, 0xCC, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0xDE, 0xAD };
    HookManager m; FakeBackend be;
    HookSpec s{ "b", "AA BB CC", reinterpret_cast<void*>(&detour), &g_orig, &wantsBuild };
    s.bodyChecks = { "DE AD" };
    s.bodyWindow = 8;
    m.add(s);
    m.installAll(text, 0, be);
    CHECK(!m.states()[0].installed);
}

TEST(resolve_only_spec_reports_the_address_and_makes_no_hook) {
    // detour 가 없는 항목은 후킹하지 않고 주소만 찾는다(다른 detour 가 직접 부를 게임 함수)
    std::array<uint8_t, 8> text{ 0x90, 0xAA, 0xBB, 0xCC, 0xDE, 0xAD, 0x90, 0x90 };
    HookManager m; FakeBackend be;
    void* address = nullptr;
    HookSpec s{ "finish", "AA BB CC", nullptr, &address, &wantsBuild };
    s.bodyChecks = { "DE AD" };
    s.bodyWindow = 6;
    m.add(s);
    m.installAll(text, 0x5000, be);
    CHECK(be.created == 0);
    CHECK(m.states()[0].installed && address == reinterpret_cast<void*>(0x5001));
    NativeControl on; on.instantBuild = true;
    m.sync(on, be);
    CHECK(m.states()[0].active && be.toggles.empty());
    m.sync(NativeControl{}, be);
    CHECK(!m.states()[0].active && be.toggles.empty());
}

TEST(resolve_only_spec_leaves_the_address_empty_when_the_layout_differs) {
    std::array<uint8_t, 8> text{ 0x90, 0xAA, 0xBB, 0xCC, 0x90, 0x90, 0x90, 0x90 };
    HookManager m; FakeBackend be;
    void* address = nullptr;
    HookSpec s{ "finish", "AA BB CC", nullptr, &address, &wantsBuild };
    s.bodyChecks = { "DE AD" };
    s.bodyWindow = 6;
    m.add(s);
    m.installAll(text, 0x5000, be);
    NativeControl on; on.instantBuild = true;
    m.sync(on, be);
    CHECK(!m.states()[0].installed && !m.states()[0].active && address == nullptr);
    CHECK(m.states()[0].error == "layout check failed: DE AD");
}

namespace {
int g_configured = 0; bool g_lastPlacement = false;
void configureProbe(const NativeControl& c) { ++g_configured; g_lastPlacement = c.ignorePlacement; }
}

TEST(configure_passes_the_control_to_an_installed_spec_on_every_sync) {
    // 한 후킹이 옵션 여럿을 맡을 때 detour 가 어느 옵션이 켜졌는지 알아야 한다
    std::array<uint8_t, 4> text{ 0xAA, 0xBB, 0xCC, 0x90 };
    HookManager m; FakeBackend be;
    HookSpec s{ "b", "AA BB CC", reinterpret_cast<void*>(&detour), &g_orig, &wantsBuild };
    s.configure = &configureProbe;
    m.add(s);
    HookSpec missing{ "missing", "DE AD", reinterpret_cast<void*>(&detour), &g_orig, &wantsBuild };
    missing.configure = &configureProbe;
    m.add(missing);
    m.installAll(text, 0, be);
    g_configured = 0;
    NativeControl c; c.ignorePlacement = true;
    m.sync(c, be);
    CHECK(g_configured == 1 && g_lastPlacement);      // 설치되지 않은 항목에는 부르지 않는다
    c.ignorePlacement = false;
    m.sync(c, be);
    CHECK(g_configured == 2 && !g_lastPlacement);     // 후킹을 켜고 끌 일이 없어도 옵션은 매번 전한다
}

TEST(create_failure_reported_and_never_enabled) {
    std::array<uint8_t, 4> text{ 0xAA, 0xBB, 0xCC, 0x90 };
    HookManager m; FakeBackend be; be.failCreate = true;
    m.add({ "b", "AA BB CC", reinterpret_cast<void*>(&detour), &g_orig, &wantsBuild });
    m.installAll(text, 0, be);
    NativeControl on; on.instantBuild = true;
    m.sync(on, be);
    CHECK(!m.states()[0].installed && m.states()[0].error == "MH_ERROR" && be.toggles.empty());
}

namespace {
std::string fragmentA() { return "\"a\":1"; }
std::string fragmentNone() { return ""; }
std::string fragmentB() { return "\"b\":[2]"; }
}

// 기능이 상태 파일에 값을 싣는 자리: 설치된 항목의 조각만, 빈 것은 빼고 쉼표로 잇는다
TEST(extras_join_the_status_fragments_of_installed_entries) {
    std::array<uint8_t, 8> text{ 0xAA, 0xBB, 0x90, 0xCC, 0xDD, 0x90, 0xEE, 0xFF };
    HookManager m; FakeBackend be;
    HookSpec a{ "a", "AA BB", reinterpret_cast<void*>(&detour), &g_orig, &wantsBuild };
    a.status = &fragmentA;
    HookSpec none{ "none", "CC DD", reinterpret_cast<void*>(&detour), &g_orig, &wantsBuild };
    none.status = &fragmentNone;
    HookSpec missing{ "missing", "12 34", reinterpret_cast<void*>(&detour), &g_orig, &wantsBuild };
    missing.status = &fragmentB;                    // 설치되지 않는 항목: 불리지 않는다
    HookSpec b{ "b", "EE FF", reinterpret_cast<void*>(&detour), &g_orig, &wantsBuild };
    b.status = &fragmentB;
    m.add(a); m.add(none); m.add(missing); m.add(b);
    CHECK(m.extras().empty());                      // 설치하기 전
    m.installAll(text, 0, be);
    CHECK(m.extras() == "\"a\":1,\"b\":[2]");
}