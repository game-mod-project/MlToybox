#pragma once
#include "app.h"
#include <optional>
#include <string>

// 복사·붙여넣기를 화면 스레드 밖에서 시스템 클립보드와 맞춘다.
// 화면 스레드는 프레임 안에서 ImGui 잠금과 App::mutex 를 쥐고 있다. 그 상태로 Windows 클립보드를 열면
// Windows 가 클립보드를 가진 창에 메시지를 보내고 기다릴 수 있고(비우기, 지연 렌더링), 그 창의 스레드가
// ImGui 잠금을 기다리고 있으면 서로 멈춘다. 그래서 화면 스레드는 App 에 적어 둔 글만 읽고 쓴다.
namespace mlt::ov {

struct ClipboardApi {
    bool (*write)(const std::string& utf8);       // 시스템 클립보드에 쓴다. 실패하면 false(다른 프로그램이 잡고 있다)
    std::optional<std::string> (*read)();         // 시스템 클립보드의 글. 열지 못했으면 값 없음, 글이 아니면 빈 글
    unsigned long (*sequence)();                  // 클립보드가 바뀔 때마다 달라지는 번호
};

// Windows 클립보드(render/clipboard)
const ClipboardApi& systemClipboard();

// ImGui 의 복사·붙여넣기가 App 의 clipboardOut / clipboardIn 을 쓰게 한다. ImGui 컨텍스트를 만든 뒤 한 번 부른다
void installClipboardCallbacks();

// 작업 스레드가 0.1초마다 tick 을 부른다
class ClipboardSync {
public:
    static constexpr int kWriteTries = 20;   // 클립보드가 바쁘면 2초 동안 다시 쓴다

    explicit ClipboardSync(const ClipboardApi& api) : api_(api) {}
    // 화면에서 복사한 글을 시스템 클립보드에 쓰고(실패하면 다음 회차에 다시), 글자 칸에 커서가 있는 동안
    // 시스템 클립보드가 바뀌었으면 읽어 App::clipboardIn 에 둔다. 잠금은 App::mutex 만, 짧게 잡는다
    void tick(App& a);

private:
    ClipboardApi api_;
    std::string out_;             // 쓰고 있는 글
    int triesLeft_ = 0;
    bool seen_ = false;           // 시스템 클립보드를 한 번이라도 읽었는가
    unsigned long sequence_ = 0;  // 마지막으로 읽었을 때의 번호
};

}
