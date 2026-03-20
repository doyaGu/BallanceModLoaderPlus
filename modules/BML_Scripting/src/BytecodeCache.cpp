#include "BytecodeCache.h"

#include <cstdio>
#include <cstring>
#include <fstream>

namespace BML::Scripting {

// FNV-1a 64-bit hash
static uint64_t FnvHash(const void *data, size_t size) {
    uint64_t hash = 0xcbf29ce484222325ULL;
    auto *bytes = static_cast<const uint8_t *>(data);
    for (size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

// Chain an additional hash value into an existing FNV-1a state.
static uint64_t FnvChain(uint64_t hash, const void *data, size_t size) {
    auto *bytes = static_cast<const uint8_t *>(data);
    for (size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

uint64_t HashSourceFile(const std::filesystem::path &path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return 0;

    std::vector<char> content((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
    if (content.empty()) return 0;

    // Include the AS engine version in the hash so cache is invalidated
    // when the engine is updated.
    uint64_t hash = FnvHash(content.data(), content.size());
    int ver = ANGELSCRIPT_VERSION;
    hash ^= FnvHash(&ver, sizeof(ver));
    return hash;
}

// Hash all section files and the AS version into a single combined hash.
static uint64_t HashSectionFiles(const std::vector<std::string> &paths) {
    uint64_t hash = 0xcbf29ce484222325ULL;
    for (const auto &p : paths) {
        std::ifstream file(p, std::ios::binary);
        if (!file) return 0;
        std::vector<char> content((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());
        hash = FnvChain(hash, content.data(), content.size());
    }
    int ver = ANGELSCRIPT_VERSION;
    hash = FnvChain(hash, &ver, sizeof(ver));
    return hash;
}

std::filesystem::path GetCachePath(const std::filesystem::path &script_path) {
    auto result = script_path;
    result.replace_extension(".asc");
    return result;
}

// V2 cache file format:
//   [0..7]    magic "BMLASCV2"
//   [8..11]   section_count (uint32_t)
//   per section: uint16_t path_len + char path[path_len]
//   [...]     combined_hash (uint64_t) — FNV-1a of all section contents + AS version
//   [...]     bytecode blob

static constexpr char kMagicV1[8] = {'B','M','L','A','S','C','V','1'};
static constexpr char kMagicV2[8] = {'B','M','L','A','S','C','V','2'};

// AngelScript bytecode stream adapter for std::vector
class VectorByteStream : public asIBinaryStream {
public:
    std::vector<uint8_t> data;
    size_t read_pos = 0;

    int Write(const void *ptr, asUINT size) override {
        auto *bytes = static_cast<const uint8_t *>(ptr);
        data.insert(data.end(), bytes, bytes + size);
        return 0;
    }

    int Read(void *ptr, asUINT size) override {
        if (read_pos + size > data.size()) return -1;
        std::memcpy(ptr, data.data() + read_pos, size);
        read_pos += size;
        return 0;
    }
};

bool TryLoadFromCache(asIScriptEngine *engine,
                       const char *module_name,
                       const std::filesystem::path &script_path,
                       asIScriptModule **out_module) {
    if (!engine || !module_name || !out_module) return false;
    *out_module = nullptr;

    auto cache_path = GetCachePath(script_path);

    // Check cache exists
    std::error_code ec;
    if (!std::filesystem::exists(cache_path, ec)) return false;

    // Read cache file
    std::ifstream file(cache_path, std::ios::binary);
    if (!file) return false;

    // Validate magic
    char magic[8];
    file.read(magic, 8);
    if (!file.good()) return false; // D6: read guard

    // V1 caches auto-invalidate (different magic)
    if (std::memcmp(magic, kMagicV1, 8) == 0) return false;
    if (std::memcmp(magic, kMagicV2, 8) != 0) return false;

    // Read section count
    uint32_t section_count = 0;
    file.read(reinterpret_cast<char *>(&section_count), sizeof(section_count));
    if (!file.good()) return false; // D6

    // Read section paths
    std::vector<std::string> section_paths;
    section_paths.reserve(section_count);
    for (uint32_t i = 0; i < section_count; ++i) {
        uint16_t path_len = 0;
        file.read(reinterpret_cast<char *>(&path_len), sizeof(path_len));
        if (!file.good()) return false; // D6

        std::string path(path_len, '\0');
        file.read(path.data(), path_len);
        if (!file.good()) return false; // D6

        section_paths.push_back(std::move(path));
    }

    // Read stored combined hash
    uint64_t stored_hash = 0;
    file.read(reinterpret_cast<char *>(&stored_hash), sizeof(stored_hash));
    if (!file.good()) return false; // D6

    // D2: Re-hash all section files and compare
    uint64_t current_hash = HashSectionFiles(section_paths);
    if (current_hash == 0 || stored_hash != current_hash) return false;

    // Read bytecode
    VectorByteStream stream;
    stream.data.assign((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    if (stream.data.empty()) return false;

    // Load bytecode into a new module
    asIScriptModule *mod = engine->GetModule(module_name, asGM_ALWAYS_CREATE);
    if (!mod) return false;

    int r = mod->LoadByteCode(&stream);
    if (r < 0) {
        engine->DiscardModule(module_name);
        return false;
    }

    *out_module = mod;
    return true;
}

void SaveToCache(asIScriptModule *module,
                  const std::filesystem::path &script_path,
                  const std::vector<std::string> &section_paths) {
    if (!module) return;

    VectorByteStream stream;
    int r = module->SaveByteCode(&stream);
    if (r < 0) return;

    // Compute combined hash of all section files
    uint64_t combined_hash = HashSectionFiles(section_paths);

    auto cache_path = GetCachePath(script_path);
    std::ofstream file(cache_path, std::ios::binary | std::ios::trunc);
    if (!file) return;

    // Write V2 header
    file.write(kMagicV2, 8);

    uint32_t section_count = static_cast<uint32_t>(section_paths.size());
    file.write(reinterpret_cast<const char *>(&section_count), sizeof(section_count));

    for (const auto &path : section_paths) {
        uint16_t path_len = static_cast<uint16_t>(path.size());
        file.write(reinterpret_cast<const char *>(&path_len), sizeof(path_len));
        file.write(path.data(), path_len);
    }

    file.write(reinterpret_cast<const char *>(&combined_hash), sizeof(combined_hash));
    file.write(reinterpret_cast<const char *>(stream.data.data()),
               static_cast<std::streamsize>(stream.data.size()));
}

} // namespace BML::Scripting
