#include "Platform/ModAbi.h"
#include "BML/Abi.h"
#include "JsonUtils.h"
#include "PathUtils.h"
#include <string_view>

namespace BML::Platform {
bool ValidateModAbi(const std::wstring &modulePath, std::string &error) {
    std::string contents;
    if (!utils::ReadFileBytesW(modulePath + L".abi.json", contents)) {
        error = "Missing native Mod ABI descriptor (.bmodp.abi.json); rebuild and package with the matching BML SDK";
        return false;
    }
    const auto document = utils::JsonDocument::Parse(contents, error);
    auto *root = document.Root();
    if(root&&yyjson_is_obj(root)&&utils::JsonGetInt(root,"schema")==1){error="Old native Mod ABI; migrate to BML_GetModInterface and rebuild with the current SDK";return false;}
    if (!root || !yyjson_is_obj(root) || utils::JsonGetInt(root, "schema") != BML_ABI_SCHEMA) {
        error = "Invalid native Mod ABI descriptor: " + error;
        return false;
    }
    if (utils::JsonGetString(root, "mod_api") != "bml.mod" || utils::JsonGetInt(root, "mod_api_major") != 1) {
        error = "Native Mod requires migration to BML_GetModInterface / bml.mod C ABI 1";
        return false;
    }
    const auto fingerprint=utils::JsonGetString(root,"sdk_abi");
    if(fingerprint.size()!=64||fingerprint.find_first_not_of("0123456789abcdef")!=std::string::npos){error="Missing or invalid diagnostic SDK ABI fingerprint";return false;}
    struct Field { const char *Name; const char *Expected; };
    const Field fields[] = {
        {"runtime", BML_RUNTIME_NAME}, {"platform", BML_TARGET_PLATFORM},
        {"architecture", BML_TARGET_ARCH}, {"host_linkage", BML_HOST_LINKAGE},

    };
    for (const auto &field : fields) {
        const auto actual = utils::JsonGetString(root, field.Name);
        if (actual != field.Expected) {
            error = std::string("Native Mod ABI mismatch for ") + field.Name +
                ": expected '" + field.Expected + "', got '" + actual + "'";
            return false;
        }
    }
    auto *extensions = yyjson_obj_get(root, "extensions");
    if (!extensions || !yyjson_is_obj(extensions)) {
        error = "Invalid native Mod extensions descriptor"; return false;
    }
    if (extensions) {
        yyjson_obj_iter iter = yyjson_obj_iter_with(extensions);
        while (auto *key = yyjson_obj_iter_next(&iter)) {
            auto *value = yyjson_obj_iter_get_val(key);
            const std::string name = yyjson_get_str(key);
            if (!yyjson_is_str(value)) { error = "Invalid extension ABI: " + name; return false; }
            const std::string actual = yyjson_get_str(value);
            if (actual.empty()) { error="Empty native Mod extension ABI: "+name;return false; }
            std::string expected;
            if (name == "engine") expected = std::string(BML_ENGINE_ABI_ID) + "-" + BML_BUILD_CONFIGURATION;
            else if (name == "imgui") expected = std::string(BML_IMGUI_ABI_ID) + "-" + BML_BUILD_CONFIGURATION;
            else { error = "Unsupported native Mod ABI extension: " + name; return false; }
            if (actual != expected) { error = "Native Mod extension ABI mismatch for " + name + ": expected '" + expected + "', got '" + actual + "'"; return false; }
        }
    }
    error.clear();
    return true;
}
}
