#include "session.h"
#include <utility>

namespace mlt::ov {

std::optional<PendingSave> beginSave(Session& s) {
    if (!s.dirty) return std::nullopt;
    PendingSave pending;
    pending.sent.swap(s.commands);
    s.control.setCommands(pending.sent);
    pending.doc = s.control;
    s.control.setCommands({});   // 명령은 이번 저장에만 실린다
    s.dirty = false;
    return pending;
}

void finishSave(Session& s, const PendingSave& pending, std::optional<long long> seq) {
    if (seq) {
        s.control.setSeq(*seq);
        s.lastSentSeq = *seq;
        s.saveFailed = false;
        return;
    }
    // 저장하는 사이 화면에서 누른 명령보다 앞에 되돌려 놓는다
    s.commands.insert(s.commands.begin(), pending.sent.begin(), pending.sent.end());
    s.dirty = true;
    s.saveFailed = true;
}

bool wantsReload(const Session& s, long long fileSeq) {
    return !s.dirty && fileSeq > s.control.seq();
}

bool adoptReloaded(Session& s, ControlDoc loaded) {
    if (s.dirty || loaded.seq() <= s.control.seq()) return false;
    s.control = std::move(loaded);
    s.control.setCommands({});   // 파일에 남아 있던 남의 명령을 우리가 다시 보내지 않는다
    s.lastSentSeq = s.control.seq();
    return true;
}

}
