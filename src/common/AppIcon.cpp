#include "common/AppIcon.h"

#include <algorithm>

#include "common/Win32.h"
#include "resources/resource.h"

namespace snaplite {

HICON CreateYanSnapIcon(int size) {
    if (HICON icon = static_cast<HICON>(
            LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_APP_ICON),
                       IMAGE_ICON, size, size, LR_DEFAULTCOLOR))) {
        return icon;
    }

    BITMAPV5HEADER header{};
    header.bV5Size = sizeof(header);
    header.bV5Width = size;
    header.bV5Height = -size;
    header.bV5Planes = 1;
    header.bV5BitCount = 32;
    header.bV5Compression = BI_BITFIELDS;
    header.bV5RedMask = 0x00FF0000;
    header.bV5GreenMask = 0x0000FF00;
    header.bV5BlueMask = 0x000000FF;
    header.bV5AlphaMask = 0xFF000000;

    void* bits = nullptr;
    HDC screen = GetDC(nullptr);
    UniqueBitmap color(CreateDIBSection(screen, reinterpret_cast<BITMAPINFO*>(&header),
                                        DIB_RGB_COLORS, &bits, nullptr, 0));
    ReleaseDC(nullptr, screen);
    UniqueBitmap mask(CreateBitmap(size, size, 1, 1, nullptr));
    if (!color || !mask || !bits) {
        return nullptr;
    }

    MemoryDc dc(nullptr);
    if (!dc) {
        return nullptr;
    }
    SelectObjectGuard selectedBitmap(dc.get(), color.get());
    RECT full{0, 0, size, size};
    FillRect(dc.get(), &full, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
    const int inset = std::max(1, size / 10);
    UniqueBrush blue(CreateSolidBrush(RGB(32, 137, 255)));
    SelectObjectGuard selectedBrush(dc.get(), blue.get());
    UniquePen outline(CreatePen(PS_SOLID, std::max(1, size / 16), RGB(17, 95, 190)));
    SelectObjectGuard selectedPen(dc.get(), outline.get());
    RoundRect(dc.get(), inset, inset, size - inset, size - inset, size / 3, size / 3);

    UniquePen white(CreatePen(PS_SOLID, std::max(2, size / 9), RGB(255, 255, 255)));
    SelectObjectGuard selectedWhite(dc.get(), white.get());
    MoveToEx(dc.get(), size * 3 / 10, size / 2, nullptr);
    LineTo(dc.get(), size * 9 / 20, size * 13 / 20);
    LineTo(dc.get(), size * 7 / 10, size * 7 / 20);

    auto* pixels = static_cast<unsigned char*>(bits);
    for (int index = 0; index < size * size * 4; index += 4) {
        pixels[index + 3] =
            (pixels[index] || pixels[index + 1] || pixels[index + 2]) ? 255 : 0;
    }
    ICONINFO info{};
    info.fIcon = TRUE;
    info.hbmColor = color.get();
    info.hbmMask = mask.get();
    return CreateIconIndirect(&info);
}

}  // namespace snaplite
