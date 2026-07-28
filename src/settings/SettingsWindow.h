#pragma once

#include <windows.h>

#include <functional>

#include "common/Win32.h"
#include "settings/Settings.h"

namespace snaplite {

class SettingsWindow {
public:
    using SaveCallback = std::function<bool(const Settings&)>;

    SettingsWindow(HINSTANCE instance, HWND owner, Settings settings, SaveCallback callback);
    ~SettingsWindow();

    bool Show();
    void BringToFront() const;
    [[nodiscard]] bool Active() const noexcept { return window_ != nullptr; }

private:
    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void CreateControls();
    void Paint();
    void DrawButton(const DRAWITEMSTRUCT& draw) const;
    void BrowseDirectory();
    void SaveAndClose();
    [[nodiscard]] int Scale(int value) const;

    HINSTANCE instance_{};
    HWND owner_{};
    HWND window_{};
    HWND hotkeyEdit_{};
    HWND actionCombo_{};
    HWND directoryEdit_{};
    HWND notificationCheck_{};
    HWND cursorCheck_{};
    HWND startupCheck_{};
    UniqueBrush pageBrush_;
    UniqueBrush cardBrush_;
    UniqueFont normalFont_;
    UniqueFont labelFont_;
    UniqueIcon largeIcon_;
    UniqueIcon smallIcon_;
    UINT dpi_{96};
    Settings settings_;
    SaveCallback callback_;
};

}  // namespace snaplite
