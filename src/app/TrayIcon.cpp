#include "app/TrayIcon.h"

#include <utility>

#include "common/AppIcon.h"
#include "resources/resource.h"

namespace snaplite {

namespace {

std::wstring BuildTooltip(const std::wstring& hotkeyLabel) {
    return L"YanSnap  |  截图：" + hotkeyLabel + L"  |  贴图：F3";
}

}  // namespace

TrayIcon::~TrayIcon() {
    Remove();
}

bool TrayIcon::Create(HWND owner) {
    owner_ = owner;
    data_ = {};
    data_.cbSize = sizeof(data_);
    data_.hWnd = owner_;
    data_.uID = 1;
    data_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    data_.uCallbackMessage = kCallbackMessage;
    icon_ = CreateYanSnapIcon(32);
    data_.hIcon = icon_ ? icon_ : LoadIconW(nullptr, IDI_APPLICATION);
    const std::wstring tooltip = BuildTooltip(hotkeyLabel_);
    lstrcpynW(data_.szTip, tooltip.c_str(), ARRAYSIZE(data_.szTip));
    if (!Shell_NotifyIconW(NIM_ADD, &data_)) {
        owner_ = nullptr;
        return false;
    }
    data_.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &data_);
    return true;
}

void TrayIcon::Remove() {
    if (owner_) {
        Shell_NotifyIconW(NIM_DELETE, &data_);
        owner_ = nullptr;
        data_ = {};
    }
    if (icon_) {
        DestroyIcon(icon_);
        icon_ = nullptr;
    }
}

void TrayIcon::ShowMenu(POINT screenPoint) const {
    HMENU menu = CreatePopupMenu();
    if (!menu) {
        return;
    }
    const std::wstring captureLabel = L"截图\t" + hotkeyLabel_;
    AppendMenuW(menu, MF_STRING, ID_TRAY_CAPTURE, captureLabel.c_str());
    AppendMenuW(menu, MF_STRING, ID_TRAY_PIN_CLIPBOARD, L"贴图（剪贴板）\tF3");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_TRAY_TOGGLE_PINS, L"显示/隐藏所有贴图");
    AppendMenuW(menu, MF_STRING, ID_TRAY_CLOSE_PINS, L"关闭所有贴图");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_TRAY_OPEN_FOLDER, L"打开截图文件夹");
    AppendMenuW(menu, MF_STRING, ID_TRAY_SETTINGS, L"设置");
    AppendMenuW(menu, MF_STRING, ID_TRAY_ABOUT, L"关于");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_TRAY_EXIT, L"退出");
    SetForegroundWindow(owner_);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN,
                   screenPoint.x, screenPoint.y, 0, owner_, nullptr);
    PostMessageW(owner_, WM_NULL, 0, 0);
    DestroyMenu(menu);
}

void TrayIcon::SetHotkeyLabel(std::wstring label) {
    hotkeyLabel_ = std::move(label);
    if (!owner_) {
        return;
    }
    data_.uFlags = NIF_TIP | NIF_SHOWTIP;
    const std::wstring tooltip = BuildTooltip(hotkeyLabel_);
    lstrcpynW(data_.szTip, tooltip.c_str(), ARRAYSIZE(data_.szTip));
    Shell_NotifyIconW(NIM_MODIFY, &data_);
    data_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
}

void TrayIcon::Notify(const wchar_t* title, const wchar_t* message) const {
    if (!owner_) {
        return;
    }
    NOTIFYICONDATAW notification = data_;
    notification.uFlags = NIF_INFO | NIF_REALTIME;
    notification.dwInfoFlags = NIIF_INFO | NIIF_NOSOUND;
    notification.uTimeout = 5000;
    lstrcpynW(notification.szInfoTitle, title, ARRAYSIZE(notification.szInfoTitle));
    lstrcpynW(notification.szInfo, message, ARRAYSIZE(notification.szInfo));
    Shell_NotifyIconW(NIM_MODIFY, &notification);
}

}  // namespace snaplite
