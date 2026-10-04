#include "features/region_scope.h"
#include "features/memory.h"

namespace mlt::region_scope {

bool ownedByMainPlayer(const uint8_t* region) {
    return mem::flagBehindPointer(region, kRegionOwnerOffset, kPawnIsMainPlayerOffset);
}

bool plausibleName(uint64_t name) {
    // FName 의 아래 32비트는 이름 표의 번호(블록 << 16 | 블록 안 자리)다. 블록은 8192개까지다
    const uint32_t index = static_cast<uint32_t>(name);
    return index != 0 && index < 0x20000000u;
}

namespace {
using NameEqualsFn = bool(__fastcall*)(uint64_t name, const char* text);
NameEqualsFn g_nameEquals = nullptr;
void* g_tagLayout = nullptr;

// 영지를 가리는 기능이 하나라도 켜져 있을 때 "쓰임"으로 보인다(주소만 찾는 항목이라 켜고 끌 후킹은 없다)
bool wanted(const NativeControl& c) { return !c.mood.neutral() || c.region.enabled; }
}

bool canTellRegions() {
    return g_nameEquals != nullptr && g_tagLayout != nullptr;
}

bool tagIs(const uint8_t* region, const std::string& key) {
    if (!region || !canTellRegions()) return false;
    const uint64_t tag = mem::read<uint64_t>(region, kRegionTagOffset);
    return plausibleName(tag) && g_nameEquals(tag, key.c_str());
}

void registerHook(HookManager& manager) {
    HookSpec name{ "region_name", kNameEqualsPattern, nullptr, reinterpret_cast<void**>(&g_nameEquals), &wanted };
    name.bodyChecks = { kNameEqualsBody };
    name.bodyWindow = kNameEqualsWindow;
    manager.add(name);

    // 패턴 자체가 오프셋(이름 +0x2B8·+0x2C0, 자격 +0x1068)을 담고 있다
    HookSpec tag{ "region_tag", kRegionNamePattern, nullptr, &g_tagLayout, &wanted };
    manager.add(tag);
}

}
