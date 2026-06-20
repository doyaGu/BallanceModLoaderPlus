#include "UpdaterManifest.h"

#include <algorithm>
#include <array>

#include "CryptoUtils.h"
#include "JsonUtils.h"
#include "UpdaterPaths.h"

namespace bmlupdater {
    namespace {
        constexpr std::string_view kPackageNamePrefix = "BMLPlus-Update-";
        constexpr std::string_view kPackageNameSuffix = ".zip";

        const uint8_t *EmbeddedPublicKeyX() {
            static const std::vector<uint8_t> bytes = HexToBytes(BML_UPDATER_PUBLIC_KEY_X);
            return bytes.size() == 32 ? bytes.data() : nullptr;
        }

        const uint8_t *EmbeddedPublicKeyY() {
            static const std::vector<uint8_t> bytes = HexToBytes(BML_UPDATER_PUBLIC_KEY_Y);
            return bytes.size() == 32 ? bytes.data() : nullptr;
        }

        bool ReadStringMember(yyjson_val *object, const char *name, std::string &out, std::string &error) {
            yyjson_val *value = yyjson_obj_get(object, name);
            if (!value || !yyjson_is_str(value)) {
                error = std::string("Missing or invalid string member: ") + name;
                return false;
            }
            out = yyjson_get_str(value);
            return true;
        }

        bool ReadHashMember(yyjson_val *object, const char *name, std::string &out, std::string &error) {
            if (!ReadStringMember(object, name, out, error)) {
                return false;
            }
            out = ToLowerAscii(out);
            if (out.size() != 64 || HexToBytes(out).size() != 32) {
                error = std::string("Invalid SHA-256 hex member: ") + name;
                return false;
            }
            return true;
        }

        bool ValidateManagedPath(std::string &path, std::string &error) {
            auto normalized = NormalizeArchivePath(path, error);
            if (!normalized) {
                error = "Invalid manifest path '" + path + "': " + error;
                return false;
            }
            path = normalized->normalized;
            if (IsDisallowedUpdaterPackagePath(path)) {
                error = "Updater package may not manage path: " + path;
                return false;
            }
            return true;
        }

        bool ParseManagedFiles(yyjson_val *root, UpdaterManifest &manifest, std::string &error) {
            yyjson_val *array = yyjson_obj_get(root, "managedFiles");
            if (!array || !yyjson_is_arr(array)) {
                error = "Missing or invalid managedFiles array";
                return false;
            }

            size_t idx = 0;
            size_t max = 0;
            yyjson_val *item = nullptr;
            yyjson_arr_foreach(array, idx, max, item) {
                if (!yyjson_is_obj(item)) {
                    error = "managedFiles entries must be objects";
                    return false;
                }
                ManagedFile file;
                if (!ReadStringMember(item, "path", file.path, error) ||
                    !ValidateManagedPath(file.path, error) ||
                    !ReadHashMember(item, "sha256", file.sha256, error)) {
                    return false;
                }
                yyjson_val *sizeValue = yyjson_obj_get(item, "size");
                if (sizeValue && yyjson_is_uint(sizeValue)) {
                    file.size = yyjson_get_uint(sizeValue);
                }
                manifest.managedFiles.push_back(std::move(file));
            }
            if (manifest.managedFiles.empty()) {
                error = "managedFiles must not be empty";
                return false;
            }
            return true;
        }

        bool ParseStringArray(yyjson_val *root, const char *name, std::vector<std::string> &out, std::string &error) {
            yyjson_val *array = yyjson_obj_get(root, name);
            if (!array) {
                return true;
            }
            if (!yyjson_is_arr(array)) {
                error = std::string("Invalid array: ") + name;
                return false;
            }
            size_t idx = 0;
            size_t max = 0;
            yyjson_val *item = nullptr;
            yyjson_arr_foreach(array, idx, max, item) {
                if (!yyjson_is_str(item)) {
                    error = std::string(name) + " entries must be strings";
                    return false;
                }
                std::string path = yyjson_get_str(item);
                if (!ValidateManagedPath(path, error)) {
                    return false;
                }
                out.push_back(std::move(path));
            }
            return true;
        }

        bool ParseRemoveFiles(yyjson_val *root, UpdaterManifest &manifest, std::string &error) {
            yyjson_val *array = yyjson_obj_get(root, "removeFiles");
            if (!array) {
                return true;
            }
            if (!yyjson_is_arr(array)) {
                error = "Invalid removeFiles array";
                return false;
            }
            size_t idx = 0;
            size_t max = 0;
            yyjson_val *item = nullptr;
            yyjson_arr_foreach(array, idx, max, item) {
                RemoveFile file;
                if (yyjson_is_str(item)) {
                    file.path = yyjson_get_str(item);
                } else if (yyjson_is_obj(item)) {
                    if (!ReadStringMember(item, "path", file.path, error)) {
                        return false;
                    }
                    yyjson_val *hashValue = yyjson_obj_get(item, "sha256");
                    if (hashValue) {
                        if (!ReadHashMember(item, "sha256", file.sha256, error)) {
                            return false;
                        }
                    }
                } else {
                    error = "removeFiles entries must be strings or objects";
                    return false;
                }
                if (!ValidateManagedPath(file.path, error)) {
                    return false;
                }
                manifest.removeFiles.push_back(std::move(file));
            }
            return true;
        }

        void IgnoreJsonResult(bool value) {
            (void)value;
        }
    } // namespace

    Result ParseManifestJson(std::string_view json, UpdaterManifest &manifest) {
        manifest = {};
        std::string error;
        utils::JsonDocument doc = utils::JsonDocument::Parse(json, error);
        if (!doc.IsValid()) {
            return Result::Failure(error);
        }
        yyjson_val *root = doc.Root();
        if (!yyjson_is_obj(root)) {
            return Result::Failure("Manifest root must be an object");
        }

        manifest.schemaVersion = static_cast<int>(utils::JsonGetInt(root, "schemaVersion", 0));
        if (manifest.schemaVersion != 1) {
            return Result::Failure("Unsupported manifest schemaVersion");
        }
        if (!ReadStringMember(root, "version", manifest.version, error)) {
            return Result::Failure(error);
        }

        yyjson_val *package = yyjson_obj_get(root, "package");
        if (!yyjson_is_obj(package)) {
            return Result::Failure("Missing package object");
        }
        if (!ReadStringMember(package, "fileName", manifest.packageFileName, error) ||
            !ReadHashMember(package, "sha256", manifest.packageSha256, error)) {
            return Result::Failure(error);
        }

        const std::string lowerName = ToLowerAscii(manifest.packageFileName);
        if (!lowerName.starts_with(ToLowerAscii(std::string(kPackageNamePrefix))) ||
            !lowerName.ends_with(ToLowerAscii(std::string(kPackageNameSuffix)))) {
            return Result::Failure("Updater manifests must reference BMLPlus-Update-*.zip");
        }

        if (!ParseManagedFiles(root, manifest, error) ||
            !ParseStringArray(root, "preserve", manifest.preserve, error) ||
            !ParseRemoveFiles(root, manifest, error)) {
            return Result::Failure(error);
        }
        return Result::Success();
    }

    Result LoadManifestFile(const std::wstring &path, UpdaterManifest &manifest) {
        const std::string json = ReadTextFile(path);
        if (json.empty()) {
            return Result::Failure("Unable to read manifest: " + PathUtf8(path));
        }
        return ParseManifestJson(json, manifest);
    }

    Result VerifyManifestSignature(const std::wstring &manifestPath,
                                   const std::wstring &signaturePath) {
        std::array<uint8_t, 32> digest{};
        if (!utils::Sha256File(manifestPath, digest.data())) {
            return Result::Failure("Unable to hash manifest");
        }
        const std::string sigText = ReadTextFile(signaturePath);
        if (sigText.empty()) {
            return Result::Failure("Unable to read manifest signature");
        }
        return VerifyDetachedSignature(ReadTextFile(manifestPath), sigText);
    }

    Result VerifyDetachedSignature(std::string_view payload, std::string_view signatureText) {
        std::array<uint8_t, 32> digest{};
        if (!utils::Sha256(reinterpret_cast<const uint8_t *>(payload.data()), payload.size(), digest.data())) {
            return Result::Failure("Unable to hash signed payload");
        }
        const std::vector<uint8_t> signature = Base64Decode(signatureText);
        if (signature.size() != 64) {
            return Result::Failure("Detached signature must decode to 64 bytes");
        }
        const uint8_t *x = EmbeddedPublicKeyX();
        const uint8_t *y = EmbeddedPublicKeyY();
        if (!x || !y) {
            return Result::Failure("Updater public key is not configured");
        }
        if (!utils::VerifyEcdsaP256Sha256RawSignature(x, y, digest.data(), signature.data())) {
            return Result::Failure(
                "Detached signature verification failed. This updater may be stale or built with a different public key; "
                "install the latest manual BMLPlus package once to bootstrap Bin/Updater.exe.");
        }
        return Result::Success();
    }

    std::string WriteManifestJson(const UpdaterManifest &manifest, std::string &error) {
        utils::MutableJsonDocument doc;
        yyjson_mut_val *root = doc.CreateObject();
        doc.SetRoot(root);
        IgnoreJsonResult(doc.AddInt(root, "schemaVersion", manifest.schemaVersion));
        IgnoreJsonResult(doc.AddString(root, "version", manifest.version));

        yyjson_mut_val *package = doc.CreateObject();
        IgnoreJsonResult(doc.AddString(package, "fileName", manifest.packageFileName));
        IgnoreJsonResult(doc.AddString(package, "sha256", manifest.packageSha256));
        IgnoreJsonResult(doc.AddValue(root, "package", package));

        yyjson_mut_val *managed = doc.CreateArray();
        for (const ManagedFile &file : manifest.managedFiles) {
            yyjson_mut_val *item = doc.CreateObject();
            IgnoreJsonResult(doc.AddString(item, "path", file.path));
            IgnoreJsonResult(doc.AddString(item, "sha256", file.sha256));
            IgnoreJsonResult(doc.AddInt(item, "size", static_cast<int64_t>(file.size)));
            IgnoreJsonResult(doc.AddValue(managed, item));
        }
        IgnoreJsonResult(doc.AddValue(root, "managedFiles", managed));

        yyjson_mut_val *preserve = doc.CreateArray();
        for (const std::string &path : manifest.preserve) {
            IgnoreJsonResult(doc.AddString(preserve, path));
        }
        IgnoreJsonResult(doc.AddValue(root, "preserve", preserve));

        yyjson_mut_val *remove = doc.CreateArray();
        for (const RemoveFile &file : manifest.removeFiles) {
            yyjson_mut_val *item = doc.CreateObject();
            IgnoreJsonResult(doc.AddString(item, "path", file.path));
            if (!file.sha256.empty()) {
                IgnoreJsonResult(doc.AddString(item, "sha256", file.sha256));
            }
            IgnoreJsonResult(doc.AddValue(remove, item));
        }
        IgnoreJsonResult(doc.AddValue(root, "removeFiles", remove));
        return doc.Write(true, error);
    }

    std::wstring DeriveManifestPath(const std::wstring &packagePath) {
        std::wstring path = packagePath;
        const size_t slash = path.find_last_of(L"\\/");
        const size_t dot = path.find_last_of(L'.');
        if (dot != std::wstring::npos && (slash == std::wstring::npos || dot > slash)) {
            path.resize(dot);
        }
        path += L".manifest.json";
        return path;
    }

    std::wstring DeriveSignaturePath(const std::wstring &manifestPath) {
        return manifestPath + L".sig";
    }
} // namespace bmlupdater
