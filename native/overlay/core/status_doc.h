#pragma once
#include "control_doc.h"
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// status.json 문서(모드가 1초마다 쓴다). 읽기 전용.
// Lua 의 JSON 인코더는 빈 표를 {} 가 아니라 [] 로 쓴다. 객체 자리에 온 배열은 빈 객체로 본다.
namespace mlt::ov {

struct FeatureStatus {
    bool active = false;
    std::optional<std::string> lastError;
};

struct NativeFeature {
    bool installed = false;
    bool active = false;
    std::optional<std::string> lastError;
};

struct NativeStatus {
    bool loaded = false;
    std::optional<std::string> error;
    bool stale = false;
    std::map<std::string, NativeFeature> features;
};

struct CommandResult {
    bool ok = false;
    std::optional<std::string> error;
    std::optional<std::vector<int>> squads;
    std::optional<int> reformed;
    std::optional<int> requested;
    std::optional<int> added;
};

struct LordStatus {
    std::optional<double> treasury;
    std::optional<int> influence;
    std::optional<int> kingsFavour;
};

struct StatusDoc {
    long long heartbeat = 0;
    bool inGame = false;
    std::optional<long long> appliedSeq;
    std::optional<std::string> bridgeError;
    std::optional<std::map<std::string, FeatureStatus>> features;                 // 이름순
    std::optional<std::vector<std::pair<std::string, CommandResult>>> commands;   // 파일에 적힌 순서
    std::optional<NativeStatus> native;
    std::optional<LordStatus> lord;
    Json raw;   // 탭이 더 읽을 항목(자원, 영지, 인구, 용병 등)
};

// 깨졌거나 객체가 아니면 값 없음
std::optional<StatusDoc> parseStatus(std::string_view text);

// 읽기 도우미
std::optional<std::string> optString(const Json* obj, const char* key);
std::optional<double> optNumber(const Json* obj, const char* key);
}
