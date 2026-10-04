#pragma once
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// control.json 문서. JSON 을 그대로 들고 있어 모르는 키를 보존한다. 아는 기능은 구조체로 읽고 쓴다.
// 기본값은 패널(Panel.Core/ControlDocument.cs)과 같다.
namespace mlt::ov {
using Json = nlohmann::ordered_json;

struct BuildSettings {
    bool enabled = false;
    bool ignorePlacement = true;
    bool instantBuild = true;
    bool instantRepair = true;
    bool noMaterials = true;
    bool noRegionLimit = true;
};

struct UpgradeSettings {
    bool enabled = false;
};

// 값이 없는 항목은 관리하지 않는다(키를 쓰지 않는다)
struct LordSettings {
    bool enabled = false;
    int intervalSec = 2;
    std::optional<int> treasury;
    std::optional<int> influence;
    std::optional<int> kingsFavour;
};

struct MilitarySettings {
    bool enabled = false;
    bool ignoreEquipment = true;
    bool ignorePopulation = true;
    bool zeroUpkeep = true;
    bool unlimitedSquads = true;
};

struct PopulationSettings {
    bool enabled = false;
    int multiplier = 2;                          // 이민 속도 배율(게임의 월간 인구 변화에 곱한다)
    int monthlyFamilies = 0;                     // 월 자연 이민 가족 수(0 = 게임 그대로. 넣으면 배율 대신 이 값이 쓰인다)
    int houseCapacity = 1;                       // 집의 수용 가족 수 배율(1 = 게임 그대로)
    int targetFamilies = 0;                      // 영지마다 최소 가족 수(0 = 끔)
    std::map<std::string, int> regionTargets;    // 영지별 값(0 = 그 영지 끔). 키가 없으면 공통 값을 따른다
};

struct ResourcesSettings {
    bool enabled = false;
    int intervalSec = 2;
    std::map<std::string, int> targets;                               // 공통 목표. 키가 없는 자원은 관리하지 않는다
    std::map<std::string, std::map<std::string, int>> regionTargets;  // 영지별 목표(키 = 영지 키). 없는 자원은 공통 목표를 따른다
};

// 커스텀 용병단 정의. units 는 분대마다 병종 id 하나(1~10개), region 은 내 영지 키(없으면 내 첫 영지),
// banner 는 깃발을 빌릴 순정 용병단 이름(없으면 카드가 들어간 칸의 것 그대로)
struct MercCompany {
    std::string name;
    std::vector<std::string> units;
    int cost = 0;
    std::optional<std::string> region;
    std::optional<std::string> banner;
    bool enabled = true;

    bool operator==(const MercCompany&) const = default;
};

// 건물 종류 하나의 저장 한도. 값이 없는 분류는 게임의 값을 그대로 둔다
struct StorageLimits {
    std::optional<int> generic;   // 일반 저장실
    std::optional<int> large;     // 목재 저장실
    std::optional<int> pantry;    // 식량 저장실

    bool empty() const { return !generic && !large && !pantry; }
    bool operator==(const StorageLimits&) const = default;
};

// 건물의 저장 용량. 패널에는 화면이 없고 설정만 보존한다(Panel.Core 의 StorageControl)
struct StorageSettings {
    bool enabled = false;
    int intervalSec = 5;
    std::map<std::string, StorageLimits> limits;   // 키 = 건물 종류 번호(게임의 건물 표의 행 이름)
};

struct MercSettings {
    bool enabled = false;
    bool refund = true;       // 내 용병단 고용비 환급 + 유지비 0
    bool lockFromAi = true;   // 커스텀 용병단은 고용 창이 닫혀 있는 동안 AI 가 살 수 없는 가격
    std::vector<MercCompany> companies;
};

class ControlDoc {
public:
    ControlDoc();
    // 깨졌거나 객체가 아니면 빈 문서. 옛 설정(자원 목표에 든 국고·영향력)을 영주 설정으로 옮긴다
    static ControlDoc parse(std::string_view text);
    // 해석하지 못하면 값 없음(파일이 깨졌는지 가릴 때 쓴다)
    static std::optional<ControlDoc> tryParse(std::string_view text);
    std::string dump() const;

    long long seq() const;
    void setSeq(long long seq);

    BuildSettings build() const;
    void setBuild(const BuildSettings& v);
    UpgradeSettings upgrade() const;
    void setUpgrade(const UpgradeSettings& v);
    LordSettings lord() const;
    void setLord(const LordSettings& v);
    MilitarySettings military() const;
    void setMilitary(const MilitarySettings& v);
    PopulationSettings population() const;
    void setPopulation(const PopulationSettings& v);
    ResourcesSettings resources() const;
    void setResources(const ResourcesSettings& v);
    MercSettings mercenaries() const;
    void setMercenaries(const MercSettings& v);
    StorageSettings storage() const;
    void setStorage(const StorageSettings& v);

    void setCommands(const std::vector<Json>& commands);

    Json& raw() { return root_; }
    const Json& raw() const { return root_; }
    Json& feature(const char* name);                   // 없으면 만든다
    const Json* findFeature(const char* name) const;   // 없거나 객체가 아니면 nullptr

private:
    Json root_;
};

// 읽기 도우미: 키가 없거나 형식이 다르면 기본값
bool boolOr(const Json* obj, const char* key, bool def);
int intOr(const Json* obj, const char* key, int def);
std::optional<int> optInt(const Json* obj, const char* key);
std::optional<std::string> optString(const Json* obj, const char* key);
const Json* objectAt(const Json* obj, const char* key);
const Json* arrayAt(const Json* obj, const char* key);
}
