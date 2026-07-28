#include "capture/DesktopCapture.h"

#include <windows.h>

#include <cstring>

#include "common/Win32.h"

namespace snaplite {

namespace {

void DrawMouseCursor(HDC target, int originX, int originY) {
    CURSORINFO cursorInfo{};
    cursorInfo.cbSize = sizeof(cursorInfo);
    if (!GetCursorInfo(&cursorInfo) || !(cursorInfo.flags & CURSOR_SHOWING)) {
        return;
    }
    ICONINFO iconInfo{};
    if (!GetIconInfo(cursorInfo.hCursor, &iconInfo)) {
        return;
    }
    const int x = cursorInfo.ptScreenPos.x - static_cast<int>(iconInfo.xHotspot) - originX;
    const int y = cursorInfo.ptScreenPos.y - static_cast<int>(iconInfo.yHotspot) - originY;
    DrawIconEx(target, x, y, cursorInfo.hCursor, 0, 0, 0, nullptr, DI_NORMAL);
    if (iconInfo.hbmMask) {
        DeleteObject(iconInfo.hbmMask);
    }
    if (iconInfo.hbmColor) {
        DeleteObject(iconInfo.hbmColor);
    }
}

}  // namespace

DesktopImage DesktopCapture::Capture(bool includeCursor) {
    DesktopImage image;
    image.originX = GetSystemMetrics(SM_XVIRTUALSCREEN);
    image.originY = GetSystemMetrics(SM_YVIRTUALSCREEN);
    image.width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    image.height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    image.stride = image.width * 4;
    if (image.width <= 0 || image.height <= 0) {
        return {};
    }

    WindowDc screen(nullptr, GetDC(nullptr));
    if (!screen.get()) {
        return {};
    }
    MemoryDc memory(screen.get());
    if (!memory) {
        return {};
    }

    BITMAPINFO bitmapInfo{};
    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth = image.width;
    bitmapInfo.bmiHeader.biHeight = -image.height;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    UniqueBitmap bitmap(CreateDIBSection(screen.get(), &bitmapInfo, DIB_RGB_COLORS, &bits, nullptr, 0));
    if (!bitmap || !bits) {
        return {};
    }
    SelectObjectGuard selected(memory.get(), bitmap.get());
    if (!BitBlt(memory.get(), 0, 0, image.width, image.height, screen.get(),
                image.originX, image.originY, SRCCOPY | CAPTUREBLT)) {
        return {};
    }
    if (includeCursor) {
        DrawMouseCursor(memory.get(), image.originX, image.originY);
    }

    const std::size_t byteCount = static_cast<std::size_t>(image.stride) * image.height;
    image.pixels.resize(byteCount);
    std::memcpy(image.pixels.data(), bits, byteCount);
    for (std::size_t index = 3; index < image.pixels.size(); index += 4) {
        image.pixels[index] = 255;
    }
    return image;
}

}  // namespace snaplite
