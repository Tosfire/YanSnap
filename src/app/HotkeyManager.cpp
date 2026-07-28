#include "app/HotkeyManager.h"

namespace snaplite {

HotkeyManager::~HotkeyManager() {
    Unregister();
}

bool HotkeyManager::Register(HWND window, UINT modifiers, UINT virtualKey) {
    Unregister();
    window_ = window;
    registered_ = RegisterHotKey(window_, kCaptureHotkeyId, modifiers, virtualKey) != FALSE;
    return registered_;
}

void HotkeyManager::Unregister() {
    if (registered_ && window_) {
        UnregisterHotKey(window_, kCaptureHotkeyId);
    }
    window_ = nullptr;
    registered_ = false;
}

}  // namespace snaplite

