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
}
