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

TEST(create_failure_reported_and_never_enabled) {
    std::array<uint8_t, 4> text{ 0xAA, 0xBB, 0xCC, 0x90 };
    HookManager m; FakeBackend be; be.failCreate = true;
    m.add({ "b", "AA BB CC", reinterpret_cast<void*>(&detour), &g_orig, &wantsBuild });
    m.installAll(text, 0, be);
    NativeControl on; on.instantBuild = true;
    m.sync(on, be);
    CHECK(!m.states()[0].installed && m.states()[0].error == "MH_ERROR" && be.toggles.empty());
}
