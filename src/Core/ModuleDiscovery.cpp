#include "ModuleDiscovery.h"

#include <filesystem>

#include "Logging.h"
#include "ModManifest.h"
#include "StringUtils.h"

namespace BML::Core {
    namespace fs = std::filesystem;

    namespace {
        constexpr char kDiscoveryLogCategory[] = "module.discovery";

        void LogFilesystemError(const char *action, const fs::path &target, const std::error_code &ec) {
            if (!ec)
                return;
            CoreLog(BML_LOG_WARN, kDiscoveryLogCategory,
                    "%s failed for %s: %s",
                    action,
                    utils::Utf16ToUtf8(target.wstring()).c_str(),
                    ec.message().c_str());
        }
    } // namespace

    bool LoadManifestsFromDirectory(const std::wstring &mods_dir, ManifestLoadResult &out_result) {
        out_result.manifests.clear();
        out_result.errors.clear();

        fs::path root(mods_dir);
        std::error_code ec;
        if (!fs::exists(root, ec) || !fs::is_directory(root, ec)) {
            CoreLog(BML_LOG_DEBUG, kDiscoveryLogCategory,
                    "Mods directory does not exist: %s", utils::Utf16ToUtf8(mods_dir).c_str());
            return true;
        }

        CoreLog(BML_LOG_INFO, kDiscoveryLogCategory,
                "Scanning mods directory: %s", utils::Utf16ToUtf8(mods_dir).c_str());

        ManifestParser parser;

        auto tryLoadFromDirectory = [&](const fs::path &dir) {
            std::error_code dirEc;
            fs::path manifest_path = dir / L"mod.toml";
            if (!fs::exists(manifest_path, dirEc) || !fs::is_regular_file(manifest_path, dirEc))
                return;

            auto manifest = std::make_unique<ModManifest>();
            ManifestParseError error;
            if (!parser.ParseFile(manifest_path.wstring(), *manifest, error)) {
                CoreLog(BML_LOG_WARN, kDiscoveryLogCategory,
                        "Manifest parse error in %s: %s",
                        utils::Utf16ToUtf8(manifest_path.wstring()).c_str(),
                        error.message.c_str());
                out_result.errors.emplace_back(std::move(error));
                return;
            }
            manifest->directory = dir.wstring();
            manifest->manifest_path = manifest_path.wstring();
            manifest->source_archive.clear();
            CoreLog(BML_LOG_DEBUG, kDiscoveryLogCategory,
                    "Loaded manifest: %s from %s",
                    manifest->package.id.c_str(),
                    utils::Utf16ToUtf8(manifest_path.wstring()).c_str());
            out_result.manifests.emplace_back(std::move(manifest));
        };

        std::error_code iterEc;
        fs::directory_iterator iter(root,
                                    fs::directory_options::skip_permission_denied,
                                    iterEc);
        fs::directory_iterator end;
        while (!iterEc && iter != end) {
            const auto &entry = *iter;

            if (entry.is_directory()) {
                tryLoadFromDirectory(entry.path());
            }

            iter.increment(iterEc);
        }

        if (iterEc) {
            LogFilesystemError("iterate mods directory", root, iterEc);
        }

        CoreLog(BML_LOG_INFO, kDiscoveryLogCategory,
                "Discovery complete: %zu manifests, %zu errors",
                out_result.manifests.size(), out_result.errors.size());
        return out_result.errors.empty();
    }

    bool BuildLoadOrder(const ManifestLoadResult &manifests,
                        std::vector<ResolvedNode> &out_order,
                        std::vector<DependencyWarning> &out_warnings,
                        DependencyResolutionError &out_error) {
        DependencyResolver resolver;
        for (const auto &manifest : manifests.manifests) {
            if (manifest) {
                resolver.RegisterManifest(*manifest);
            }
        }
        return resolver.Resolve(out_order, out_warnings, out_error);
    }
} // namespace BML::Core
