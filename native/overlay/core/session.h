#pragma once
#include "control_doc.h"
#include <optional>
#include <vector>

// 화면과 작업 스레드가 나눠 쓰는 설정 문서 상태와, 저장·다시 읽기의 순서 규칙.
// 잠금은 쓰는 쪽(ui/app.h 의 App::mutex)이 잡는다. 파일 입출력은 여기서 하지 않는다.
namespace mlt::ov {

struct Session {
    ControlDoc control;              // 메모리의 설정 문서
    bool dirty = false;              // 저장해야 한다
    std::vector<Json> commands;      // 다음 저장에 실어 보낼 일회성 명령
    long long lastSentSeq = 0;       // 마지막으로 저장했거나 읽은 seq
    bool saveFailed = false;
};

struct PendingSave {
    ControlDoc doc;                  // 파일에 쓸 문서(명령 포함)
    std::vector<Json> sent;          // 이 저장에 실은 명령
};

// 저장할 것이 없으면 값 없음. 있으면 명령을 실은 사본을 돌려주고, 메모리에서는 명령과 변경 표시를 비운다
std::optional<PendingSave> beginSave(Session& s);
// 저장 결과를 반영한다. seq 가 없으면 실패한 것이다: 명령을 되돌려 놓고 변경 표시를 다시 켠다
void finishSave(Session& s, const PendingSave& pending, std::optional<long long> seq);
// 파일의 seq 가 메모리보다 크고 저장 대기 중인 변경이 없으면 참(패널 등 밖에서 바꿨으니 다시 읽는다)
bool wantsReload(const Session& s, long long fileSeq);
// 다시 읽은 문서를 받아들인다. 그 사이 변경이 생겼거나 더 새롭지 않으면 버리고 false
bool adoptReloaded(Session& s, ControlDoc loaded);
}
