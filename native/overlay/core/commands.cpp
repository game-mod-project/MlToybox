#include "commands.h"
#include <random>

namespace mlt::ov {

std::string newCommandId() {
    static std::mt19937_64 rng{std::random_device{}()};
    static const char* hex = "0123456789abcdef";
    std::string id;
    id.reserve(32);
    for (int part = 0; part < 2; ++part) {
        unsigned long long v = rng();
        for (int i = 0; i < 16; ++i) {
            id.push_back(hex[v & 0xF]);
            v >>= 4;
        }
    }
    return id;
}

static Json base(const char* type, long long nowEpochSeconds) {
    Json c = Json::object();
    c["id"] = newCommandId();
    c["type"] = type;
    c["issuedAt"] = nowEpochSeconds;
    return c;
}

static void setRegion(Json& c, const std::optional<std::string>& region) {
    if (region) c["region"] = *region;
}

Json makeSetLord(const std::string& key, int value, long long nowEpochSeconds) {
    Json c = base("setLord", nowEpochSeconds);
    c["key"] = key;
    c["value"] = value;
    return c;
}

Json makeSpawnSquads(const std::string& unit, int count, long long nowEpochSeconds, const std::optional<std::string>& region) {
    Json c = base("spawnSquads", nowEpochSeconds);
    c["unit"] = unit;
    c["count"] = count;
    setRegion(c, region);
    return c;
}

Json makeReformSquads(long long nowEpochSeconds, const std::optional<std::string>& region) {
    Json c = base("reformSquads", nowEpochSeconds);
    setRegion(c, region);
    return c;
}

Json makeCustomizeRetinue(int squadId, long long nowEpochSeconds, const std::optional<std::string>& region) {
    Json c = base("customizeRetinue", nowEpochSeconds);
    c["value"] = squadId;
    setRegion(c, region);
    return c;
}

Json makeAddFamilies(int count, long long nowEpochSeconds, const std::optional<std::string>& region) {
    Json c = base("addFamilies", nowEpochSeconds);
    c["count"] = count;
    setRegion(c, region);
    return c;
}

}
