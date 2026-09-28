#include "json.h"
#include <cctype>
#include <cstdlib>

namespace mlt {

const Json* Json::get(std::string_view key) const {
    if (type != Type::Object || !o) return nullptr;
    for (auto& [k, v] : *o) if (k == key) return &v;
    return nullptr;
}

namespace {
struct Parser {
    std::string_view t;
    size_t i = 0;

    void ws() { while (i < t.size() && (t[i] == ' ' || t[i] == '\t' || t[i] == '\r' || t[i] == '\n')) ++i; }
    bool eat(char c) { ws(); if (i < t.size() && t[i] == c) { ++i; return true; } return false; }
    bool lit(std::string_view w) { if (t.substr(i, w.size()) == w) { i += w.size(); return true; } return false; }

    static void utf8(std::string& out, unsigned cp) {
        if (cp < 0x80) out += static_cast<char>(cp);
        else if (cp < 0x800) { out += static_cast<char>(0xC0 | (cp >> 6)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
        else { out += static_cast<char>(0xE0 | (cp >> 12)); out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
    }

    bool str(std::string& out) {
        if (!eat('"')) return false;
        while (i < t.size()) {
            char c = t[i++];
            if (c == '"') return true;
            if (c != '\\') { out += c; continue; }
            if (i >= t.size()) return false;
            char e = t[i++];
            switch (e) {
                case '"': out += '"'; break;   case '\\': out += '\\'; break; case '/': out += '/'; break;
                case 'b': out += '\b'; break;  case 'f': out += '\f'; break;  case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;  case 't': out += '\t'; break;
                case 'u': {
                    if (i + 4 > t.size()) return false;
                    unsigned cp = 0;
                    for (int k = 0; k < 4; ++k) {
                        char h = t[i++]; cp <<= 4;
                        if (h >= '0' && h <= '9') cp |= h - '0';
                        else if (h >= 'a' && h <= 'f') cp |= h - 'a' + 10;
                        else if (h >= 'A' && h <= 'F') cp |= h - 'A' + 10;
                        else return false;
                    }
                    utf8(out, cp);
                    break;
                }
                default: return false;
            }
        }
        return false;
    }

    bool value(Json& v) {
        ws();
        if (i >= t.size()) return false;
        char c = t[i];
        if (c == '{') {
            ++i; v.type = Json::Type::Object; v.o = std::make_shared<JsonObject>();
            if (eat('}')) return true;
            do {
                std::string k; Json child;
                ws();
                if (!str(k) || !eat(':') || !value(child)) return false;
                v.o->emplace_back(std::move(k), std::move(child));
            } while (eat(','));
            return eat('}');
        }
        if (c == '[') {
            ++i; v.type = Json::Type::Array; v.a = std::make_shared<JsonArray>();
            if (eat(']')) return true;
            do { Json child; if (!value(child)) return false; v.a->push_back(std::move(child)); } while (eat(','));
            return eat(']');
        }
        if (c == '"') { v.type = Json::Type::String; return str(v.s); }
        if (lit("true")) { v.type = Json::Type::Bool; v.b = true; return true; }
        if (lit("false")) { v.type = Json::Type::Bool; v.b = false; return true; }
        if (lit("null")) { v.type = Json::Type::Null; return true; }
        size_t start = i;
        while (i < t.size() && (std::isdigit(static_cast<unsigned char>(t[i])) || t[i] == '-' || t[i] == '+' || t[i] == '.' || t[i] == 'e' || t[i] == 'E')) ++i;
        if (start == i) return false;
        std::string num(t.substr(start, i - start));
        char* end = nullptr;
        v.type = Json::Type::Number;
        v.n = std::strtod(num.c_str(), &end);
        return end && *end == '\0';
    }
};
}

std::optional<Json> parseJson(std::string_view text) {
    if (text.size() >= 3 && text.substr(0, 3) == "\xEF\xBB\xBF") text.remove_prefix(3);
    Parser p{ text };
    Json v;
    if (!p.value(v)) return std::nullopt;
    p.ws();
    if (p.i != text.size()) return std::nullopt;
    return v;
}

std::string escapeJson(std::string_view s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break; case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break; case '\r': out += "\\r"; break; case '\t': out += "\\t"; break;
            default: out += c;
        }
    }
    return out;
}

}
