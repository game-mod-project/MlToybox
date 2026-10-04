#pragma once
#include "hooks.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// 광물 매장지(소금·철·점토)의 남은 양을 내 영지에서만 목표까지 채운다 (buildid 24905706, findings "자원 매장지 — 구조와 값 쓰기 조사").
// 남은 양은 매장지 객체의 리플렉션에 없는 자리에 있어 Lua 로는 읽거나 쓸 수 없다.
// 날짜가 넘어갈 때의 게임 함수를 후킹해, 그 뒤에 엔진의 매장지 목록을 돌며 채우고 지금 값을 상태 파일에 적는다(Lua 가 읽어 탭에 보인다).
// 버섯·열매·물고기·장어·돌은 덩어리마다 양이 있고 리플렉션에 보여 Lua 가 맡는다(features/region.lua).
namespace mlt::deposits {
// void AWeatherMaster 의 날짜가 넘어갈 때의 처리(구현 0x144D930D0)
constexpr const char* kNewDayPattern = "40 55 41 55 41 56 48 8D AC 24 D0 FD FF FF 48 81 EC 30 03 00 00";
// 영지의 발전 특전을 그 영지의 매장지에 반영하는 영지 함수(구현 0x144BC3C10). 엔진의 매장지 목록과 매장지의 필드 오프셋을 본문으로 확인하려고 주소만 찾는다
constexpr const char* kNodeLayoutPattern = "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 48 83 EC 20 48 8B 59 20 33 F6";
// 세이브를 불러올 때 광물 매장지에 남은 양을 넣는 명령들(0x144C1B9CF):
// cmp dword [r8+2B8h],0; jg; cmp dword [r8+2C8h],0; jne; mov [r8+310h],edx. 함수의 시작이 아니라 이 자리를 찾아 세 오프셋을 확인한다
constexpr const char* kAmountWritePattern =
    "41 83 B8 B8 02 00 00 00 0F 8F ?? ?? ?? ?? 41 83 B8 C8 02 00 00 00 0F 85 ?? ?? ?? ?? 41 89 90 10 03 00 00";
// 영지의 주인과 그 주인이 플레이어인지를 읽는 영지 함수(0x144BC2ED0). 두 오프셋을 확인하려고 주소만 찾는다
constexpr const char* kOwnerCheckPattern =
    "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 48 89 7C 24 20 41 54 41 56 41 57 48 83 EC 20 4C 8D A1 68 0D 00 00";
// bool FName == const char* (구현 0x14124BB30). 영지의 태그를 설정의 영지 키와 견줄 때 부른다. 주소만 찾는다
constexpr const char* kNameEqualsPattern =
    "48 89 5C 24 18 48 89 74 24 20 57 48 83 EC 20 48 8B F2 48 8B D9 48 85 D2 0F 84 ?? ?? ?? ?? 48 C7 C7 FF FF FF FF 48 FF C7 80 3C 3A 00";

constexpr std::ptrdiff_t kWeatherEngineOffset = 0x2A8;     // 날씨 객체가 든 엔진 포인터
constexpr std::ptrdiff_t kEngineNodesOffset = 0x700;       // 엔진의 매장지 목록(TArray<AResourceNode*>: 포인터, +8 에 개수)
constexpr std::ptrdiff_t kNodeTypeOffset = 0x2A8;          // ENodeType(int32)
constexpr std::ptrdiff_t kNodeRegionOffset = 0x2B0;        // ARegion*
constexpr std::ptrdiff_t kNodeSquadOffset = 0x2B8;         // 동물 무리 번호(무리가 아니면 0 이하)
constexpr std::ptrdiff_t kNodeClumpCountOffset = 0x2C8;    // resourceClumps 의 개수
constexpr std::ptrdiff_t kNodeAmountOffset = 0x310;        // 남은 양(int32). 덩어리가 없고 무리가 아닌 매장지만 쓴다
constexpr std::ptrdiff_t kRegionOwnerOffset = 0x350;       // APawnCPP* ARegion::ownerPawn
constexpr std::ptrdiff_t kPawnIsMainPlayerOffset = 0x34C;  // bool APawnCPP::isMainPlayer
constexpr std::ptrdiff_t kRegionTagOffset = 0x2B0;         // FName ARegion::regionUniqueTag
constexpr int32_t kSalt = 1, kIron = 2, kClay = 3;         // ENodeType
constexpr int kMaxTarget = 1000000;
constexpr int32_t kMaxNodes = 4096;                        // 이보다 많은 매장지는 레이아웃이 바뀐 것으로 본다

bool isMineral(int32_t type);

// 그 종류의 목표. 영지 목표(region)가 있고 그 종류가 정해져 있으면(0 이상) 그것, 아니면 공통 목표. 광물이 아니면 0
int targetOf(const MineralTargets& common, const MineralTargets* region, int32_t type);

// 남은 양이 목표보다 적으면 목표(상한 kMaxTarget). 목표가 없으면(0 이하) 그대로
int32_t topUp(int32_t amount, int target);

// 내 영지의 광물 매장지 하나의 지금 값
struct NodeInfo {
    std::uintptr_t region = 0;   // 영지 객체의 주소(Lua 가 자기 영지의 주소와 견준다)
    int32_t type = 0;
    int32_t amount = 0;
    bool operator==(const NodeInfo&) const = default;
};

// isRegion(영지, 키): 그 영지의 태그가 키와 같은가. 없으면(이름 비교 함수를 못 찾았으면) 공통 목표만 쓴다
using IsRegionFn = std::function<bool(const uint8_t* region, const std::string& key)>;

// 엔진의 매장지 목록을 돌아, 주인이 플레이어인 영지의 광물 매장지를 목표까지 채우고 채운 뒤의 값을 out 에 모은다.
// 목록의 개수나 포인터가 말이 안 되면 아무것도 하지 않고 false
bool process(const uint8_t* engine, const RegionControl& control, const IsRegionFn& isRegion, std::vector<NodeInfo>& out);

// native_status.json 에 끼울 조각: "nodes":[{"region":"<주소 16진수>","type":2,"amount":1154},…]. 매장지가 없으면 빈 글
std::string renderNodes(const std::vector<NodeInfo>& nodes);
// 마지막으로 날짜가 넘어갔을 때 모은 것을 위 꼴로. 기능이 꺼져 있으면 빈 글
std::string snapshotJson();

void registerHook(HookManager& manager);
}
