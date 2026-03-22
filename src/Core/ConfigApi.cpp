#include "ConfigStore.h"
#include "ApiRegistrationMacros.h"

namespace BML::Core {
    BML_Result BML_API_ConfigGet(BML_Mod mod, const BML_ConfigKey *key, BML_ConfigValue *out_value) {
        return Kernel().config->GetValue(mod, key, out_value);
    }

    BML_Result BML_API_ConfigSet(BML_Mod mod, const BML_ConfigKey *key, const BML_ConfigValue *value) {
        return Kernel().config->SetValue(mod, key, value);
    }

    BML_Result BML_API_ConfigReset(BML_Mod mod, const BML_ConfigKey *key) {
        return Kernel().config->ResetValue(mod, key);
    }

    BML_Result BML_API_ConfigEnumerate(BML_Mod mod, BML_ConfigEnumCallback callback, void *user_data) {
        return Kernel().config->EnumerateValues(mod, callback, user_data);
    }

    BML_Result BML_API_RegisterConfigLoadHooks(BML_Mod owner,
                                               const BML_ConfigLoadHooks *hooks) {
        return RegisterConfigLoadHooks(owner, hooks);
    }

    // Batch operations
    BML_Result BML_API_ConfigBatchBegin(BML_Mod mod, BML_ConfigBatch *out_batch) {
        return Kernel().config->BatchBegin(mod, out_batch);
    }

    BML_Result BML_API_ConfigBatchSet(BML_ConfigBatch batch, const BML_ConfigKey *key, const BML_ConfigValue *value) {
        return Kernel().config->BatchSet(batch, key, value);
    }

    BML_Result BML_API_ConfigBatchCommit(BML_ConfigBatch batch) {
        return Kernel().config->BatchCommit(batch);
    }

    BML_Result BML_API_ConfigBatchDiscard(BML_ConfigBatch batch) {
        return Kernel().config->BatchDiscard(batch);
    }

    // -- Config Typed Shortcuts --

    BML_Result BML_API_ConfigGetInt(BML_Mod mod, const char *category,
                                     const char *name, int32_t default_value,
                                     int32_t *out_value) {
        if (!category || !name || !out_value) return BML_RESULT_INVALID_ARGUMENT;
        BML_ConfigKey key = BML_CONFIG_KEY_INIT(category, name);
        BML_ConfigValue value{};
        value.struct_size = sizeof(BML_ConfigValue);
        BML_Result r = Kernel().config->GetValue(mod, &key, &value);
        if (r == BML_RESULT_NOT_FOUND) {
            *out_value = default_value;
            return BML_RESULT_OK;
        }
        if (r != BML_RESULT_OK) return r;
        if (value.type != BML_CONFIG_INT) return BML_RESULT_CONFIG_TYPE_MISMATCH;
        *out_value = value.data.int_value;
        return BML_RESULT_OK;
    }

    BML_Result BML_API_ConfigGetFloat(BML_Mod mod, const char *category,
                                       const char *name, float default_value,
                                       float *out_value) {
        if (!category || !name || !out_value) return BML_RESULT_INVALID_ARGUMENT;
        BML_ConfigKey key = BML_CONFIG_KEY_INIT(category, name);
        BML_ConfigValue value{};
        value.struct_size = sizeof(BML_ConfigValue);
        BML_Result r = Kernel().config->GetValue(mod, &key, &value);
        if (r == BML_RESULT_NOT_FOUND) {
            *out_value = default_value;
            return BML_RESULT_OK;
        }
        if (r != BML_RESULT_OK) return r;
        if (value.type != BML_CONFIG_FLOAT) return BML_RESULT_CONFIG_TYPE_MISMATCH;
        *out_value = value.data.float_value;
        return BML_RESULT_OK;
    }

    BML_Result BML_API_ConfigGetBool(BML_Mod mod, const char *category,
                                      const char *name, BML_Bool default_value,
                                      BML_Bool *out_value) {
        if (!category || !name || !out_value) return BML_RESULT_INVALID_ARGUMENT;
        BML_ConfigKey key = BML_CONFIG_KEY_INIT(category, name);
        BML_ConfigValue value{};
        value.struct_size = sizeof(BML_ConfigValue);
        BML_Result r = Kernel().config->GetValue(mod, &key, &value);
        if (r == BML_RESULT_NOT_FOUND) {
            *out_value = default_value;
            return BML_RESULT_OK;
        }
        if (r != BML_RESULT_OK) return r;
        if (value.type != BML_CONFIG_BOOL) return BML_RESULT_CONFIG_TYPE_MISMATCH;
        *out_value = value.data.bool_value;
        return BML_RESULT_OK;
    }

    BML_Result BML_API_ConfigGetString(BML_Mod mod, const char *category,
                                        const char *name, const char *default_value,
                                        const char **out_value) {
        if (!category || !name || !out_value) return BML_RESULT_INVALID_ARGUMENT;
        BML_ConfigKey key = BML_CONFIG_KEY_INIT(category, name);
        BML_ConfigValue value{};
        value.struct_size = sizeof(BML_ConfigValue);
        BML_Result r = Kernel().config->GetValue(mod, &key, &value);
        if (r == BML_RESULT_NOT_FOUND) {
            *out_value = default_value;
            return BML_RESULT_OK;
        }
        if (r != BML_RESULT_OK) return r;
        if (value.type != BML_CONFIG_STRING) return BML_RESULT_CONFIG_TYPE_MISMATCH;
        *out_value = value.data.string_value;
        return BML_RESULT_OK;
    }

    void RegisterConfigApis() {
        BML_BEGIN_API_REGISTRATION();

        // Core config APIs
        BML_REGISTER_API_GUARDED(bmlConfigGet, "config", BML_API_ConfigGet);
        BML_REGISTER_API_GUARDED(bmlConfigSet, "config", BML_API_ConfigSet);
        BML_REGISTER_API_GUARDED(bmlConfigReset, "config", BML_API_ConfigReset);
        BML_REGISTER_API_GUARDED(bmlConfigEnumerate, "config", BML_API_ConfigEnumerate);

        // Batch operations
        BML_REGISTER_API_GUARDED(bmlConfigBatchBegin, "config", BML_API_ConfigBatchBegin);
        BML_REGISTER_API_GUARDED(bmlConfigBatchSet, "config", BML_API_ConfigBatchSet);
        BML_REGISTER_API_GUARDED(bmlConfigBatchCommit, "config", BML_API_ConfigBatchCommit);
        BML_REGISTER_API_GUARDED(bmlConfigBatchDiscard, "config", BML_API_ConfigBatchDiscard);

        // Config hooks registration
        BML_REGISTER_API_GUARDED(
            bmlRegisterConfigLoadHooks, "config", BML_API_RegisterConfigLoadHooks);

        // Typed shortcuts
        BML_REGISTER_API_GUARDED(bmlConfigGetInt, "config", BML_API_ConfigGetInt);
        BML_REGISTER_API_GUARDED(bmlConfigGetFloat, "config", BML_API_ConfigGetFloat);
        BML_REGISTER_API_GUARDED(bmlConfigGetBool, "config", BML_API_ConfigGetBool);
        BML_REGISTER_API_GUARDED(bmlConfigGetString, "config", BML_API_ConfigGetString);
    }
} // namespace BML::Core
