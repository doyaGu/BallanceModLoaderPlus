#pragma once

#include "UpdaterTypes.h"
#include "JsonUtils.h"

namespace bmlupdater {
Result ReadUpdateTarget(yyjson_val *object, UpdateTarget &target);
void WriteUpdateTarget(utils::MutableJsonDocument &doc, yyjson_mut_val *root, const UpdateTarget &target);
Result ReadRuntimeTarget(const UpdaterContext &context, UpdateTarget &target);
Result CheckPackageTarget(const UpdaterContext &context, const UpdaterManifest &manifest);
bool IsValidUpdateVersion(std::string_view version);
std::string LoaderPath(const UpdateTarget &target);
bool IsAllowedUpdatePath(std::string_view path, const UpdateTarget &target);
std::string ArchivePathKey(std::string_view path);
}
