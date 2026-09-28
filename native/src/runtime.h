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
}
