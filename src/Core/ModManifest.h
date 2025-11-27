#ifndef BML_CORE_MODMANIFEST_H
#define BML_CORE_MODMANIFEST_H

#include <optional>
#include <string>
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

    struct ModManifest {
        ModPackage package;
        std::vector<ModDependency> dependencies;
        std::vector<ModConflict> conflicts;
        std::vector<std::string> capabilities;
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
