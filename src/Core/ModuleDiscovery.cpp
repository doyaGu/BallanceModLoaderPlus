#include "ModuleDiscovery.h"

#include <filesystem>
#include <cwchar>
#include <cwctype>
#include <sstream>
#include <vector>

#include "Logging.h"
#include "ModManifest.h"
#include "PathUtils.h"
#include "StringUtils.h"

namespace BML::Core {
    namespace fs = std::filesystem;

    namespace {
        constexpr char kDiscoveryLogCategory[] = "module.discovery";
        constexpr const wchar_t *kArchiveExtension = L".bp";
        constexpr const wchar_t *kArchiveCacheDir = L".bp-cache";

        void LogFilesystemError(const char *action, const fs::path &target, const std::error_code &ec) {
            if (!ec)
                return;
            CoreLog(BML_LOG_WARN, kDiscoveryLogCategory,
                    "%s failed for %s: %s",
                    action,
                    utils::Utf16ToUtf8(target.wstring()).c_str(),
                    ec.message().c_str());
        }

        bool EqualsIgnoreCase(const std::wstring &lhs, const wchar_t *rhs) {
            if (!rhs)
                return lhs.empty();
#ifdef _WIN32
            return _wcsicmp(lhs.c_str(), rhs) == 0;
#else
            std::wstring rhs_str(rhs);
            if (lhs.size() != rhs_str.size())
                return false;
            for (size_t i = 0; i < lhs.size(); ++i) {
                if (towlower(lhs[i]) != towlower(rhs_str[i]))
                    return false;
            }
            return true;
#endif
        }

        bool HasBpExtension(const fs::path &path) {
            auto ext = path.extension().wstring();
            return !ext.empty() && EqualsIgnoreCase(ext, kArchiveExtension);
        }

        bool IsCacheDirectory(const fs::path &path) {
            return EqualsIgnoreCase(path.filename().wstring(), kArchiveCacheDir);
        }

        fs::path PrepareArchiveCache(const fs::path &mods_dir) {
            fs::path cache = mods_dir / kArchiveCacheDir;
            std::error_code ec;
            fs::remove_all(cache, ec);
            LogFilesystemError("remove cache directory", cache, ec);
            ec.clear();
            fs::create_directories(cache, ec);
            if (ec) {
                LogFilesystemError("create cache directory", cache, ec);
                return {};
            }
            return cache;
        }

        fs::path NormalizeExtractionRoot(const fs::path &base) {
            fs::path current = base;
            for (int depth = 0; depth < 4; ++depth) {
                std::error_code ec;
                auto manifest = current / L"mod.toml";
                if (fs::exists(manifest, ec) && fs::is_regular_file(manifest, ec))
                    return current;

                std::vector<fs::path> subdirs;
                bool hasFiles = false;
                std::error_code iterEc;
                fs::directory_iterator iter(current,
                                             fs::directory_options::skip_permission_denied,
                                             iterEc);
                fs::directory_iterator end;
                while (!iterEc && iter != end) {
                    const auto &entry = *iter;
                    if (entry.is_directory()) {
                        subdirs.push_back(entry.path());
                    } else if (entry.is_regular_file()) {
                        hasFiles = true;
                        break;
                    }
                    iter.increment(iterEc);
                }

                if (iterEc) {
                    LogFilesystemError("iterate extracted archive", current, iterEc);
                    break;
                }

                if (hasFiles || subdirs.size() != 1)
                    break;

                current = subdirs[0];
            }
            return current;
        }

        void AddExtractionError(const fs::path &archive,
                                const std::string &reason,
                                ManifestLoadResult &out_result) {
            ManifestParseError error;
            std::ostringstream oss;
            oss << "Failed to extract archive '" << utils::Utf16ToUtf8(archive.filename().wstring()) << "'";
            if (!reason.empty())
                oss << ": " << reason;
            error.message = oss.str();
            error.file = utils::Utf16ToUtf8(archive.wstring());
            out_result.errors.emplace_back(std::move(error));
        }

        fs::path ExtractArchive(const fs::path &archive,
                                const fs::path &cache_root,
                                ManifestLoadResult &out_result) {
            fs::path target = cache_root / archive.stem();
            std::error_code ec;
            fs::remove_all(target, ec);

            if (!utils::ExtractZipW(archive.wstring(), target.wstring())) {
                AddExtractionError(archive, "unsupported or corrupt bp package", out_result);
                return {};
            }

            return NormalizeExtractionRoot(target);
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

        bool cacheInitialized = false;
        fs::path cacheRoot;
        ManifestParser parser;

        auto ensureCache = [&]() -> fs::path {
            if (cacheInitialized)
                return cacheRoot;
            cacheRoot = PrepareArchiveCache(root);
            cacheInitialized = true;
            return cacheRoot;
        };

        auto tryLoadFromDirectory = [&](const fs::path &dir, const fs::path *source_archive) {
            std::error_code dirEc;
            fs::path manifest_path = dir / L"mod.toml";
            if (!fs::exists(manifest_path, dirEc) || !fs::is_regular_file(manifest_path, dirEc))
                return;

            auto manifest = std::make_unique<ModManifest>();
            ManifestParseError error;
            if (!parser.ParseFile(manifest_path.wstring(), *manifest, error)) {
                out_result.errors.emplace_back(std::move(error));
                return;
            }
            manifest->directory = dir.wstring();
            manifest->manifest_path = manifest_path.wstring();
            if (source_archive) {
                manifest->source_archive = source_archive->wstring();
            } else {
                manifest->source_archive.clear();
            }
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
                if (!IsCacheDirectory(entry.path()))
                    tryLoadFromDirectory(entry.path(), nullptr);
                iter.increment(iterEc);
                continue;
            }

            if (!entry.is_regular_file() || !HasBpExtension(entry.path())) {
                iter.increment(iterEc);
                continue;
            }

            auto cache = ensureCache();
            if (cache.empty()) {
                AddExtractionError(entry.path(), "unable to prepare archive cache directory", out_result);
                iter.increment(iterEc);
                continue;
            }

            auto extracted = ExtractArchive(entry.path(), cache, out_result);
            if (!extracted.empty()) {
                fs::path archive_path = entry.path();
                tryLoadFromDirectory(extracted, &archive_path);
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
