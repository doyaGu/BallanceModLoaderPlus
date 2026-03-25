#include "ModManifest.h"

#include "SemanticVersion.h"

#include <filesystem>
#include <string>
#include <string_view>

#include <toml++/toml.hpp>

#include "StringUtils.h"

namespace BML::Core {
    static void SetParseError(ManifestParseError &error,
                              const std::wstring &path,
                              const std::string &message) {
        error.message = message;
        error.file = utils::Utf16ToUtf8(path);
    }

    static constexpr size_t kMaxStringLength = 4096;
    static constexpr size_t kMaxArraySize = 256;

    static bool ReadString(const toml::node *node, std::string &out) {
        if (!node)
            return false;
        if (auto val = node->value<std::string_view>()) {
            if (val->size() > kMaxStringLength)
                return false;
            out.assign(val->begin(), val->end());
            return true;
        }
        return false;
    }

    static bool ReadBool(const toml::node *node, bool &out) {
        if (!node)
            return false;
        if (auto val = node->value<bool>()) {
            out = *val;
            return true;
        }
        return false;
    }

    static bool ContainsWhitespace(std::string_view text) {
        for (char c : text) {
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
                return true;
        }
        return false;
    }

    static bool ValidateModuleId(std::string_view id,
                                 const std::string &field_name,
                                 ManifestParseError &error,
                                 const std::wstring &path) {
        if (id.empty()) {
            SetParseError(error, path, field_name + " must be a non-empty string");
            return false;
        }
        if (ContainsWhitespace(id)) {
            SetParseError(error, path, field_name + " must not contain whitespace");
            return false;
        }
        return true;
    }

    static bool ValidateInterfaceId(std::string_view id,
                                    const std::string &field_name,
                                    ManifestParseError &error,
                                    const std::wstring &path) {
        if (id.empty()) {
            SetParseError(error, path, field_name + " must not be empty");
            return false;
        }
        if (ContainsWhitespace(id)) {
            SetParseError(error, path, field_name + " must not contain whitespace");
            return false;
        }
        return true;
    }

    static bool ValidateRelativePathWithinModule(const std::string &value,
                                                 const std::wstring &module_directory,
                                                 const std::string &field_name,
                                                 ManifestParseError &error,
                                                 const std::wstring &path) {
        std::filesystem::path parsed_path(utils::Utf8ToUtf16(value));
        if (parsed_path.is_absolute()) {
            SetParseError(error, path, field_name + " must be a relative path");
            return false;
        }

        std::filesystem::path base = std::filesystem::path(module_directory).lexically_normal();
        std::filesystem::path resolved = (base / parsed_path).lexically_normal();
        std::filesystem::path relative = resolved.lexically_relative(base);
        if (relative.empty() || relative.native().starts_with(L"..")) {
            SetParseError(error, path, field_name + " must stay within the module directory");
            return false;
        }

        return true;
    }

    static bool PopulateStringArray(const toml::node *node,
                                    const std::string &field_name,
                                    std::vector<std::string> &out_values,
                                    ManifestParseError &error,
                                    const std::wstring &path) {
        auto arr = node->as_array();
        if (!arr) {
            SetParseError(error, path, field_name + " must be an array of non-empty strings");
            return false;
        }

        out_values.clear();
        for (const auto &entry : *arr) {
            if (out_values.size() >= kMaxArraySize) {
                SetParseError(error, path, field_name + " array exceeds maximum size");
                return false;
            }

            std::string value;
            if (!ReadString(&entry, value) || value.empty()) {
                SetParseError(error, path, field_name + " entries must be non-empty strings");
                return false;
            }

            out_values.emplace_back(std::move(value));
        }

        return true;
    }

    static bool PopulatePackage(const toml::table &pkg, ModPackage &out_pkg, ManifestParseError &error,
                                const std::wstring &path) {
        if (!ReadString(pkg.get("id"), out_pkg.id)) {
            SetParseError(error, path, "[package] id must be a non-empty string");
            return false;
        }
        if (!ValidateModuleId(out_pkg.id, "[package] id", error, path)) {
            return false;
        }
        if (!ReadString(pkg.get("name"), out_pkg.name) || out_pkg.name.empty()) {
            SetParseError(error, path, "[package] name must be a non-empty string");
            return false;
        }
        if (!ReadString(pkg.get("version"), out_pkg.version) || out_pkg.version.empty()) {
            SetParseError(error, path, "[package] version must be a non-empty string");
            return false;
        }

        std::string versionError;
        if (!ParseSemanticVersion(out_pkg.version, out_pkg.parsed_version)) {
            SetParseError(error, path, "Invalid package version: " + out_pkg.version);
            return false;
        }

        out_pkg.entry.clear();
        ReadString(pkg.get("entry"), out_pkg.entry);
        out_pkg.description.clear();
        ReadString(pkg.get("description"), out_pkg.description);

        out_pkg.authors.clear();
        if (auto authorsNode = pkg.get("authors")) {
            if (!PopulateStringArray(authorsNode, "[package] authors", out_pkg.authors, error, path)) {
                return false;
            }
        }

        out_pkg.capabilities.clear();
        if (auto capabilitiesNode = pkg.get("capabilities")) {
            if (!PopulateStringArray(capabilitiesNode, "[package] capabilities", out_pkg.capabilities, error, path)) {
                return false;
            }
        }

        return true;
    }

    static bool PopulateDependencies(const toml::table &depsTable,
                                     std::vector<ModDependency> &out_deps,
                                     ManifestParseError &error,
                                     const std::wstring &path) {
        for (const auto &[key, value] : depsTable) {
            ModDependency dep;
            dep.id = key.str();
            if (!ValidateModuleId(dep.id, "Dependency id", error, path)) {
                return false;
            }

            std::string versionExpr;
            if (value.is_string()) {
                if (!ReadString(&value, versionExpr)) {
                    SetParseError(error, path, "Dependency version constraint must be a string");
                    return false;
                }
            } else if (auto tbl = value.as_table()) {
                if (!ReadString(tbl->get("version"), versionExpr) || versionExpr.empty()) {
                    SetParseError(error, path, "Dependency table must contain a non-empty 'version' field");
                    return false;
                }
                bool optional = false;
                if (ReadBool(tbl->get("optional"), optional)) {
                    dep.optional = optional;
                }
            } else {
                SetParseError(error, path, "Dependency '" + dep.id + "' must be a string or table");
                return false;
            }

            dep.requirement.raw_expression = versionExpr;
            std::string rangeError;
            if (!ParseSemanticVersionRange(versionExpr, dep.requirement, rangeError)) {
                SetParseError(error, path, "Dependency version constraint invalid: " + rangeError);
                return false;
            }
            out_deps.emplace_back(std::move(dep));
        }

        return true;
    }

    static bool PopulateConflicts(const toml::table &conflictsTable,
                                  std::vector<ModConflict> &out_conflicts,
                                  ManifestParseError &error,
                                  const std::wstring &path) {
        for (const auto &[key, value] : conflictsTable) {
            ModConflict conflict;
            conflict.id = key.str();
            if (!ValidateModuleId(conflict.id, "Conflict id", error, path)) {
                return false;
            }

            std::string versionExpr;
            if (value.is_string()) {
                if (!ReadString(&value, versionExpr)) {
                    SetParseError(error, path, "Conflict version constraint must be a string");
                    return false;
                }
            } else if (auto tbl = value.as_table()) {
                ReadString(tbl->get("reason"), conflict.reason);
                ReadString(tbl->get("version"), versionExpr);
            } else {
                SetParseError(error, path, "Conflict '" + conflict.id + "' must be a string or table");
                return false;
            }

            // Empty or "*" means "any version"
            if (versionExpr.empty()) {
                versionExpr = "*";
            }

            conflict.requirement.raw_expression = versionExpr;
            if (versionExpr != "*") {
                std::string rangeError;
                if (!ParseSemanticVersionRange(versionExpr, conflict.requirement, rangeError)) {
                    SetParseError(error, path, "Conflict version constraint invalid: " + rangeError);
                    return false;
                }
            } else {
                conflict.requirement.parsed = false;
            }

            out_conflicts.emplace_back(std::move(conflict));
        }

        return true;
    }

    static bool PopulateRequires(const toml::table &requiresTable,
                                 std::vector<ModInterfaceRequirement> &out_requires,
                                 ManifestParseError &error,
                                 const std::wstring &path) {
        for (const auto &[key, value] : requiresTable) {
            ModInterfaceRequirement req;
            req.interface_id = key.str();
            if (!ValidateInterfaceId(req.interface_id, "[requires] interface id", error, path)) {
                return false;
            }

            std::string versionExpr;
            if (value.is_string()) {
                if (!ReadString(&value, versionExpr) || versionExpr.empty()) {
                    SetParseError(error, path,
                                  "[requires] interface '" + req.interface_id +
                                  "' version must be a non-empty string");
                    return false;
                }
            } else if (auto tbl = value.as_table()) {
                if (!ReadString(tbl->get("version"), versionExpr) || versionExpr.empty()) {
                    SetParseError(error, path,
                                  "[requires] interface '" + req.interface_id + "' table must contain a non-empty 'version' field");
                    return false;
                }
                bool optional = false;
                if (ReadBool(tbl->get("optional"), optional)) {
                    req.optional = optional;
                }
            } else {
                SetParseError(error, path,
                              "[requires] interface '" + req.interface_id + "' must be a string or table");
                return false;
            }

            req.requirement.raw_expression = versionExpr;
            std::string rangeError;
            if (!ParseSemanticVersionRange(versionExpr, req.requirement, rangeError)) {
                SetParseError(error, path, "[requires] version constraint invalid: " + rangeError);
                return false;
            }
            out_requires.emplace_back(std::move(req));
        }
        return true;
    }

    static bool PopulateInterfaces(const toml::table &interfacesTable,
                                   std::vector<ModInterfaceExport> &out_interfaces,
                                 ManifestParseError &error,
                                 const std::wstring &path) {
        for (const auto &[key, value] : interfacesTable) {
            ModInterfaceExport iface;
            iface.interface_id = key.str();
            if (!ValidateInterfaceId(iface.interface_id, "[interfaces] interface id", error, path)) {
                return false;
            }

            if (value.is_string()) {
                if (!ReadString(&value, iface.version) || iface.version.empty()) {
                    SetParseError(error, path,
                                  "[interfaces] interface '" + iface.interface_id + "' version must be a non-empty string");
                    return false;
                }
            } else if (auto tbl = value.as_table()) {
                if (!ReadString(tbl->get("version"), iface.version) || iface.version.empty()) {
                    SetParseError(error, path,
                                  "[interfaces] interface '" + iface.interface_id + "' table must contain a non-empty 'version' field");
                    return false;
                }
                ReadString(tbl->get("description"), iface.description);
            } else {
                SetParseError(error, path,
                              "[interfaces] interface '" + iface.interface_id + "' must be a string or table");
                return false;
            }

            if (!ParseSemanticVersion(iface.version, iface.parsed_version)) {
                SetParseError(error, path,
                              "[interfaces] invalid version for interface '" + iface.interface_id + "': " + iface.version);
                return false;
            }

            out_interfaces.emplace_back(std::move(iface));
        }
        return true;
    }

    // Keep in sync with docs/mod-manifest-schema.md "Recognized Top-Level Fields"
    static bool IsKnownSection(std::string_view key) {
        static constexpr std::string_view kKnownSections[] = {
            "package", "dependencies", "conflicts",
            "requires", "interfaces", "assets",
        };
        for (auto known : kKnownSections) {
            if (key == known) return true;
        }
        return false;
    }

    static bool FlattenTomlNode(const toml::node &node,
                                const std::string &prefix,
                                std::unordered_map<std::string, ManifestValue> &out,
                                ManifestParseError &error,
                                const std::wstring &path) {
        if (auto tbl = node.as_table()) {
            for (const auto &[key, value] : *tbl) {
                std::string childKey = prefix.empty()
                    ? std::string(key.str())
                    : prefix + "." + std::string(key.str());
                if (!FlattenTomlNode(value, childKey, out, error, path)) {
                    return false;
                }
            }
            return true;
        } else if (node.is_array()) {
            SetParseError(error, path, "Custom field '" + prefix + "' does not support arrays");
            return false;
        } else if (node.is_boolean()) {
            out[prefix] = *node.value<bool>();
            return true;
        } else if (node.is_integer()) {
            out[prefix] = *node.value<int64_t>();
            return true;
        } else if (node.is_floating_point()) {
            out[prefix] = *node.value<double>();
            return true;
        } else if (node.is_string()) {
            out[prefix] = std::string(*node.value<std::string_view>());
            return true;
        } else if (node.is_date() || node.is_time() || node.is_date_time()) {
            SetParseError(error, path, "Custom field '" + prefix + "' uses an unsupported TOML type");
            return false;
        }
        SetParseError(error, path, "Custom field '" + prefix + "' uses an unsupported TOML type");
        return false;
    }

    bool ManifestParser::ParseFile(const std::wstring &path,
                                   ModManifest &out_manifest,
                                   ManifestParseError &out_error) const {
        std::string narrowPath = utils::Utf16ToUtf8(path);
#if TOML_EXCEPTIONS
        toml::table table;
        try {
            table = toml::parse_file(narrowPath);
        } catch (const toml::parse_error &err) {
            out_error.message = std::string(err.description());
            out_error.file = narrowPath;
            const auto &source = err.source();
            out_error.line = static_cast<int>(source.begin.line);
            out_error.column = static_cast<int>(source.begin.column);
            return false;
        }
#else
        auto parseResult = toml::parse_file(narrowPath);
        if (!parseResult) {
            const auto &err = parseResult.error();
            out_error.message = std::string(err.description());
            out_error.file = narrowPath;
            const auto &source = err.source();
            out_error.line = static_cast<int>(source.begin.line);
            out_error.column = static_cast<int>(source.begin.column);
            return false;
        }
        toml::table table = std::move(parseResult).table();
#endif

        auto pkgTable = table["package"].as_table();
        if (!pkgTable) {
            SetParseError(out_error, path, "Missing [package] table");
            return false;
        }
        if (!PopulatePackage(*pkgTable, out_manifest.package, out_error, path)) {
            return false;
        }

        std::wstring manifest_directory = std::filesystem::path(path).parent_path().wstring();
        if (!out_manifest.package.entry.empty()) {
            if (!ValidateRelativePathWithinModule(out_manifest.package.entry,
                                                  manifest_directory,
                                                  "[package] entry",
                                                  out_error,
                                                  path)) {
                return false;
            }
        }

        out_manifest.dependencies.clear();
        if (auto depsNode = table["dependencies"]) {
            auto depsTable = depsNode.as_table();
            if (!depsTable) {
                SetParseError(out_error, path, "[dependencies] must be a table");
                return false;
            }
            if (!PopulateDependencies(*depsTable, out_manifest.dependencies, out_error, path)) {
                return false;
            }
        }

        out_manifest.conflicts.clear();
        if (auto conflictsNode = table["conflicts"]) {
            auto conflictsTable = conflictsNode.as_table();
            if (!conflictsTable) {
                SetParseError(out_error, path, "[conflicts] must be a table");
                return false;
            }
            if (!PopulateConflicts(*conflictsTable, out_manifest.conflicts, out_error, path)) {
                return false;
            }
        }

        out_manifest.requires_.clear();
        if (auto requiresNode = table["requires"]) {
            auto requiresTable = requiresNode.as_table();
            if (!requiresTable) {
                SetParseError(out_error, path, "[requires] must be a table");
                return false;
            }
            if (!PopulateRequires(*requiresTable, out_manifest.requires_, out_error, path)) {
                return false;
            }
        }

        out_manifest.interfaces.clear();
        if (auto interfacesNode = table["interfaces"]) {
            auto interfacesTable = interfacesNode.as_table();
            if (!interfacesTable) {
                SetParseError(out_error, path, "[interfaces] must be a table");
                return false;
            }
            if (!PopulateInterfaces(*interfacesTable, out_manifest.interfaces, out_error, path)) {
                return false;
            }
        }

        out_manifest.assets = {};
        if (const toml::node *assetsNode = table.get("assets")) {
            auto assetsTable = assetsNode->as_table();
            if (!assetsTable) {
                SetParseError(out_error, path, "[assets] must be a table");
                return false;
            }

            if (auto mountNode = assetsTable->get("mount")) {
                if (!ReadString(mountNode, out_manifest.assets.mount) || out_manifest.assets.mount.empty()) {
                    SetParseError(out_error, path, "[assets] mount must be a non-empty string");
                    return false;
                }
                if (!ValidateRelativePathWithinModule(out_manifest.assets.mount,
                                                      manifest_directory,
                                                      "[assets] mount",
                                                      out_error,
                                                      path)) {
                    return false;
                }
            }
        }

        // Collect custom (unrecognized) sections into custom_fields map
        out_manifest.custom_fields.clear();
        for (const auto &[key, value] : table) {
            if (IsKnownSection(key.str())) {
                continue;
            }

            if (!value.is_table()) {
                SetParseError(out_error, path,
                              "Custom field section '" + std::string(key.str()) +
                              "' must be a top-level table");
                return false;
            }

            if (!FlattenTomlNode(value,
                                 std::string(key.str()),
                                 out_manifest.custom_fields,
                                 out_error,
                                 path)) {
                return false;
            }
        }

        out_manifest.manifest_path = path;
        out_manifest.directory = std::move(manifest_directory);

        return true;
    }
} // namespace BML::Core
