#pragma once
#include "control_doc.h"
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// [건설] 탭의 저장 용량 표. 줄은 건물 종류, 칸은 저장실(일반·목재·식량)의 한도다.
// 설정값은 그 종류의 기본 한도를 대신한다. 모드가 내 영지의 건물마다 적용한다(features/storage.lua).
namespace mlt::ov {

// 저장실의 종류. 번호는 표의 열 순서와 배열의 자리다(게임의 EStorageType 과 같다)
enum class StorageKind { Generic = 0, Large = 1, Pantry = 2 };
inline constexpr int kStorageKinds = 3;
// 넣을 수 있는 가장 큰 한도(모드의 features/storage.lua 의 MAX 와 같다)
inline constexpr int kStorageLimitMax = 1000000;

// 건물 종류 하나. 모드가 시작할 때 bridge/catalog.json 의 buildings 에 쓴다(features/storage_catalog.lua)
struct BuildingInfo {
    std::string id;                  // 건물 종류 번호. 설정의 키
    std::string name;                // 게임의 한글 이름
    std::array<int, 3> defaults{};   // 게임의 기본 한도: 일반, 목재, 식량. 0 = 그 저장실이 없다
};

class BuildingCatalog {
public:
    // {"buildings":[{"id","name","generic","large","pantry"}…]}. 깨졌거나 건물 목록이 없으면 빈 목록
    static BuildingCatalog parse(std::string_view json);
    const std::vector<BuildingInfo>& buildings() const { return buildings_; }
    bool empty() const { return buildings_.empty(); }

private:
    std::vector<BuildingInfo> buildings_;
};

struct StorageRow {
    std::string id;
    std::string name;
    std::array<int, 3> defaults{};
    std::array<std::optional<int>, 3> values;   // 설정값. 없으면 게임의 값 그대로
    int order = 0;                              // 모드가 적은 순서(저장 건물이 먼저)

    bool operator==(const StorageRow&) const = default;
};

std::optional<int>& limitOf(StorageLimits& limits, StorageKind kind);

// 표의 줄: 모드가 알려 준 건물 목록의 순서. 목록에 없는 건물의 설정값은 줄로 보이지 않을 뿐 설정에는 남는다.
// 목록을 아직 읽지 못했으면(catalog 가 없으면) 빈 표
std::vector<StorageRow> buildStorageRows(const BuildingCatalog* catalog, const StorageSettings& settings);
// 이름이나 번호에 그 글이 들어 있는 줄. 앞뒤 공백은 빼고, 비어 있으면 전부
std::vector<StorageRow> filterStorageRows(const std::vector<StorageRow>& rows, std::string_view search);

// 표의 열. 번호는 화면의 열 순서와 같다
enum class StorageColumn { Name = 0, Generic = 1, Large = 2, Pantry = 3 };
// column 이 없으면 모드가 적은 순서. 숫자 열은 기본 한도로 정렬한다(값을 고치는 동안 줄이 자리를 옮기지 않게).
// 그 저장실이 없는 건물은 오름차순에서도 내림차순에서도 뒤에 둔다
void sortStorageRows(std::vector<StorageRow>& rows, std::optional<StorageColumn> column, bool descending);

// 칸 하나를 고친다. 값을 모두 지운 건물은 설정에서 뺀다
void setStorageLimit(StorageSettings& settings, const std::string& id, StorageKind kind, std::optional<int> value);
// 보이는 줄(shown)의, 그 건물에 있는 저장실마다 기본 한도의 multiplier 배를 넣는다
void fillStorageLimits(StorageSettings& settings, const std::vector<StorageRow>& shown, int multiplier);
// 보이는 줄의 값을 지운다. filtered 가 거짓이면(추리지 않았으면) 모두 지운다(표에 보이지 않는 건물의 값 포함)
void clearStorageLimits(StorageSettings& settings, const std::vector<StorageRow>& shown, bool filtered);
}
