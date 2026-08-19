#pragma once

#include <string>

#include "export/ImageData.h"

namespace snaplite {

struct OcrRecognitionResult {
    bool success{};
    std::wstring text;
    std::wstring languageTag;
    std::wstring error;
};

class OcrRecognizer {
public:
    static OcrRecognitionResult Recognize(const ImageData& image);
};

}  // namespace snaplite
