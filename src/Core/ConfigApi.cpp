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
} // namespace BML::Core
