#include "test.h"
#include <exception>

std::vector<TestCase>& testRegistry() { static std::vector<TestCase> r; return r; }

int main() {
    int failed = 0;
    for (auto& t : testRegistry()) {
        try { t.fn(); std::printf("PASS %s\n", t.name); }
        catch (const TestFailure& f) { ++failed; std::printf("FAIL %s: %s\n", t.name, f.msg.c_str()); }
        catch (const std::exception& e) { ++failed; std::printf("FAIL %s: exception %s\n", t.name, e.what()); }
    }
    std::printf("%zu tests, %d failed\n", testRegistry().size(), failed);
    return failed ? 1 : 0;
}
