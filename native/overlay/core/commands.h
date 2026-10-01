#pragma once
#include "control_doc.h"
#include <string>

// 모드가 한 번만 실행하는 일회성 명령(control.json 의 commands). issuedAt 이 60초 넘게 지나면 모드가 버린다.
namespace mlt::ov {
// 32자리 16진수
std::string newCommandId();
// 영주 값(treasury / influence / kingsFavour)을 그 값으로 한 번 맞춘다
Json makeSetLord(const std::string& key, int value, long long nowEpochSeconds);
}
