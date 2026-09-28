#include "test.h"
#include "features/instant_build.h"
#include <cstring>
#include <vector>

using namespace mlt;

namespace {
struct FakePart { alignas(8) uint8_t bytes[0x320]{}; };
void setF(uint8_t* base, std::ptrdiff_t off, float v) { std::memcpy(base + off, &v, sizeof v); }
float getF(const uint8_t* base, std::ptrdiff_t off) { float v; std::memcpy(&v, base + off, sizeof v); return v; }

struct FakeMaster {
    alignas(8) uint8_t bytes[0x400]{};
    void setParts(void** data, int32_t num) {
        const auto address = reinterpret_cast<std::uintptr_t>(data);   // TArray.Data 필드에 포인터 값을 기록
        std::memcpy(bytes + instant_build::kPartsOffset, &address, sizeof address);
        std::memcpy(bytes + instant_build::kPartsNumOffset, &num, sizeof num);
    }
};
}

TEST(complete_parts_fills_hp_to_max) {
    FakePart a, b;
    setF(a.bytes, instant_build::kPartHpOffset, 10.f);  setF(a.bytes, instant_build::kPartMaxHpOffset, 100.f);
    setF(b.bytes, instant_build::kPartHpOffset, 100.f); setF(b.bytes, instant_build::kPartMaxHpOffset, 50.f);
    void* parts[] = { &a, nullptr, &b };
    FakeMaster m; m.setParts(parts, 3);
    int changed = instant_build::completeParts(m.bytes);
    CHECK(changed == 1);
    CHECK(getF(a.bytes, instant_build::kPartHpOffset) == 100.f);
    CHECK(getF(b.bytes, instant_build::kPartHpOffset) == 100.f);   // 이미 max 이상이면 그대로
}

TEST(complete_parts_ignores_null_and_bad_counts) {
    FakeMaster m;
    CHECK(instant_build::completeParts(nullptr) == 0);
    m.setParts(nullptr, 5);
    CHECK(instant_build::completeParts(m.bytes) == 0);
    void* parts[] = { nullptr };
    m.setParts(parts, -1);
    CHECK(instant_build::completeParts(m.bytes) == 0);
    m.setParts(parts, 100000);                                      // 비정상적으로 큰 Num 은 거부
    CHECK(instant_build::completeParts(m.bytes) == 0);
}
