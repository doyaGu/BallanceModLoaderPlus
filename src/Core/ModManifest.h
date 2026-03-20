#ifndef BML_CORE_MODMANIFEST_H
#define BML_CORE_MODMANIFEST_H

#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "SemanticVersion.h"

namespace BML::Core {
    struct ModDependency {
        std::string id;
        SemanticVersionRange requirement;
        bool optional{false};
    };

    struct ModConflict {
        std::string id;
        SemanticVersionRange requirement;
        std::string reason;
    };

    struct ModPackage {
        std::string id;
        std::string name;
        std::string version;
        SemanticVersion parsed_version;
        std::vector<std::string> authors;
        std::string description;
        std::string entry;
    };

    struct ModProvidedInterface {
        std::string interface_id;
        std::string version;
        SemanticVersion parsed_version;
        std::string description;
    };

    struct ModInterfaceRequirement {
        std::string interface_id;
        SemanticVersionRange requirement;
        bool optional{false};
    };

    struct ModAssetConfig {
        std::string mount;  // Subdirectory relative to mod root (e.g. "assets")
    };

    using ManifestValue = std::variant<std::string, int64_t, double, bool>;

    struct ModManifest {
        ModPackage package;
        std::vector<ModDependency> dependencies;
        std::vector<ModInterfaceRequirement> requires_;
        std::vector<ModConflict> conflicts;
        std::vector<std::string> capabilities;
        std::vector<ModProvidedInterface> provides;
        ModAssetConfig assets;
        std::unordered_map<std::string, ManifestValue> custom_fields;
        std::wstring manifest_path;
        std::wstring directory;
        std::wstring source_archive;
    };

    struct ManifestParseError {
        std::string message;
        std::optional<std::string> file;
        std::optional<int> line;
        std::optional<int> column;
    };

    class ManifestParser {
    public:
        ManifestParser() = default;

        bool ParseFile(const std::wstring &path, ModManifest &out_manifest, ManifestParseError &out_error) const;
    };
} // namespace BML::Core

#endif // BML_CORE_MODMANIFEST_H
