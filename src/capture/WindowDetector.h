#pragma once

#include <windows.h>

#include <optional>

#include "common/Geometry.h"

namespace snaplite {

class WindowDetector {
public:
    static std::optional<RectI> Detect(POINT screenPoint, DWORD excludedProcessId,
                                       RectI desktopBounds);
};

}  // namespace snaplite

