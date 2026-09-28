#include <windows.h>
#include "runtime.h"

static DWORD WINAPI Worker(LPVOID self) {
    mlt::runWorker(self);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        mlt::pinModuleContaining(reinterpret_cast<const void*>(&Worker));   // 언로드 금지 (spec §11.4)
        if (HANDLE t = CreateThread(nullptr, 0, Worker, module, 0, nullptr)) CloseHandle(t);
    }
    return TRUE;
}
