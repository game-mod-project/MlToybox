#include "test.h"
#include "features/memory.h"

using namespace mlt;

namespace {
struct Block { alignas(8) uint8_t bytes[0x40]{}; };
}

TEST(memory_reads_and_writes_at_an_offset_whatever_the_alignment) {
    Block b;
    mem::write<int32_t>(b.bytes, 0x11, 0x12345678);            // 정렬되지 않은 자리
    CHECK(mem::read<int32_t>(b.bytes, 0x11) == 0x12345678);
    CHECK(b.bytes[0x11] == 0x78 && b.bytes[0x14] == 0x12);
    CHECK(mem::readPtr(b.bytes, 0x20) == nullptr);
    mem::write<std::uintptr_t>(b.bytes, 0x20, reinterpret_cast<std::uintptr_t>(b.bytes + 8));
    CHECK(mem::readPtr(b.bytes, 0x20) == b.bytes + 8);
}

// "건물(영지)의 주인이 플레이어인가"의 꼴: 객체가 가리키는 다른 객체의 플래그 바이트
TEST(memory_flag_behind_pointer_follows_the_pointer_and_reads_the_flag) {
    Block object, target;
    CHECK(!mem::flagBehindPointer(object.bytes, 0x10, 0x30));  // 포인터가 비어 있다
    mem::write<std::uintptr_t>(object.bytes, 0x10, reinterpret_cast<std::uintptr_t>(target.bytes));
    CHECK(!mem::flagBehindPointer(object.bytes, 0x10, 0x30));  // 플래그가 0 이다
    target.bytes[0x30] = 1;
    CHECK(mem::flagBehindPointer(object.bytes, 0x10, 0x30));
    CHECK(!mem::flagBehindPointer(nullptr, 0x10, 0x30));
}
