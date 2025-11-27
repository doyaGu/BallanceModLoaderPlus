#ifndef BML_CORE_CONFIG_STORE_H
#define BML_CORE_CONFIG_STORE_H

#include <atomic>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>

#include "bml_config.h"
#include "bml_errors.h"
#include "bml_extension.h"

#include "ModHandle.h"

namespace BML::Core {

struct ModManifest;

using ::BML_Mod_T;

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

class ConfigStore {
public:
    static ConfigStore &Instance();

    BML_Result GetValue(BML_Mod mod,
                        const BML_ConfigKey *key,
                        BML_ConfigValue *out_value);

    BML_Result SetValue(BML_Mod mod,
                        const BML_ConfigKey *key,
                        const BML_ConfigValue *value);

    BML_Result ResetValue(BML_Mod mod,
                          const BML_ConfigKey *key);

    BML_Result EnumerateValues(BML_Mod mod,
                               BML_ConfigEnumCallback callback,
                               void *user_data);

    void FlushAndRelease(BML_Mod mod);

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

    mutable std::shared_mutex m_DocumentsMutex;
    std::unordered_map<BML_Mod, std::unique_ptr<ConfigDocument>> m_Documents;
};

void RegisterConfigApis();
BML_Result RegisterConfigLoadHooks(const BML_ConfigLoadHooks *hooks);

} // namespace BML::Core

#endif // BML_CORE_CONFIG_STORE_H
