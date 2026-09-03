#include "PlayerFrameCapture.h"

#include "CKAll.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#ifndef DIRECTDRAW_VERSION
#define DIRECTDRAW_VERSION 0x0700
#endif
#include <ddraw.h>
#include <d3d9.h>

#include <cstddef>
#include <cstring>
#include <vector>

namespace BML::PlayerTest {

namespace {

std::uint8_t ColorComponent(std::uint32_t pixel, std::uint32_t mask) {
    if (!mask)
        return 0;
    unsigned shift = 0;
    while (((mask >> shift) & 1u) == 0u)
        ++shift;
    const std::uint32_t value = (pixel & mask) >> shift;
    const std::uint32_t maximum = mask >> shift;
    return static_cast<std::uint8_t>((value * 255u + maximum / 2u) / maximum);
}

bool WriteFrame(const char *path, const void *sourceData,
                std::ptrdiff_t sourcePitch, std::uint32_t width,
                std::uint32_t height, std::uint32_t bits,
                std::uint32_t redMask, std::uint32_t greenMask,
                std::uint32_t blueMask, long &nativeError) {
    if (!path || !*path || !sourceData || !width || !height ||
        (bits != 16 && bits != 24 && bits != 32)) {
        nativeError = E_INVALIDARG;
        return false;
    }
    const std::uint32_t targetPitch = (width * 3u + 3u) & ~3u;
    std::vector<std::uint8_t> pixels(
        static_cast<std::size_t>(targetPitch) * height, 0);
    const auto *source = static_cast<const std::uint8_t *>(sourceData);
    if (sourcePitch < 0)
        source -= sourcePitch * static_cast<std::ptrdiff_t>(height - 1u);
    const std::uint32_t sourceBytes = bits / 8u;
    for (std::uint32_t y = 0; y < height; ++y) {
        const auto *sourceRow = source +
            sourcePitch * static_cast<std::ptrdiff_t>(y);
        auto *targetRow = pixels.data() +
            static_cast<std::size_t>(targetPitch) * y;
        for (std::uint32_t x = 0; x < width; ++x) {
            std::uint32_t pixel = 0;
            std::memcpy(&pixel, sourceRow + x * sourceBytes, sourceBytes);
            targetRow[x * 3u] = ColorComponent(pixel, blueMask);
            targetRow[x * 3u + 1u] = ColorComponent(pixel, greenMask);
            targetRow[x * 3u + 2u] = ColorComponent(pixel, redMask);
        }
    }

    BITMAPFILEHEADER file{};
    BITMAPINFOHEADER info{};
    info.biSize = sizeof(info);
    info.biWidth = static_cast<LONG>(width);
    info.biHeight = -static_cast<LONG>(height);
    info.biPlanes = 1;
    info.biBitCount = 24;
    info.biCompression = BI_RGB;
    info.biSizeImage = static_cast<DWORD>(pixels.size());
    file.bfType = 0x4d42;
    file.bfOffBits = sizeof(file) + sizeof(info);
    file.bfSize = file.bfOffBits + info.biSizeImage;

    HANDLE output = CreateFileA(path, GENERIC_WRITE, 0, nullptr,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                                nullptr);
    if (output == INVALID_HANDLE_VALUE) {
        nativeError = static_cast<long>(GetLastError());
        return false;
    }
    DWORD written = 0;
    const bool headerWritten =
        WriteFile(output, &file, sizeof(file), &written, nullptr) &&
        written == sizeof(file) &&
        WriteFile(output, &info, sizeof(info), &written, nullptr) &&
        written == sizeof(info);
    const DWORD pixelBytes = static_cast<DWORD>(pixels.size());
    const bool pixelsWritten = headerWritten &&
        WriteFile(output, pixels.data(), pixelBytes,
                  &written, nullptr) &&
        written == pixelBytes;
    DWORD writeError = pixelsWritten ? ERROR_SUCCESS : GetLastError();
    if (!pixelsWritten && writeError == ERROR_SUCCESS)
        writeError = ERROR_WRITE_FAULT;
    CloseHandle(output);
    if (!pixelsWritten) {
        nativeError = static_cast<long>(writeError);
        DeleteFileA(path);
        return false;
    }
    nativeError = 0;
    return true;
}

} // namespace

bool SaveRenderFrame(CKRenderContext *render, const char *path,
                     std::uint32_t &version, long &nativeError) {
    VxDirectXData *directX = render ? render->GetDirectXInfo() : nullptr;
    version = directX ? directX->DxVersion : 0;
    if (!directX) {
        nativeError = E_NOINTERFACE;
        return false;
    }

    if (directX->DxVersion == 0x0900) {
        auto *device = static_cast<IDirect3DDevice9 *>(directX->D3DDevice);
        if (!device) {
            nativeError = E_NOINTERFACE;
            return false;
        }
        IDirect3DSurface9 *backBuffer = nullptr;
        HRESULT result = device->GetBackBuffer(
            0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer);
        if (FAILED(result) || !backBuffer) {
            nativeError = result;
            return false;
        }
        D3DSURFACE_DESC description{};
        result = backBuffer->GetDesc(&description);
        IDirect3DSurface9 *memory = nullptr;
        if (SUCCEEDED(result)) {
            result = device->CreateOffscreenPlainSurface(
                description.Width, description.Height, description.Format,
                D3DPOOL_SYSTEMMEM, &memory, nullptr);
        }
        if (SUCCEEDED(result))
            result = device->GetRenderTargetData(backBuffer, memory);
        D3DLOCKED_RECT locked{};
        if (SUCCEEDED(result))
            result = memory->LockRect(&locked, nullptr, D3DLOCK_READONLY);
        if (FAILED(result)) {
            if (memory)
                memory->Release();
            backBuffer->Release();
            nativeError = result;
            return false;
        }

        std::uint32_t bits = 0;
        std::uint32_t redMask = 0;
        std::uint32_t greenMask = 0;
        std::uint32_t blueMask = 0;
        switch (description.Format) {
        case D3DFMT_A8R8G8B8:
        case D3DFMT_X8R8G8B8:
            bits = 32;
            redMask = 0x00ff0000u;
            greenMask = 0x0000ff00u;
            blueMask = 0x000000ffu;
            break;
        case D3DFMT_R5G6B5:
            bits = 16;
            redMask = 0xf800u;
            greenMask = 0x07e0u;
            blueMask = 0x001fu;
            break;
        case D3DFMT_A1R5G5B5:
        case D3DFMT_X1R5G5B5:
            bits = 16;
            redMask = 0x7c00u;
            greenMask = 0x03e0u;
            blueMask = 0x001fu;
            break;
        default:
            break;
        }
        const bool saved = WriteFrame(
            path, locked.pBits, locked.Pitch, description.Width,
            description.Height, bits, redMask, greenMask, blueMask,
            nativeError);
        memory->UnlockRect();
        memory->Release();
        backBuffer->Release();
        return saved;
    }

    if (directX->DxVersion == 0x0700) {
        if (!directX->DDBackBuffer) {
            nativeError = E_NOINTERFACE;
            return false;
        }
        auto *surface = static_cast<IDirectDrawSurface7 *>(
            directX->DDBackBuffer);
        DDSURFACEDESC2 description{};
        description.dwSize = sizeof(description);
        const HRESULT result = surface->Lock(
            nullptr, &description, DDLOCK_WAIT | DDLOCK_READONLY, nullptr);
        if (FAILED(result)) {
            nativeError = result;
            return false;
        }
        const bool saved = WriteFrame(
            path, description.lpSurface, description.lPitch,
            description.dwWidth, description.dwHeight,
            description.ddpfPixelFormat.dwRGBBitCount,
            description.ddpfPixelFormat.dwRBitMask,
            description.ddpfPixelFormat.dwGBitMask,
            description.ddpfPixelFormat.dwBBitMask, nativeError);
        surface->Unlock(nullptr);
        return saved;
    }

    nativeError = E_NOINTERFACE;
    return false;
}

} // namespace BML::PlayerTest
