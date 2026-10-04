#include "test.h"
#include "runtime.h"
#include <atomic>
#include <thread>
#include <windows.h>

// 네이티브 DLL 과 오버레이 DLL 은 MinHook 을 각각 들고 있고(사본이 둘), MinHook 은 후킹을 켜고 끌 때 다른 스레드를 모두 멈춘다.
// 두 DLL 의 작업 스레드가 같은 때에 그러면 서로를 멈춘 채 깨어나지 못한다. 그래서 그 일은 이 잠금을 쥐고 한다
TEST(hook_gate_lets_one_thread_through_at_a_time) {
    std::atomic<bool> secondEntered{ false };
    std::thread second;
    bool firstHeld = false, enteredWhileHeld = true;
    {
        mlt::HookGate first;
        firstHeld = first.held();
        second = std::thread([&] {
            mlt::HookGate gate;
            secondEntered = true;
        });
        Sleep(150);
        enteredWhileHeld = secondEntered.load();
    }
    second.join();
    CHECK(firstHeld);
    CHECK(!enteredWhileHeld);          // 첫 스레드가 놓을 때까지 기다렸다
    CHECK(secondEntered.load());       // 놓은 뒤에 들어왔다
}

TEST(hook_gate_can_be_taken_again_by_the_thread_that_holds_it) {
    mlt::HookGate outer;
    mlt::HookGate inner;               // 같은 스레드는 기다리지 않는다
    CHECK(outer.held() && inner.held());
}
