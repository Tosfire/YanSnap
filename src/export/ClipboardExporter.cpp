#include "export/ClipboardExporter.h"

#include <cstdlib>
#include <cstring>

namespace snaplite {

namespace {

bool OpenClipboardWithRetry(HWND owner) {
    for (int attempt = 0; attempt < 6; ++attempt) {
        if (OpenClipboard(owner)) {
            return true;
        }
        Sleep(20);
    }
    return false;
}

std::optional<ImageData> ReadDib(HGLOBAL memory) {
    if (!memory) {
        return std::nullopt;
    }
    const auto* data = static_cast<const std::uint8_t*>(GlobalLock(memory));
    if (!data) {
        return std::nullopt;
    }
    const SIZE_T totalBytes = GlobalSize(memory);
    if (totalBytes < sizeof(BITMAPINFOHEADER)) {
        GlobalUnlock(memory);
        return std::nullopt;
    }
    const auto* header = reinterpret_cast<const BITMAPINFOHEADER*>(data);
    const int width = header->biWidth;
    const int height = std::abs(header->biHeight);
    const int bitCount = header->biBitCount;
    if (width <= 0 || height <= 0 || (bitCount != 24 && bitCount != 32)) {
        GlobalUnlock(memory);
        return std::nullopt;
    }

    std::size_t pixelOffset = header->biSize;
    if (header->biSize == sizeof(BITMAPINFOHEADER) &&
        header->biCompression == BI_BITFIELDS) {
        pixelOffset += 3 * sizeof(DWORD);
    }
    if (header->biClrUsed > 0) {
        pixelOffset += static_cast<std::size_t>(header->biClrUsed) * sizeof(RGBQUAD);
    }
    const int sourceStride = ((width * bitCount + 31) / 32) * 4;
    const std::size_t required = pixelOffset +
        static_cast<std::size_t>(sourceStride) * static_cast<std::size_t>(height);
    if (required > totalBytes) {
        GlobalUnlock(memory);
        return std::nullopt;
    }

    ImageData image;
    image.width = width;
    image.height = height;
    image.stride = width * 4;
    image.pixels.resize(static_cast<std::size_t>(image.stride) * image.height);
    const auto* sourcePixels = data + pixelOffset;
    const bool topDown = header->biHeight < 0;
    for (int y = 0; y < height; ++y) {
        const int sourceY = topDown ? y : height - y - 1;
        const auto* source = sourcePixels + static_cast<std::size_t>(sourceY) * sourceStride;
        auto* destination = image.pixels.data() + static_cast<std::size_t>(y) * image.stride;
        for (int x = 0; x < width; ++x) {
            destination[x * 4] = source[x * bitCount / 8];
            destination[x * 4 + 1] = source[x * bitCount / 8 + 1];
            destination[x * 4 + 2] = source[x * bitCount / 8 + 2];
            destination[x * 4 + 3] = 255;
        }
    }
    GlobalUnlock(memory);
    return image;
}

std::optional<ImageData> ReadBitmap(HBITMAP bitmap) {
    if (!bitmap) {
        return std::nullopt;
    }
    BITMAP description{};
    if (!GetObjectW(bitmap, sizeof(description), &description) ||
        description.bmWidth <= 0 || description.bmHeight <= 0) {
        return std::nullopt;
    }
    ImageData image;
    image.width = description.bmWidth;
    image.height = description.bmHeight;
    image.stride = image.width * 4;
    image.pixels.resize(static_cast<std::size_t>(image.stride) * image.height);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = image.width;
    info.bmiHeader.biHeight = -image.height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    HDC dc = GetDC(nullptr);
    const int rows = GetDIBits(dc, bitmap, 0, static_cast<UINT>(image.height),
                               image.pixels.data(), &info, DIB_RGB_COLORS);
    ReleaseDC(nullptr, dc);
    if (rows != image.height) {
        return std::nullopt;
    }
    for (std::size_t index = 3; index < image.pixels.size(); index += 4) {
        image.pixels[index] = 255;
    }
    return image;
}

}  // namespace

bool ClipboardExporter::Copy(HWND owner, const ImageData& image) {
    if (!image.Valid() || !OpenClipboardWithRetry(owner)) {
        return false;
    }
    if (!EmptyClipboard()) {
        CloseClipboard();
        return false;
    }

    const SIZE_T headerSize = sizeof(BITMAPV5HEADER);
    const SIZE_T pixelBytes = image.pixels.size();
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, headerSize + pixelBytes);
    if (!memory) {
        CloseClipboard();
        return false;
    }

    auto* destination = static_cast<std::uint8_t*>(GlobalLock(memory));
    if (!destination) {
        GlobalFree(memory);
        CloseClipboard();
        return false;
    }
    auto* header = reinterpret_cast<BITMAPV5HEADER*>(destination);
    header->bV5Size = sizeof(BITMAPV5HEADER);
    header->bV5Width = image.width;
    header->bV5Height = -image.height;
    header->bV5Planes = 1;
    header->bV5BitCount = 32;
    header->bV5Compression = BI_BITFIELDS;
    header->bV5SizeImage = static_cast<DWORD>(pixelBytes);
    header->bV5RedMask = 0x00FF0000;
    header->bV5GreenMask = 0x0000FF00;
    header->bV5BlueMask = 0x000000FF;
    header->bV5AlphaMask = 0xFF000000;
    header->bV5CSType = 0x73524742;  // 'sRGB'
    header->bV5Intent = LCS_GM_IMAGES;
    std::memcpy(destination + headerSize, image.pixels.data(), pixelBytes);
    GlobalUnlock(memory);

    const bool success = SetClipboardData(CF_DIBV5, memory) != nullptr;
    if (!success) {
        GlobalFree(memory);
    }
    CloseClipboard();
    return success;
}

bool ClipboardExporter::CopyText(HWND owner, std::wstring_view text) {
    if (text.empty() || !OpenClipboardWithRetry(owner)) {
        return false;
    }
    if (!EmptyClipboard()) {
        CloseClipboard();
        return false;
    }

    const SIZE_T byteCount = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, byteCount);
    if (!memory) {
        CloseClipboard();
        return false;
    }
    void* destination = GlobalLock(memory);
    if (!destination) {
        GlobalFree(memory);
        CloseClipboard();
        return false;
    }
    std::memcpy(destination, text.data(), text.size() * sizeof(wchar_t));
    GlobalUnlock(memory);

    const bool success = SetClipboardData(CF_UNICODETEXT, memory) != nullptr;
    if (!success) {
        GlobalFree(memory);
    }
    CloseClipboard();
    return success;
}

std::optional<ImageData> ClipboardExporter::Read(HWND owner) {
    if (!OpenClipboardWithRetry(owner)) {
        return std::nullopt;
    }
    std::optional<ImageData> image;
    if (IsClipboardFormatAvailable(CF_DIBV5)) {
        image = ReadDib(static_cast<HGLOBAL>(GetClipboardData(CF_DIBV5)));
    }
    if (!image && IsClipboardFormatAvailable(CF_DIB)) {
        image = ReadDib(static_cast<HGLOBAL>(GetClipboardData(CF_DIB)));
    }
    if (!image && IsClipboardFormatAvailable(CF_BITMAP)) {
        image = ReadBitmap(static_cast<HBITMAP>(GetClipboardData(CF_BITMAP)));
    }
    CloseClipboard();
    return image;
}

}  // namespace snaplite
