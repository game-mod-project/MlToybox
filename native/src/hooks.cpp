#include "hooks.h"
#include "scanner.h"
#include <algorithm>

namespace mlt {

void HookManager::add(HookSpec spec) {
    Entry e{ std::move(spec) };
    e.state.name = e.spec.name;
    entries_.push_back(std::move(e));
}

void HookManager::installAll(std::span<const uint8_t> text, uintptr_t textAddress, HookBackend& backend) {
    for (auto& e : entries_) {
        auto r = scanUnique(text, e.spec.pattern);
        if (r.result == ScanResult::NotFound) { e.state.error = "pattern not found"; continue; }
        if (r.result == ScanResult::Ambiguous) { e.state.error = "pattern ambiguous"; continue; }
        if (r.result == ScanResult::BadPattern) { e.state.error = "bad pattern"; continue; }
        auto body = text.subspan(r.offset, std::min(e.spec.bodyWindow, text.size() - r.offset));
        std::string missing;
        for (const auto& check : e.spec.bodyChecks) {
            auto p = parsePattern(check);
            if (!p || findAll(body, *p, 1).empty()) { missing = check; break; }
        }
        if (!missing.empty()) { e.state.error = "layout check failed: " + missing; continue; }
        void* target = reinterpret_cast<void*>(textAddress + r.offset);
        std::string err;
        if (!backend.create(target, e.spec.detour, e.spec.original, err)) { e.state.error = err; continue; }
        e.target = target;
        e.state.installed = true;
    }
}

void HookManager::sync(const NativeControl& control, HookBackend& backend) {
    for (auto& e : entries_) {
        if (!e.state.installed) continue;
        bool want = e.spec.wanted(control);
        if (want == e.state.active) continue;
        std::string err;
        if (backend.setEnabled(e.target, want, err)) { e.state.active = want; e.state.error.clear(); }
        else e.state.error = err;
    }
}

std::vector<FeatureState> HookManager::states() const {
    std::vector<FeatureState> out;
    for (auto& e : entries_) out.push_back(e.state);
    return out;
}

}
