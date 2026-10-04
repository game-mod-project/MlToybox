#pragma once
#include "hooks.h"
#include <cstddef>
#include <cstdint>
#include <string>

// 영지(ARegion)를 가리는 일에 여러 기능이 함께 쓰는 것: 주인이 플레이어인가, 설정의 영지 키와 같은 영지인가 (buildid 24905706).
// 게임이 업데이트되면 여기의 패턴과 오프셋만 고치면 된다(기능마다 자기 헤더에 복사해 두지 않는다).
namespace mlt::region_scope {
// 영지의 주인과 그 주인이 플레이어인지를 읽는 영지 함수(0x144BC2ED0. 영지의 문제 목록에 문제를 넣는 함수다).
// 기능마다 자기 이름의 항목으로 이 함수를 찾아 두 오프셋을 확인한다
constexpr const char* kOwnerCheckPattern =
    "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 48 89 7C 24 20 41 54 41 56 41 57 48 83 EC 20 4C 8D A1 68 0D 00 00";
// mov rax,[rbp+350h]; test rax,rax; je; cmp byte [rax+34Ch],0   영지의 주인과 isMainPlayer
constexpr const char* kOwnerCheckBody = "48 8B 85 50 03 00 00 48 85 C0 74 10 80 B8 4C 03 00 00 00";
constexpr size_t kOwnerCheckWindow = 0x140;

// bool FName == const char* (구현 0x14124BB30). 영지의 태그를 설정의 영지 키와 견줄 때 부른다
constexpr const char* kNameEqualsPattern =
    "48 89 5C 24 18 48 89 74 24 20 57 48 83 EC 20 48 8B F2 48 8B D9 48 85 D2 0F 84 ?? ?? ?? ?? 48 C7 C7 FF FF FF FF 48 FF C7 80 3C 3A 00";
constexpr const char* kNameEqualsBody = "80 3A 5F";   // cmp byte [rdx],5Fh   글자열의 첫 글자를 본다(두 번째 인수가 const char*)
constexpr size_t kNameEqualsWindow = 0x60;

// 세이브를 만들 때 영지의 이름(FString, +0x2B8·+0x2C0)과 자격(+0x1068)을 옮겨 적는 명령들(0x144AFD610):
// mov eax,[r15+2C0h]; test eax,eax; mov rdx,[r15+2B8h]; lea r8d,[rax-1]; cmove r8d,r14d; call …; mov eax,[r15+1068h].
// 태그(+0x2B0)를 직접 읽는 게임 코드는 찾지 못했다. 이름은 태그 바로 뒤의 필드라, 이 자리가 맞으면 태그의 자리도 맞다고 본다
constexpr const char* kRegionNamePattern =
    "41 8B 87 C0 02 00 00 85 C0 49 8B 97 B8 02 00 00 44 8D 40 FF 45 0F 44 C6 E8 ?? ?? ?? ?? 41 8B 87 68 10 00 00";

constexpr std::ptrdiff_t kRegionOwnerOffset = 0x350;        // APawnCPP* ARegion::ownerPawn
constexpr std::ptrdiff_t kPawnIsMainPlayerOffset = 0x34C;   // bool APawnCPP::isMainPlayer
constexpr std::ptrdiff_t kRegionTagOffset = 0x2B0;          // FName ARegion::regionUniqueTag (8바이트: 이름 번호, 숫자 꼬리)

// 영지의 주인이 플레이어인가
bool ownedByMainPlayer(const uint8_t* region);

// 이름 표의 번호로 말이 되는 값인가(0 이 아니고, 이름 표의 블록 범위 안). 잘못 읽은 값을 게임 함수에 넘기지 않으려고 본다
bool plausibleName(uint64_t name);

// 영지를 가릴 수 있는가: 이름 비교 함수와 태그의 자리를 둘 다 확인했다
bool canTellRegions();
// 그 영지의 태그가 key 와 같은가. 가릴 수 없거나 태그 값이 말이 안 되면 false
bool tagIs(const uint8_t* region, const std::string& key);

// 주소만 찾는 항목 둘: region_name(이름 비교 함수), region_tag(태그의 자리 확인)
void registerHook(HookManager& manager);
}
