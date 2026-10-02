#include "frame_gate.h"

namespace mlt::ov {

void FrameGate::reset(size_t slots) {
    lastUse_.assign(slots, 0);
    next_ = 0;
}

size_t FrameGate::add() {
    lastUse_.push_back(0);
    return lastUse_.size() - 1;
}

bool FrameGate::ready(size_t slot, unsigned long long completed) const {
    return slot < lastUse_.size() && completed >= lastUse_[slot];
}

size_t FrameGate::firstReady(unsigned long long completed) const {
    for (size_t i = 0; i < lastUse_.size(); ++i) {
        if (completed >= lastUse_[i]) return i;
    }
    return npos;
}

unsigned long long FrameGate::submitted(size_t slot) {
    const unsigned long long value = ++next_;
    if (slot < lastUse_.size()) lastUse_[slot] = value;
    return value;
}

}
