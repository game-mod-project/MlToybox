#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

// 게임 객체의 메모리를 오프셋으로 읽고 쓰는 도우미. 정렬을 가정하지 않는다(memcpy).
// 오프셋은 기능마다 자기 헤더에 두고, 그 기능의 bodyChecks 가 게임 코드와 맞는지 확인한다
namespace mlt::mem {
template <class T> T read(const uint8_t* base, std::ptrdiff_t off) { T v; std::memcpy(&v, base + off, sizeof v); return v; }
template <class T> void write(uint8_t* base, std::ptrdiff_t off, T v) { std::memcpy(base + off, &v, sizeof v); }

inline const uint8_t* readPtr(const uint8_t* base, std::ptrdiff_t off) {
    return reinterpret_cast<const uint8_t*>(read<std::uintptr_t>(base, off));
}

// object 의 pointerOffset 에 든 포인터가 가리키는 객체의 flagOffset 바이트가 0 이 아닌가. object 나 포인터가 없으면 false.
// "건물(영지)의 주인이 플레이어인가"가 이 꼴이다: 주인 폰(ownerPawn)의 isMainPlayer
inline bool flagBehindPointer(const uint8_t* object, std::ptrdiff_t pointerOffset, std::ptrdiff_t flagOffset) {
    if (!object) return false;
    const uint8_t* target = readPtr(object, pointerOffset);
    return target && target[flagOffset] != 0;
}
}
