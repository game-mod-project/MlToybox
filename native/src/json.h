#pragma once
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mlt {
struct Json;
using JsonArray = std::vector<Json>;
using JsonObject = std::vector<std::pair<std::string, Json>>;

struct Json {
    enum class Type { Null, Bool, Number, String, Array, Object };
    Type type = Type::Null;
    bool b = false;
    double n = 0;
    std::string s;
    std::shared_ptr<JsonArray> a;
    std::shared_ptr<JsonObject> o;

    const Json* get(std::string_view key) const;
    bool asBool(bool def) const { return type == Type::Bool ? b : def; }
    double asNumber(double def) const { return type == Type::Number ? n : def; }
};

std::optional<Json> parseJson(std::string_view text);
std::string escapeJson(std::string_view text);
}
