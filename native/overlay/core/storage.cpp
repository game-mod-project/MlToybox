#include "storage.h"
#include "text.h"
#include <algorithm>

namespace mlt::ov {

BuildingCatalog BuildingCatalog::parse(std::string_view json) {
    BuildingCatalog catalog;
    Json root = Json::parse(json.begin(), json.end(), nullptr, false);
    if (root.is_discarded()) return catalog;
    const Json* list = arrayAt(&root, "buildings");
    if (!list) return catalog;
    for (const Json& item : *list) {
        if (!item.is_object()) continue;
        BuildingInfo info;
        info.id = optString(&item, "id").value_or("");
        if (info.id.empty()) continue;
        info.name = optString(&item, "name").value_or(info.id);
        info.defaults = { std::max(0, intOr(&item, "generic", 0)), std::max(0, intOr(&item, "large", 0)), std::max(0, intOr(&item, "pantry", 0)) };
        catalog.buildings_.push_back(std::move(info));
    }
    return catalog;
}

std::optional<int>& limitOf(StorageLimits& limits, StorageKind kind) {
    switch (kind) {
        case StorageKind::Large: return limits.large;
        case StorageKind::Pantry: return limits.pantry;
        case StorageKind::Generic: break;
    }
    return limits.generic;
}

std::vector<StorageRow> buildStorageRows(const BuildingCatalog* catalog, const StorageSettings& settings) {
    std::vector<StorageRow> rows;
    if (!catalog) return rows;
    int order = 0;
    for (const BuildingInfo& info : catalog->buildings()) {
        StorageRow row;
        row.id = info.id;
        row.name = info.name;
        row.defaults = info.defaults;
        row.order = order++;
        if (auto it = settings.limits.find(info.id); it != settings.limits.end()) {
            row.values = { it->second.generic, it->second.large, it->second.pantry };
        }
        rows.push_back(std::move(row));
    }
    return rows;
}

std::vector<StorageRow> filterStorageRows(const std::vector<StorageRow>& rows, std::string_view search) {
    const std::string needle = lowerAscii(trim(search));
    if (needle.empty()) return rows;
    std::vector<StorageRow> out;
    for (const StorageRow& row : rows) {
        if (lowerAscii(row.name).find(needle) != std::string::npos || row.id.find(needle) != std::string::npos) out.push_back(row);
    }
    return out;
}

void sortStorageRows(std::vector<StorageRow>& rows, std::optional<StorageColumn> column, bool descending) {
    std::sort(rows.begin(), rows.end(), [&](const StorageRow& a, const StorageRow& b) {
        if (!column) return a.order < b.order;
        if (*column == StorageColumn::Name) {
            const std::string la = lowerAscii(a.name), lb = lowerAscii(b.name);   // UTF-8 바이트 순서: 영문 다음에 한글 가나다순
            if (la != lb) return descending ? la > lb : la < lb;
            return a.order < b.order;
        }
        const size_t kind = static_cast<size_t>(*column) - 1;
        const int va = a.defaults[kind], vb = b.defaults[kind];
        if ((va > 0) != (vb > 0)) return va > 0;   // 그 저장실이 있는 건물이 먼저
        if (va != vb) return descending ? va > vb : va < vb;
        return a.order < b.order;
    });
}

void setStorageLimit(StorageSettings& settings, const std::string& id, StorageKind kind, std::optional<int> value) {
    auto it = settings.limits.find(id);
    if (it == settings.limits.end()) {
        if (!value) return;
        it = settings.limits.emplace(id, StorageLimits()).first;
    }
    limitOf(it->second, kind) = value;
    if (it->second.empty()) settings.limits.erase(it);
}

void fillStorageLimits(StorageSettings& settings, const std::vector<StorageRow>& shown, int multiplier) {
    for (const StorageRow& row : shown) {
        for (int kind = 0; kind < kStorageKinds; ++kind) {
            const long long base = row.defaults[static_cast<size_t>(kind)];
            if (base <= 0) continue;
            const long long value = std::min<long long>(base * multiplier, kStorageLimitMax);
            setStorageLimit(settings, row.id, static_cast<StorageKind>(kind), static_cast<int>(value));
        }
    }
}

void clearStorageLimits(StorageSettings& settings, const std::vector<StorageRow>& shown, bool filtered) {
    if (!filtered) {
        settings.limits.clear();
        return;
    }
    for (const StorageRow& row : shown) settings.limits.erase(row.id);
}

}
