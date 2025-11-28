#ifndef BML_CORE_CONFIG_STORE_H
#define BML_CORE_CONFIG_STORE_H

#include <atomic>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <toml++/toml.hpp>

#include "bml_config.h"
#include "bml_errors.h"
#include "bml_extension.h"

#include "ModHandle.h"

namespace BML::Core {
    // Current schema version for config files
    constexpr int32_t kConfigSchemaVersion = 1;

    // ========================================================================
    // Config Migration - Support for upgrading config files between versions
    // ========================================================================

    // Migration function signature: receives the TOML root table, source version, target version
    // Returns true if migration was successful
    using ConfigMigrationFn = bool (*)(toml::table &root, int32_t from_version, int32_t to_version, void *user_data);

    struct ConfigMigrationEntry {
        int32_t from_version;      // Source schema version
        int32_t to_version;        // Target schema version
        ConfigMigrationFn migrate; // Migration function
        void *user_data;           // Optional user data passed to migrate function
    };

    struct ConfigEntry {
        BML_ConfigType type{BML_CONFIG_BOOL};
        BML_Bool bool_value{BML_FALSE};
        int32_t int_value{0};
        float float_value{0.0f};
        std::string string_value;
    };

    struct ConfigCategory {
        std::unordered_map<std::string, ConfigEntry> entries;
    };

    struct ConfigDocument {
        BML_Mod_T *owner{nullptr};
        std::wstring path;
        std::atomic<bool> loaded{false};
        mutable std::shared_mutex mutex;
        std::unordered_map<std::string, ConfigCategory> categories;
    };

    // ========================================================================
    // Config Batch - Internal structure for batch operations
    // ========================================================================

    struct ConfigBatchEntry {
        std::string category;
        std::string name;
        ConfigEntry value;
    };

    struct ConfigBatchContext {
        BML_Mod mod;
        std::vector<ConfigBatchEntry> entries;
        bool committed{false};
        bool discarded{false};
    };

    class ConfigStore {
    public:
        static ConfigStore &Instance();

        BML_Result GetValue(BML_Mod mod,
                            const BML_ConfigKey *key,
                            BML_ConfigValue *out_value);

        BML_Result SetValue(BML_Mod mod,
                            const BML_ConfigKey *key,
                            const BML_ConfigValue *value);

        BML_Result ResetValue(BML_Mod mod, const BML_ConfigKey *key);

        BML_Result EnumerateValues(BML_Mod mod,
                                   BML_ConfigEnumCallback callback,
                                   void *user_data);

        void FlushAndRelease(BML_Mod mod);

        // Batch operations
        BML_Result BatchBegin(BML_Mod mod, BML_ConfigBatch *out_batch);
        BML_Result BatchSet(BML_ConfigBatch batch, const BML_ConfigKey *key, const BML_ConfigValue *value);
        BML_Result BatchCommit(BML_ConfigBatch batch);
        BML_Result BatchDiscard(BML_ConfigBatch batch);

        // Migration support
        BML_Result RegisterMigration(int32_t from_version, int32_t to_version,
                                     ConfigMigrationFn migrate, void *user_data = nullptr);
        void ClearMigrations();
        size_t GetMigrationCount() const;

        // For testing: get current schema version
        static constexpr int32_t GetCurrentSchemaVersion() { return kConfigSchemaVersion; }

    private:
        ConfigStore() = default;

        ConfigDocument *GetOrCreateDocument(BML_Mod mod);
        ConfigDocument *LookupDocument(BML_Mod mod);
        bool EnsureLoaded(ConfigDocument &doc);
        bool LoadDocument(ConfigDocument &doc);
        bool SaveDocument(const ConfigDocument &doc);
        std::wstring BuildConfigPath(const BML_Mod_T &mod) const;
        ConfigEntry *FindEntry(ConfigDocument &doc, const BML_ConfigKey *key);
        ConfigEntry &GetOrCreateEntry(ConfigDocument &doc, const BML_ConfigKey *key);

        // Migration helpers
        bool MigrateConfig(toml::table &root, int32_t from_version, int32_t to_version);
        std::vector<ConfigMigrationEntry> BuildMigrationPath(int32_t from_version, int32_t to_version) const;

        mutable std::shared_mutex m_DocumentsMutex;
        std::unordered_map<BML_Mod, std::unique_ptr<ConfigDocument>> m_Documents;

        // Batch management
        mutable std::mutex m_BatchMutex;
        std::unordered_map<BML_ConfigBatch, std::unique_ptr<ConfigBatchContext>> m_Batches;
        std::atomic<uintptr_t> m_NextBatchId{1};

        // Migration registry
        mutable std::mutex m_MigrationMutex;
        std::vector<ConfigMigrationEntry> m_Migrations;
    };

    void RegisterConfigApis();
    BML_Result RegisterConfigLoadHooks(const BML_ConfigLoadHooks *hooks);
} // namespace BML::Core

#endif // BML_CORE_CONFIG_STORE_H
