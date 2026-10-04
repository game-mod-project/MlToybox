#pragma once
#include "features/region_scope.h"
#include "hooks.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <vector>

// 자격(Approval)과 공공질서를 내 영지에서만 조절한다 (buildid 24905706, findings "자격·공공질서 — 게임의 방식").
// 게임은 영지마다 하루에 한 번 두 값을 다시 계산한다: 자격 = 50 + 요인의 합, 공공질서 = 100 + 요인의 합(올림, 0~100).
// 그 계산 함수 둘을 후킹해, 게임이 값을 쓴 직후에 주인이 플레이어인 영지의 값만 다시 정한다.
// 같은 날의 뒤따르는 계산(공공질서의 "자격 낮음" 요인, 월간 인구 변화)은 바뀐 값을 읽는다.
namespace mlt::mood {
// void ARegion::updateApproval() (구현 0x144BF21D0)
constexpr const char* kApprovalPattern = "40 55 53 57 41 55 48 8D AC 24 48 FF FF FF 48 81 EC B8 01 00 00";
// void ARegion::updatePublicOrder() (구현 0x144BD7FA0)
constexpr const char* kOrderPattern = "48 8B C4 48 89 58 20 55 56 57 41 56 41 57 48 8D 68 A1";
// void ARegion::addProblem(EProblem, void*, void*) (구현 0x144BC2ED0). 후킹하지 않고 주소만 찾는다.
// 다른 기능이 영지의 주인 오프셋을 확인할 때 찾는 함수와 같다
constexpr const char* kAddProblemPattern = region_scope::kOwnerCheckPattern;
// void ARegion::removeProblem(EProblem, void*, void*) (구현 0x144BF1C40). 주소만 찾는다
constexpr const char* kRemoveProblemPattern = "48 83 EC 28 4D 8B D9 45 33 D2";

// 한 값(자격 또는 공공질서)의 자리. 요인 목록은 TArray<FApprovalMemory>(원소 0x18, 효과는 +0x14)다
struct StatLayout {
    std::ptrdiff_t value;      // int32 값
    std::ptrdiff_t factors;    // 요인 목록(데이터 포인터, +8 에 개수)
    float base;                // 요인이 없을 때의 값
    bool policies;             // 정책 효과 목록도 더하는가
};
constexpr StatLayout kApproval{ 0x1068, 0x1070, 50.0f, true };   // ARegion::Approval, summedApprovalFactors
constexpr StatLayout kOrder{ 0x1084, 0x1098, 100.0f, false };    // ARegion::publicOrder, summedPublicOrderFactors
constexpr std::ptrdiff_t kFactorSize = 0x18, kFactorEffectOffset = 0x14;
constexpr std::ptrdiff_t kRegionPoliciesOffset = 0xCF8;          // 정책 효과 목록(리플렉션에 없음). 원소 0xC, 효과는 +4
constexpr std::ptrdiff_t kPolicySize = 0xC, kPolicyEffectOffset = 4;
constexpr std::ptrdiff_t kRegionOwnerOffset = region_scope::kRegionOwnerOffset;             // 이 기능의 본문 검사로도 확인한다
constexpr std::ptrdiff_t kPawnIsMainPlayerOffset = region_scope::kPawnIsMainPlayerOffset;
constexpr int kMaxGood = 10;
constexpr int kLowApproval = 25;          // 게임이 "자격 낮음" 문제를 넣는 경계(이 값 미만)
constexpr uint8_t kProblemLowApproval = 38;   // EProblem::LowApproval
constexpr int32_t kMaxFactors = 4096;     // 이보다 많은 요인은 레이아웃이 바뀐 것으로 본다

// 게임과 같은 올림(FMath::CeilToInt)
int ceilToInt(float v);

// 게임이 계산하는 값: clamp(올림(base + Σ effects), 0, 100)
int vanilla(float base, std::span<const float> effects);

// 배율을 건 값: 오르는 요인에는 good(1~kMaxGood)을, 깎이는 요인에는 bad(0~100)%를 곱해 같은 방식으로 더한다
int adjusted(float base, std::span<const float> effects, const MoodStat& s);

// 영지에 쓸 값. game 은 게임이 방금 쓴 값이다.
//  고정값이 있으면 그 값(1~100).
//  아니면 배율: 요인 목록을 읽었고(readable) 우리 계산이 게임 값과 같을 때만 적용한다. 다르면 모르는 요인이 있다는 뜻이라 게임 값을 둔다.
int decide(int game, float base, std::span<const float> effects, bool readable, const MoodStat& s);

// 영지의 요인 효과를 out 에 모은다(자격은 정책 효과까지). 목록의 개수나 포인터가 말이 안 되면 false
bool readEffects(const uint8_t* region, const StatLayout& layout, std::vector<float>& out);

// 그 영지에 쓸 설정. isRegion(key) 가 참인 영지 설정이 있으면 그것, 없으면(또는 영지를 가릴 수 없으면) 공통 설정
const MoodSet& pick(const MoodControl& control, const std::function<bool(const std::string&)>& isRegion);

struct Outcome { bool updated = false; int game = 0; int value = 0; };
using UpdateFn = void(__fastcall*)(void* region);
// 게임의 갱신 함수를 부르고, 게임이 값을 썼으면 설정대로 다시 정한다.
// 부르기 전에 값 자리에 표시(-1)를 넣는다: 게임이 계산을 건너뛴 것(가족이 없는 영지)을 알아보고,
// 자격 함수가 "지난 값"으로 모드가 올려 둔 값을 읽어 날마다 "자격 매우 낮음" 알림을 띄우지 않게 한다.
Outcome runUpdate(uint8_t* region, const StatLayout& layout, const MoodStat& stat, UpdateFn original);

enum class ProblemAction { None, Add, Remove };
// 게임 값과 바꾼 값이 "자격 낮음" 경계의 서로 다른 쪽이면, 바꾼 값에 맞게 문제를 넣거나 뺀다
ProblemAction problemAction(int game, int value);

void registerHook(HookManager& manager);
}
