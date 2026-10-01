#pragma once
#include <string>

namespace mlt::ov {

// fn(arg) 를 구조적 예외(접근 위반 등)로 감싼다. 예외가 나면 false 를 돌려주고 code 에 예외 코드를 적는다
bool runGuarded(void (*fn)(void*), void* arg, unsigned long* code);

// 예외 코드를 "0xC0000005" 꼴로 적는다
std::string hexCode(unsigned long code);

// 구조적 예외가 나면 C++ 소멸자가 불리지 않아 잠금이 풀리지 않는다.
// 잠금을 쥐고 있는지를 held 에 적어 두고, 예외 처리기가 held 가 참인 잠금을 직접 푼다
template <class Mutex>
class TrackedLock {
public:
    TrackedLock(Mutex& m, bool& held) : m_(m), held_(held) { m_.lock(); held_ = true; }
    ~TrackedLock() { held_ = false; m_.unlock(); }
    TrackedLock(const TrackedLock&) = delete;
    TrackedLock& operator=(const TrackedLock&) = delete;
private:
    Mutex& m_;
    bool& held_;
};
}
