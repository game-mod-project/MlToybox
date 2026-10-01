#include "clipboard.h"
#include <windows.h>

namespace mlt::ov {

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
