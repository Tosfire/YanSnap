#include "export/PngEncoder.h"

#include <windows.h>
#include <wincodec.h>
#include <shlobj.h>

#include <filesystem>
#include <iomanip>
#include <sstream>

namespace snaplite {

namespace {

template <typename T>
void SafeRelease(T*& object) {
    if (object) {
        object->Release();
        object = nullptr;
    }
}

void SetError(std::wstring* error, const wchar_t* message, HRESULT result = S_OK) {
    if (!error) {
        return;
    }
    *error = message;
    if (FAILED(result)) {
        wchar_t code[32]{};
        wsprintfW(code, L" (0x%08lX)", static_cast<unsigned long>(result));
        *error += code;
    }
}

}  // namespace

bool PngEncoder::Save(const std::wstring& path, const ImageData& image, std::wstring* error) {
    if (!image.Valid()) {
        SetError(error, L"输出图像无效");
        return false;
    }

    const HRESULT initializeResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool shouldUninitialize = SUCCEEDED(initializeResult);
    IWICImagingFactory* factory = nullptr;
    IWICStream* stream = nullptr;
    IWICBitmapEncoder* encoder = nullptr;
    IWICBitmapFrameEncode* frame = nullptr;
    IPropertyBag2* properties = nullptr;

    HRESULT result = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                      IID_PPV_ARGS(&factory));
    if (SUCCEEDED(result)) {
        result = factory->CreateStream(&stream);
    }
    if (SUCCEEDED(result)) {
        result = stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE);
    }
    if (SUCCEEDED(result)) {
        result = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder);
    }
    if (SUCCEEDED(result)) {
        result = encoder->Initialize(stream, WICBitmapEncoderNoCache);
    }
    if (SUCCEEDED(result)) {
        result = encoder->CreateNewFrame(&frame, &properties);
    }
    if (SUCCEEDED(result)) {
        result = frame->Initialize(properties);
    }
    if (SUCCEEDED(result)) {
        result = frame->SetSize(static_cast<UINT>(image.width), static_cast<UINT>(image.height));
    }
    WICPixelFormatGUID pixelFormat = GUID_WICPixelFormat32bppBGRA;
    if (SUCCEEDED(result)) {
        result = frame->SetPixelFormat(&pixelFormat);
    }
    if (SUCCEEDED(result) && pixelFormat != GUID_WICPixelFormat32bppBGRA) {
        result = WINCODEC_ERR_UNSUPPORTEDPIXELFORMAT;
    }
    if (SUCCEEDED(result)) {
        result = frame->WritePixels(static_cast<UINT>(image.height), static_cast<UINT>(image.stride),
                                    static_cast<UINT>(image.pixels.size()),
                                    const_cast<BYTE*>(image.pixels.data()));
    }
    if (SUCCEEDED(result)) {
        result = frame->Commit();
    }
    if (SUCCEEDED(result)) {
        result = encoder->Commit();
    }

    SafeRelease(properties);
    SafeRelease(frame);
    SafeRelease(encoder);
    SafeRelease(stream);
    SafeRelease(factory);
    if (shouldUninitialize) {
        CoUninitialize();
    }
    if (FAILED(result)) {
        DeleteFileW(path.c_str());
        SetError(error, L"PNG 写入失败", result);
        return false;
    }
    return true;
}

std::wstring PngEncoder::DefaultScreenshotsDirectory() {
    PWSTR picturesPath = nullptr;
    std::filesystem::path directory;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Pictures, KF_FLAG_CREATE, nullptr, &picturesPath))) {
        directory = std::filesystem::path(picturesPath) / L"Screenshots";
        CoTaskMemFree(picturesPath);
    } else {
        wchar_t fallback[MAX_PATH]{};
        GetEnvironmentVariableW(L"USERPROFILE", fallback, ARRAYSIZE(fallback));
        directory = std::filesystem::path(fallback) / L"Pictures" / L"Screenshots";
    }
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    return directory.wstring();
}

std::wstring PngEncoder::SuggestedFileName(const std::wstring& directory) {
    SYSTEMTIME time{};
    GetLocalTime(&time);
    std::wostringstream baseName;
    baseName << L"Screenshot_" << std::setfill(L'0')
             << std::setw(4) << time.wYear << std::setw(2) << time.wMonth
             << std::setw(2) << time.wDay << L"_"
             << std::setw(2) << time.wHour << std::setw(2) << time.wMinute
             << std::setw(2) << time.wSecond;

    std::filesystem::path candidate = std::filesystem::path(directory) / (baseName.str() + L".png");
    for (int suffix = 2; std::filesystem::exists(candidate); ++suffix) {
        candidate = std::filesystem::path(directory) /
                    (baseName.str() + L"_" + std::to_wstring(suffix) + L".png");
    }
    return candidate.filename().wstring();
}

}  // namespace snaplite

