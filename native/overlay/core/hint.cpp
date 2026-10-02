#include "hint.h"

namespace mlt::ov {

void HintTimer::onReady(unsigned long long nowMs) {
    until_ = nowMs + kShowMs;
}

void HintTimer::update(bool connected, bool inGame, unsigned long long nowMs) {
    if (!connected) return;
    if (inGame && !wasInGame_) until_ = nowMs + kShowMs;
    wasInGame_ = inGame;
}

bool HintTimer::active(unsigned long long nowMs) const {
    return nowMs < until_;
}

}
