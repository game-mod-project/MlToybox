#pragma once
#include "hooks.h"
#include <cstddef>
#include <cstdint>

// 게임 버그 방어: AI 지역 민병대 평가 함수(APawnCPP 멤버, 이름 없음)가 분대원 Home 을 null 검사 없이 읽는다.
// 민병대원 가족이 집을 잠깐 비운 순간(Home == null) 호출되면 [null+0xCA8] 읽기로 크래시한다(모드 없이도 재현, buildid 24905706).
// 위험한 순간에는 호출을 건너뛴다. 원본이 쿨다운(region+0x7C8)을 세우기 전이므로 다음 AI 틱에 다시 실행된다.
namespace mlt::militia_guard {
constexpr const char* kPattern = "48 8B C4 48 89 50 10 48 89 48 08 55 53 48 8D 68 A8 48 81 EC 48 01 00 00 80 BA E8 02 00 00 00";
constexpr std::ptrdiff_t kPawnEngineOffset = 0x340;        // APawnCPP::masterPtr (ARTSMultiEngineCPP*)
constexpr std::ptrdiff_t kPawnCommandedOffset = 0xA10;     // APawnCPP::commandedSquads (TArray<int32>) Data
constexpr std::ptrdiff_t kPawnCommandedNumOffset = 0xA18;
constexpr std::ptrdiff_t kEngineSquadsOffset = 0x5B0;      // ARTSMultiEngineCPP::squads (TArray<FSquad>) Data
constexpr std::ptrdiff_t kEngineSquadsNumOffset = 0x5B8;
constexpr std::ptrdiff_t kSquadSize = 0x3E8;
constexpr std::ptrdiff_t kSquadTypeOffset = 0x10;          // ESquadType
constexpr std::ptrdiff_t kSquadOriginOffset = 0x1F8;       // originRegion
constexpr std::ptrdiff_t kSquadRecruitsOffset = 0x348;     // assignedRecruits (TArray<ASMUnit*>) Data
constexpr std::ptrdiff_t kSquadRecruitsNumOffset = 0x350;
constexpr std::ptrdiff_t kUnitHomeOffset = 0x340;          // ASMUnit::Home
constexpr uint8_t kMilitiaType = 1;
constexpr int32_t kMaxCount = 100000;                      // 비정상 Num 방어

// 원본이 순회할 분대원 모두가 Home 을 가졌으면 true. 레이아웃이 이상하면 false(건너뛰는 쪽이 안전).
bool isSafe(const uint8_t* pawn, const uint8_t* region);
void registerHook(HookManager& manager);
}
