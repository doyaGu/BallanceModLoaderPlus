#include "imgui_internal.h"
#include <SDL3/SDL_iostream.h>
#include <limits>

ImFileHandle ImFileOpen(const char *path, const char *mode) { return SDL_IOFromFile(path, mode); }
bool ImFileClose(ImFileHandle file) { return SDL_CloseIO(file); }
ImU64 ImFileGetSize(ImFileHandle file) { return static_cast<ImU64>(SDL_GetIOSize(file)); }
ImU64 ImFileRead(void *data, ImU64 size, ImU64 count, ImFileHandle file) {
    if (!size || count > std::numeric_limits<size_t>::max() / size) return 0;
    return SDL_ReadIO(file, data, static_cast<size_t>(size * count)) / size;
}
ImU64 ImFileWrite(const void *data, ImU64 size, ImU64 count, ImFileHandle file) {
    if (!size || count > std::numeric_limits<size_t>::max() / size) return 0;
    return SDL_WriteIO(file, data, static_cast<size_t>(size * count)) / size;
}
