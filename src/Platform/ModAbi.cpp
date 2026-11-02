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
    if (!root || !yyjson_is_obj(root) || utils::JsonGetInt(root, "schema") != BML_ABI_SCHEMA) {
        error = "Invalid native Mod ABI descriptor: " + error;
        return false;
    }
    struct Field { const char *Name; const char *Expected; };
    const Field fields[] = {
        {"runtime", BML_RUNTIME_NAME}, {"platform", BML_TARGET_PLATFORM},
        {"architecture", BML_TARGET_ARCH}, {"host_linkage", BML_HOST_LINKAGE},
        {"sdk_abi", BML_SDK_ABI_ID}, {"configuration", BML_BUILD_CONFIGURATION},
    };
    for (const auto &field : fields) {
        const auto actual = utils::JsonGetString(root, field.Name);
        if (actual != field.Expected) {
            error = std::string("Native Mod ABI mismatch for ") + field.Name +
                ": expected '" + field.Expected + "', got '" + actual + "'";
            return false;
        }
    }
    error.clear();
    return true;
}
}
