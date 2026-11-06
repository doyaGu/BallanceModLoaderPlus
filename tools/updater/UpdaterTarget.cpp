#include "UpdaterTarget.h"

#include <algorithm>
#include <cctype>
#include "UpdaterPaths.h"

namespace bmlupdater {
Result ReadUpdateTarget(yyjson_val *object, UpdateTarget &target) {
    target = {};
    if (!yyjson_is_obj(object)) return Result::Failure("Missing update target");
    auto member = [&](const char *name) {
        auto *value = yyjson_obj_get(object, name);
        return yyjson_is_str(value) ? std::string(yyjson_get_str(value), yyjson_get_len(value)) : std::string();
    };
    target.runtime = member("runtime");
    target.platform = member("platform");
    target.architecture = member("architecture");
    target.hostLinkage = member("host_linkage");
    if (target.runtime != "sdl" ||
        (target.platform != "windows" && target.platform != "linux" && target.platform != "macos") ||
        (target.architecture != "x86" && target.architecture != "x64" && target.architecture != "arm64") ||
        (target.hostLinkage != "dynamic" && target.hostLinkage != "static") ||
        (target.architecture == "x86" && target.platform != "windows"))
        return Result::Failure("Invalid SDL update target");
    return Result::Success();
}

void WriteUpdateTarget(utils::MutableJsonDocument &doc, yyjson_mut_val *root, const UpdateTarget &target) {
    auto *object = doc.CreateObject();
    (void)doc.AddString(object, "runtime", target.runtime);
    (void)doc.AddString(object, "platform", target.platform);
    (void)doc.AddString(object, "architecture", target.architecture);
    (void)doc.AddString(object, "host_linkage", target.hostLinkage);
    (void)doc.AddValue(root, "target", object);
}

Result ReadRuntimeTarget(const UpdaterContext &context, UpdateTarget &target) {
    std::string error;
    auto doc = utils::JsonDocument::ParseFile(JoinPath(context.gameRoot, L"ModLoader/BML.abi.json"), error);
    if (!doc.IsValid() || (utils::JsonGetInt(doc.Root(), "schema", 0) != 1 && utils::JsonGetInt(doc.Root(), "schema", 0) != 2))
        return Result::Failure("Missing or invalid ModLoader/BML.abi.json: " + error);
    auto result = ReadUpdateTarget(doc.Root(), target);
    if (!result.ok) return result;
    if (target.platform != BML_UPDATER_PLATFORM || target.architecture != BML_UPDATER_ARCH)
        return Result::Failure("Installed runtime does not match this updater's platform/architecture");
    return Result::Success();
}

Result CheckPackageTarget(const UpdaterContext &context, const UpdaterManifest &manifest) {
    UpdateTarget installed;
    auto result = ReadRuntimeTarget(context, installed);
    if (!result.ok) return result;
    if (manifest.schemaVersion != 2 || manifest.target != installed)
        return Result::Failure("Update package target does not match the installed runtime");
    const std::string expected = installed.hostLinkage == "static" ? "player-runtime" : "bml-runtime";
    if (manifest.packageType != expected)
        return Result::Failure("Update package type must be " + expected);
    return Result::Success();
}

bool IsValidUpdateVersion(std::string_view version) {
    return !version.empty() && version.size() <= 128 &&
        std::isalnum(static_cast<unsigned char>(version.front())) &&
        std::all_of(version.begin(), version.end(), [](unsigned char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                   (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '+' || c == '_';
        });
}

std::string ArchivePathKey(std::string_view path) {
#ifdef _WIN32
    return ToLowerAscii(std::string(path));
#else
    return std::string(path);
#endif
}

bool IsAllowedUpdatePath(std::string_view path, const UpdateTarget &target) {
    if (IsDisallowedUpdaterPackagePath(path)) return false;
    if (path == "ModLoader/BML.abi.json" || path.starts_with("ModLoader/Fonts/") ||
        path.starts_with("ModLoader/Licenses/")) return true;
    if (target.hostLinkage == "static")
        return path == (target.platform == "windows" ? "Player.exe" : "Player") ||
            path.starts_with("Bin/") || path.starts_with("Managers/") || path.starts_with("BuildingBlocks/") ||
            path.starts_with("Plugins/") || path.starts_with("RenderEngines/");
    const std::string prefix = target.platform == "windows" ? "" : "lib";
    const std::string suffix = target.platform == "windows" ? ".dll" : target.platform == "macos" ? ".dylib" : ".so";
    if (path == "Managers/" + prefix + "BMLPlus" + suffix || path == "BuildingBlocks/" + prefix + "AngelScript" + suffix) return true;
    if (!path.starts_with("Bin/") || path.substr(4).find('/') != std::string_view::npos) return false;
    const auto name = ToLowerAscii(std::string(path.substr(4)));
    if (name.starts_with("ck2") || name.starts_with("libck2") || name.starts_with("vxmath") ||
        name.starts_with("libvxmath") || name.starts_with("sdl3") || name.starts_with("libsdl3") ||
        name.starts_with("player")) return false;
    return name.ends_with(".dll") || name.ends_with(".dylib") || name.ends_with(".so") || name.find(".so.") != std::string::npos;
}
}
