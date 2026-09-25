#include "UpdaterDoctor.h"

#include <Windows.h>

#include <array>
#include <string>
#include <vector>

#include "StringUtils.h"
#include "PathUtils.h"
#include "UpdaterPaths.h"

namespace bmlupdater {
namespace {
    struct PeImageInfo {
        WORD Machine = 0;
        WORD OptionalMagic = 0;
    };

    bool ReadAt(HANDLE file, std::uint64_t offset, void *buffer, DWORD size) {
        LARGE_INTEGER position{};
        position.QuadPart = static_cast<LONGLONG>(offset);
        if (!SetFilePointerEx(file, position, nullptr, FILE_BEGIN))
            return false;
        DWORD read = 0;
        return ReadFile(file, buffer, size, &read, nullptr) && read == size;
    }

    bool ReadPeImageInfo(const std::wstring &path, PeImageInfo &info) {
        HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                  nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
            return false;

        LARGE_INTEGER size{};
        IMAGE_DOS_HEADER dos{};
        DWORD signature = 0;
        IMAGE_FILE_HEADER header{};
        WORD optionalMagic = 0;
        const bool valid = GetFileSizeEx(file, &size) && size.QuadPart >= sizeof(IMAGE_DOS_HEADER) &&
                           ReadAt(file, 0, &dos, sizeof(dos)) && dos.e_magic == IMAGE_DOS_SIGNATURE &&
                           dos.e_lfanew >= 0 &&
                           static_cast<std::uint64_t>(dos.e_lfanew) + sizeof(signature) + sizeof(header) +
                                   sizeof(optionalMagic) <= static_cast<std::uint64_t>(size.QuadPart) &&
                           ReadAt(file, static_cast<std::uint64_t>(dos.e_lfanew), &signature, sizeof(signature)) &&
                           signature == IMAGE_NT_SIGNATURE &&
                           ReadAt(file, static_cast<std::uint64_t>(dos.e_lfanew) + sizeof(signature),
                                  &header, sizeof(header)) &&
                           header.SizeOfOptionalHeader >= sizeof(optionalMagic) &&
                           ReadAt(file, static_cast<std::uint64_t>(dos.e_lfanew) + sizeof(signature) + sizeof(header),
                                  &optionalMagic, sizeof(optionalMagic));
        CloseHandle(file);
        if (!valid)
            return false;

        info.Machine = header.Machine;
        info.OptionalMagic = optionalMagic;
        return true;
    }

    std::string QueryVersionString(const std::wstring &path, const wchar_t *name) {
        DWORD ignored = 0;
        const DWORD size = GetFileVersionInfoSizeW(path.c_str(), &ignored);
        if (size == 0)
            return {};

        std::vector<unsigned char> data(size);
        if (!GetFileVersionInfoW(path.c_str(), 0, size, data.data()))
            return {};

        struct Translation {
            WORD Language;
            WORD CodePage;
        };

        Translation *translations = nullptr;
        UINT translationBytes = 0;
        if (!VerQueryValueW(data.data(), L"\\VarFileInfo\\Translation",
                            reinterpret_cast<void **>(&translations), &translationBytes) ||
            translationBytes < sizeof(Translation))
            return {};

        wchar_t block[128] = {};
        swprintf_s(block, L"\\StringFileInfo\\%04x%04x\\%s",
                   translations[0].Language, translations[0].CodePage, name);
        wchar_t *value = nullptr;
        UINT valueCharacters = 0;
        if (!VerQueryValueW(data.data(), block, reinterpret_cast<void **>(&value), &valueCharacters) ||
            !value || valueCharacters == 0)
            return {};
        return utils::Utf16ToUtf8(value);
    }

    std::string QueryFileVersion(const std::wstring &path) {
        std::string version = QueryVersionString(path, L"ProductVersion");
        if (version.empty())
            version = QueryVersionString(path, L"FileVersion");
        return version.empty() ? "unknown" : version;
    }

    bool CanLoadSystemLibrary(const wchar_t *name) {
        HMODULE module = LoadLibraryExW(name, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!module)
            return false;
        FreeLibrary(module);
        return true;
    }

    void AddFileDiagnostic(std::vector<std::string> &diagnostics, const char *name,
                           const std::wstring &path, bool &healthy) {
        if (!RegularFileExists(path)) {
            diagnostics.push_back(std::string("file.") + name + "=missing");
            healthy = false;
            return;
        }
        diagnostics.push_back(std::string("file.") + name + "=present; version=" + QueryFileVersion(path));
    }
}

Result RunDoctorChecks(const UpdaterContext &context, std::vector<std::string> &diagnostics) {
    diagnostics.clear();
    diagnostics.push_back("gameRoot=" + PathUtf8(context.gameRoot));
    diagnostics.push_back("stateRoot=" + PathUtf8(context.updaterStateRoot));

    if (!DirectoryExists(context.gameRoot))
        return Result::Failure("Game root does not exist");

    bool healthy = true;
    const std::wstring player = JoinPath(JoinPath(context.gameRoot, L"Bin"), L"Player.exe");
    const std::wstring ck2 = JoinPath(JoinPath(context.gameRoot, L"Bin"), L"CK2.dll");
    const std::wstring render = JoinPath(JoinPath(context.gameRoot, L"RenderEngines"), L"CK2_3D.dll");
    const std::wstring bml = JoinPath(JoinPath(context.gameRoot, L"BuildingBlocks"), L"BMLPlus.dll");
    AddFileDiagnostic(diagnostics, "player", player, healthy);
    AddFileDiagnostic(diagnostics, "ck2", ck2, healthy);
    AddFileDiagnostic(diagnostics, "ck2_3d", render, healthy);
    AddFileDiagnostic(diagnostics, "bml", bml, healthy);

    if (RegularFileExists(bml)) {
        PeImageInfo image;
        if (!ReadPeImageInfo(bml, image)) {
            diagnostics.push_back("runtime.bmlMachine=invalid-pe");
            healthy = false;
        } else if (image.Machine != IMAGE_FILE_MACHINE_I386 || image.OptionalMagic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
            diagnostics.push_back("runtime.bmlMachine=not-i386");
            healthy = false;
        } else {
            diagnostics.push_back("runtime.bmlMachine=i386");
        }
    }

    const std::array<const wchar_t *, 3> runtimes = {L"MSVCP140.dll", L"VCRUNTIME140.dll", L"ucrtbase.dll"};
    for (const wchar_t *runtime : runtimes) {
        const bool available = CanLoadSystemLibrary(runtime);
        diagnostics.push_back("runtime." + utils::Utf16ToUtf8(runtime) + "=" + (available ? "available" : "missing"));
        healthy &= available;
    }

    const bool gameWritable = CanCreateFileInDirectory(context.gameRoot);
    const bool stateWritable = CanCreateFileInDirectory(context.updaterStateRoot);
    diagnostics.push_back(std::string("write.gameRoot=") + (gameWritable ? "ok" : "denied"));
    diagnostics.push_back(std::string("write.stateRoot=") + (stateWritable ? "ok" : "denied"));
    healthy &= gameWritable && stateWritable;

    const std::array<std::wstring, 4> obsoleteFiles = {
        JoinPath(JoinPath(context.gameRoot, L"Bin"), L"BMLPlus.dll"),
        JoinPath(JoinPath(context.gameRoot, L"BuildingBlocks"), L"BML.dll"),
        JoinPath(JoinPath(context.gameRoot, L"BuildingBlocks"), L"BMLPlusDebug.dll"),
        JoinPath(JoinPath(context.gameRoot, L"BuildingBlocks"), L"BMLPlus_d.dll"),
    };
    bool mixedRuntime = false;
    for (const std::wstring &path : obsoleteFiles)
        mixedRuntime |= RegularFileExists(path);
    diagnostics.push_back(std::string("runtime.mixedInstall=") + (mixedRuntime ? "warning" : "none"));

    const UINT codePage = GetACP();
    std::string encodedPath;
    const bool ansiCompatible = utils::TryEncodePathForCodePage(
        context.gameRoot, codePage, encodedPath);
    diagnostics.push_back("path.acp=" + std::to_string(codePage));
    diagnostics.push_back(std::string("path.ansiCompatible=") + (ansiCompatible ? "true" : "false"));
    if (!ansiCompatible) {
        const std::wstring shortPath = utils::GetShortPathW(context.gameRoot);
        const bool shortPathAvailable = !shortPath.empty() && shortPath != context.gameRoot &&
                                        utils::TryEncodePathForCodePage(
                                            shortPath, codePage, encodedPath);
        diagnostics.push_back(std::string("path.shortFallback=") +
                              (shortPathAvailable ? "available" : "unavailable"));
    } else {
        diagnostics.push_back("path.shortFallback=not-needed");
    }

    diagnostics.push_back(PathExists(JoinPath(context.updaterStateRoot, L"installed.manifest.json"))
                              ? "installed manifest found" : "installed manifest missing");
    diagnostics.push_back(PathExists(JoinPath(context.updaterStateRoot, L"pending.json"))
                              ? "pending transaction hint found" : "no pending transaction hint");
    diagnostics.push_back(PathExists(JoinPath(context.updaterStateRoot, L"sources.json"))
                              ? "remote source configured" : "remote source not configured");

    return healthy ? Result::Success("doctor passed") :
                     Result::Failure("doctor found incompatible or missing requirements");
}
} // namespace bmlupdater
