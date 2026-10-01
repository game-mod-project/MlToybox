#include <windows.h>
#include "overlay/worker.h"
#include "runtime.h"

static DWORD WINAPI Worker(LPVOID self) {
    mlt::ov::runOverlayWorker(self);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        // 후킹과 창 프로시저가 살아 있는 동안 DLL 이 내려가면 게임이 튕긴다. 프로세스가 끝날 때까지 고정한다
        mlt::pinModuleContaining(reinterpret_cast<const void*>(&Worker));
        if (HANDLE t = CreateThread(nullptr, 0, Worker, module, 0, nullptr)) CloseHandle(t);
    }
    return TRUE;
}
