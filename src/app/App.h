#pragma once

#include <windows.h>

#include <memory>
#include <vector>

#include "app/HotkeyManager.h"
#include "app/SingleInstance.h"
#include "app/TrayIcon.h"
#include "common/Win32.h"
#include "export/ImageData.h"
#include "settings/Settings.h"
#include "settings/SettingsWindow.h"

namespace snaplite {

class OverlayWindow;
class PinWindow;

class App {
public:
    explicit App(HINSTANCE instance);
    ~App();
    int Run();

private:
    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

    bool Initialize();
    void ShowBackgroundStatus();
    void BeginCapture();
    void HandleCommand(UINT command);
    void ShowSettings();
    bool ApplySettings(const Settings& settings);
    void CreatePin(ImageData image, POINT screenPosition);
    void PinClipboard();
    void TogglePins();
    void ClosePins();

    HINSTANCE instance_{};
    HWND window_{};
    SingleInstance singleInstance_;
    HotkeyManager hotkey_;
    TrayIcon tray_;
    Settings settings_;
    std::unique_ptr<SettingsWindow> settingsWindow_;
    std::unique_ptr<OverlayWindow> overlay_;
    std::vector<std::unique_ptr<PinWindow>> pins_;
    UniqueIcon appIcon_;
    bool pinHotkeyRegistered_{};
};

}  // namespace snaplite
