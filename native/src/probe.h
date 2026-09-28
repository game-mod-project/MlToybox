#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

// 개발용 메모리 프로브 (spec §11.3): bridge\probe_request.txt 의 "seq address size" 를 읽어
// 해당 메모리를 bridge\probe_<seq>.bin 으로 덤프한다.
namespace mlt {
struct ProbeRequest { long long seq = 0; uintptr_t address = 0; size_t size = 0; };
constexpr size_t kMaxProbeSize = 1 << 20;
std::optional<ProbeRequest> parseProbeRequest(std::string_view text);
// 자기 프로세스 메모리를 안전하게 복사한다(잘못된 주소면 false, 예외 없음)
bool copyProcessMemory(uintptr_t address, size_t size, std::string& out);
}
