#include "clipboard.h"
#include <windows.h>

namespace mlt::ov {

std::optional<std::string> readClipboardText() {
    if (!OpenClipboard(nullptr)) return std::nullopt;
    std::string out;
    if (const HANDLE data = GetClipboardData(CF_UNICODETEXT)) {
        if (const auto* wide = static_cast<const wchar_t*>(GlobalLock(data))) {
            const int length = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);   // 끝의 0 을 포함한 바이트 수
            if (length > 1) {
                out.resize(static_cast<size_t>(length));
                WideCharToMultiByte(CP_UTF8, 0, wide, -1, out.data(), length, nullptr, nullptr);
                out.resize(static_cast<size_t>(length) - 1);
            }
            GlobalUnlock(data);
        }
    }
    CloseClipboard();
    return out;
}

unsigned long clipboardSequence() {
    return GetClipboardSequenceNumber();
}

bool writeClipboardText(const std::string& utf8) {
    const int length = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);   // 끝의 0 을 포함한 글자 수
    if (length <= 0) return false;
    const HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, static_cast<SIZE_T>(length) * sizeof(wchar_t));
    if (!memory) return false;
    if (auto* dest = static_cast<wchar_t*>(GlobalLock(memory))) {
        MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, dest, length);
        GlobalUnlock(memory);
    }
    if (!OpenClipboard(nullptr)) {
        GlobalFree(memory);
        return false;
    }
    EmptyClipboard();
    const bool ok = SetClipboardData(CF_UNICODETEXT, memory) != nullptr;   // 성공하면 메모리는 클립보드의 것이 된다
    if (!ok) GlobalFree(memory);
    CloseClipboard();
    return ok;
}

}
