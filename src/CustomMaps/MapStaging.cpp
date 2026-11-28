#include "CustomMaps/MapStaging.h"

#include <chrono>
#include <filesystem>
#include "Platform/FileSystemNative.h"
#include "Platform/FileError.h"
#include "StringUtils.h"

#include <utility>

#include "CKPathManager.h"

#include "CustomMaps/CustomMapLoad.h"
#include "PathUtils.h"

namespace CustomMap {
namespace {

constexpr wchar_t StagingRootName[] = L"BMLPlus.Staging";
#if BML_RUNTIME_SDL
constexpr std::uint32_t StagingCodePage = 65001;
#else
// Relative staging names consist only of ASCII path components.
constexpr std::uint32_t StagingCodePage = 20127;
#endif

std::wstring MakeSessionName() {
    wchar_t name[48]{};
    std::swprintf(name, sizeof(name) / sizeof(name[0]), L"%08lX-%016llX",
                  static_cast<unsigned long>(BML::Platform::ProcessId()),
                  static_cast<unsigned long long>(std::chrono::steady_clock::now().time_since_epoch().count()));
    name[(sizeof(name) / sizeof(name[0])) - 1] = L'\0';
    return name;
}

bool DeleteOwnedFile(const std::wstring &path) {
    if (path.empty())
        return true;
    return utils::DeleteFileW(path) || !utils::PathExistsW(path);
}

}

MapStaging::~MapStaging() {
    Shutdown();
}

void MapStaging::Initialize(const std::wstring &gameDirectory,
                            const std::wstring &tempDirectory) {
    Shutdown();

    const std::wstring binDirectory = utils::GetDirectoryW(utils::GetExecutablePathW());
    m_TempDirectory = tempDirectory;
    m_RootDirectory = utils::CombinePathW(binDirectory, StagingRootName);
    m_RelativeDirectory = utils::CombinePathW(StagingRootName, MakeSessionName());
    m_SessionDirectory = utils::CombinePathW(binDirectory, m_RelativeDirectory);
}

void MapStaging::Shutdown() {
    if (!m_PendingFile.empty())
        DeleteOwnedFile(m_PendingFile);
    m_PendingAttempt = 0;
    m_PendingFile.clear();

    ReleaseLoaded();
    for (const std::wstring &file : m_RetainedFiles)
        DeleteOwnedFile(file);
    m_RetainedFiles.clear();
    std::error_code ignored;
    const auto removeEmptyDirectory = [&](const std::wstring &directory) {
        if (!directory.empty())
            std::filesystem::remove(std::filesystem::u8path(utils::Utf16ToUtf8(directory)), ignored);
    };
    removeEmptyDirectory(m_SessionDirectory);
    removeEmptyDirectory(m_RootDirectory);

    m_TempDirectory.clear();
    m_RootDirectory.clear();
    m_SessionDirectory.clear();
    m_RelativeDirectory.clear();
}

bool MapStaging::Prepare(const std::wstring &sourcePath, std::uint64_t attempt,
                         CKPathManager *pathManager, StagedMap &staged,
                         std::string &error) {
    staged = {};
    error.clear();
    if (attempt == 0) {
        error = "the staging attempt id is invalid";
        return false;
    }
    if (m_PendingAttempt != 0 || !m_PendingFile.empty()) {
        error = "another prepared map is still pending";
        return false;
    }
    if (sourcePath.empty() || !utils::FileExistsW(sourcePath)) {
        error = "the source map no longer exists";
        return false;
    }

    const std::wstring extension = utils::GetExtensionW(sourcePath);
    if (utils::CompareLocaleStrings(utils::Utf16ToUtf8(extension), ".nmo") != 0 &&
        utils::CompareLocaleStrings(utils::Utf16ToUtf8(extension), ".cmo") != 0) {
        error = "only .nmo and .cmo maps can be staged";
        return false;
    }

    std::wstring resolvedSource = utils::ResolvePathW(sourcePath);
    if (resolvedSource.empty())
        resolvedSource = sourcePath;
    const std::wstring fileName = CustomMapLoad::MakeTempFileName(
        resolvedSource, extension, attempt);

    if (!m_SessionDirectory.empty() && !m_RelativeDirectory.empty()) {
        const std::wstring destination = utils::CombinePathW(m_SessionDirectory, fileName);
        const std::wstring relativePath = utils::CombinePathW(m_RelativeDirectory, fileName);
        utils::CreateFileTreeW(m_SessionDirectory);
        if (utils::CopyFileW(sourcePath, destination) &&
            utils::TryEncodePathForCodePage(relativePath, StagingCodePage, staged.LoadPath) &&
            CanResolve(pathManager, staged.LoadPath)) {
            staged.Attempt = attempt;
            m_PendingAttempt = attempt;
            m_PendingFile = destination;
            return true;
        }
        DiscardFile(destination);
        staged.LoadPath.clear();
    }

    if (m_TempDirectory.empty()) {
        error = "the relative staging path was unavailable and no fallback temp directory exists";
        return false;
    }

    const std::wstring fallback = utils::CombinePathW(
        utils::CombinePathW(m_TempDirectory, L"Maps"), fileName);
    utils::CreateFileTreeW(utils::GetDirectoryW(fallback));
    if (!utils::CopyFileW(sourcePath, fallback)) {
        DiscardFile(fallback);
        error = "neither the game staging directory nor the fallback temp directory is writable";
        return false;
    }

#if BML_RUNTIME_SDL
    if (!utils::TryEncodePathForCodePage(fallback, StagingCodePage, staged.LoadPath)) {
        DiscardFile(fallback);
        error = "the fallback staging path is not representable in UTF-8";
        return false;
    }
#else
    if (!utils::TryEncodePathForActiveCodePage(fallback, staged.LoadPath)) {
        const std::wstring shortPath = utils::GetShortPathW(fallback);
        if (!utils::TryEncodePathForActiveCodePage(shortPath, staged.LoadPath)) {
            DiscardFile(fallback);
            error = "the fallback staging path is not representable in the active code page";
            return false;
        }
    }
#endif

    if (!CanResolve(pathManager, staged.LoadPath)) {
        DiscardFile(fallback);
        staged.LoadPath.clear();
        error = "Virtools could not resolve the prepared map path";
        return false;
    }

    staged.Attempt = attempt;
    m_PendingAttempt = attempt;
    m_PendingFile = fallback;
    return true;
}

bool MapStaging::Complete(std::uint64_t attempt, StagedMapCompletion completion) {
    if (attempt == 0 || attempt != m_PendingAttempt || m_PendingFile.empty())
        return false;

    switch (completion) {
    case StagedMapCompletion::Loaded:
        ReleaseLoaded();
        m_LoadedFile = std::move(m_PendingFile);
        break;
    case StagedMapCompletion::Discarded:
        DiscardFile(m_PendingFile);
        break;
    case StagedMapCompletion::Retained:
        m_RetainedFiles.push_back(std::move(m_PendingFile));
        break;
    }

    m_PendingAttempt = 0;
    m_PendingFile.clear();
    return true;
}

void MapStaging::ReleaseLoaded() {
    DiscardFile(m_LoadedFile);
    m_LoadedFile.clear();
}

void MapStaging::DiscardFile(const std::wstring &path) {
    if (!path.empty() && !DeleteOwnedFile(path))
        m_RetainedFiles.push_back(path);
}

bool MapStaging::CanResolve(CKPathManager *pathManager,
                            const std::string &loadPath) {
    if (!pathManager || loadPath.empty())
        return false;

    XString resolved(loadPath.c_str());
    return pathManager->ResolveFileName(resolved, DATA_PATH_IDX, -1) == CK_OK;
}

}
