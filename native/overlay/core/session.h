#pragma once
#include "control_doc.h"
#include <optional>
#include <string>
#include <vector>

// 화면과 작업 스레드가 나눠 쓰는 설정 문서 상태와, 저장·다시 읽기의 순서 규칙.
// 잠금은 쓰는 쪽(ui/app.h 의 App::mutex)이 잡는다. 파일 입출력은 여기서 하지 않는다.
namespace mlt::ov {

struct Session {
    ControlDoc control;              // 메모리의 설정 문서(명령은 여기에 두지 않는다)
    bool dirty = false;              // 저장해야 한다
    std::vector<Json> commands;      // 보낸(보낼) 일회성 명령. 모드가 실행했다고 알릴 때까지 저장할 때마다 싣는다
    long long lastSentSeq = 0;       // 마지막으로 저장했거나 읽은 seq
    bool saveFailed = false;
};

struct PendingSave {
    ControlDoc doc;                  // 파일에 쓸 문서(명령 포함)
};

// 모드는 이보다 오래된 명령을 버린다(mod 의 core/commands.lua MAX_AGE_SEC)
inline constexpr long long kCommandMaxAgeSec = 60;

// 저장할 것이 없으면 값 없음. 있으면 아직 끝나지 않은 명령을 모두 실은 사본을 돌려주고 변경 표시를 끈다
std::optional<PendingSave> beginSave(Session& s);
// 저장 결과를 반영한다. seq 가 없으면 실패한 것이다: 변경 표시를 다시 켠다(명령은 그대로 남아 있다)
void finishSave(Session& s, const PendingSave& pending, std::optional<long long> seq);
// 파일의 seq 가 메모리보다 크고 저장 대기 중인 변경이 없으면 참(패널 등 밖에서 바꿨으니 다시 읽는다)
bool wantsReload(const Session& s, long long fileSeq);
// 다시 읽은 문서를 받아들인다. 그 사이 변경이 생겼거나 더 새롭지 않으면 버리고 false
bool adoptReloaded(Session& s, ControlDoc loaded);
// 모드가 실행했다고 알린 명령(status.json 의 commands 에 id 가 있다)과, 모드가 버릴 만큼 오래된 명령을 뺀다.
// 모드는 control.json 을 1초마다 읽으므로, 명령을 한 번만 실으면 그 전에 나간 다음 저장이 명령을 지운다.
// 모드는 같은 id 를 한 번만 실행하므로 여러 번 실어도 된다.
void dropFinishedCommands(Session& s, const std::vector<std::string>& reportedIds, long long nowEpochSeconds);
}
