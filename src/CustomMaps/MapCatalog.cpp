#include "CustomMaps/MapCatalog.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <cwchar>
#include <io.h>
#include <new>
#include <windows.h>

#undef CompareString

#include "BML/ILogger.h"
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
    return utils::CompareString(name, rhs.name) < 0;
}

MapCatalog::MapCatalog() : m_Root(std::make_unique<MapEntry>(nullptr, MAP_ENTRY_DIR)) {}

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
    } else if (ExploreMaps(newRoot.get(), maxDepth, logger) == ScanResult::Failure) {
        return false;
    }

    m_Root = std::move(newRoot);
    return true;
}

MapCatalog::ScanResult MapCatalog::ExploreMaps(MapEntry *maps, int depth, ILogger *logger) {
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
                if (ExploreMaps(child, depth - 1, logger) == ScanResult::Failure)
                    return ScanResult::Failure;
            } else if (IsSupportedFileType(fileinfo.name)) {
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
