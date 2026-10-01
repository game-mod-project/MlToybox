#include "session.h"
#include "status_doc.h"
#include <algorithm>
#include <utility>

namespace mlt::ov {

std::optional<PendingSave> beginSave(Session& s) {
    if (!s.dirty) return std::nullopt;
    PendingSave pending;
    pending.doc = s.control;
    pending.doc.setCommands(s.commands);
    s.dirty = false;
    return pending;
}

void finishSave(Session& s, const PendingSave&, std::optional<long long> seq) {
    if (seq) {
        s.control.setSeq(*seq);
        s.lastSentSeq = *seq;
        s.saveFailed = false;
        return;
    }
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

void dropFinishedCommands(Session& s, const std::vector<std::string>& reportedIds, long long nowEpochSeconds) {
    auto finished = [&](const Json& command) {
        const auto id = optString(&command, "id");
        if (id && std::find(reportedIds.begin(), reportedIds.end(), *id) != reportedIds.end()) return true;
        const auto issued = optNumber(&command, "issuedAt");
        return !issued || nowEpochSeconds - static_cast<long long>(*issued) > kCommandMaxAgeSec;
    };
    s.commands.erase(std::remove_if(s.commands.begin(), s.commands.end(), finished), s.commands.end());
}

}
