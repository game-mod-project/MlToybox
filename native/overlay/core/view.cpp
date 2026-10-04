#include "view.h"

namespace mlt::ov {

std::string formatThousands(long long n) {
    const bool negative = n < 0;
    std::string digits = std::to_string(negative ? -n : n);
    std::string out;
    for (size_t i = 0; i < digits.size(); ++i) {
        if (i > 0 && (digits.size() - i) % 3 == 0) out.push_back(',');
        out.push_back(digits[i]);
    }
    return negative ? "-" + out : out;
}

std::vector<ScopeOption> spawnRegionOptions(const std::vector<RegionInfo>* regions) {
    std::vector<ScopeOption> options;
    if (regions) {
        for (const RegionInfo& r : *regions) options.push_back({ r.key, regionLabel(r.name, r.key) });
    }
    if (options.empty()) options.push_back({ std::nullopt, "내 첫 영지" });
    return options;
}

std::string reformText(const SpawnStatus* spawn) {
    if (!spawn) return "해제된 생성 분대: -";
    std::string text = "해제된 생성 분대: " + std::to_string(spawn->disbanded) + "개";
    if (!spawn->byUnit.empty()) {
        text += " (";
        for (size_t i = 0; i < spawn->byUnit.size(); ++i) {
            if (i > 0) text += ", ";
            text += unitLabel(spawn->byUnit[i].first) + " " + std::to_string(spawn->byUnit[i].second);
        }
        text += ")";
    }
    if (spawn->pending > 0) text += " — 빈 카드 정리 중 " + std::to_string(spawn->pending);
    return text;
}

bool canReform(const SpawnStatus* spawn) {
    return spawn && spawn->disbanded > 0 && spawn->pending == 0;
}

std::string retinueLabel(const RetinueSquad& squad) {
    std::string kind = squad.kind;
    if (kind == "spawned") kind = "생성";
    else if (kind == "mercenary") kind = "용병";
    return "#" + std::to_string(squad.id) + " " + unitLabel(squad.unit) + " ×" + std::to_string(squad.count) + " (" + kind + ")";
}

std::vector<ScopeOption> populationScopeOptions(const PopulationStatus* population) {
    return commonAndRegionOptions(population ? &population->regions : nullptr);
}

std::vector<std::string> populationInfo(const PopulationStatus* population, const std::optional<std::string>& regionKey) {
    if (!population) return { "현재: - (게임에 들어가면 표시됩니다)" };
    const PopulationRegion* region = nullptr;
    if (regionKey) {
        for (const PopulationRegion& r : population->regions) {
            if (r.key == *regionKey) region = &r;
        }
    }
    const std::string scope = region ? region->name : "모든 내 영지 합계";
    const int families = region ? region->families : population->families;
    const int people = region ? region->population : population->population;
    const int homeless = region ? region->homeless : population->homeless;
    const int freeSlots = region ? region->freeSlots : population->freeSlots;
    const int unassigned = region ? region->unassigned : population->unassigned;
    // 네이티브가 배율을 맡으면 게임이 직접 들이므로 모드가 따로 들인 가족이 없다. 그때는 뒤의 말을 뺀다
    std::string session = "이번 세션(전체): 자연 이민 " + std::to_string(population->natural) + "가족";
    if (population->multiplied > 0) session += " → 배율로 추가 " + std::to_string(population->multiplied) + "가족";
    return {
        "현재(" + scope + "): 가족 " + std::to_string(families) + " · 인구 " + std::to_string(people) + " · 집 없는 가족 " + std::to_string(homeless)
            + " · 빈 자리 " + std::to_string(freeSlots) + " · 미배치 가족 " + std::to_string(unassigned),
        session,
    };
}

std::vector<ScopeOption> moodScopeOptions(const MoodStatus* mood) {
    return commonAndRegionOptions(mood ? &mood->regions : nullptr);
}

std::vector<std::string> moodLines(const MoodStatus* mood) {
    if (!mood) return { "현재: - (게임에 들어가면 표시됩니다)" };
    if (mood->regions.empty()) return { "현재: 내 영지가 없습니다" };
    std::vector<std::string> lines;
    for (const MoodRegion& r : mood->regions) {
        lines.push_back("현재 " + r.name + ": 자격 " + std::to_string(r.approval) + " · 공공질서 " + std::to_string(r.order));
    }
    return lines;
}

std::optional<bool> nativeInstalled(const NativeStatus* native, std::initializer_list<const char*> names) {
    if (!native) return std::nullopt;
    if (!native->loaded) return false;
    if (native->stale || native->features.empty()) return std::nullopt;   // 상태 파일을 아직 못 읽었거나 오래됐다
    for (const char* name : names) {
        const auto it = native->features.find(name);
        if (it == native->features.end() || !it->second.installed) return false;
    }
    return true;
}

std::string moodNativeNote(const NativeStatus* native) {
    // 계산 함수 둘을 맡지 못하면 배율을 걸 수 없다. 고정값은 Lua 가 써 넣어 유지한다
    const std::optional<bool> hooks = nativeInstalled(native, { "mood_approval", "mood_order" });
    if (!hooks) return "";
    if (!*hooks) {
        return "네이티브 DLL 이 게임의 계산을 맡지 못했습니다(게임이 업데이트됐을 수 있습니다). 배율은 적용되지 않고 고정값만 모드가 2초마다 다시 써 넣습니다. "
               "게임이 하루에 한 번 다시 계산할 때 잠깐 게임의 값이 보이고, 그 값이 낮으면 \"자격 매우 낮음\" 알림과 \"자격 낮음\" 문제가 날마다 생길 수 있습니다.";
    }
    std::string note;
    if (nativeInstalled(native, { "region_name", "region_tag" }) == false) {
        note += "영지를 가리는 게임 코드를 찾지 못해 영지별 설정이 온전히 적용되지 않습니다(게임이 다시 계산할 때는 공통 설정이 쓰입니다). ";
    }
    if (nativeInstalled(native, { "mood_problem_add", "mood_problem_remove" }) == false) note += "\"자격 낮음\" 문제 표시는 게임이 계산한 값을 따릅니다.";
    return note;
}

std::vector<std::string> mercStatusLines(const MercenaryStatus* m) {
    if (!m) return { "고용 창: - (용병 기능이 꺼져 있거나 게임 밖)" };
    std::string slots;
    for (const MercSlot& s : m->slots) {
        if (!slots.empty()) slots += ", ";
        slots += s.name + (s.custom ? "(커스텀)" : "") + " " + formatThousands(s.cost);
    }
    if (slots.empty()) slots = "(비어 있음)";
    std::vector<std::string> lines = {
        "고용 창: " + slots,
        "고용 중: 내 용병단 " + std::to_string(m->hiredMine) + "개, AI " + std::to_string(m->hiredAi) + "개 · 맵을 불러온 뒤 환급 " + formatThousands(m->refunded),
    };
    for (const MercSkipped& s : m->skipped) lines.push_back("띄우지 못함: " + s.name + " — " + s.reason);
    if (m->note && !m->note->empty()) lines.push_back("참고: " + *m->note);
    return lines;
}

}
