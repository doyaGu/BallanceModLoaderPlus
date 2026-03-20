#include "BindConfig.h"

#include <cassert>
#include <string>

#include "bml_config.h"
#include "../ScriptInstance.h"
#include "../ModuleScope.h"

namespace BML::Scripting {

namespace {

#define CFG (g_Builtins ? g_Builtins->Config : nullptr)

// --- Getters ---

static int Script_ConfigGetInt(const std::string &cat, const std::string &key, int def) {
    auto *inst = t_CurrentScript;
    if (!inst || !CFG) return def;
    BML_ConfigKey ck = BML_CONFIG_KEY_INIT(cat.c_str(), key.c_str());
    BML_ConfigValue val{}; val.struct_size = sizeof(val);
    if (CFG->Get(inst->mod_handle, &ck, &val) != BML_RESULT_OK) return def;
    return (val.type == BML_CONFIG_INT) ? val.data.int_value : def;
}

static float Script_ConfigGetFloat(const std::string &cat, const std::string &key, float def) {
    auto *inst = t_CurrentScript;
    if (!inst || !CFG) return def;
    BML_ConfigKey ck = BML_CONFIG_KEY_INIT(cat.c_str(), key.c_str());
    BML_ConfigValue val{}; val.struct_size = sizeof(val);
    if (CFG->Get(inst->mod_handle, &ck, &val) != BML_RESULT_OK) return def;
    return (val.type == BML_CONFIG_FLOAT) ? val.data.float_value : def;
}

static bool Script_ConfigGetBool(const std::string &cat, const std::string &key, bool def) {
    auto *inst = t_CurrentScript;
    if (!inst || !CFG) return def;
    BML_ConfigKey ck = BML_CONFIG_KEY_INIT(cat.c_str(), key.c_str());
    BML_ConfigValue val{}; val.struct_size = sizeof(val);
    if (CFG->Get(inst->mod_handle, &ck, &val) != BML_RESULT_OK) return def;
    return (val.type == BML_CONFIG_BOOL) ? (val.data.bool_value != BML_FALSE) : def;
}

static std::string Script_ConfigGetString(const std::string &cat, const std::string &key,
                                           const std::string &def) {
    auto *inst = t_CurrentScript;
    if (!inst || !CFG) return def;
    BML_ConfigKey ck = BML_CONFIG_KEY_INIT(cat.c_str(), key.c_str());
    BML_ConfigValue val{}; val.struct_size = sizeof(val);
    if (CFG->Get(inst->mod_handle, &ck, &val) != BML_RESULT_OK) return def;
    return (val.type == BML_CONFIG_STRING && val.data.string_value)
        ? std::string(val.data.string_value) : def;
}

// --- Setters ---

static void Script_ConfigSetInt(const std::string &cat, const std::string &key, int value) {
    auto *inst = t_CurrentScript;
    if (!inst || !CFG) return;
    BML_ConfigKey ck = BML_CONFIG_KEY_INIT(cat.c_str(), key.c_str());
    BML_ConfigValue val{}; val.struct_size = sizeof(val);
    val.type = BML_CONFIG_INT; val.data.int_value = value;
    CFG->Set(inst->mod_handle, &ck, &val);
}

static void Script_ConfigSetFloat(const std::string &cat, const std::string &key, float value) {
    auto *inst = t_CurrentScript;
    if (!inst || !CFG) return;
    BML_ConfigKey ck = BML_CONFIG_KEY_INIT(cat.c_str(), key.c_str());
    BML_ConfigValue val{}; val.struct_size = sizeof(val);
    val.type = BML_CONFIG_FLOAT; val.data.float_value = value;
    CFG->Set(inst->mod_handle, &ck, &val);
}

static void Script_ConfigSetBool(const std::string &cat, const std::string &key, bool value) {
    auto *inst = t_CurrentScript;
    if (!inst || !CFG) return;
    BML_ConfigKey ck = BML_CONFIG_KEY_INIT(cat.c_str(), key.c_str());
    BML_ConfigValue val{}; val.struct_size = sizeof(val);
    val.type = BML_CONFIG_BOOL; val.data.bool_value = value ? BML_TRUE : BML_FALSE;
    CFG->Set(inst->mod_handle, &ck, &val);
}

static void Script_ConfigSetString(const std::string &cat, const std::string &key,
                                    const std::string &value) {
    auto *inst = t_CurrentScript;
    if (!inst || !CFG) return;
    BML_ConfigKey ck = BML_CONFIG_KEY_INIT(cat.c_str(), key.c_str());
    BML_ConfigValue val{}; val.struct_size = sizeof(val);
    val.type = BML_CONFIG_STRING; val.data.string_value = value.c_str();
    CFG->Set(inst->mod_handle, &ck, &val);
}

// --- State persistence (BML_CONFIG_FLAG_INTERNAL, hidden from ModMenu) ---

static void Script_SaveState(const std::string &key, const std::string &value) {
    auto *inst = t_CurrentScript;
    if (!inst || !CFG) return;
    BML_ConfigKey ck = BML_CONFIG_KEY_INIT("_state", key.c_str());
    BML_ConfigValue val{}; val.struct_size = sizeof(val);
    val.type = BML_CONFIG_STRING; val.data.string_value = value.c_str();
    val.flags = BML_CONFIG_FLAG_INTERNAL;
    CFG->Set(inst->mod_handle, &ck, &val);
}

static std::string Script_LoadState(const std::string &key, const std::string &def) {
    auto *inst = t_CurrentScript;
    if (!inst || !CFG) return def;
    BML_ConfigKey ck = BML_CONFIG_KEY_INIT("_state", key.c_str());
    BML_ConfigValue val{}; val.struct_size = sizeof(val);
    if (CFG->Get(inst->mod_handle, &ck, &val) != BML_RESULT_OK) return def;
    return (val.type == BML_CONFIG_STRING && val.data.string_value)
        ? std::string(val.data.string_value) : def;
}

static void Script_SaveStateInt(const std::string &key, int value) {
    auto *inst = t_CurrentScript;
    if (!inst || !CFG) return;
    BML_ConfigKey ck = BML_CONFIG_KEY_INIT("_state", key.c_str());
    BML_ConfigValue val{}; val.struct_size = sizeof(val);
    val.type = BML_CONFIG_INT; val.data.int_value = value;
    val.flags = BML_CONFIG_FLAG_INTERNAL;
    CFG->Set(inst->mod_handle, &ck, &val);
}

static int Script_LoadStateInt(const std::string &key, int def) {
    auto *inst = t_CurrentScript;
    if (!inst || !CFG) return def;
    BML_ConfigKey ck = BML_CONFIG_KEY_INIT("_state", key.c_str());
    BML_ConfigValue val{}; val.struct_size = sizeof(val);
    if (CFG->Get(inst->mod_handle, &ck, &val) != BML_RESULT_OK) return def;
    return (val.type == BML_CONFIG_INT) ? val.data.int_value : def;
}

static void Script_SaveStateFloat(const std::string &key, float value) {
    auto *inst = t_CurrentScript;
    if (!inst || !CFG) return;
    BML_ConfigKey ck = BML_CONFIG_KEY_INIT("_state", key.c_str());
    BML_ConfigValue val{}; val.struct_size = sizeof(val);
    val.type = BML_CONFIG_FLOAT; val.data.float_value = value;
    val.flags = BML_CONFIG_FLAG_INTERNAL;
    CFG->Set(inst->mod_handle, &ck, &val);
}

static float Script_LoadStateFloat(const std::string &key, float def) {
    auto *inst = t_CurrentScript;
    if (!inst || !CFG) return def;
    BML_ConfigKey ck = BML_CONFIG_KEY_INIT("_state", key.c_str());
    BML_ConfigValue val{}; val.struct_size = sizeof(val);
    if (CFG->Get(inst->mod_handle, &ck, &val) != BML_RESULT_OK) return def;
    return (val.type == BML_CONFIG_FLOAT) ? val.data.float_value : def;
}

#undef CFG

} // namespace

void RegisterConfigBindings(asIScriptEngine *engine) {
    int r;
    r = engine->RegisterGlobalFunction("int bmlConfigGetInt(const string &in, const string &in, int = 0)",
        asFUNCTION(Script_ConfigGetInt), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("float bmlConfigGetFloat(const string &in, const string &in, float = 0.0f)",
        asFUNCTION(Script_ConfigGetFloat), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("bool bmlConfigGetBool(const string &in, const string &in, bool = false)",
        asFUNCTION(Script_ConfigGetBool), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("string bmlConfigGetString(const string &in, const string &in, const string &in = \"\")",
        asFUNCTION(Script_ConfigGetString), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("void bmlConfigSetInt(const string &in, const string &in, int)",
        asFUNCTION(Script_ConfigSetInt), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("void bmlConfigSetFloat(const string &in, const string &in, float)",
        asFUNCTION(Script_ConfigSetFloat), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("void bmlConfigSetBool(const string &in, const string &in, bool)",
        asFUNCTION(Script_ConfigSetBool), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("void bmlConfigSetString(const string &in, const string &in, const string &in)",
        asFUNCTION(Script_ConfigSetString), asCALL_CDECL); assert(r >= 0);

    // State persistence
    r = engine->RegisterGlobalFunction("void bmlSaveState(const string &in, const string &in)",
        asFUNCTION(Script_SaveState), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("string bmlLoadState(const string &in, const string &in = \"\")",
        asFUNCTION(Script_LoadState), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("void bmlSaveStateInt(const string &in, int)",
        asFUNCTION(Script_SaveStateInt), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("int bmlLoadStateInt(const string &in, int = 0)",
        asFUNCTION(Script_LoadStateInt), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("void bmlSaveStateFloat(const string &in, float)",
        asFUNCTION(Script_SaveStateFloat), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("float bmlLoadStateFloat(const string &in, float = 0.0f)",
        asFUNCTION(Script_LoadStateFloat), asCALL_CDECL); assert(r >= 0);
}

} // namespace BML::Scripting
