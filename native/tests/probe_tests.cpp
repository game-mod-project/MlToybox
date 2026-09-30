#include "test.h"
#include "probe.h"

using namespace mlt;

TEST(probe_request_parses_seq_address_size) {
    auto r = parseProbeRequest("7 0x2A3E32907B8 5376\r\n");
    CHECK(r.has_value());
    CHECK(r->seq == 7);
    CHECK(r->address == 0x2A3E32907B8ull);
    CHECK(r->size == 5376);
}

TEST(probe_request_accepts_address_without_prefix) {
    auto r = parseProbeRequest("1 2A3E32907B8 16");
    CHECK(r.has_value() && r->address == 0x2A3E32907B8ull);
}

TEST(probe_request_rejects_bad_input) {
    CHECK(!parseProbeRequest("").has_value());
    CHECK(!parseProbeRequest("1 0x10").has_value());            // size 없음
    CHECK(!parseProbeRequest("x 0x10 4").has_value());
    CHECK(!parseProbeRequest("1 zz 4").has_value());
    CHECK(!parseProbeRequest("1 0x10 0").has_value());          // size 0
    CHECK(!parseProbeRequest("1 0x10 99999999").has_value());   // 상한 초과
}

TEST(probe_copy_reads_own_memory_and_fails_safely) {
    const char data[] = "hello";
    std::string out;
    CHECK(copyProcessMemory(reinterpret_cast<uintptr_t>(data), 5, out));
    CHECK(out == "hello");
    CHECK(!copyProcessMemory(0x10, 8, out));                    // 잘못된 주소는 예외 없이 false
}
