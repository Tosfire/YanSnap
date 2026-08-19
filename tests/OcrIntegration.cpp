#include <windows.h>

#include <iostream>
#include <string>

#include "common/Win32.h"
#include "ocr/OcrRecognizer.h"

namespace {

snaplite::ImageData CreateTextImage() {
    constexpr int width = 1180;
    constexpr int height = 330;

    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    HDC screen = GetDC(nullptr);
    snaplite::MemoryDc memory(screen);
    void* pixels = nullptr;
    snaplite::UniqueBitmap bitmap(CreateDIBSection(
        screen, &info, DIB_RGB_COLORS, &pixels, nullptr, 0));
    ReleaseDC(nullptr, screen);
    if (!memory || !bitmap || !pixels) {
        return {};
    }

    snaplite::SelectObjectGuard selectedBitmap(memory.get(), bitmap.get());
    RECT canvas{0, 0, width, height};
    FillRect(memory.get(), &canvas,
             reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
    SetBkMode(memory.get(), TRANSPARENT);
    SetTextColor(memory.get(), RGB(20, 24, 32));

    snaplite::UniqueFont font(CreateFontW(
        -64, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI"));
    snaplite::SelectObjectGuard selectedFont(memory.get(), font.get());
    RECT textBounds{45, 35, width - 45, height - 30};
    DrawTextW(memory.get(),
              L"YanSnap OCR 12345\r\nhttps://example.com\r\n中文识别",
              -1, &textBounds, DT_LEFT | DT_TOP | DT_NOPREFIX);

    snaplite::ImageData image;
    image.width = width;
    image.height = height;
    image.stride = width * 4;
    const auto byteCount = static_cast<std::size_t>(image.stride) * height;
    const auto* first = static_cast<const std::uint8_t*>(pixels);
    image.pixels.assign(first, first + byteCount);
    return image;
}

bool Contains(const std::wstring& text, const wchar_t* token) {
    return text.find(token) != std::wstring::npos;
}

}  // namespace

int wmain() {
    const snaplite::ImageData image = CreateTextImage();
    if (!image.Valid()) {
        std::wcerr << L"Failed to create OCR test image.\n";
        return 1;
    }

    const snaplite::OcrRecognitionResult result =
        snaplite::OcrRecognizer::Recognize(image);
    if (!result.success) {
        std::wcerr << L"OCR failed: " << result.error << L'\n';
        return 2;
    }

    std::wcout << L"OCR language: " << result.languageTag << L"\n\n"
               << result.text << L'\n';
    if (!Contains(result.text, L"YanSnap") ||
        !Contains(result.text, L"12345") ||
        !Contains(result.text, L"example.com") ||
        !Contains(result.text, L"中文识别")) {
        std::wcerr << L"OCR output is missing an expected Chinese, English, numeric, or URL token.\n";
        return 3;
    }
    return 0;
}
