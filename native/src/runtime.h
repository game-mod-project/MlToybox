#pragma once
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace mlt {
long long nowEpochSeconds();
std::optional<std::string> readFileUtf8(const std::filesystem::path& path);
bool writeFileAtomic(const std::filesystem::path& path, std::string_view content);
struct TextSection { const uint8_t* data = nullptr; size_t size = 0; uintptr_t base = 0; };
TextSection mainModuleText();
void runWorker(void* selfModule);
// 주소를 포함한 모듈을 프로세스 종료까지 고정한다. 후킹이 살아 있는 동안 DLL 이 언로드되면 게임이 크래시하므로
// Lua 상태 종료(package.loadlib 핸들 해제)에도 언로드되지 않게 한다.
bool pinModuleContaining(const void* address);

// 후킹을 켜고 끄는 동안 쥐는 잠금. 네이티브 DLL 과 오버레이 DLL 은 MinHook 을 각각 정적으로 링크해 사본이 둘이고,
// 사본끼리는 서로의 잠금을 모른다. MinHook 은 후킹을 켜고 끌 때 다른 스레드를 모두 멈추는데, 두 DLL 의 작업 스레드가
// 같은 때에 그러면 서로를 멈춘 채 깨어나지 못해 게임 전체가 멈춘다. 이름 있는 뮤텍스라 DLL 이 달라도 같은 것을 쥔다.
// 같은 스레드는 겹쳐 쥘 수 있다. 뮤텍스를 만들지 못했으면 held() 가 거짓이고, 잠금 없이 진행한다
class HookGate {
public:
    HookGate();
    ~HookGate();
    HookGate(const HookGate&) = delete;
    HookGate& operator=(const HookGate&) = delete;
    bool held() const { return held_; }

private:
    void* mutex_ = nullptr;
    bool held_ = false;
};
}
