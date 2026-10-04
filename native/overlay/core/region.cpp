#include "region.h"
#include "view.h"
#include <algorithm>

namespace mlt::ov {

const std::vector<DepositKind>& depositKinds() {
    static const std::vector<DepositKind> kinds = {
        { "Salt", "소금", true }, { "Iron", "철", true }, { "Clay", "점토", true }, { "Stone", "돌", false },
        { "Fish", "물고기", false }, { "Eel", "장어", false }, { "Berries", "열매", false }, { "Mushrooms", "버섯", false },
    };
    return kinds;
}

std::vector<ScopeOption> regionScopeOptions(const RegionStatus* status) {
    return commonAndRegionOptions(status ? &status->regions : nullptr);
}

namespace {
std::optional<int> find(const std::map<std::string, int>& map, const std::string& key) {
    const auto it = map.find(key);
    return it == map.end() ? std::nullopt : std::optional<int>(it->second);
}

// 매장지 하나: 다시 차는 것은 "양 / 용량", 나머지는 양
std::string one(const DepositInfo& d) {
    std::string text = formatThousands(d.amount);
    if (d.capacity && *d.capacity > 0) text += " / " + formatThousands(*d.capacity);
    return text;
}

std::string currentOf(const RegionStatus* status, const std::optional<std::string>& scope, const std::string& kind) {
    if (!status) return "-";
    if (scope) {
        std::string text;
        for (const RegionState& r : status->regions) {
            if (r.key != *scope) continue;
            for (const DepositInfo& d : r.deposits) {
                if (d.kind != kind) continue;
                if (!text.empty()) text += ", ";
                text += one(d);
            }
        }
        return text.empty() ? "-" : text;
    }
    long long sum = 0;
    int count = 0;
    for (const RegionState& r : status->regions) {
        for (const DepositInfo& d : r.deposits) {
            if (d.kind != kind) continue;
            sum += d.amount;
            ++count;
        }
    }
    if (count == 0) return "-";
    return formatThousands(sum) + " (" + std::to_string(count) + "곳)";
}
}

std::vector<DepositRow> buildDepositRows(const RegionSettings& settings, const RegionStatus* status, const std::optional<std::string>& scope) {
    const std::map<std::string, int>* own = nullptr;
    if (scope) {
        const auto it = settings.regionTargets.find(*scope);
        if (it != settings.regionTargets.end()) own = &it->second;
    }
    std::vector<DepositRow> rows;
    for (const DepositKind& kind : depositKinds()) {
        DepositRow row;
        row.key = kind.key;
        row.label = kind.label;
        row.mineral = kind.mineral;
        row.current = currentOf(status, scope, row.key);
        if (scope) {
            if (own) row.target = find(*own, row.key);
            row.inherited = find(settings.targets, row.key);
        } else {
            row.target = find(settings.targets, row.key);
        }
        rows.push_back(std::move(row));
    }
    return rows;
}

void setDepositTarget(RegionSettings& settings, const std::optional<std::string>& scope, const std::string& kind, std::optional<int> value) {
    if (value) value = std::clamp(*value, 0, kDepositTargetMax);
    if (!scope) {
        // 공통 목표의 0 은 "채우지 않는다"와 같다: 키를 지운다(1 로 맞춰 "1 을 유지"가 되지 않게)
        if (value && *value > 0) settings.targets[kind] = *value;
        else settings.targets.erase(kind);
        return;
    }
    if (value) {
        settings.regionTargets[*scope][kind] = *value;
        return;
    }
    const auto it = settings.regionTargets.find(*scope);
    if (it == settings.regionTargets.end()) return;
    it->second.erase(kind);
    if (it->second.empty()) settings.regionTargets.erase(it);
}

std::string livestockLine(const RegionStatus* status) {
    if (!status) return "가축 상인: - (게임에 들어가면 표시됩니다)";
    if (status->regions.empty()) return "가축 상인: 내 영지가 없습니다";
    std::string text = "가축 상인: ";
    for (size_t i = 0; i < status->regions.size(); ++i) {
        const RegionState& r = status->regions[i];
        if (i > 0) text += " · ";
        text += r.name + (r.livestockWait > 0 ? " 방문까지 " + std::to_string(r.livestockWait) + "일" : std::string(" 지금 주문 가능"));
    }
    return text;
}

std::string depositNativeNote(const NativeStatus* native) {
    const std::optional<bool> hooks = nativeInstalled(native, { "deposits_day", "deposits_nodes", "deposits_amount", "deposits_owner" });
    if (!hooks) return "";
    if (!*hooks) {
        return "네이티브 DLL 이 광물 매장지를 맡지 못했습니다(게임이 업데이트됐을 수 있습니다). 소금·철·점토는 채워지지 않고 지금 값도 보이지 않습니다. 나머지 종류는 그대로 동작합니다.";
    }
    if (nativeInstalled(native, { "region_name", "region_tag" }) == false) {
        return "영지를 가리는 게임 코드를 찾지 못해 소금·철·점토의 영지별 목표는 쓰이지 않고 공통 목표가 적용됩니다.";
    }
    return "";
}

}
