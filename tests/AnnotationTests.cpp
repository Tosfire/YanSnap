#include <windows.h>

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>

#include "annotation/Annotation.h"
#include "annotation/UndoStack.h"
#include "capture/DesktopCapture.h"
#include "export/ImageComposer.h"
#include "export/PngEncoder.h"

int wmain() {
    snaplite::DesktopImage desktop;
    desktop.width = 320;
    desktop.height = 200;
    desktop.stride = desktop.width * 4;
    desktop.pixels.resize(static_cast<std::size_t>(desktop.stride) * desktop.height);
    for (int y = 0; y < desktop.height; ++y) {
        for (int x = 0; x < desktop.width; ++x) {
            auto* pixel = desktop.pixels.data() + static_cast<std::size_t>(y) * desktop.stride + x * 4;
            pixel[0] = static_cast<std::uint8_t>((x * 3 + y) % 256);
            pixel[1] = static_cast<std::uint8_t>((x + y * 2) % 256);
            pixel[2] = static_cast<std::uint8_t>((x * 2 + y * 3) % 256);
            pixel[3] = 255;
        }
    }

    snaplite::UndoStack stack;
    stack.Add(std::make_shared<snaplite::RectangleAnnotation>(POINT{12, 12}, POINT{150, 90}));
    stack.Add(std::make_shared<snaplite::ArrowAnnotation>(POINT{30, 150}, POINT{210, 45}));
    auto pen = std::make_shared<snaplite::PenAnnotation>(POINT{20, 110});
    pen->AddPoint(POINT{55, 125});
    pen->AddPoint(POINT{95, 112});
    stack.Add(pen);
    stack.Add(std::make_shared<snaplite::TextAnnotation>(POINT{165, 130}, L"中文标注"));
    stack.Add(std::make_shared<snaplite::MosaicAnnotation>(POINT{220, 20}, POINT{310, 110}, 12));

    const bool undoOk = stack.Items().size() == 5 && stack.Undo() && stack.Items().size() == 4 &&
                        stack.Redo() && stack.Items().size() == 5;
    const snaplite::ImageData result =
        snaplite::ImageComposer::Compose(desktop, desktop.Bounds(), stack.Items());
    if (!result.Valid()) {
        return 2;
    }
    const auto pixelAt = [&result](int x, int y) {
        return result.pixels.data() + static_cast<std::size_t>(y) * result.stride + x * 4;
    };
    const auto* first = pixelAt(220, 20);
    bool mosaicOk = true;
    for (int y = 20; y < 32; ++y) {
        for (int x = 220; x < 232; ++x) {
            const auto* pixel = pixelAt(x, y);
            mosaicOk = mosaicOk && pixel[0] == first[0] && pixel[1] == first[1] && pixel[2] == first[2];
        }
    }

    std::wstring error;
    const std::filesystem::path outputDirectory =
        std::filesystem::temp_directory_path() / L"YanSnapTests";
    std::error_code directoryError;
    std::filesystem::create_directories(outputDirectory, directoryError);
    const std::filesystem::path outputPath =
        outputDirectory / L"annotation-smoke.png";
    const bool saved =
        !directoryError &&
        snaplite::PngEncoder::Save(outputPath.wstring(), result, &error);
    std::error_code cleanupError;
    std::filesystem::remove(outputPath, cleanupError);
    std::wcout << L"undo-redo=" << undoOk << L" mosaic=" << mosaicOk
               << L" png=" << saved << L" error=" << error << L"\n";
    return undoOk && mosaicOk && saved ? 0 : 1;
}
