#include "ConfigStore.h"
#include "ApiRegistrationMacros.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <shared_mutex>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#include "Context.h"
#include "ModHandle.h"
#include "ModManifest.h"
#include "CoreErrors.h"
#include "Logging.h"

#include "StringUtils.h"

namespace BML::Core {
    namespace {
        enum class ConfigHookPhase { Pre, Post };

        constexpr char kTypeBool[] = "bool";
        constexpr char kTypeInt[] = "int";
        constexpr char kTypeFloat[] = "float";
        constexpr char kTypeString[] = "string";

        std::vector<BML_ConfigLoadHooks> g_ConfigHooks;
        std::shared_mutex g_ConfigHooksMutex;

        // Use CoreLog for consistent logging
        inline void DebugLog(const std::string &message) {
            CoreLog(BML_LOG_DEBUG, "config.store", "%s", message.c_str());
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

        std::wstring SanitizeFileName(std::wstring name) {
            if (name.empty()) {
                return std::wstring(L"mod");
            }

            constexpr wchar_t whitespace_chars[] = L" \t\r\n.";
            const std::wstring whitespace(whitespace_chars);

            auto trim = [&](bool from_start) {
                size_t pos = from_start ? name.find_first_not_of(whitespace)
                                        : name.find_last_not_of(whitespace);
                if (pos == std::wstring::npos) {
                    name.clear();
                } else if (from_start) {
                    name.erase(0, pos);
                } else {
                    name.erase(pos + 1);
                }
            };

            trim(true);
            trim(false);

            static const std::wstring invalid_chars = L"<>:\"/\\|?*";
            for (auto &ch : name) {
                if (ch < 0x20 || invalid_chars.find(ch) != std::wstring::npos) {
                    ch = L'_';
                }
            }

            if (name.empty()) {
                name.assign(L"mod");
            }

            static const std::array<std::wstring, 22> reserved_names = {
                L"CON", L"PRN", L"AUX", L"NUL",
                L"COM1", L"COM2", L"COM3", L"COM4", L"COM5", L"COM6", L"COM7", L"COM8", L"COM9",
                L"LPT1", L"LPT2", L"LPT3", L"LPT4", L"LPT5", L"LPT6", L"LPT7", L"LPT8", L"LPT9"
            };

            std::wstring upper = utils::ToUpper(name);
            for (const auto &reserved : reserved_names) {
                if (upper == reserved) {
                    name.push_back(L'_');
                    break;
                }
            }

            return name;
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

        // Get BML_Context for callbacks
        BML_Context bmlCtx = Context::Instance().GetHandle();

        for (const auto &entry : snapshot) {
            auto callback = (phase == ConfigHookPhase::Pre) ? entry.on_pre_load : entry.on_post_load;
            if (callback) {
                // Exception isolation: one failing hook should not abort others
                try {
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

    BML_Result ConfigStore::ResetValue(BML_Mod mod, const BML_ConfigKey *key) {
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

            // Check schema version and perform migration if needed
            int32_t fileSchemaVersion = 1; // Default for files without version field
            if (auto version = root["schema_version"].value<int64_t>()) {
                fileSchemaVersion = static_cast<int32_t>(*version);
            }

            // Perform migration if schema version is outdated
            if (fileSchemaVersion < kConfigSchemaVersion) {
                if (MigrateConfig(root, fileSchemaVersion, kConfigSchemaVersion)) {
                    // Update schema version in the document
                    root.insert_or_assign("schema_version", static_cast<int64_t>(kConfigSchemaVersion));
                    DebugLog("ConfigStore: migrated config from version " +
                        std::to_string(fileSchemaVersion) + " to " +
                        std::to_string(kConfigSchemaVersion));
                } else {
                    DebugLog("ConfigStore: migration failed from version " +
                        std::to_string(fileSchemaVersion) + " to " +
                        std::to_string(kConfigSchemaVersion));
                    // Continue loading with best effort even if migration fails
                }
            }

            if (auto *array = root["entry"].as_array()) {
                for (const toml::node &node : *array) {
                    const auto *entryTable = node.as_table();
                    if (!entryTable)
                        continue;

                    auto category = entryTable->get("category")
                                        ? entryTable->get("category")->value<std::string>()
                                        : std::nullopt;
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
            DebugLog(
                std::string("ConfigStore: parse failed for ") + narrowPath + ": " + std::string(err.description()));
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
        root.insert_or_assign("schema_version", static_cast<int64_t>(kConfigSchemaVersion));

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
                std::filesystem::remove(tempPath, ec); // Cleanup temp file
                return false;
            }
        } catch (const std::exception &ex) {
            auto narrow = path.string();
            DebugLog(std::string("ConfigStore: failed to save ") + narrow + ": " + ex.what());
            std::filesystem::remove(tempPath, ec); // Cleanup temp file on error
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
        safeName = SanitizeFileName(std::move(safeName));
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

    // ========================================================================
    // Batch Operations Implementation
    // ========================================================================

    BML_Result ConfigStore::BatchBegin(BML_Mod mod, BML_ConfigBatch *out_batch) {
        if (!out_batch) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        auto batch_ctx = std::make_unique<ConfigBatchContext>();
        batch_ctx->mod = mod;

        // Generate unique batch ID
        uintptr_t batch_id = m_NextBatchId.fetch_add(1, std::memory_order_relaxed);
        auto *batch_ptr = reinterpret_cast<BML_ConfigBatch_T *>(batch_id);

        {
            std::lock_guard<std::mutex> lock(m_BatchMutex);
            m_Batches[batch_ptr] = std::move(batch_ctx);
        }

        *out_batch = batch_ptr;
        return BML_RESULT_OK;
    }

    BML_Result ConfigStore::BatchSet(BML_ConfigBatch batch, const BML_ConfigKey *key, const BML_ConfigValue *value) {
        if (!batch || !key || !value) {
            return BML_RESULT_INVALID_ARGUMENT;
        }
        if (!ValidateKey(key) || !ValidateValue(value)) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        std::lock_guard<std::mutex> lock(m_BatchMutex);

        auto it = m_Batches.find(batch);
        if (it == m_Batches.end()) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        ConfigBatchContext *ctx = it->second.get();
        if (ctx->committed || ctx->discarded) {
            return BML_RESULT_INVALID_STATE;
        }

        // Create batch entry
        ConfigBatchEntry entry;
        entry.category = key->category;
        entry.name = key->name;
        entry.value.type = value->type;

        switch (value->type) {
        case BML_CONFIG_BOOL:
            entry.value.bool_value = value->data.bool_value;
            break;
        case BML_CONFIG_INT:
            entry.value.int_value = value->data.int_value;
            break;
        case BML_CONFIG_FLOAT:
            entry.value.float_value = value->data.float_value;
            break;
        case BML_CONFIG_STRING:
            entry.value.string_value = value->data.string_value ? value->data.string_value : "";
            break;
        default:
            return BML_RESULT_INVALID_ARGUMENT;
        }

        ctx->entries.push_back(std::move(entry));
        return BML_RESULT_OK;
    }

    BML_Result ConfigStore::BatchCommit(BML_ConfigBatch batch) {
        if (!batch) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        std::unique_ptr<ConfigBatchContext> batch_ctx;

        {
            std::lock_guard<std::mutex> lock(m_BatchMutex);
            auto it = m_Batches.find(batch);
            if (it == m_Batches.end()) {
                return BML_RESULT_INVALID_ARGUMENT;
            }

            ConfigBatchContext *ctx = it->second.get();
            if (ctx->committed || ctx->discarded) {
                return BML_RESULT_INVALID_STATE;
            }

            ctx->committed = true;
            batch_ctx = std::move(it->second);
            m_Batches.erase(it);
        }

        // Apply all changes atomically to the document
        ConfigDocument *doc = GetOrCreateDocument(batch_ctx->mod);
        if (!doc) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        // First ensure the document is loaded (may acquire doc->mutex internally)
        if (!EnsureLoaded(*doc)) {
            return BML_RESULT_IO_ERROR;
        }

        {
            std::unique_lock doc_lock(doc->mutex);

            // Apply all batch entries
            for (const auto &entry : batch_ctx->entries) {
                auto &category = doc->categories[entry.category];
                auto &config_entry = category.entries[entry.name];
                config_entry = entry.value;
            }

            // Save document
            if (!SaveDocument(*doc)) {
                return BML_RESULT_IO_ERROR;
            }
        }

        return BML_RESULT_OK;
    }

    BML_Result ConfigStore::BatchDiscard(BML_ConfigBatch batch) {
        if (!batch) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        std::lock_guard<std::mutex> lock(m_BatchMutex);

        auto it = m_Batches.find(batch);
        if (it == m_Batches.end()) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        ConfigBatchContext *ctx = it->second.get();
        if (ctx->committed || ctx->discarded) {
            return BML_RESULT_INVALID_STATE;
        }

        ctx->discarded = true;
        m_Batches.erase(it);
        return BML_RESULT_OK;
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

    // ========================================================================
    // Migration Implementation
    // ========================================================================

    BML_Result ConfigStore::RegisterMigration(int32_t from_version, int32_t to_version,
                                              ConfigMigrationFn migrate, void *user_data) {
        if (!migrate) {
            return BML_RESULT_INVALID_ARGUMENT;
        }
        if (from_version >= to_version) {
            return BML_RESULT_INVALID_ARGUMENT;
        }
        if (from_version < 0 || to_version < 0) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        std::lock_guard<std::mutex> lock(m_MigrationMutex);

        // Check for duplicate registration
        for (const auto &entry : m_Migrations) {
            if (entry.from_version == from_version && entry.to_version == to_version) {
                return BML_RESULT_ALREADY_EXISTS;
            }
        }

        ConfigMigrationEntry entry;
        entry.from_version = from_version;
        entry.to_version = to_version;
        entry.migrate = migrate;
        entry.user_data = user_data;
        m_Migrations.push_back(entry);

        DebugLog("ConfigStore: registered migration from v" + std::to_string(from_version) +
            " to v" + std::to_string(to_version));
        return BML_RESULT_OK;
    }

    void ConfigStore::ClearMigrations() {
        std::lock_guard<std::mutex> lock(m_MigrationMutex);
        m_Migrations.clear();
    }

    size_t ConfigStore::GetMigrationCount() const {
        std::lock_guard<std::mutex> lock(m_MigrationMutex);
        return m_Migrations.size();
    }

    std::vector<ConfigMigrationEntry> ConfigStore::BuildMigrationPath(int32_t from_version, int32_t to_version) const {
        // Lock is assumed to be held by caller
        std::vector<ConfigMigrationEntry> path;

        if (from_version >= to_version) {
            return path; // No migration needed or invalid
        }

        // Use a greedy approach: find migrations that progress towards target
        // This allows for both direct jumps (v1->v3) and step-by-step (v1->v2->v3)
        int32_t current = from_version;

        while (current < to_version) {
            // Find the best migration from current version
            // Prefer direct jump to target, otherwise closest increment
            const ConfigMigrationEntry *best = nullptr;

            for (const auto &entry : m_Migrations) {
                if (entry.from_version == current && entry.to_version <= to_version) {
                    if (!best || entry.to_version > best->to_version) {
                        best = &entry;
                    }
                }
            }

            if (!best) {
                // No migration path found
                DebugLog("ConfigStore: no migration path from v" + std::to_string(current) +
                    " to v" + std::to_string(to_version));
                return {}; // Return empty to indicate failure
            }

            path.push_back(*best);
            current = best->to_version;
        }

        return path;
    }

    bool ConfigStore::MigrateConfig(toml::table &root, int32_t from_version, int32_t to_version) {
        if (from_version >= to_version) {
            return true; // Nothing to do
        }

        std::vector<ConfigMigrationEntry> migration_path;
        {
            std::lock_guard<std::mutex> lock(m_MigrationMutex);
            migration_path = BuildMigrationPath(from_version, to_version);
        }

        if (migration_path.empty()) {
            // No registered migrations - this is OK for v1 configs
            // They use the current format and don't need migration
            if (from_version == 1 && to_version == kConfigSchemaVersion) {
                return true;
            }
            DebugLog("ConfigStore: no migration path available from v" +
                std::to_string(from_version) + " to v" + std::to_string(to_version));
            return false;
        }

        // Execute migrations in sequence
        for (const auto &migration : migration_path) {
            DebugLog("ConfigStore: executing migration v" + std::to_string(migration.from_version) +
                " -> v" + std::to_string(migration.to_version));

            try {
                if (!migration.migrate(root, migration.from_version, migration.to_version, migration.user_data)) {
                    DebugLog("ConfigStore: migration function returned false");
                    return false;
                }
            } catch (const std::exception &ex) {
                DebugLog(std::string("ConfigStore: migration threw exception: ") + ex.what());
                return false;
            } catch (...) {
                DebugLog("ConfigStore: migration threw unknown exception");
                return false;
            }
        }

        return true;
    }
} // namespace BML::Core
