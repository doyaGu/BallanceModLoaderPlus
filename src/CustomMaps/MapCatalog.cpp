#include "CustomMaps/MapCatalog.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <cwchar>
#include <SDL3/SDL_filesystem.h>
#include "Platform/FileSystemNative.h"
#include "Platform/FileError.h"

#include "Logging/Logger.h"
#include "PathUtils.h"
#include "StringUtils.h"

MapEntry::~MapEntry() {
    if (m_BeingDeleted)
        return;
    m_BeingDeleted = true;

    for (auto *child : children) {
        if (child && !child->m_BeingDeleted) {
            child->parent = nullptr;
            delete child;
        }
    }
    children.clear();

    if (parent && !parent->m_BeingDeleted) {
        const auto it = std::find(parent->children.begin(), parent->children.end(), this);
        if (it != parent->children.end())
            parent->children.erase(it);
    }
}

bool MapEntry::operator<(const MapEntry &rhs) const {
    if (type != rhs.type)
        return type < rhs.type;
    return utils::CompareLocaleStrings(name, rhs.name) < 0;
}

MapCatalog::MapCatalog() : m_Root(std::make_unique<MapEntry>(nullptr, MAP_ENTRY_DIR)) {}

bool MapCatalog::ResolveFile(const std::wstring &mapsDirectory, std::string_view relativePath,
                             std::wstring &path, std::string &error) {
    path.clear();
    error.clear();
    if (mapsDirectory.empty() || relativePath.empty()) {
        error = "provide a map path relative to ModLoader/Maps";
        return false;
    }

    std::wstring widePath;
    if (!utils::TryUtf8ToUtf16(relativePath, widePath)) {
        error = "the map path is not valid UTF-8";
        return false;
    }

    std::replace(widePath.begin(), widePath.end(), L'\\', L'/');
    if (utils::IsPathRootedW(widePath) || widePath.find(L':') != std::wstring::npos) {
        error = "use a path relative to ModLoader/Maps";
        return false;
    }
    for (std::size_t begin = 0; begin < widePath.size();) {
        auto end = widePath.find(L'/', begin);
        if (end == std::wstring::npos) end = widePath.size();
        if (widePath.substr(begin, end - begin) == L"..") {
            error = "parent-directory paths are not allowed";
            return false;
        }
        begin = end + 1;
    }
    if (!IsSupportedFileType(widePath)) {
        error = "only .nmo and .cmo maps can be loaded";
        return false;
    }

    const std::wstring candidate = utils::CombinePathW(mapsDirectory, widePath);
    return ValidateFile(mapsDirectory, candidate, path, error);
}

bool MapCatalog::ValidateFile(const std::wstring &mapsDirectory, const std::wstring &candidate,
                              std::wstring &path, std::string &error) {
    path.clear();
    error.clear();
    if (mapsDirectory.empty() || candidate.empty() || !IsSupportedFileType(candidate)) {
        error = "only .nmo and .cmo maps can be loaded";
        return false;
    }
    if (!utils::FileExistsW(candidate)) {
        error = "map file was not found";
        return false;
    }
    std::wstring filePath;
    if (!utils::TryGetFinalPathInsideRootW(candidate, mapsDirectory, filePath)) {
        error = "the map could not be resolved inside ModLoader/Maps";
        return false;
    }
    if (!IsSupportedFileType(filePath)
    ) {
        error = "the resolved map path is not a supported file";
        return false;
    }

    path = std::move(filePath);
    return true;
}

bool MapCatalog::Refresh(const std::wstring &path, int maxDepth, Logger *logger) {
    auto newRoot = std::unique_ptr<MapEntry>(new(std::nothrow) MapEntry(nullptr, MAP_ENTRY_DIR));
    if (!newRoot) {
        if (logger)
            logger->Error("Failed to allocate memory for maps root");
        return false;
    }

    newRoot->name = "Maps";
    newRoot->path = path;

    SDL_PathInfo info{};
    const auto nativePath = utils::Utf16ToUtf8(path);
    if (!SDL_GetPathInfo(nativePath.c_str(), &info)) {
        const auto error = BML::Platform::LastFileError();
        if (error != BML::Platform::FileError::NotFound &&
            error != BML::Platform::FileError::PathNotFound) return false;
    } else if (info.type != SDL_PATHTYPE_DIRECTORY) {
        if (logger) logger->Error("Maps path is not a directory: %s", nativePath.c_str());
        return false;
    } else {
        std::wstring rootPath;
        if (!utils::TryGetFinalPathW(path, rootPath) ||
            ExploreMaps(newRoot.get(), maxDepth, logger, rootPath) == ScanResult::Failure)
            return false;
    }
    m_Root = std::move(newRoot);
    return true;
}

std::vector<std::string> MapCatalog::ListFiles(std::string_view fragment, std::size_t limit) const {
    std::vector<std::string> paths;
    if (!m_Root || m_Root->path.empty() || limit == 0)
        return paths;

    const std::string search(fragment);
    std::vector<const MapEntry *> pending{m_Root.get()};
    while (!pending.empty() && paths.size() < limit) {
        const MapEntry *entry = pending.back();
        pending.pop_back();
        if (entry->type == MAP_ENTRY_DIR) {
            for (auto it = entry->children.rbegin(); it != entry->children.rend(); ++it)
                pending.push_back(*it);
            continue;
        }

        if (entry->path.size() <= m_Root->path.size())
            continue;
        std::wstring relative = entry->path.substr(m_Root->path.size());
        if (!relative.empty() && (relative.front() == L'\\' || relative.front() == L'/'))
            relative.erase(relative.begin());

        const std::string name = utils::Utf16ToUtf8(relative);
        if (!utils::ContainsUtf8CaseInsensitive(name, search))
            continue;
        paths.push_back(name);
    }
    return paths;
}

MapCatalog::ScanResult MapCatalog::ExploreMaps(MapEntry *maps, int depth, Logger *logger,
                                               const std::wstring &rootPath) {
    if (!maps || maps->type != MAP_ENTRY_DIR || maps->path.empty())
        return ScanResult::Failure;
    if (depth <= 0)
        return ScanResult::Success;

    struct Scan {
        MapEntry *Maps;
        int Depth;
        Logger *Log;
        const std::wstring &Root;
    } scan{maps, depth, logger, rootPath};
    const auto directory = utils::Utf16ToUtf8(maps->path);
    const bool ok = SDL_EnumerateDirectory(directory.c_str(), [](void *data, const char *, const char *name) {
        auto &scan = *static_cast<Scan *>(data);
        try {
            auto path = utils::CombinePathW(scan.Maps->path, utils::Utf8ToUtf16(name));
            std::wstring finalPath;
            if (!utils::TryGetFinalPathInsideRootW(path, scan.Root, finalPath)) return SDL_ENUM_CONTINUE;
            const bool dir = utils::DirectoryExistsW(path);
            if (!dir && (!IsSupportedFileType(path) || !IsSupportedFileType(finalPath))) return SDL_ENUM_CONTINUE;
            // Do not recurse through directory links (cycles and repeated subtrees).
            if (dir && BML::Platform::IsLink(utils::Utf16ToUtf8(path))) return SDL_ENUM_CONTINUE;
            auto entry = std::make_unique<MapEntry>(scan.Maps, dir ? MAP_ENTRY_DIR : MAP_ENTRY_FILE);
            entry->name = dir ? name : utils::RemoveExtensionUtf8(name);
            entry->path = std::move(path);
            scan.Maps->children.push_back(entry.get());
            auto *child = entry.release();
            if (dir && ExploreMaps(child, scan.Depth - 1, scan.Log, scan.Root) == ScanResult::Failure)
                return SDL_ENUM_FAILURE;
            return SDL_ENUM_CONTINUE;
        } catch (...) { return SDL_ENUM_FAILURE; }
    }, &scan);
    if (!ok) return ScanResult::Failure;
    if (!maps->children.empty()) {
        try {
            std::sort(maps->children.begin(), maps->children.end(),
                      [](const MapEntry *lhs, const MapEntry *rhs) {
                          return lhs && rhs && *lhs < *rhs;
                      });
        } catch (...) {
            if (logger)
                logger->Error("Failed to sort map entries");
            return ScanResult::Failure;
        }
    }
    return ScanResult::Success;
}

bool MapCatalog::IsSupportedFileType(const std::wstring &path) {
    const size_t dotPos = path.find_last_of(L'.');
    if (path.empty() || dotPos == std::wstring::npos || dotPos >= path.length() - 1)
        return false;

    std::wstring extension = path.substr(dotPos);
    std::transform(extension.begin(), extension.end(), extension.begin(), ::towlower);
    return extension == L".nmo" || extension == L".cmo";
}
