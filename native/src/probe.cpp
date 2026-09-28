#include "probe.h"
#include <charconv>
#include <windows.h>

namespace mlt {

namespace {
std::string_view nextToken(std::string_view& s) {
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n')) ++i;
    size_t j = i;
    while (j < s.size() && !(s[j] == ' ' || s[j] == '\t' || s[j] == '\r' || s[j] == '\n')) ++j;
    auto tok = s.substr(i, j - i);
    s.remove_prefix(j);
    return tok;
}

template <typename T>
bool parseNumber(std::string_view tok, T& out, int base) {
    if (tok.empty()) return false;
    auto [p, ec] = std::from_chars(tok.data(), tok.data() + tok.size(), out, base);
    return ec == std::errc{} && p == tok.data() + tok.size();
}
}

std::optional<ProbeRequest> parseProbeRequest(std::string_view text) {
    ProbeRequest r;
    auto seqTok = nextToken(text);
    auto addrTok = nextToken(text);
    auto sizeTok = nextToken(text);
    if (addrTok.size() > 2 && addrTok[0] == '0' && (addrTok[1] == 'x' || addrTok[1] == 'X')) addrTok.remove_prefix(2);
    unsigned long long addr = 0;
    if (!parseNumber(seqTok, r.seq, 10) || !parseNumber(addrTok, addr, 16) || !parseNumber(sizeTok, r.size, 10)) return std::nullopt;
    if (r.size == 0 || r.size > kMaxProbeSize) return std::nullopt;
    r.address = static_cast<uintptr_t>(addr);
    return r;
}

bool copyProcessMemory(uintptr_t address, size_t size, std::string& out) {
    out.assign(size, '\0');
    SIZE_T read = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<LPCVOID>(address), out.data(), size, &read) || read != size) {
        out.clear();
        return false;
    }
    return true;
}

}
