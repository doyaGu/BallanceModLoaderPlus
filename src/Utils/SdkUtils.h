#ifndef BML_SDKUTILS_H
#define BML_SDKUTILS_H

#include <string>

namespace utils {

// Which module an address belongs to, and where that module sits on disk. The
// handle is an opaque module identity (an HMODULE value): not reference
// counted, so it is only something to compare against, never something to keep
// or to free. Windows.h stays out of this header on purpose -- the .cpp casts
// internally.
void *GetModuleFromAddress(const void *address);

// The directory a loaded module sits in. GetModuleFileNameW truncates instead
// of failing when the buffer is too small, so the buffer grows until the name
// fits. Empty when the module is null or its name cannot be read.
std::wstring GetModuleDirectory(void *module);

std::wstring GetModuleDirectoryFromAddress(const void *address);

} // namespace utils

#endif // BML_SDKUTILS_H
