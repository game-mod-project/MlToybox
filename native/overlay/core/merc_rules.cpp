#include "merc_rules.h"
#include "text.h"
#include <algorithm>
#include <map>

namespace mlt::ov {

const std::vector<std::string>& vanillaMercNames() {
    // findings "용병 고용 — 목록 보충과 커스텀 용병단"
    static const std::vector<std::string> names = {
        "battle_brothers", "brigands", "brigands_small", "brotherhood_of_the_forest", "crazy_goose", "greencaps",
        "hildebolts_army", "hildebolts_army_large", "huntsmen", "vultures", "wayward_sons",
    };
    return names;
}

bool isVanillaMercName(std::string_view name) {
    const std::string lower = lowerAscii(name);
    const auto& names = vanillaMercNames();
    return std::find(names.begin(), names.end(), lower) != names.end();
}

std::optional<std::string> normalizeBanner(const std::optional<std::string>& banner) {
    if (!banner) return std::nullopt;
    const std::string lower = lowerAscii(trim(*banner));
    const auto& names = vanillaMercNames();
    if (std::find(names.begin(), names.end(), lower) == names.end()) return std::nullopt;
    return lower;
}

std::vector<ScopeOption> bannerOptions() {
    std::vector<ScopeOption> options = { { std::nullopt, kInheritBannerLabel } };
    for (const std::string& name : vanillaMercNames()) options.push_back({ name, name });
    return options;
}

std::vector<ScopeOption> mercRegionOptions(const std::vector<RegionInfo>& regions, const std::vector<std::optional<std::string>>& keep) {
    std::vector<ScopeOption> options = { { std::nullopt, kFirstRegionLabel } };
    for (const RegionInfo& r : regions) options.push_back({ r.key, regionLabel(r.name, r.key) });
    for (const auto& key : keep) {
        if (!key) continue;
        const bool listed = std::any_of(options.begin(), options.end(), [&](const ScopeOption& o) { return o.key == key; });
        if (!listed) options.push_back({ *key, *key });   // 이름을 모르는 영지는 키로 보여 준다
    }
    return options;
}

std::optional<std::string> validateCompany(const MercCompany& c, const std::vector<MercCompany>& all, int self) {
    const std::string name = trim(c.name);
    if (name.empty()) return "이름을 입력하세요.";
    if (codePointCount(name) > static_cast<size_t>(kMercNameMax)) return "이름은 " + std::to_string(kMercNameMax) + "자 이하여야 합니다.";
    if (isVanillaMercName(name)) return "게임의 용병단 이름과 겹칩니다.";
    for (size_t i = 0; i < all.size(); ++i) {
        if (static_cast<int>(i) != self && equalsIgnoreCaseAscii(trim(all[i].name), name)) return "같은 이름의 용병단이 이미 있습니다.";
    }
    if (c.units.empty() || c.units.size() > static_cast<size_t>(kMercMaxSquads)) return "분대는 1~" + std::to_string(kMercMaxSquads) + "개여야 합니다.";
    for (const std::string& unit : c.units) {
        if (!isKnownUnit(unit)) return "쓸 수 없는 병종입니다: " + unit;
    }
    if (c.cost < 0) return "고용비는 0 이상이어야 합니다.";
    return std::nullopt;
}

std::string unitSummary(const std::vector<std::string>& units) {
    std::vector<std::string> order;
    std::map<std::string, int> counts;
    for (const std::string& unit : units) {
        if (counts[unit]++ == 0) order.push_back(unit);
    }
    std::string out;
    for (const std::string& unit : order) {
        if (!out.empty()) out += ", ";
        out += unitLabel(unit) + " × " + std::to_string(counts[unit]);
    }
    return out;
}

bool canEnableCompany(const std::vector<MercCompany>& all, int self) {
    int enabled = 0;
    for (size_t i = 0; i < all.size(); ++i) {
        if (static_cast<int>(i) != self && all[i].enabled) ++enabled;
    }
    return enabled < kMercMaxEnabled;
}

MercRegistration registerCompany(std::vector<MercCompany>& all, int editing, MercCompany draft) {
    const bool isNew = editing < 0 || editing >= static_cast<int>(all.size());
    if (isNew) editing = -1;
    draft.name = trim(draft.name);
    draft.banner = normalizeBanner(draft.banner);
    draft.enabled = isNew ? canEnableCompany(all, -1) : all[static_cast<size_t>(editing)].enabled;
    MercRegistration result;
    if (auto reason = validateCompany(draft, all, editing)) {
        result.message = *reason;
        return result;
    }
    result.ok = true;
    result.message = "등록했습니다.";
    if (isNew) {
        if (!draft.enabled) result.message = "사용 중인 용병단이 " + std::to_string(kMercMaxEnabled) + "개라 '사용'을 끈 채로 등록했습니다.";
        all.push_back(std::move(draft));
        result.index = static_cast<int>(all.size()) - 1;
    } else {
        all[static_cast<size_t>(editing)] = std::move(draft);
        result.index = editing;
    }
    return result;
}

std::optional<std::string> setCompanyEnabled(std::vector<MercCompany>& all, int index, bool enabled) {
    if (index < 0 || index >= static_cast<int>(all.size())) return "없는 용병단입니다.";
    if (enabled && !canEnableCompany(all, index)) return "사용은 최대 " + std::to_string(kMercMaxEnabled) + "개입니다.";
    all[static_cast<size_t>(index)].enabled = enabled;
    return std::nullopt;
}

}
