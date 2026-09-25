#include "CustomMaps/MapStaging.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <utility>

#include "CKPathManager.h"

#include "CustomMaps/CustomMapLoad.h"
#include "PathUtils.h"

namespace CustomMap {
namespace {

constexpr wchar_t StagingRootName[] = L"BMLPlus.Staging";

std::wstring MakeSessionName() {
    wchar_t name[48]{};
    _snwprintf(name, sizeof(name) / sizeof(name[0]), L"%08lX-%016llX",
               static_cast<unsigned long>(::GetCurrentProcessId()),
               static_cast<unsigned long long>(::GetTickCount64()));
    name[(sizeof(name) / sizeof(name[0])) - 1] = L'\0';
    return name;
}

}

MapStaging::~MapStaging() {
    Shutdown();
}

void MapStaging::Initialize(const std::wstring &gameDirectory,
                            const std::wstring &tempDirectory) {
    Shutdown();

    const std::wstring binDirectory = utils::CombinePathW(gameDirectory, L"Bin");
    m_TempDirectory = tempDirectory;
    m_RootDirectory = utils::CombinePathW(binDirectory, StagingRootName);
    m_RelativeDirectory = utils::CombinePathW(StagingRootName, MakeSessionName());
    m_SessionDirectory = utils::CombinePathW(binDirectory, m_RelativeDirectory);
}

void MapStaging::Shutdown() {
    ReleaseLoaded();
    if (!m_SessionDirectory.empty())
        ::RemoveDirectoryW(m_SessionDirectory.c_str());
    if (!m_RootDirectory.empty())
        ::RemoveDirectoryW(m_RootDirectory.c_str());

    m_TempDirectory.clear();
    m_RootDirectory.clear();
    m_SessionDirectory.clear();
    m_RelativeDirectory.clear();
}

bool MapStaging::Prepare(const std::wstring &sourcePath, std::uint64_t attempt,
                         CKPathManager *pathManager, StagedMap &staged,
                         std::string &error) const {
    staged = {};
    error.clear();
    if (sourcePath.empty() || !utils::FileExistsW(sourcePath)) {
        error = "the source map no longer exists";
        return false;
    }

    const std::wstring extension = utils::GetExtensionW(sourcePath);
    if (_wcsicmp(extension.c_str(), L".nmo") != 0 &&
        _wcsicmp(extension.c_str(), L".cmo") != 0) {
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
        if (utils::CopyFileW(sourcePath, destination) &&
            utils::TryEncodePathForCodePage(relativePath, 20127, staged.LoadPath) &&
            CanResolve(pathManager, staged.LoadPath)) {
            staged.FilePath = destination;
            return true;
        }
        utils::DeleteFileW(destination);
        staged.LoadPath.clear();
    }

    if (m_TempDirectory.empty()) {
        error = "the relative staging path was unavailable and no fallback temp directory exists";
        return false;
    }

    const std::wstring fallback = utils::CombinePathW(
        utils::CombinePathW(m_TempDirectory, L"Maps"), fileName);
    if (!utils::CopyFileW(sourcePath, fallback)) {
        utils::DeleteFileW(fallback);
        error = "neither the game staging directory nor the fallback temp directory is writable";
        return false;
    }

    if (!utils::TryEncodePathForActiveCodePage(fallback, staged.LoadPath)) {
        const std::wstring shortPath = utils::GetShortPathW(fallback);
        if (shortPath.empty() ||
            !utils::TryEncodePathForActiveCodePage(shortPath, staged.LoadPath)) {
            utils::DeleteFileW(fallback);
            error = "the fallback staging path is not representable in the active code page and has no usable 8.3 path";
            return false;
        }
    }

    if (!CanResolve(pathManager, staged.LoadPath)) {
        utils::DeleteFileW(fallback);
        staged.LoadPath.clear();
        error = "Virtools could not resolve the prepared map path";
        return false;
    }

    staged.FilePath = fallback;
    return true;
}

void MapStaging::AdoptLoaded(std::wstring filePath) {
    ReleaseLoaded();
    m_LoadedFile = std::move(filePath);
}

void MapStaging::ReleaseLoaded() {
    if (!m_LoadedFile.empty())
        utils::DeleteFileW(m_LoadedFile);
    m_LoadedFile.clear();
}

bool MapStaging::CanResolve(CKPathManager *pathManager,
                            const std::string &loadPath) {
    if (!pathManager || loadPath.empty())
        return false;

    XString resolved(loadPath.c_str());
    return pathManager->ResolveFileName(resolved, DATA_PATH_IDX, -1) == CK_OK;
}

}
