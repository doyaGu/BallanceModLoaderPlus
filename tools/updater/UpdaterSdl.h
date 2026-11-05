#pragma once

#include "UpdaterTypes.h"
#include <SDL3/SDL_iostream.h>

namespace bmlupdater {
// Native operations here cover only permissions, locking and durable directory
// flushes, which SDL's filesystem API does not expose.
uint32_t FileMode(const std::wstring &path);
bool SetFileMode(const std::wstring &path, uint32_t mode, std::string &error);
bool SyncDirectory(const std::wstring &path);
bool CopyFileAtomic(const std::wstring &from, const std::wstring &to, uint32_t mode, std::string &error,
                    std::string_view expectedHash = {});
Result CheckPlayerStopped();
Result CheckUpdaterPaths(const UpdaterContext &context);
std::wstring FindPlayer(const std::wstring &gameRoot);
bool IsExecutableFile(const std::wstring &path);
bool IsCompatibleBinary(const std::wstring &path, const UpdateTarget &target, bool executable);
std::string DetectInstalledVersionFromRuntime(const std::wstring &gameRoot);

class UpdaterLock {
public:
    explicit UpdaterLock(const std::wstring &stateRoot);
    ~UpdaterLock();
    UpdaterLock(const UpdaterLock &) = delete;
    UpdaterLock &operator=(const UpdaterLock &) = delete;
    explicit operator bool() const { return m_Stream != nullptr; }
private:
    SDL_IOStream *m_Stream{};
};
}
