#pragma once

#include <windows.h>
#include <shellapi.h>

#include <string>

namespace snaplite {

class TrayIcon {
public:
    static constexpr UINT kCallbackMessage = WM_APP + 10;

    ~TrayIcon();

    bool Create(HWND owner);
    void Remove();
    void ShowMenu(POINT screenPoint) const;
    void Notify(const wchar_t* title, const wchar_t* message) const;
    void SetHotkeyLabel(std::wstring label);

private:
    HWND owner_{};
    HICON icon_{};
    NOTIFYICONDATAW data_{};
    std::wstring hotkeyLabel_{L"F1"};
};

}  // namespace snaplite
