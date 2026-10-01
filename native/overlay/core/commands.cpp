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

Json makeSetLord(const std::string& key, int value, long long nowEpochSeconds) {
    Json c = base("setLord", nowEpochSeconds);
    c["key"] = key;
    c["value"] = value;
    return c;
}

}
