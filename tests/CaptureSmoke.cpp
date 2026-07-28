#include <windows.h>

#include <iostream>

#include "capture/DesktopCapture.h"
#include "export/ImageComposer.h"
#include "export/PngEncoder.h"

int wmain() {
    SetProcessDPIAware();
    const snaplite::DesktopImage desktop = snaplite::DesktopCapture::Capture(false);
    std::wcout << L"valid=" << desktop.Valid() << L" origin=" << desktop.originX << L","
               << desktop.originY << L" size=" << desktop.width << L"x" << desktop.height
               << L" bytes=" << desktop.pixels.size() << L"\n";
    if (!desktop.Valid()) {
        return 1;
    }
    const int width = desktop.width > 320 ? 320 : desktop.width;
    const int height = desktop.height > 200 ? 200 : desktop.height;
    const snaplite::RectI selection{
        desktop.originX, desktop.originY, desktop.originX + width, desktop.originY + height};
    const snaplite::ImageData image = snaplite::ImageComposer::Crop(desktop, selection);
    const std::wstring path = L"build\\capture-smoke.png";
    std::wstring error;
    const bool saved = snaplite::PngEncoder::Save(path, image, &error);
    std::wcout << L"crop=" << image.width << L"x" << image.height
               << L" save=" << saved << L" error=" << error << L"\n";
    return saved ? 0 : 2;
}
