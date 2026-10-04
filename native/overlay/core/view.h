#pragma once
#include "status_doc.h"
#include "units.h"
#include <initializer_list>
#include <optional>
#include <string>
#include <vector>

// 탭에 보일 문구와 선택지를 상태에서 만든다. 문구는 패널과 같다. ImGui 에 의존하지 않아 단위 테스트한다.
// 포인터 인자가 nullptr 이면 "모드가 그 정보를 주지 않았다"(기능이 꺼져 있거나 게임 밖)는 뜻이다.
namespace mlt::ov {

// 3000 -> "3,000"
std::string formatThousands(long long n);

// [군사] 병력 생성·재구성 위치. 내 영지 목록이 없으면 "내 첫 영지" 한 줄
std::vector<ScopeOption> spawnRegionOptions(const std::vector<RegionInfo>* regions);
// "해제된 생성 분대: 3개 (친위대 - 1단계 2, 민병대 - 농민 1) — 빈 카드 정리 중 1"
std::string reformText(const SpawnStatus* spawn);
// 재구성 버튼을 누를 수 있는가: 해제된 분대가 있고 빈 카드 정리가 끝났다
bool canReform(const SpawnStatus* spawn);
// "#63 친위대 - 1단계 ×36 (생성)"
std::string retinueLabel(const RetinueSquad& squad);

// [인구] 범위: "공통 (모든 내 영지)"과 영지들
std::vector<ScopeOption> populationScopeOptions(const PopulationStatus* population);
// 현재 상태 두 줄(정보가 없으면 한 줄). regionKey 가 없거나 그 영지를 모르면 모든 영지 합계
std::vector<std::string> populationInfo(const PopulationStatus* population, const std::optional<std::string>& regionKey);

// 네이티브 DLL 이 그 항목들을 모두 설치했는가. 값 없음 = 아직 모른다(상태를 못 받았거나 오래됐거나 항목 목록이 없다).
// DLL 을 올리지 못했으면 false
std::optional<bool> nativeInstalled(const NativeStatus* native, std::initializer_list<const char*> names);

// 화면에 보일 것을 재료가 바뀔 때만 다시 만든다(탭은 프레임마다 그려지고 재료는 1초에 한 번쯤 바뀐다).
// get(key, make): key 가 지난번과 같으면 지난 값을, 다르면 make() 의 값을 돌려준다
template <class Key, class Value>
class Memo {
public:
    template <class Make>
    const Value& get(const Key& key, Make&& make) {
        if (!key_ || !(*key_ == key)) {
            value_ = make();
            key_ = key;
        }
        return value_;
    }

private:
    std::optional<Key> key_;
    Value value_{};
};

// [자격·질서] 범위: "공통 (모든 내 영지)"과 영지들
std::vector<ScopeOption> moodScopeOptions(const MoodStatus* mood);
// 영지마다 지금 값 한 줄: "현재 Wilde Wand: 자격 100 · 공공질서 96"
std::vector<std::string> moodLines(const MoodStatus* mood);
// 네이티브 DLL 이 맡지 못한 일이 있을 때의 안내. 다 맡았거나 아직 모르면(native 없음) 빈 글
std::string moodNativeNote(const NativeStatus* native);

// [용병] 고용 창 상태. "고용 창: …", "고용 중: …", "띄우지 못함: 이름 — 이유", "참고: …"
std::vector<std::string> mercStatusLines(const MercenaryStatus* mercenaries);
}
