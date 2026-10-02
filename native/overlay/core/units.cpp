#include "units.h"

namespace mlt::ov {

const std::vector<UnitOption>& units() {
    static const std::vector<UnitOption> list = {
        { "militia", "민병대 - 농민" },
        { "spearMilitia", "민병대 - 창" },
        { "militiaPole", "민병대 - 장창" },
        { "militiaFoot", "민병대 - 보병" },
        { "bowMilitia", "민병대 - 활" },
        { "crossbowMilitia", "민병대 - 석궁" },
        { "retinue_tier1", "친위대 - 1단계" },
        { "retinue_tier3", "친위대 - 3단계" },
        { "mercenary_spearmen", "용병 - 창병" },
        { "mercenary_infantry", "용병 - 보병" },
        { "Mercenary_Archers", "용병 - 궁수" },
        { "mercenary_heavy_archers", "용병 - 중궁수" },
        { "mercenary_crossbowmen", "용병 - 석궁병" },
    };
    return list;
}

std::string unitLabel(std::string_view id) {
    for (const UnitOption& u : units()) {
        if (u.id == id) return u.label;
    }
    return std::string(id);
}

bool isKnownUnit(std::string_view id) {
    for (const UnitOption& u : units()) {
        if (u.id == id) return true;
    }
    return false;
}

std::string regionLabel(const std::string& name, const std::string& key) {
    return name + " (" + key + ")";
}

int indexOfKey(const std::vector<ScopeOption>& options, const std::optional<std::string>& key) {
    for (size_t i = 0; i < options.size(); ++i) {
        if (options[i].key == key) return static_cast<int>(i);
    }
    return 0;
}

}
