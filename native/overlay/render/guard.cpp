#include "guard.h"
#include <cstdio>
#include <windows.h>

namespace mlt::ov {

// __try 를 쓰는 함수에는 소멸자가 있는 지역 객체를 둘 수 없다. 그래서 이 함수는 호출만 한다
bool runGuarded(void (*fn)(void*), void* arg, unsigned long* code) {
    __try {
        fn(arg);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        if (code) *code = GetExceptionCode();
        return false;
    }
}

std::string hexCode(unsigned long code) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "0x%08lX", code);
    return buf;
}

}
