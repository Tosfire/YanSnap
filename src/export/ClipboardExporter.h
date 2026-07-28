#pragma once

#include <windows.h>

#include <optional>

#include "export/ImageData.h"

namespace snaplite {

class ClipboardExporter {
public:
    static bool Copy(HWND owner, const ImageData& image);
    static std::optional<ImageData> Read(HWND owner);
};

}  // namespace snaplite
