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
