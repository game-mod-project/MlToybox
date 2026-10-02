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
    std::vector<ScopeOption> options = { { std::nullopt, "공통 (모든 내 영지)" } };
    if (population) {
        for (const PopulationRegion& r : population->regions) options.push_back({ r.key, regionLabel(r.name, r.key) });
    }
    return options;
}

std::vector<std::string> populationInfo(const PopulationStatus* population, const std::optional<std::string>& regionKey) {
    if (!population) return { "현재: - (인구 기능이 꺼져 있거나 게임 밖)" };
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
    return {
        "현재(" + scope + "): 가족 " + std::to_string(families) + " · 인구 " + std::to_string(people) + " · 집 없는 가족 " + std::to_string(homeless)
            + " · 빈 자리 " + std::to_string(freeSlots) + " · 미배치 가족 " + std::to_string(unassigned),
        "이번 세션(전체): 자연 이민 " + std::to_string(population->natural) + "가족 → 배율로 추가 " + std::to_string(population->multiplied) + "가족",
    };
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
