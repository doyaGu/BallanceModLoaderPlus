#ifndef BML_UPDATER_MANIFEST_H
#define BML_UPDATER_MANIFEST_H

#include <string>

#include "UpdaterTypes.h"

namespace bmlupdater {
    [[nodiscard]] Result ParseManifestJson(std::string_view json, UpdaterManifest &manifest);
    [[nodiscard]] Result LoadManifestFile(const std::wstring &path, UpdaterManifest &manifest);
    [[nodiscard]] Result VerifyManifestSignature(const std::wstring &manifestPath,
                                                 const std::wstring &signaturePath);
    [[nodiscard]] Result VerifyDetachedSignature(std::string_view payload, std::string_view signatureText);
    [[nodiscard]] std::string WriteManifestJson(const UpdaterManifest &manifest, std::string &error);
    [[nodiscard]] std::wstring DeriveManifestPath(const std::wstring &packagePath);
    [[nodiscard]] std::wstring DeriveSignaturePath(const std::wstring &manifestPath);
} // namespace bmlupdater

#endif // BML_UPDATER_MANIFEST_H
