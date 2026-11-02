#pragma once
#include <SDL3/SDL_iostream.h>
#include <cstdint>
#include <string>

namespace BML::Platform {
// SDL owns ordinary filesystem operations. These operations have no SDL
// equivalent: physical-path resolution, symlink identity, durable flush,
// publication without replacement, cwd mutation and executable filename.
std::string CanonicalPath(const std::string &path);
bool IsLink(const std::string &path);
bool SyncFile(SDL_IOStream *stream);
bool PublishNewFile(const std::string &source, const std::string &destination);
bool SetWorkingDirectory(const std::string &path);
std::string ExecutablePath();
std::uint32_t LastFileError();
std::uint64_t ProcessId();
}
