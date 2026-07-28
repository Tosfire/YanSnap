#pragma once

#include <windows.h>

namespace snaplite {

class SingleInstance {
public:
    SingleInstance();
    ~SingleInstance();

    SingleInstance(const SingleInstance&) = delete;
    SingleInstance& operator=(const SingleInstance&) = delete;

    [[nodiscard]] bool IsPrimary() const noexcept { return primary_; }
    [[nodiscard]] UINT ActivationMessage() const noexcept { return activationMessage_; }
    bool NotifyPrimary() const;

private:
    HANDLE mutex_{};
    bool primary_{};
    UINT activationMessage_{};
};

}  // namespace snaplite

