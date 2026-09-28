#pragma once
#include "control.h"
#include "status.h"
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace mlt {
struct HookSpec {
    std::string name;
    std::string pattern;
    void* detour;
    void** original;
    bool (*wanted)(const NativeControl&);
    // 레이아웃 검사: 일치한 함수 시작부터 bodyWindow 바이트 안에 모두 있어야 하는 패턴들
    // (detour 가 의존하는 필드 오프셋을 쓰는 명령). 게임 업데이트 뒤 같은 프롤로그의 다른 함수에 설치되는 것을 막는다.
    std::vector<std::string> bodyChecks = {};
    size_t bodyWindow = 0x200;
};

class HookBackend {
public:
    virtual ~HookBackend() = default;
    virtual bool create(void* target, void* detour, void** original, std::string& err) = 0;
    virtual bool setEnabled(void* target, bool on, std::string& err) = 0;
};

class HookManager {
public:
    void add(HookSpec spec);
    void installAll(std::span<const uint8_t> text, uintptr_t textAddress, HookBackend& backend);
    void sync(const NativeControl& control, HookBackend& backend);
    std::vector<FeatureState> states() const;
private:
    struct Entry { HookSpec spec; void* target = nullptr; FeatureState state; };
    std::vector<Entry> entries_;
};

void registerFeatures(HookManager& manager);
}
