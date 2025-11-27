#include "ConfigStore.h"
#include "ApiRegistrationMacros.h"
#include "bml_api_ids.h"
#include "bml_capabilities.h"

#include <Windows.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <limits>
#include <mutex>
#include <shared_mutex>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#include <toml++/toml.hpp>

#include "ApiRegistry.h"
#include "Context.h"
#include "ModHandle.h"
#include "ModManifest.h"
#include "CoreErrors.h"

#include "StringUtils.h"

namespace BML::Core {

namespace {

enum class ConfigHookPhase {
    Pre,
    Post
};

constexpr char kTypeBool[] = "bool";
constexpr char kTypeInt[] = "int";
constexpr char kTypeFloat[] = "float";
constexpr char kTypeString[] = "string";

std::vector<BML_ConfigLoadHooks> g_ConfigHooks;
std::shared_mutex g_ConfigHooksMutex;

void DebugLog(const std::string &message) {
    std::string line = "[BML config] " + message + '\n';
    OutputDebugStringA(line.c_str());
}

BML_Mod ResolveTargetMod(BML_Mod handle) {
    if (handle)
        return handle;
    return Context::GetCurrentModule();
}

bool ValidateKey(const BML_ConfigKey *key) {
    return key && key->category && key->name && key->category[0] != '\0' && key->name[0] != '\0';
}

bool ValidateValue(const BML_ConfigValue *value) {
    if (!value)
        return false;
    switch (value->type) {
    case BML_CONFIG_BOOL:
    case BML_CONFIG_INT:
    case BML_CONFIG_FLOAT:
        return true;
    case BML_CONFIG_STRING:
        return true;
    default:
        return false;
    }
}

void FillValueStruct(const ConfigEntry &entry, BML_ConfigValue &out) {
    out.type = entry.type;
    switch (entry.type) {
    case BML_CONFIG_BOOL:
        out.data.bool_value = entry.bool_value ? BML_TRUE : BML_FALSE;
        break;
    case BML_CONFIG_INT:
        out.data.int_value = entry.int_value;
        break;
    case BML_CONFIG_FLOAT:
        out.data.float_value = entry.float_value;
        break;
    case BML_CONFIG_STRING:
        out.data.string_value = entry.string_value.c_str();
        break;
    default:
        out.data.string_value = "";
        break;
    }
}

std::string TypeToString(BML_ConfigType type) {
    switch (type) {
    case BML_CONFIG_BOOL:
        return kTypeBool;
    case BML_CONFIG_INT:
        return kTypeInt;
    case BML_CONFIG_FLOAT:
        return kTypeFloat;
    case BML_CONFIG_STRING:
        return kTypeString;
    default:
        return "unknown";
    }
}

std::optional<BML_ConfigType> ParseType(const std::string &value) {
    if (value == kTypeBool)
        return BML_CONFIG_BOOL;
    if (value == kTypeInt)
        return BML_CONFIG_INT;
    if (value == kTypeFloat)
        return BML_CONFIG_FLOAT;
    if (value == kTypeString)
        return BML_CONFIG_STRING;
    return std::nullopt;
}

} // namespace

static void DispatchConfigHooks(const ConfigDocument &doc, ConfigHookPhase phase) {
    std::vector<BML_ConfigLoadHooks> snapshot;
    {
        std::shared_lock lock(g_ConfigHooksMutex);
        snapshot = g_ConfigHooks;
    }
    if (snapshot.empty())
        return;

    std::string configPathUtf8;
    const char *configPath = nullptr;
    if (!doc.path.empty()) {
        configPathUtf8 = utils::Utf16ToUtf8(doc.path.c_str());
        configPath = configPathUtf8.c_str();
    }

    BML_ConfigLoadContext loadCtx{};
    loadCtx.struct_size = sizeof(BML_ConfigLoadContext);
    loadCtx.api_version = bmlGetApiVersion();
    loadCtx.mod = doc.owner;
    loadCtx.mod_id = doc.owner ? doc.owner->id.c_str() : nullptr;
    loadCtx.config_path = configPath;

    // Get BML_Context for callbacks (Task 2.3)
    BML_Context bmlCtx = Context::Instance().GetHandle();

    for (const auto &entry : snapshot) {
        auto callback = (phase == ConfigHookPhase::Pre) ? entry.on_pre_load : entry.on_post_load;
        if (callback) {
            // Exception isolation: one failing hook should not abort others
            try {
                // Task 2.3: unified callback signature (ctx, load_ctx, user_data)
                callback(bmlCtx, &loadCtx, entry.user_data);
            } catch (const std::exception &ex) {
                DebugLog(std::string("ConfigStore: hook exception: ") + ex.what());
            } catch (...) {
                DebugLog("ConfigStore: hook threw unknown exception");
            }
        }
    }
}

ConfigStore &ConfigStore::Instance() {
    static ConfigStore store;
    return store;
}

BML_Result ConfigStore::GetValue(BML_Mod mod,
                                 const BML_ConfigKey *key,
                                 BML_ConfigValue *out_value) {
    if (!ValidateKey(key) || !out_value)
        return BML_RESULT_INVALID_ARGUMENT;

    auto *doc = GetOrCreateDocument(ResolveTargetMod(mod));
    if (!doc)
        return BML_RESULT_INVALID_STATE;
    if (!EnsureLoaded(*doc))
        return BML_RESULT_IO_ERROR;

    {
        std::shared_lock docLock(doc->mutex);
        auto *entry = FindEntry(*doc, key);
        if (!entry)
            return BML_RESULT_NOT_FOUND;
        FillValueStruct(*entry, *out_value);
    }
    return BML_RESULT_OK;
}

BML_Result ConfigStore::SetValue(BML_Mod mod,
                                 const BML_ConfigKey *key,
                                 const BML_ConfigValue *value) {
    if (!ValidateKey(key) || !ValidateValue(value))
        return BML_RESULT_INVALID_ARGUMENT;

    auto *doc = GetOrCreateDocument(ResolveTargetMod(mod));
    if (!doc)
        return BML_RESULT_INVALID_STATE;
    if (!EnsureLoaded(*doc))
        return BML_RESULT_IO_ERROR;

    {
        std::unique_lock docLock(doc->mutex);
        ConfigEntry &entry = GetOrCreateEntry(*doc, key);
        entry.type = value->type;

        switch (value->type) {
        case BML_CONFIG_BOOL:
            entry.bool_value = value->data.bool_value ? BML_TRUE : BML_FALSE;
            break;
        case BML_CONFIG_INT:
            entry.int_value = value->data.int_value;
            break;
        case BML_CONFIG_FLOAT:
            entry.float_value = value->data.float_value;
            break;
        case BML_CONFIG_STRING:
            entry.string_value = value->data.string_value ? value->data.string_value : "";
            break;
        default:
            break;
        }

        if (!SaveDocument(*doc))
            return BML_RESULT_IO_ERROR;
    }

    return BML_RESULT_OK;
}

BML_Result ConfigStore::ResetValue(BML_Mod mod,
                                   const BML_ConfigKey *key) {
    if (!ValidateKey(key))
        return BML_RESULT_INVALID_ARGUMENT;

    auto *doc = GetOrCreateDocument(ResolveTargetMod(mod));
    if (!doc)
        return BML_RESULT_INVALID_STATE;
    if (!EnsureLoaded(*doc))
        return BML_RESULT_IO_ERROR;

    {
        std::unique_lock docLock(doc->mutex);
        auto categoryIt = doc->categories.find(key->category);
        if (categoryIt == doc->categories.end())
            return BML_RESULT_NOT_FOUND;

        auto &entries = categoryIt->second.entries;
        auto entryIt = entries.find(key->name);
        if (entryIt == entries.end())
            return BML_RESULT_NOT_FOUND;

        entries.erase(entryIt);
        if (entries.empty()) {
            doc->categories.erase(categoryIt);
        }

        if (!SaveDocument(*doc))
            return BML_RESULT_IO_ERROR;
    }

    return BML_RESULT_OK;
}

BML_Result ConfigStore::EnumerateValues(BML_Mod mod,
                                        BML_ConfigEnumCallback callback,
                                        void *user_data) {
    if (!callback)
        return BML_RESULT_INVALID_ARGUMENT;

    std::vector<std::tuple<std::string, std::string, ConfigEntry>> snapshot;
    auto *doc = GetOrCreateDocument(ResolveTargetMod(mod));
    if (!doc)
        return BML_RESULT_INVALID_STATE;
    if (!EnsureLoaded(*doc))
        return BML_RESULT_IO_ERROR;

    {
        std::shared_lock docLock(doc->mutex);
        snapshot.reserve(doc->categories.size());
        for (const auto &catPair : doc->categories) {
            for (const auto &entryPair : catPair.second.entries) {
                snapshot.emplace_back(catPair.first, entryPair.first, entryPair.second);
            }
        }
    }

    BML_Context bmlCtx = Context::Instance().GetHandle();
    for (auto &item : snapshot) {
        BML_ConfigKey key{};
        key.struct_size = sizeof(BML_ConfigKey);
        key.category = std::get<0>(item).c_str();
        key.name = std::get<1>(item).c_str();
        BML_ConfigValue value{};
        value.struct_size = sizeof(BML_ConfigValue);
        FillValueStruct(std::get<2>(item), value);
        callback(bmlCtx, &key, &value, user_data);
    }

    return BML_RESULT_OK;
}

void ConfigStore::FlushAndRelease(BML_Mod mod) {
    if (!mod)
        return;

    std::unique_ptr<ConfigDocument> doc;
    {
        std::unique_lock lock(m_DocumentsMutex);
        auto it = m_Documents.find(mod);
        if (it == m_Documents.end())
            return;
        doc = std::move(it->second);
        m_Documents.erase(it);
    }

    if (doc) {
        std::unique_lock docLock(doc->mutex);
        SaveDocument(*doc);
    }
}

ConfigDocument *ConfigStore::GetOrCreateDocument(BML_Mod mod) {
    if (!mod)
        return nullptr;

    {
        std::shared_lock readLock(m_DocumentsMutex);
        auto it = m_Documents.find(mod);
        if (it != m_Documents.end())
            return it->second.get();
    }

    std::unique_lock writeLock(m_DocumentsMutex);
    auto existing = m_Documents.find(mod);
    if (existing != m_Documents.end())
        return existing->second.get();

    auto doc = std::make_unique<ConfigDocument>();
    doc->owner = Context::Instance().ResolveModHandle(mod);
    if (!doc->owner || !doc->owner->manifest) {
        DebugLog("ConfigStore: unable to resolve manifest for module");
        return nullptr;
    }

    doc->path = BuildConfigPath(*doc->owner);
    if (doc->path.empty()) {
        DebugLog("ConfigStore: manifest directory not set");
        return nullptr;
    }

    auto *raw = doc.get();
    m_Documents.emplace(mod, std::move(doc));
    return raw;
}

ConfigDocument *ConfigStore::LookupDocument(BML_Mod mod) {
    if (!mod)
        return nullptr;
    std::shared_lock lock(m_DocumentsMutex);
    auto it = m_Documents.find(mod);
    return it != m_Documents.end() ? it->second.get() : nullptr;
}

bool ConfigStore::EnsureLoaded(ConfigDocument &doc) {
    if (doc.loaded.load(std::memory_order_acquire))
        return true;

    std::unique_lock lock(doc.mutex);
    if (doc.loaded.load(std::memory_order_relaxed))
        return true;

    DispatchConfigHooks(doc, ConfigHookPhase::Pre);
    doc.categories.clear();
    if (!LoadDocument(doc)) {
        doc.loaded.store(false, std::memory_order_release);
        return false;
    }
    DispatchConfigHooks(doc, ConfigHookPhase::Post);

    doc.loaded.store(true, std::memory_order_release);
    return true;
}

bool ConfigStore::LoadDocument(ConfigDocument &doc) {
    if (doc.path.empty())
        return false;

    std::filesystem::path path(doc.path);
    if (!std::filesystem::exists(path))
        return true;

    auto narrowPath = utils::Utf16ToUtf8(doc.path.c_str());

    try {
        toml::table root = toml::parse_file(narrowPath.c_str());

        // Check schema version for future migration support
        constexpr int32_t kCurrentSchemaVersion = 1;
        int32_t fileSchemaVersion = 1;  // Default for files without version field
        if (auto version = root["schema_version"].value<int64_t>()) {
            fileSchemaVersion = static_cast<int32_t>(*version);
        }

        // Future: Add migration logic here when schema changes
        // if (fileSchemaVersion < kCurrentSchemaVersion) {
        //     MigrateConfig(root, fileSchemaVersion, kCurrentSchemaVersion);
        // }
        (void)fileSchemaVersion;  // Suppress unused warning for now
        (void)kCurrentSchemaVersion;

        if (auto *array = root["entry"].as_array()) {
            for (const toml::node &node : *array) {
                const auto *entryTable = node.as_table();
                if (!entryTable)
                    continue;

                auto category = entryTable->get("category") ? entryTable->get("category")->value<std::string>() : std::nullopt;
                auto name = entryTable->get("name") ? entryTable->get("name")->value<std::string>() : std::nullopt;
                auto type = entryTable->get("type") ? entryTable->get("type")->value<std::string>() : std::nullopt;
                if (!category || category->empty() || !name || name->empty() || !type)
                    continue;

                auto parsedType = ParseType(*type);
                if (!parsedType)
                    continue;

                ConfigEntry entry;
                entry.type = *parsedType;
                switch (*parsedType) {
                case BML_CONFIG_BOOL: {
                    const auto *valNode = entryTable->get("value");
                    if (!valNode)
                        continue;
                    if (auto val = valNode->value<bool>())
                        entry.bool_value = *val ? BML_TRUE : BML_FALSE;
                    else
                        continue;
                    break;
                }
                case BML_CONFIG_INT: {
                    const auto *valNode = entryTable->get("value");
                    if (!valNode)
                        continue;
                    if (auto val = valNode->value<int64_t>())
                        entry.int_value = static_cast<int32_t>(*val);
                    else
                        continue;
                    break;
                }
                case BML_CONFIG_FLOAT: {
                    const auto *valNode = entryTable->get("value");
                    if (!valNode)
                        continue;
                    if (auto val = valNode->value<double>())
                        entry.float_value = static_cast<float>(*val);
                    else
                        continue;
                    break;
                }
                case BML_CONFIG_STRING: {
                    const auto *valNode = entryTable->get("value");
                    if (!valNode)
                        continue;
                    if (auto val = valNode->value<std::string_view>())
                        entry.string_value.assign(val->begin(), val->end());
                    else
                        continue;
                    break;
                }
                default:
                    continue;
                }

                doc.categories[*category].entries[*name] = std::move(entry);
            }
        }
    } catch (const toml::parse_error &err) {
        DebugLog(std::string("ConfigStore: parse failed for ") + narrowPath + ": " + std::string(err.description()));
        return false;
    } catch (const std::exception &ex) {
        DebugLog(std::string("ConfigStore: error reading ") + narrowPath + ": " + ex.what());
        return false;
    }

    return true;
}

bool ConfigStore::SaveDocument(const ConfigDocument &doc) {
    if (doc.path.empty())
        return false;

    std::filesystem::path path(doc.path);
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
        DebugLog(std::string("ConfigStore: failed to create directory: ") + ec.message());
        return false;
    }

    toml::table root;

    // Write schema version for future migration support
    constexpr int64_t kCurrentSchemaVersion = 1;
    root.insert_or_assign("schema_version", kCurrentSchemaVersion);

    toml::array entries;

    for (const auto &catPair : doc.categories) {
        for (const auto &entryPair : catPair.second.entries) {
            toml::table record;
            record.insert_or_assign("category", catPair.first);
            record.insert_or_assign("name", entryPair.first);
            record.insert_or_assign("type", TypeToString(entryPair.second.type));
            switch (entryPair.second.type) {
            case BML_CONFIG_BOOL:
                record.insert_or_assign("value", entryPair.second.bool_value == BML_TRUE);
                break;
            case BML_CONFIG_INT:
                record.insert_or_assign("value", static_cast<int64_t>(entryPair.second.int_value));
                break;
            case BML_CONFIG_FLOAT:
                record.insert_or_assign("value", static_cast<double>(entryPair.second.float_value));
                break;
            case BML_CONFIG_STRING:
                record.insert_or_assign("value", entryPair.second.string_value);
                break;
            default:
                record.insert_or_assign("value", "");
                break;
            }
            entries.emplace_back(std::move(record));
        }
    }

    root.insert_or_assign("entry", entries);

    // Atomic write: write to temp file then rename to ensure consistency
    std::filesystem::path tempPath = path;
    tempPath += L".tmp";

    try {
        {
            std::ofstream stream(tempPath, std::ios::binary | std::ios::trunc);
            if (!stream.is_open()) {
                DebugLog("ConfigStore: failed to open temp file for writing");
                return false;
            }
            stream << root;
            stream.flush();
            if (!stream.good()) {
                DebugLog("ConfigStore: write failed (disk full?)");
                return false;
            }
        } // Close stream before rename

        // Atomic rename (overwrites existing file)
        std::filesystem::rename(tempPath, path, ec);
        if (ec) {
            DebugLog(std::string("ConfigStore: rename failed: ") + ec.message());
            std::filesystem::remove(tempPath, ec);  // Cleanup temp file
            return false;
        }
    } catch (const std::exception &ex) {
        auto narrow = path.string();
        DebugLog(std::string("ConfigStore: failed to save ") + narrow + ": " + ex.what());
        std::filesystem::remove(tempPath, ec);  // Cleanup temp file on error
        return false;
    }

    return true;
}

std::wstring ConfigStore::BuildConfigPath(const BML_Mod_T &mod) const {
    if (!mod.manifest)
        return {};
    std::filesystem::path base(mod.manifest->directory);
    if (base.empty())
        return {};
    std::filesystem::path dir = base / L"config";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    auto safeName = mod.id.empty() ? std::wstring(L"mod") : utils::ToWString(mod.id);
    std::filesystem::path full = dir / (safeName + L".toml");
    return full.wstring();
}

ConfigEntry *ConfigStore::FindEntry(ConfigDocument &doc, const BML_ConfigKey *key) {
    auto categoryIt = doc.categories.find(key->category);
    if (categoryIt == doc.categories.end())
        return nullptr;
    auto entryIt = categoryIt->second.entries.find(key->name);
    if (entryIt == categoryIt->second.entries.end())
        return nullptr;
    return &entryIt->second;
}

ConfigEntry &ConfigStore::GetOrCreateEntry(ConfigDocument &doc, const BML_ConfigKey *key) {
    auto &category = doc.categories[key->category];
    return category.entries[key->name];
}

// Type-safe accessor helpers
namespace {

BML_Result GetTypedValue(BML_Mod mod, const BML_ConfigKey *key, BML_ConfigType expected, void *out) {
    if (!key || !out)
        return BML_RESULT_INVALID_ARGUMENT;
    
    BML_ConfigValue value{};
    value.struct_size = sizeof(BML_ConfigValue);
    BML_Result result = ConfigStore::Instance().GetValue(mod, key, &value);
    if (result != BML_RESULT_OK)
        return result;
    
    if (value.type != expected)
        return BML_RESULT_CONFIG_TYPE_MISMATCH;
    
    switch (expected) {
    case BML_CONFIG_BOOL:
        *static_cast<BML_Bool *>(out) = value.data.bool_value;
        break;
    case BML_CONFIG_INT:
        *static_cast<int32_t *>(out) = value.data.int_value;
        break;
    case BML_CONFIG_FLOAT:
        *static_cast<float *>(out) = value.data.float_value;
        break;
    case BML_CONFIG_STRING:
        *static_cast<const char **>(out) = value.data.string_value;
        break;
    default:
        return BML_RESULT_INVALID_ARGUMENT;
    }
    return BML_RESULT_OK;
}

BML_Result SetTypedValue(BML_Mod mod, const BML_ConfigKey *key, BML_ConfigType type, const void *val) {
    if (!key)
        return BML_RESULT_INVALID_ARGUMENT;
    
    BML_ConfigValue value{};
    value.struct_size = sizeof(BML_ConfigValue);
    value.type = type;
    
    switch (type) {
    case BML_CONFIG_BOOL:
        value.data.bool_value = *static_cast<const BML_Bool *>(val);
        break;
    case BML_CONFIG_INT:
        value.data.int_value = *static_cast<const int32_t *>(val);
        break;
    case BML_CONFIG_FLOAT:
        value.data.float_value = *static_cast<const float *>(val);
        break;
    case BML_CONFIG_STRING:
        value.data.string_value = static_cast<const char *>(val);
        break;
    default:
        return BML_RESULT_INVALID_ARGUMENT;
    }
    
    return ConfigStore::Instance().SetValue(mod, key, &value);
}

} // namespace

// ============================================================================
// Config API Implementation Functions
// ============================================================================

namespace {

BML_Result BML_API_ConfigGet(BML_Mod mod, const BML_ConfigKey *key, BML_ConfigValue *out_value) {
    return ConfigStore::Instance().GetValue(mod, key, out_value);
}

BML_Result BML_API_ConfigSet(BML_Mod mod, const BML_ConfigKey *key, const BML_ConfigValue *value) {
    return ConfigStore::Instance().SetValue(mod, key, value);
}

BML_Result BML_API_ConfigReset(BML_Mod mod, const BML_ConfigKey *key) {
    return ConfigStore::Instance().ResetValue(mod, key);
}

BML_Result BML_API_ConfigEnumerate(BML_Mod mod, BML_ConfigEnumCallback callback, void *user_data) {
    return ConfigStore::Instance().EnumerateValues(mod, callback, user_data);
}

BML_Result BML_API_GetConfigCaps(BML_ConfigStoreCaps *out_caps) {
    if (!out_caps)
        return BML_RESULT_INVALID_ARGUMENT;
    BML_ConfigStoreCaps caps{};
    caps.struct_size = sizeof(BML_ConfigStoreCaps);
    caps.api_version = bmlGetApiVersion();
    caps.feature_flags = BML_CONFIG_CAP_GET |
                         BML_CONFIG_CAP_SET |
                         BML_CONFIG_CAP_RESET |
                         BML_CONFIG_CAP_ENUMERATE |
                         BML_CONFIG_CAP_PERSISTENCE;
    caps.supported_type_mask = BML_CONFIG_TYPE_MASK(BML_CONFIG_BOOL) |
                                BML_CONFIG_TYPE_MASK(BML_CONFIG_INT) |
                                BML_CONFIG_TYPE_MASK(BML_CONFIG_FLOAT) |
                                BML_CONFIG_TYPE_MASK(BML_CONFIG_STRING);
    caps.max_category_length = (std::numeric_limits<uint32_t>::max)();
    caps.max_name_length = (std::numeric_limits<uint32_t>::max)();
    caps.max_string_bytes = (std::numeric_limits<uint32_t>::max)();
    caps.threading_model = BML_THREADING_FREE;
    *out_caps = caps;
    return BML_RESULT_OK;
}

BML_Result BML_API_ConfigGetInt(BML_Mod mod, const BML_ConfigKey *key, int32_t *out_value) {
    return GetTypedValue(mod, key, BML_CONFIG_INT, out_value);
}

BML_Result BML_API_ConfigGetFloat(BML_Mod mod, const BML_ConfigKey *key, float *out_value) {
    return GetTypedValue(mod, key, BML_CONFIG_FLOAT, out_value);
}

BML_Result BML_API_ConfigGetBool(BML_Mod mod, const BML_ConfigKey *key, BML_Bool *out_value) {
    return GetTypedValue(mod, key, BML_CONFIG_BOOL, out_value);
}

BML_Result BML_API_ConfigGetString(BML_Mod mod, const BML_ConfigKey *key, const char **out_value) {
    return GetTypedValue(mod, key, BML_CONFIG_STRING, out_value);
}

BML_Result BML_API_ConfigSetInt(BML_Mod mod, const BML_ConfigKey *key, int32_t value) {
    return SetTypedValue(mod, key, BML_CONFIG_INT, &value);
}

BML_Result BML_API_ConfigSetFloat(BML_Mod mod, const BML_ConfigKey *key, float value) {
    return SetTypedValue(mod, key, BML_CONFIG_FLOAT, &value);
}

BML_Result BML_API_ConfigSetBool(BML_Mod mod, const BML_ConfigKey *key, BML_Bool value) {
    return SetTypedValue(mod, key, BML_CONFIG_BOOL, &value);
}

BML_Result BML_API_ConfigSetString(BML_Mod mod, const BML_ConfigKey *key, const char *value) {
    return SetTypedValue(mod, key, BML_CONFIG_STRING, value);
}

BML_Result BML_API_RegisterConfigLoadHooks(const BML_ConfigLoadHooks *hooks) {
    return RegisterConfigLoadHooks(hooks);
}

} // namespace

// ============================================================================
// API Registration
// ============================================================================

void RegisterConfigApis() {
    BML_BEGIN_API_REGISTRATION();
    
    // Core config APIs
    BML_REGISTER_API_GUARDED_WITH_CAPS(bmlConfigGet, "config", BML_API_ConfigGet, BML_CAP_CONFIG_BASIC);
    BML_REGISTER_API_GUARDED_WITH_CAPS(bmlConfigSet, "config", BML_API_ConfigSet, BML_CAP_CONFIG_BASIC);
    BML_REGISTER_API_GUARDED_WITH_CAPS(bmlConfigReset, "config", BML_API_ConfigReset, BML_CAP_CONFIG_BASIC);
    BML_REGISTER_API_GUARDED_WITH_CAPS(bmlConfigEnumerate, "config", BML_API_ConfigEnumerate, BML_CAP_CONFIG_BASIC);
    BML_REGISTER_API_GUARDED_WITH_CAPS(bmlGetConfigCaps, "config", BML_API_GetConfigCaps, BML_CAP_CONFIG_BASIC);
    
    // Type-safe accessors
    BML_REGISTER_API_GUARDED_WITH_CAPS(bmlConfigGetInt, "config", BML_API_ConfigGetInt, BML_CAP_CONFIG_BASIC);
    BML_REGISTER_API_GUARDED_WITH_CAPS(bmlConfigGetFloat, "config", BML_API_ConfigGetFloat, BML_CAP_CONFIG_BASIC);
    BML_REGISTER_API_GUARDED_WITH_CAPS(bmlConfigGetBool, "config", BML_API_ConfigGetBool, BML_CAP_CONFIG_BASIC);
    BML_REGISTER_API_GUARDED_WITH_CAPS(bmlConfigGetString, "config", BML_API_ConfigGetString, BML_CAP_CONFIG_BASIC);
    BML_REGISTER_API_GUARDED_WITH_CAPS(bmlConfigSetInt, "config", BML_API_ConfigSetInt, BML_CAP_CONFIG_BASIC);
    BML_REGISTER_API_GUARDED_WITH_CAPS(bmlConfigSetFloat, "config", BML_API_ConfigSetFloat, BML_CAP_CONFIG_BASIC);
    BML_REGISTER_API_GUARDED_WITH_CAPS(bmlConfigSetBool, "config", BML_API_ConfigSetBool, BML_CAP_CONFIG_BASIC);
    BML_REGISTER_API_GUARDED_WITH_CAPS(bmlConfigSetString, "config", BML_API_ConfigSetString, BML_CAP_CONFIG_BASIC);
    
    // Config hooks registration
    BML_REGISTER_API_GUARDED_WITH_CAPS(bmlRegisterConfigLoadHooks, "config", BML_API_RegisterConfigLoadHooks, BML_CAP_CONFIG_BASIC);
}

BML_Result RegisterConfigLoadHooks(const BML_ConfigLoadHooks *hooks) {
    if (!hooks || hooks->struct_size < sizeof(BML_ConfigLoadHooks)) {
        return BML_RESULT_INVALID_ARGUMENT;
    }
    if (!hooks->on_pre_load && !hooks->on_post_load) {
        return BML_RESULT_INVALID_ARGUMENT;
    }

    BML_ConfigLoadHooks copy = *hooks;
    copy.struct_size = sizeof(BML_ConfigLoadHooks);

    {
        std::unique_lock lock(g_ConfigHooksMutex);
        g_ConfigHooks.push_back(copy);
    }

    return BML_RESULT_OK;
}

} // namespace BML::Core
