#ifndef BML_SDKUTILS_H
#define BML_SDKUTILS_H

// Utilities whose signatures mention a public BML SDK type (BMLVersion, IMod,
// ...). StringUtils, PathUtils and the rest stay independent of the SDK
// headers so their consumers inherit neither the Virtools includes nor the
// conformance flags those headers need; a helper moves here exactly when its
// interface requires an SDK type. Everything else follows the other Utils
// categories: namespace utils, usable from anywhere in the loader and tests.

#include <string>

#include "BML/IMod.h"

namespace utils {

// The one parser for version strings a Mod reports about itself, in any of
// the places those strings cross the loader: IBML dependency checks, script
// metadata, hot-reload validation, and the dev-tools dependency panel. It is
// deliberately lenient: it reads up to three numeric groups and skips
// anything that is not a digit:
//   "1.2" -> (1,2,0); "1.2.3-x" -> (1,2,3); "v2" -> (2,0,0);
//   null, empty, or no digits at all -> (0,0,0)
//
// This replaced two parsers that disagreed: this shape, and an
// sscanf("%d.%d.%d") copy that required a leading digit and answered (0,0,0)
// for "v1.2.3". Load-time checks accepted versions the hot-reload check then
// rejected, so the lenient shape won and every caller shares this one.
BMLVersion ParseVersion(const char *value);

inline BMLVersion ParseVersion(const std::string &value) {
    return ParseVersion(value.c_str());
}

// Which module an address belongs to, and where that module sits on disk. The
// handle is an opaque module identity (an HMODULE value): not reference
// counted, so it is only something to compare against, never something to keep
// or to free. Windows.h stays out of this header on purpose -- the .cpp casts
// internally -- so ParseVersion consumers do not inherit it. The
// command-ownership map and the IMC owner resolution both attribute a call
// through these; they used to each carry their own copy of the
// GetModuleHandleExA sequence.
void *GetModuleFromAddress(const void *address);

// The directory a loaded module sits in. GetModuleFileNameW truncates instead
// of failing when the buffer is too small, so the buffer grows until the name
// fits. Empty when the module is null or its name cannot be read.
std::wstring GetModuleDirectory(void *module);

std::wstring GetModuleDirectoryFromAddress(const void *address);

} // namespace utils

#endif // BML_SDKUTILS_H
