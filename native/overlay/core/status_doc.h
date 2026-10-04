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

struct RegionInfo {
    std::string key;    // 영지 키(regionUniqueTag)
    std::string name;
};

struct RegionResources {
    std::string key;
    std::string name;
    std::map<std::string, double> values;   // 그 영지의 재고
};

// 해제돼 빈 카드가 된 생성 분대
struct SpawnStatus {
    int disbanded = 0;
    int pending = 0;                                   // 빈 카드 정리 중
    std::vector<std::pair<std::string, int>> byUnit;   // 병종 id 와 수. 파일에 적힌 순서
};

struct RetinueSquad {
    int id = 0;
    std::string unit;
    int count = 0;
    std::string kind;   // spawned(병력 생성) | mercenary(커스텀 용병 고용)
};

// 모드가 만든 수행원 분대와, 지금 꾸미기 화면이 열려 있는 분대
struct RetinueStatus {
    std::vector<RetinueSquad> squads;
    std::optional<int> editing;
};

struct PopulationRegion {
    std::string key;
    std::string name;
    int families = 0;
    int population = 0;
    int homeless = 0;
    int freeSlots = 0;
    int unassigned = 0;
};

struct PopulationStatus {
    int families = 0;
    int population = 0;
    int homeless = 0;
    int freeSlots = 0;
    int natural = 0;
    int multiplied = 0;
    int unassigned = 0;
    std::vector<PopulationRegion> regions;
};

// 내 영지의 지금 자격·공공질서
struct MoodRegion {
    std::string key;
    std::string name;
    int approval = 0;
    int order = 0;
};

struct MoodStatus {
    std::vector<MoodRegion> regions;
};

struct MercSlot {
    std::string name;
    int cost = 0;
    bool custom = false;
};

struct MercSkipped {
    std::string name;
    std::string reason;
};

struct MercenaryStatus {
    std::vector<MercSlot> slots;
    int hiredMine = 0;
    int hiredAi = 0;
    int refunded = 0;   // 맵을 불러온 뒤의 환급 합계
    std::vector<MercSkipped> skipped;
    std::optional<std::string> note;
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
    std::optional<std::vector<std::string>> resourceIds;      // 관리할 수 있는 자원 이름
    std::optional<std::map<std::string, double>> resources;   // 모든 내 영지 합계
    std::optional<std::vector<RegionResources>> regions;      // 영지별 재고
    std::optional<std::vector<RegionInfo>> playerRegions;     // 내 영지
    std::optional<SpawnStatus> spawn;
    std::optional<RetinueStatus> retinue;
    std::optional<PopulationStatus> population;
    std::optional<MercenaryStatus> mercenaries;
    std::optional<MoodStatus> mood;
    Json raw;
};

// 깨졌거나 객체가 아니면 값 없음
std::optional<StatusDoc> parseStatus(std::string_view text);

// 읽기 도우미
std::optional<double> optNumber(const Json* obj, const char* key);
}
