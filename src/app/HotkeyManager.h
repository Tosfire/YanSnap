#pragma once

#include <windows.h>

namespace snaplite {

class HotkeyManager {
public:
    static constexpr int kCaptureHotkeyId = 1;

    ~HotkeyManager();

    bool Register(HWND window, UINT modifiers = MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT,
                  UINT virtualKey = 'A');
    void Unregister();
    [[nodiscard]] bool IsRegistered() const noexcept { return registered_; }

private:
    HWND window_{};
    bool registered_{};
};

}  // namespace snaplite

