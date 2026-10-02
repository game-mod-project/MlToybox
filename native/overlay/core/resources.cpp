#include "resources.h"
#include <set>
#include <utility>

namespace mlt::ov {

bool isLordWide(std::string_view id) {
    return id == "Treasury" || id == "Influence";
}

std::vector<ScopeOption> resourceScopeOptions(const StatusDoc* status) {
    std::vector<ScopeOption> options = { { std::nullopt, "공통 (모든 내 영지, 현재=합계)" } };
    if (status && status->regions) {
        for (const RegionResources& r : *status->regions) options.push_back({ r.key, regionLabel(r.name, r.key) });
    }
    return options;
}

const std::map<std::string, double>* resourceCurrent(const StatusDoc* status, const std::optional<std::string>& key) {
    if (!status) return nullptr;
    if (!key) return status->resources ? &*status->resources : nullptr;
    if (!status->regions) return nullptr;
    for (const RegionResources& r : *status->regions) {
        if (r.key == *key) return &r.values;
    }
    return nullptr;
}

std::map<std::string, int> resourceTargets(const ResourcesSettings& settings, const std::optional<std::string>& key) {
    if (!key) return settings.targets;
    auto it = settings.regionTargets.find(*key);
    return it == settings.regionTargets.end() ? std::map<std::string, int>() : it->second;
}

void storeResourceTargets(ResourcesSettings& settings, const std::optional<std::string>& key, std::map<std::string, int> targets) {
    if (!key) settings.targets = std::move(targets);
    else if (targets.empty()) settings.regionTargets.erase(*key);
    else settings.regionTargets[*key] = std::move(targets);
}

std::vector<ResourceRow> buildResourceRows(const StatusDoc* status, const ResourcesSettings& settings, const std::optional<std::string>& key) {
    const std::map<std::string, double>* current = resourceCurrent(status, key);
    const std::map<std::string, int> targets = resourceTargets(settings, key);
    std::set<std::string> ids;
    const bool listed = status && status->resourceIds && !status->resourceIds->empty();
    if (listed) ids.insert(status->resourceIds->begin(), status->resourceIds->end());
    if (current) {
        for (const auto& entry : *current) ids.insert(entry.first);
    }
    // 모드가 자원 목록을 알려 주면 그 목록이 표다. 목표만 남아 있고 목록에 없는 자원(게임이 더 쓰지 않아 뺀 것)은 보이지 않는다.
    // 목록이 없을 때(게임 밖)는 목표가 있는 자원을 보여 준다
    if (!listed) {
        for (const auto& entry : targets) ids.insert(entry.first);
    }

    std::vector<ResourceRow> rows;
    for (const std::string& id : ids) {
        if (key && isLordWide(id)) continue;
        ResourceRow row;
        row.id = id;
        if (current) {
            if (auto it = current->find(id); it != current->end()) row.current = it->second;
        }
        if (auto it = targets.find(id); it != targets.end()) row.target = it->second;
        rows.push_back(std::move(row));
    }
    return rows;
}

}
