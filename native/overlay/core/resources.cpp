#include "resources.h"
#include "text.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <utility>

namespace mlt::ov {

ResourceCatalog ResourceCatalog::parse(std::string_view json) {
    ResourceCatalog catalog;
    Json root = Json::parse(json.begin(), json.end(), nullptr, false);
    if (root.is_discarded()) return catalog;
    const Json* list = arrayAt(&root, "resources");
    if (!list) return catalog;
    int order = 0;
    for (const Json& item : *list) {
        if (!item.is_object()) continue;
        ResourceInfo info;
        info.id = optString(&item, "id").value_or("");
        if (info.id.empty()) continue;
        info.name = optString(&item, "name").value_or(info.id);
        info.category = optString(&item, "category").value_or("");
        info.group = optString(&item, "group").value_or("");
        info.order = order++;
        catalog.byId_[info.id] = std::move(info);
    }
    return catalog;
}

const ResourceInfo* ResourceCatalog::find(std::string_view id) const {
    auto it = byId_.find(id);
    return it == byId_.end() ? nullptr : &it->second;
}

bool isLordWide(std::string_view id) {
    return id == "Treasury" || id == "Influence";
}

std::vector<ScopeOption> resourceScopeOptions(const StatusDoc* status) {
    return commonAndRegionOptions(status && status->regions ? &*status->regions : nullptr, "공통 (모든 내 영지, 현재=합계)");
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

std::vector<ResourceRow> buildResourceRows(const StatusDoc* status, const ResourcesSettings& settings, const std::optional<std::string>& key,
                                           const ResourceCatalog* catalog) {
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
    int unknown = std::numeric_limits<int>::max() / 2;   // 이름 표에 없는 자원은 맨 뒤(id 순)
    for (const std::string& id : ids) {
        if (key && isLordWide(id)) continue;
        ResourceRow row;
        row.id = id;
        if (current) {
            if (auto it = current->find(id); it != current->end()) row.current = it->second;
        }
        if (auto it = targets.find(id); it != targets.end()) row.target = it->second;
        if (const ResourceInfo* info = catalog ? catalog->find(id) : nullptr) {
            row.name = info->name;
            row.category = info->category.empty() ? kOtherCategory : info->category;
            row.group = info->group;
            row.order = info->order;
        } else {
            row.name = id;
            row.category = kOtherCategory;
            row.order = unknown++;
        }
        rows.push_back(std::move(row));
    }
    return rows;
}

std::string resourceClassLabel(const ResourceRow& row) {
    return row.group.empty() ? row.category : row.category + " · " + row.group;
}

std::vector<std::string> resourceCategories(const std::vector<ResourceRow>& rows) {
    std::vector<const ResourceRow*> ordered;
    for (const ResourceRow& row : rows) ordered.push_back(&row);
    std::stable_sort(ordered.begin(), ordered.end(), [](const ResourceRow* a, const ResourceRow* b) { return a->order < b->order; });
    std::vector<std::string> out;
    for (const ResourceRow* row : ordered) {
        if (std::find(out.begin(), out.end(), row->category) == out.end()) out.push_back(row->category);
    }
    return out;
}

std::vector<ResourceRow> filterResourceRows(const std::vector<ResourceRow>& rows, const ResourceFilter& filter) {
    const std::string needle = lowerAscii(trim(filter.search));
    std::vector<ResourceRow> out;
    for (const ResourceRow& row : rows) {
        if (!filter.category.empty() && row.category != filter.category) continue;
        if (!needle.empty()) {
            const bool hit = lowerAscii(row.name).find(needle) != std::string::npos || lowerAscii(row.id).find(needle) != std::string::npos
                || lowerAscii(row.category).find(needle) != std::string::npos || lowerAscii(row.group).find(needle) != std::string::npos;
            if (!hit) continue;
        }
        out.push_back(row);
    }
    return out;
}

namespace {
bool gameOrder(const ResourceRow& a, const ResourceRow& b) {
    if (a.order != b.order) return a.order < b.order;
    return a.id < b.id;
}

// 값이 있는 줄이 먼저. 둘 다 있으면 방향대로, 같으면 게임의 순서
template <typename T>
bool byValue(const std::optional<T>& a, const std::optional<T>& b, bool descending, const ResourceRow& ra, const ResourceRow& rb) {
    if (a.has_value() != b.has_value()) return a.has_value();
    if (a && *a != *b) return descending ? *a > *b : *a < *b;
    return gameOrder(ra, rb);
}
}

void sortResourceRows(std::vector<ResourceRow>& rows, ResourceColumn column, bool descending) {
    std::sort(rows.begin(), rows.end(), [&](const ResourceRow& a, const ResourceRow& b) {
        switch (column) {
            case ResourceColumn::Name: {
                const std::string la = lowerAscii(a.name), lb = lowerAscii(b.name);   // UTF-8 바이트 순서: 영문 다음에 한글 가나다순
                if (la != lb) return descending ? la > lb : la < lb;
                return gameOrder(a, b);
            }
            case ResourceColumn::Current: return byValue(a.current, b.current, descending, a, b);
            case ResourceColumn::Target: return byValue(a.target, b.target, descending, a, b);
            case ResourceColumn::Category: break;
        }
        return descending ? gameOrder(b, a) : gameOrder(a, b);
    });
}

void ResourceOrder::arrange(std::vector<ResourceRow>& rows, ResourceColumn column, bool descending, const std::string& context) {
    std::vector<std::string> now;
    for (const ResourceRow& row : rows) now.push_back(row.id);
    std::sort(now.begin(), now.end());
    std::vector<std::string> kept = ids_;
    std::sort(kept.begin(), kept.end());
    if (has_ && column == column_ && descending == descending_ && context == context_ && now == kept) {
        // 기억해 둔 순서대로 놓는다
        std::map<std::string, size_t> position;
        for (size_t i = 0; i < ids_.size(); ++i) position[ids_[i]] = i;
        std::sort(rows.begin(), rows.end(), [&](const ResourceRow& a, const ResourceRow& b) { return position[a.id] < position[b.id]; });
        return;
    }
    sortResourceRows(rows, column, descending);
    has_ = true;
    column_ = column;
    descending_ = descending;
    context_ = context;
    ids_.clear();
    for (const ResourceRow& row : rows) ids_.push_back(row.id);
}

std::map<std::string, std::vector<RegionOverride>> regionOverrides(const StatusDoc* status, const ResourcesSettings& settings) {
    std::vector<std::pair<std::string, std::string>> regions;   // 키와 이름
    if (status && status->regions) {
        for (const RegionResources& r : *status->regions) regions.emplace_back(r.key, r.name);
    } else {
        for (const auto& entry : settings.regionTargets) regions.emplace_back(entry.first, entry.first);
    }
    std::map<std::string, std::vector<RegionOverride>> out;
    for (const auto& [key, name] : regions) {
        auto it = settings.regionTargets.find(key);
        if (it == settings.regionTargets.end()) continue;
        for (const auto& [id, target] : it->second) out[id].push_back({ key, name, target });
    }
    return out;
}

std::string regionOverrideText(const std::vector<RegionOverride>& overrides) {
    std::string text;
    for (const RegionOverride& o : overrides) {
        if (!text.empty()) text += '\n';
        text += (o.name == o.key ? o.key : regionLabel(o.name, o.key)) + ": " + std::to_string(o.target);
    }
    return text;
}

std::map<std::string, int> clearedTargets(std::map<std::string, int> targets, const std::vector<ResourceRow>& shown, bool filtered) {
    if (!filtered) return {};
    for (const ResourceRow& row : shown) targets.erase(row.id);
    return targets;
}

std::vector<float> columnShares(const std::vector<float>& widths) {
    float total = 0.0f;
    for (float w : widths) {
        if (!(w > 0.0f)) return {};
        total += w;
    }
    std::vector<float> out;
    for (float w : widths) out.push_back(std::round(w / total * 1000.0f) / 10.0f);
    return out;
}

bool columnSharesDiffer(const std::vector<float>& saved, const std::vector<float>& now) {
    if (saved.size() != now.size()) return true;
    for (size_t i = 0; i < saved.size(); ++i) {
        if (std::fabs(saved[i] - now[i]) > 0.5f) return true;
    }
    return false;
}

}
