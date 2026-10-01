#pragma once
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

class ControlDoc {
public:
    ControlDoc();
    // 깨졌거나 객체가 아니면 빈 문서. 옛 설정(자원 목표에 든 국고·영향력)을 영주 설정으로 옮긴다
    static ControlDoc parse(std::string_view text);
    std::string dump() const;

    long long seq() const;
    void setSeq(long long seq);

    BuildSettings build() const;
    void setBuild(const BuildSettings& v);
    UpgradeSettings upgrade() const;
    void setUpgrade(const UpgradeSettings& v);
    LordSettings lord() const;
    void setLord(const LordSettings& v);

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
const Json* objectAt(const Json* obj, const char* key);
}
