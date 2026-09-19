#include "CustomMaps/MapCatalog.h"

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstring>
#include <cwchar>
#include <filesystem>
#include <io.h>
#include <new>
#include <windows.h>

#include <utf8.h>

#undef CompareString

#include "BML/ILogger.h"
#include "PathUtils.h"
#include "StringUtils.h"

namespace {
bool ContainsCaseInsensitiveUtf8(const std::string &text, const std::string &fragment) {
    if (fragment.empty())
        return true;
    if (text.find('\0') != std::string::npos || fragment.find('\0') != std::string::npos)
        return false;

    const auto *textUtf8 = reinterpret_cast<const utf8_int8_t *>(text.c_str());
    const auto *fragmentUtf8 = reinterpret_cast<const utf8_int8_t *>(fragment.c_str());
    return utf8valid(textUtf8) == nullptr && utf8valid(fragmentUtf8) == nullptr &&
           utf8casestr(textUtf8, fragmentUtf8) != nullptr;
}
}

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
    return utils::CompareString(name, rhs.name) < 0;
}

MapCatalog::MapCatalog() : m_Root(std::make_unique<MapEntry>(nullptr, MAP_ENTRY_DIR)) {}

bool MapCatalog::ResolveFile(const std::wstring &mapsDirectory, std::string_view relativePath,
                             std::wstring &path, std::string &error) {
    path.clear();
    error.clear();
    if (mapsDirectory.empty() || relativePath.empty() || relativePath.size() > INT_MAX ||
        relativePath.find('\0') != std::string_view::npos) {
        error = "provide a map path relative to ModLoader/Maps";
        return false;
    }

    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                           relativePath.data(), static_cast<int>(relativePath.size()),
                                           nullptr, 0);
    if (length <= 0) {
        error = "the map path is not valid UTF-8";
        return false;
    }

    std::wstring widePath(static_cast<std::size_t>(length), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, relativePath.data(),
                            static_cast<int>(relativePath.size()), widePath.data(), length) != length) {
        error = "the map path is not valid UTF-8";
        return false;
    }

    const std::filesystem::path requested(widePath);
    if (requested.has_root_path()) {
        error = "use a path relative to ModLoader/Maps";
        return false;
    }
    for (const auto &part : requested) {
        if (part == L"..") {
            error = "parent-directory paths are not allowed";
            return false;
        }
    }
    if (!IsSupportedFileType(widePath)) {
        error = "only .nmo and .cmo maps can be loaded";
        return false;
    }

    const std::wstring candidate = (std::filesystem::path(mapsDirectory) / requested).wstring();
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
    if (candidate.size() >= MAX_PATH) {
        error = "the map path is too long";
        return false;
    }

    const DWORD attributes = GetFileAttributesW(candidate.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY)) {
        error = "map file was not found";
        return false;
    }

    std::wstring rootPath;
    std::wstring filePath;
    if (!utils::TryGetFinalPathW(mapsDirectory, rootPath) ||
        !utils::TryGetFinalPathW(candidate, filePath)) {
        error = "could not resolve the map path";
        return false;
    }
    if (!utils::IsPathInsideRootW(filePath, rootPath)) {
        error = "the map is outside ModLoader/Maps";
        return false;
    }
    if (filePath.size() >= MAX_PATH || !IsSupportedFileType(filePath)) {
        error = "the resolved map path is not a supported file";
        return false;
    }

    path = std::move(filePath);
    return true;
}

bool MapCatalog::Refresh(const std::wstring &path, int maxDepth, ILogger *logger) {
    auto newRoot = std::unique_ptr<MapEntry>(new(std::nothrow) MapEntry(nullptr, MAP_ENTRY_DIR));
    if (!newRoot) {
        if (logger)
            logger->Error("Failed to allocate memory for maps root");
        return false;
    }

    newRoot->name = "Maps";
    newRoot->path = path;

    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        const DWORD error = GetLastError();
        if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) {
            if (logger) {
                logger->Error("Failed to inspect maps directory %s: Windows error %lu",
                              utils::Utf16ToUtf8(path).c_str(),
                              static_cast<unsigned long>(error));
            }
            return false;
        }
    } else if ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        if (logger)
            logger->Error("Maps path is not a directory: %s", utils::Utf16ToUtf8(path).c_str());
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
        if (!ContainsCaseInsensitiveUtf8(name, search))
            continue;
        paths.push_back(name);
    }
    return paths;
}

MapCatalog::ScanResult MapCatalog::ExploreMaps(MapEntry *maps, int depth, ILogger *logger,
                                               const std::wstring &rootPath) {
    if (!maps || maps->type != MAP_ENTRY_DIR || maps->path.empty())
        return ScanResult::Failure;
    if (depth <= 0)
        return ScanResult::Success;

    std::wstring searchPath = maps->path;
    if (searchPath.length() > MAX_PATH) {
        if (logger)
            logger->Error("Path too long: %s", utils::Utf16ToUtf8(maps->path).c_str());
        return ScanResult::Failure;
    }
    searchPath.append(L"\\*");

    _wfinddata_t fileinfo = {};
    const intptr_t handle = _wfindfirst(searchPath.c_str(), &fileinfo);
    if (handle == -1) {
        const int error = errno;
        if (error != ENOENT && logger) {
            const std::string message = "Failed to explore maps directory " +
                                        utils::Utf16ToUtf8(maps->path) + ": " + strerror(error);
            logger->Error("%s", message.c_str());
        }
        return error == ENOENT ? ScanResult::Success : ScanResult::Failure;
    }

    struct FileHandleGuard {
        intptr_t Handle;
        ~FileHandleGuard() { if (Handle != -1) _findclose(Handle); }
    } guard{handle};

    try {
        do {
            if (wcscmp(fileinfo.name, L".") == 0 || wcscmp(fileinfo.name, L"..") == 0)
                continue;

            std::wstring fullPath = maps->path;
            if (fullPath.length() + wcslen(fileinfo.name) + 2 > MAX_PATH) {
                if (logger) {
                    logger->Warn("Skipping file with path too long: %s\\%s",
                                 utils::Utf16ToUtf8(maps->path).c_str(),
                                 utils::Utf16ToUtf8(fileinfo.name).c_str());
                }
                continue;
            }
            fullPath.append(L"\\").append(fileinfo.name);

            std::wstring finalPath;
            if (!utils::TryGetFinalPathW(fullPath, finalPath) ||
                !utils::IsPathInsideRootW(finalPath, rootPath))
                continue;

            if (wcschr(fileinfo.name, L'\\') || wcschr(fileinfo.name, L'/') ||
                wcsstr(fileinfo.name, L"..")) {
                continue;
            }

            if (fileinfo.attrib & _A_SUBDIR) {
                auto entry = std::unique_ptr<MapEntry>(new(std::nothrow) MapEntry(maps, MAP_ENTRY_DIR));
                if (!entry) {
                    if (logger)
                        logger->Error("Memory allocation failed for directory entry");
                    return ScanResult::Failure;
                }
                entry->name = utils::Utf16ToUtf8(fileinfo.name);
                entry->path = fullPath;
                maps->children.push_back(entry.get());
                MapEntry *child = entry.release();
                if (ExploreMaps(child, depth - 1, logger, rootPath) == ScanResult::Failure)
                    return ScanResult::Failure;
            } else if (IsSupportedFileType(fileinfo.name) && IsSupportedFileType(finalPath)) {
                std::wstring filename = fileinfo.name;
                const size_t dotPos = filename.find_last_of(L'.');
                if (dotPos != std::wstring::npos)
                    filename = filename.substr(0, dotPos);

                auto entry = std::unique_ptr<MapEntry>(new(std::nothrow) MapEntry(maps, MAP_ENTRY_FILE));
                if (!entry) {
                    if (logger)
                        logger->Error("Memory allocation failed for file entry");
                    return ScanResult::Failure;
                }
                entry->name = utils::Utf16ToUtf8(filename);
                entry->path = fullPath;
                maps->children.push_back(entry.get());
                entry.release();
            }
        } while (_wfindnext(handle, &fileinfo) == 0);

        const int enumerationError = errno;
        if (enumerationError != ENOENT) {
            if (logger) {
                logger->Error("Failed while enumerating maps directory %s: %s",
                              utils::Utf16ToUtf8(maps->path).c_str(),
                              strerror(enumerationError));
            }
            return ScanResult::Failure;
        }
    } catch (const std::exception &e) {
        if (logger)
            logger->Error("Exception while exploring maps: %s", e.what());
        return ScanResult::Failure;
    } catch (...) {
        if (logger)
            logger->Error("Unknown exception while exploring maps");
        return ScanResult::Failure;
    }

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
