#include "ConfigStore.h"
#include "ApiRegistrationMacros.h"
#include "bml_api_ids.h"
#include "bml_capabilities.h"

namespace BML::Core {
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

    BML_Result BML_API_ConfigGetCaps(BML_ConfigStoreCaps *out_caps) {
        if (!out_caps)
            return BML_RESULT_INVALID_ARGUMENT;
        BML_ConfigStoreCaps caps{};
        caps.struct_size = sizeof(BML_ConfigStoreCaps);
        caps.api_version = bmlGetApiVersion();
        caps.feature_flags = BML_CONFIG_CAP_GET |
            BML_CONFIG_CAP_SET |
            BML_CONFIG_CAP_RESET |
            BML_CONFIG_CAP_ENUMERATE |
            BML_CONFIG_CAP_PERSISTENCE |
            BML_CONFIG_CAP_BATCH;
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

    BML_Result BML_API_RegisterConfigLoadHooks(const BML_ConfigLoadHooks *hooks) {
        return RegisterConfigLoadHooks(hooks);
    }

    // Batch operations
    BML_Result BML_API_ConfigBatchBegin(BML_Mod mod, BML_ConfigBatch *out_batch) {
        return ConfigStore::Instance().BatchBegin(mod, out_batch);
    }

    BML_Result BML_API_ConfigBatchSet(BML_ConfigBatch batch, const BML_ConfigKey *key, const BML_ConfigValue *value) {
        return ConfigStore::Instance().BatchSet(batch, key, value);
    }

    BML_Result BML_API_ConfigBatchCommit(BML_ConfigBatch batch) {
        return ConfigStore::Instance().BatchCommit(batch);
    }

    BML_Result BML_API_ConfigBatchDiscard(BML_ConfigBatch batch) {
        return ConfigStore::Instance().BatchDiscard(batch);
    }

    void RegisterConfigApis() {
        BML_BEGIN_API_REGISTRATION();

        // Core config APIs
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlConfigGet, "config", BML_API_ConfigGet, BML_CAP_CONFIG_BASIC);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlConfigSet, "config", BML_API_ConfigSet, BML_CAP_CONFIG_BASIC);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlConfigReset, "config", BML_API_ConfigReset, BML_CAP_CONFIG_BASIC);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlConfigEnumerate, "config", BML_API_ConfigEnumerate, BML_CAP_CONFIG_BASIC);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlConfigGetCaps, "config", BML_API_ConfigGetCaps, BML_CAP_CONFIG_BASIC);

        // Batch operations
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlConfigBatchBegin, "config", BML_API_ConfigBatchBegin, BML_CAP_CONFIG_BASIC);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlConfigBatchSet, "config", BML_API_ConfigBatchSet, BML_CAP_CONFIG_BASIC);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlConfigBatchCommit, "config", BML_API_ConfigBatchCommit, BML_CAP_CONFIG_BASIC);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlConfigBatchDiscard, "config", BML_API_ConfigBatchDiscard, BML_CAP_CONFIG_BASIC);

        // Config hooks registration
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlRegisterConfigLoadHooks, "config", BML_API_RegisterConfigLoadHooks, BML_CAP_CONFIG_BASIC);
    }
} // namespace BML::Core
