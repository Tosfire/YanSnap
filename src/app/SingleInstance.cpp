#include "app/SingleInstance.h"

namespace snaplite {

namespace {
constexpr wchar_t kMutexName[] = L"Local\\YanSnap.SingleInstance.v1";
constexpr wchar_t kActivationMessageName[] = L"YanSnap.ShowBackgroundStatus.v1";
constexpr wchar_t kWindowClassName[] = L"YanSnap.MainWindow.v1";
}

SingleInstance::SingleInstance() {
    activationMessage_ = RegisterWindowMessageW(kActivationMessageName);
    SetLastError(ERROR_SUCCESS);
    mutex_ = CreateMutexW(nullptr, FALSE, kMutexName);
    primary_ = mutex_ != nullptr && GetLastError() != ERROR_ALREADY_EXISTS;
}

SingleInstance::~SingleInstance() {
    if (mutex_) {
        CloseHandle(mutex_);
    }
}

bool SingleInstance::NotifyPrimary() const {
    HWND window = FindWindowW(kWindowClassName, nullptr);
    if (!window || activationMessage_ == 0) {
        return false;
    }
    return PostMessageW(window, activationMessage_, 0, 0) != FALSE;
}

}  // namespace snaplite
