#pragma once

#include <string>

#include "export/ImageData.h"

namespace snaplite {

class PngEncoder {
public:
    static bool Save(const std::wstring& path, const ImageData& image, std::wstring* error = nullptr);
    static std::wstring DefaultScreenshotsDirectory();
    static std::wstring SuggestedFileName(const std::wstring& directory);
};

}  // namespace snaplite

