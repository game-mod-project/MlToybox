#pragma once
#include "control_doc.h"
#include <optional>
#include <string>

// 모드가 한 번만 실행하는 일회성 명령(control.json 의 commands). issuedAt 이 60초 넘게 지나면 모드가 버린다.
// region 은 영지 키다. 값이 없으면 키를 쓰지 않는다(패널과 같다. 모드가 정한 기본 영지를 쓴다)
namespace mlt::ov {
// 32자리 16진수
std::string newCommandId();
// 영주 값(treasury / influence / kingsFavour)을 그 값으로 한 번 맞춘다
Json makeSetLord(const std::string& key, int value, long long nowEpochSeconds);
// 병종 unit 의 분대를 count 개 만든다. region 이 없으면 내 첫 영지
Json makeSpawnSquads(const std::string& unit, int count, long long nowEpochSeconds, const std::optional<std::string>& region);
// 해제돼 빈 카드가 된 생성 분대를 같은 병종으로 다시 만든다
Json makeReformSquads(long long nowEpochSeconds, const std::optional<std::string>& region);
// 모드가 만든 수행원 분대(squadId)에 게임의 꾸미기 화면을 연다. region 은 화면을 열 영주 저택의 영지
Json makeCustomizeRetinue(int squadId, long long nowEpochSeconds, const std::optional<std::string>& region);
// 가족을 count 만큼 들인다. region 이 없으면 빈 자리가 많은 영지부터
Json makeAddFamilies(int count, long long nowEpochSeconds, const std::optional<std::string>& region);
}
