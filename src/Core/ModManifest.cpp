#include "ModManifest.h"

#include "SemanticVersion.h"

#include <filesystem>
#include <string>
#include <string_view>

#include <toml++/toml.hpp>

#include "StringUtils.h"

namespace BML::Core {
    static std::string Narrow(const std::wstring &wide) {
        return utils::Utf16ToUtf8(wide);
    }

    static void SetParseError(ManifestParseError &error,
                              const std::wstring &path,
                              const std::string &message) {
        error.message = message;
        error.file = Narrow(path);
    }

    static bool ReadString(const toml::node *node, std::string &out) {
        if (!node)
            return false;
        if (auto val = node->value<std::string_view>()) {
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

    static bool PopulatePackage(const toml::table &pkg, ModPackage &out_pkg, ManifestParseError &error,
                                const std::wstring &path) {
        if (!ReadString(pkg.get("id"), out_pkg.id) || out_pkg.id.empty()) {
            SetParseError(error, path, "[package] id must be a non-empty string");
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
            if (auto arr = authorsNode->as_array()) {
                for (const auto &node : *arr) {
                    std::string author;
                    if (ReadString(&node, author) && !author.empty()) {
                        out_pkg.authors.emplace_back(std::move(author));
                    }
                }
            } else {
                SetParseError(error, path, "[package] authors must be an array of strings");
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
            if (dep.id.empty()) {
                SetParseError(error, path, "Dependency id must not be empty");
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
            if (conflict.id.empty()) {
                SetParseError(error, path, "Conflict id must not be empty");
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

    static bool PopulateCapabilities(const toml::node *node,
                                     std::vector<std::string> &out_caps,
                                     ManifestParseError &error,
                                     const std::wstring &path) {
        if (!node)
            return true;
        auto arr = node->as_array();
        if (!arr) {
            SetParseError(error, path, "capabilities must be an array of strings");
            return false;
        }
        for (const auto &entry : *arr) {
            std::string cap;
            if (!ReadString(&entry, cap) || cap.empty()) {
                SetParseError(error, path, "capabilities entries must be non-empty strings");
                return false;
            }
            out_caps.emplace_back(std::move(cap));
        }
        return true;
    }

    bool ManifestParser::ParseFile(const std::wstring &path,
                                   ModManifest &out_manifest,
                                   ManifestParseError &out_error) const {
        std::string narrowPath = Narrow(path);
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

        auto pkgTable = table["package"].as_table();
        if (!pkgTable) {
            SetParseError(out_error, path, "Missing [package] table");
            return false;
        }
        if (!PopulatePackage(*pkgTable, out_manifest.package, out_error, path)) {
            return false;
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

        out_manifest.capabilities.clear();
        if (!PopulateCapabilities(table["capabilities"].node(), out_manifest.capabilities, out_error, path)) {
            return false;
        }

        out_manifest.manifest_path = path;
        out_manifest.directory = std::filesystem::path(path).parent_path().wstring();

        return true;
    }
} // namespace BML::Core
