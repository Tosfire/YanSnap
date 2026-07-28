#include "export/ImageComposer.h"

#include <cstring>

namespace snaplite {

ImageData ImageComposer::Crop(const DesktopImage& desktop, RectI selection) {
    ImageData result;
    selection = Intersect(selection, desktop.Bounds());
    if (!desktop.Valid() || selection.Empty()) {
        return result;
    }
    result.width = selection.Width();
    result.height = selection.Height();
    result.stride = result.width * 4;
    result.pixels.resize(static_cast<std::size_t>(result.stride) * result.height);

    const int sourceX = selection.left - desktop.originX;
    const int sourceY = selection.top - desktop.originY;
    for (int row = 0; row < result.height; ++row) {
        const auto* source = desktop.pixels.data() +
            static_cast<std::size_t>(sourceY + row) * desktop.stride +
            static_cast<std::size_t>(sourceX) * 4;
        auto* destination = result.pixels.data() + static_cast<std::size_t>(row) * result.stride;
        std::memcpy(destination, source, static_cast<std::size_t>(result.stride));
    }
    return result;
}

ImageData ImageComposer::Compose(const DesktopImage& desktop, RectI selection,
                                 const std::vector<AnnotationPtr>& annotations) {
    ImageData result = Crop(desktop, selection);
    if (!result.Valid() || annotations.empty()) {
        return result;
    }

    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = result.width;
    info.bmiHeader.biHeight = -result.height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    HDC dc = CreateCompatibleDC(nullptr);
    if (!bitmap || !dc || !bits) {
        if (bitmap) {
            DeleteObject(bitmap);
        }
        if (dc) {
            DeleteDC(dc);
        }
        return {};
    }
    HGDIOBJ previous = SelectObject(dc, bitmap);
    std::memcpy(bits, result.pixels.data(), result.pixels.size());
    RenderContext context{
        dc,
        static_cast<std::uint8_t*>(bits),
        result.width,
        result.height,
        result.stride,
        -selection.left,
        -selection.top,
        false,
    };
    for (const auto& annotation : annotations) {
        annotation->Draw(context);
    }
    std::memcpy(result.pixels.data(), bits, result.pixels.size());
    for (std::size_t index = 3; index < result.pixels.size(); index += 4) {
        result.pixels[index] = 255;
    }
    SelectObject(dc, previous);
    DeleteObject(bitmap);
    DeleteDC(dc);
    return result;
}

}  // namespace snaplite
