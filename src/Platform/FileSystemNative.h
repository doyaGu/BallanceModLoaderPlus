#pragma once
#if BML_RUNTIME_SDL
#include <SDL3/SDL_iostream.h>
#endif
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace BML::Platform {
// SDL owns ordinary filesystem operations. Use native calls for operations
// SDL does not expose or when the caller needs an exact native error code.
std::string CanonicalPath(const std::string &path);
bool IsLink(const std::string &path);
bool MatchesPeImage(const std::filesystem::path &path, std::string_view architecture,
                    bool executable);
bool RemoveFile(const std::string &path);
#ifdef _WIN32
bool ReadFileBytes(const std::wstring &path, std::string &out, std::uint32_t &error);
bool PublishFile(const std::wstring &source, const std::wstring &destination,
                 bool replace, std::uint32_t &error);
bool EncodePathForCodePage(std::wstring_view path, std::uint32_t codePage, std::string &out);
std::wstring ShortPath(const std::wstring &path);
#endif
#if BML_RUNTIME_SDL
bool SyncFile(SDL_IOStream *stream);
#endif
bool PublishFile(const std::string &source, const std::string &destination,
                 bool replace, std::uint32_t &error);
bool SetWorkingDirectory(const std::string &path);
std::string ExecutablePath();
std::uint32_t LastFileError();
std::uint64_t ProcessId();
}
