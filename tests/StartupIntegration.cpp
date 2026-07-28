#include <windows.h>

#include <filesystem>
#include <iostream>
#include <string>

#include "resources/resource.h"

namespace {

constexpr wchar_t kMainWindowClass[] = L"YanSnap.MainWindow.v1";
constexpr wchar_t kOverlayWindowClass[] = L"YanSnap.OverlayWindow.v1";

std::filesystem::path ExecutableDirectory() {
    std::wstring path(32768, L'\0');
    const DWORD length =
        GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    path.resize(length);
    return std::filesystem::path(path).parent_path();
}

bool Launch(const std::filesystem::path& executable, PROCESS_INFORMATION* process) {
    std::wstring command = L"\"" + executable.wstring() + L"\"";
    STARTUPINFOW startup{sizeof(startup)};
    return CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr,
                          FALSE, 0, nullptr, executable.parent_path().c_str(),
                          &startup, process) != FALSE;
}

HWND WaitForMainWindow(DWORD timeoutMilliseconds) {
    const DWORD started = GetTickCount();
    while (GetTickCount() - started < timeoutMilliseconds) {
        if (HWND window = FindWindowW(kMainWindowClass, nullptr)) {
            return window;
        }
        Sleep(25);
    }
    return nullptr;
}

}  // namespace

int wmain() {
    if (FindWindowW(kMainWindowClass, nullptr)) {
        std::wcerr << L"YanSnap is already running; startup test skipped.\n";
        return 2;
    }

    const std::filesystem::path executable =
        ExecutableDirectory() / L"YanSnap.exe";
    PROCESS_INFORMATION primary{};
    if (!Launch(executable, &primary)) {
        std::wcerr << L"failed to launch primary YanSnap process\n";
        return 1;
    }

    HWND mainWindow = WaitForMainWindow(5000);
    Sleep(400);
    const bool firstLaunchStayedInBackground =
        mainWindow != nullptr &&
        FindWindowW(kOverlayWindowClass, nullptr) == nullptr;

    PROCESS_INFORMATION secondary{};
    const bool secondaryLaunched = Launch(executable, &secondary);
    bool secondaryExited = false;
    if (secondaryLaunched) {
        secondaryExited =
            WaitForSingleObject(secondary.hProcess, 3000) == WAIT_OBJECT_0;
        CloseHandle(secondary.hThread);
        CloseHandle(secondary.hProcess);
    }
    Sleep(400);

    const bool repeatedLaunchStayedInBackground =
        mainWindow != nullptr &&
        FindWindowW(kOverlayWindowClass, nullptr) == nullptr;
    const bool primaryStillRunning =
        WaitForSingleObject(primary.hProcess, 0) == WAIT_TIMEOUT;

    if (mainWindow) {
        PostMessageW(mainWindow, WM_COMMAND, ID_TRAY_EXIT, 0);
    }
    WaitForSingleObject(primary.hProcess, 3000);
    CloseHandle(primary.hThread);
    CloseHandle(primary.hProcess);

    std::cout << "startup-background=" << firstLaunchStayedInBackground
              << " repeated-background=" << repeatedLaunchStayedInBackground
              << " single-instance="
              << (secondaryLaunched && secondaryExited && primaryStillRunning)
              << '\n';
    return firstLaunchStayedInBackground &&
                   repeatedLaunchStayedInBackground &&
                   secondaryLaunched && secondaryExited && primaryStillRunning
               ? 0
               : 1;
}
