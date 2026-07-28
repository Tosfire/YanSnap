#include <windows.h>
#include <objbase.h>

#include "app/App.h"

namespace {

void EnablePerMonitorDpiAwareness() {
    if (SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
        return;
    }
    SetProcessDPIAware();
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    EnablePerMonitorDpiAwareness();
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    snaplite::App app(instance);
    const int exitCode = app.Run();
    if (SUCCEEDED(comResult)) {
        CoUninitialize();
    }
    return exitCode;
}
