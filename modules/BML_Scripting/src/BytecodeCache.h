#ifndef BML_SCRIPTING_BYTECODE_CACHE_H
#define BML_SCRIPTING_BYTECODE_CACHE_H

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <angelscript.h>

namespace BML::Scripting {

// Compute a hash of the script source file for cache validation.
uint64_t HashSourceFile(const std::filesystem::path &path);

// Get the cache path for a given script file (same dir, .asc extension).
std::filesystem::path GetCachePath(const std::filesystem::path &script_path);

// Try loading bytecode from cache. Returns true if cache is valid and loaded.
// On success, the module is ready for use (no Build needed).
bool TryLoadFromCache(asIScriptEngine *engine,
                       const char *module_name,
                       const std::filesystem::path &script_path,
                       asIScriptModule **out_module);

// Save compiled module bytecode to cache (V2: includes dependency tracking).
void SaveToCache(asIScriptModule *module,
                  const std::filesystem::path &script_path,
                  const std::vector<std::string> &section_paths);

} // namespace BML::Scripting

#endif
