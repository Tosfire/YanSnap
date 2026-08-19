#include "ocr/OcrRecognizer.h"

#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cwctype>
#include <optional>
#include <string_view>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Globalization.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Media.Ocr.h>
#include <winrt/base.h>

namespace snaplite {

namespace {

using winrt::Windows::Globalization::Language;
using winrt::Windows::Graphics::Imaging::BitmapAlphaMode;
using winrt::Windows::Graphics::Imaging::BitmapBufferAccessMode;
using winrt::Windows::Graphics::Imaging::BitmapPixelFormat;
using winrt::Windows::Graphics::Imaging::SoftwareBitmap;
using winrt::Windows::Media::Ocr::OcrEngine;

class ApartmentScope {
public:
    ApartmentScope() {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        initialized_ = true;
    }

    ~ApartmentScope() {
        if (initialized_) {
            winrt::uninit_apartment();
        }
    }

    ApartmentScope(const ApartmentScope&) = delete;
    ApartmentScope& operator=(const ApartmentScope&) = delete;

private:
    bool initialized_{};
};

std::wstring Lowercase(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](wchar_t character) { return std::towlower(character); });
    return value;
}

bool IsChineseLanguage(const Language& language) {
    const std::wstring tag = Lowercase(language.LanguageTag().c_str());
    return tag == L"zh" || tag.starts_with(L"zh-");
}

OcrEngine SelectEngine() {
    const auto languages = OcrEngine::AvailableRecognizerLanguages();
    for (const Language& language : languages) {
        if (IsChineseLanguage(language)) {
            if (OcrEngine engine = OcrEngine::TryCreateFromLanguage(language)) {
                return engine;
            }
        }
    }

    if (OcrEngine engine = OcrEngine::TryCreateFromUserProfileLanguages()) {
        return engine;
    }

    for (const Language& language : languages) {
        const std::wstring tag = Lowercase(language.LanguageTag().c_str());
        if (tag == L"en" || tag.starts_with(L"en-")) {
            if (OcrEngine engine = OcrEngine::TryCreateFromLanguage(language)) {
                return engine;
            }
        }
    }

    for (const Language& language : languages) {
        if (OcrEngine engine = OcrEngine::TryCreateFromLanguage(language)) {
            return engine;
        }
    }
    return nullptr;
}

ImageData ResizeForOcr(const ImageData& source, int maxDimension) {
    if (!source.Valid() || maxDimension <= 0 ||
        std::max(source.width, source.height) <= maxDimension) {
        return source;
    }

    const double scale = static_cast<double>(maxDimension) /
                         static_cast<double>(std::max(source.width, source.height));
    ImageData resized;
    resized.width = std::max(1, static_cast<int>(std::floor(source.width * scale)));
    resized.height = std::max(1, static_cast<int>(std::floor(source.height * scale)));
    resized.stride = resized.width * 4;
    resized.pixels.resize(static_cast<std::size_t>(resized.stride) * resized.height);

    for (int y = 0; y < resized.height; ++y) {
        const int sourceY = std::min(source.height - 1,
                                     y * source.height / resized.height);
        auto* destinationRow = resized.pixels.data() +
            static_cast<std::size_t>(y) * resized.stride;
        const auto* sourceRow = source.pixels.data() +
            static_cast<std::size_t>(sourceY) * source.stride;
        for (int x = 0; x < resized.width; ++x) {
            const int sourceX = std::min(source.width - 1,
                                         x * source.width / resized.width);
            std::memcpy(destinationRow + static_cast<std::size_t>(x) * 4,
                        sourceRow + static_cast<std::size_t>(sourceX) * 4, 4);
        }
    }
    return resized;
}

SoftwareBitmap MakeSoftwareBitmap(const ImageData& image) {
    SoftwareBitmap bitmap(BitmapPixelFormat::Bgra8, image.width, image.height,
                          BitmapAlphaMode::Ignore);
    auto buffer = bitmap.LockBuffer(BitmapBufferAccessMode::Write);
    const auto plane = buffer.GetPlaneDescription(0);
    auto reference = buffer.CreateReference();
    BYTE* destination = reference.data();
    const UINT32 capacity = reference.Capacity();

    const std::size_t required = static_cast<std::size_t>(plane.StartIndex) +
        static_cast<std::size_t>(std::max(0, image.height - 1)) * plane.Stride +
        static_cast<std::size_t>(image.width) * 4;
    if (!destination || required > capacity) {
        throw winrt::hresult_error(E_UNEXPECTED, L"OCR 位图缓冲区无效。");
    }

    destination += plane.StartIndex;
    for (int row = 0; row < image.height; ++row) {
        std::memcpy(destination + static_cast<std::size_t>(row) * plane.Stride,
                    image.pixels.data() + static_cast<std::size_t>(row) * image.stride,
                    static_cast<std::size_t>(image.width) * 4);
    }
    return bitmap;
}

std::wstring NormalizeLineEndings(std::wstring text) {
    std::wstring normalized;
    normalized.reserve(text.size() + text.size() / 8);
    for (std::size_t index = 0; index < text.size(); ++index) {
        if (text[index] == L'\r') {
            normalized += L"\r\n";
            if (index + 1 < text.size() && text[index + 1] == L'\n') {
                ++index;
            }
        } else if (text[index] == L'\n') {
            normalized += L"\r\n";
        } else {
            normalized.push_back(text[index]);
        }
    }

    const auto isEdgeWhitespace = [](wchar_t character) {
        return character == L' ' || character == L'\t' ||
               character == L'\r' || character == L'\n';
    };
    while (!normalized.empty() && isEdgeWhitespace(normalized.front())) {
        normalized.erase(normalized.begin());
    }
    while (!normalized.empty() && isEdgeWhitespace(normalized.back())) {
        normalized.pop_back();
    }

    const auto isCjkCharacter = [](wchar_t character) {
        const unsigned int codePoint = static_cast<unsigned int>(character);
        return (codePoint >= 0x3400 && codePoint <= 0x4DBF) ||
               (codePoint >= 0x4E00 && codePoint <= 0x9FFF) ||
               (codePoint >= 0xF900 && codePoint <= 0xFAFF);
    };
    std::wstring compacted;
    compacted.reserve(normalized.size());
    for (std::size_t index = 0; index < normalized.size(); ++index) {
        const bool cjkSpacing = normalized[index] == L' ' && index > 0 &&
            index + 1 < normalized.size() &&
            isCjkCharacter(normalized[index - 1]) &&
            isCjkCharacter(normalized[index + 1]);
        if (!cjkSpacing) {
            compacted.push_back(normalized[index]);
        }
    }
    return compacted;
}

}  // namespace

OcrRecognitionResult OcrRecognizer::Recognize(const ImageData& image) {
    OcrRecognitionResult result;
    if (!image.Valid()) {
        result.error = L"截图图像无效，无法进行文字识别。";
        return result;
    }

    try {
        ApartmentScope apartment;
        OcrEngine engine = SelectEngine();
        if (!engine) {
            result.error =
                L"未找到可用的 Windows OCR 语言包。请在 Windows“语言和区域”设置中"
                L"为中文或英文安装“光学字符识别”语言功能。";
            return result;
        }

        const int maximum = static_cast<int>(OcrEngine::MaxImageDimension());
        ImageData prepared = ResizeForOcr(image, maximum);
        SoftwareBitmap bitmap = MakeSoftwareBitmap(prepared);
        const auto recognition = engine.RecognizeAsync(bitmap).get();
        result.success = true;
        result.text = NormalizeLineEndings(recognition.Text().c_str());
        result.languageTag = engine.RecognizerLanguage().LanguageTag().c_str();
        return result;
    } catch (const winrt::hresult_error& error) {
        result.error = L"文字识别失败：" + std::wstring(error.message().c_str());
    } catch (const std::exception&) {
        result.error = L"文字识别失败，请稍后重试。";
    }
    return result;
}

}  // namespace snaplite
