#include "PackageInstallState.h"

#include <filesystem>
#include <fstream>
#include "JsonUtils.h"
#include "StringUtils.h"
#include "TimeUtils.h"

namespace BML::Core {
    namespace {
        enum class StateReadFailure {
            None,
            Open,
            Parse,
        };

        bool ValidateStateSchema(const yyjson_val *root, std::string &error) {
            if (!root || !yyjson_is_obj(root)) {
                error = "Package state root must be an object";
                return false;
            }

            if (yyjson_val *installedNode = yyjson_obj_get(root, "installed");
                installedNode) {
                if (!yyjson_is_obj(installedNode)) {
                    error = "'installed' must be an object";
                    return false;
                }

                yyjson_obj_iter iter = yyjson_obj_iter_with(installedNode);
                yyjson_val *key = nullptr;
                while ((key = yyjson_obj_iter_next(&iter)) != nullptr) {
                    yyjson_val *value = yyjson_obj_iter_get_val(key);
                    if (!value || !yyjson_is_obj(value)) {
                        error = "installed records must be objects";
                        return false;
                    }
                }
            }
            if (yyjson_val *packagesNode = yyjson_obj_get(root, "packages");
                packagesNode) {
                if (!yyjson_is_obj(packagesNode)) {
                    error = "'packages' must be an object";
                    return false;
                }

                yyjson_obj_iter iter = yyjson_obj_iter_with(packagesNode);
                yyjson_val *key = nullptr;
                while ((key = yyjson_obj_iter_next(&iter)) != nullptr) {
                    yyjson_val *value = yyjson_obj_iter_get_val(key);
                    if (!value || !yyjson_is_obj(value)) {
                        error = "package history records must be objects";
                        return false;
                    }
                }
            }
            if (yyjson_val *eventsNode = yyjson_obj_get(root, "events");
                eventsNode) {
                if (!yyjson_is_arr(eventsNode)) {
                    error = "'events' must be an array";
                    return false;
                }

                size_t index = 0;
                size_t max = 0;
                yyjson_val *value = nullptr;
                yyjson_arr_foreach(eventsNode, index, max, value) {
                    if (!value || !yyjson_is_obj(value)) {
                        error = "package events must be objects";
                        return false;
                    }
                }
            }
            return true;
        }

        std::filesystem::path BuildSiblingPath(const std::filesystem::path &path, const std::wstring &suffix) {
            return path.parent_path() / std::filesystem::path(path.filename().wstring() + suffix);
        }

        StateReadFailure ReadStateRoot(const std::filesystem::path &path,
                                       utils::JsonDocument &document,
                                       std::string &error) {
            document = utils::JsonDocument::ParseFile(path, error);
            if (!document.IsValid()) {
                return error == "Unable to open JSON file"
                    ? StateReadFailure::Open
                    : StateReadFailure::Parse;
            }
            yyjson_val *root = document.Root();
            if (!ValidateStateSchema(root, error)) {
                document = {};
                return StateReadFailure::Parse;
            }
            return StateReadFailure::None;
        }
    } // namespace

    bool PackageInstallState::Load(const std::wstring &state_path, std::string &error) {
        error.clear();
        m_SchemaVersion = 1;
        m_Installed.clear();
        m_Packages.clear();
        m_Events.clear();
        m_LastLoadWarning.clear();

        std::filesystem::path path(state_path);
        const std::filesystem::path backupPath = BuildSiblingPath(path, L".bak");
        if (!std::filesystem::exists(path)) {
            if (!std::filesystem::exists(backupPath)) {
                return true;
            }
        }

        utils::JsonDocument document;
        std::string readError;
        StateReadFailure readFailure = StateReadFailure::None;
        std::filesystem::path loadedFrom;

        if (std::filesystem::exists(path)) {
            readFailure = ReadStateRoot(path, document, readError);
            if (readFailure == StateReadFailure::Open) {
                error = readError;
                return false;
            }
            if (readFailure == StateReadFailure::None) {
                loadedFrom = path;
            }
        }

        if (readFailure == StateReadFailure::Parse) {
            const std::wstring corruptSuffix =
                L".corrupt." + utils::Utf8ToUtf16(utils::GetCurrentUtcCompactTimestamp());
            const std::filesystem::path corruptPath = BuildSiblingPath(path, corruptSuffix);
            std::error_code renameEc;
            std::filesystem::rename(path, corruptPath, renameEc);

            m_LastLoadWarning = "Package state file was corrupt and has been reset";
            if (!renameEc) {
                m_LastLoadWarning += " (" + utils::Utf16ToUtf8(corruptPath.filename().wstring()) + ")";
            }

            readError.clear();
            readFailure = StateReadFailure::None;
            loadedFrom.clear();
        }

        if (!loadedFrom.empty() && loadedFrom == path) {
            // Primary state file was loaded successfully.
        } else if (std::filesystem::exists(backupPath)) {
            utils::JsonDocument backupDocument;
            std::string backupError;
            const StateReadFailure backupFailure = ReadStateRoot(backupPath, backupDocument, backupError);
            if (backupFailure == StateReadFailure::None) {
                document = std::move(backupDocument);
                loadedFrom = backupPath;
                if (m_LastLoadWarning.empty()) {
                    m_LastLoadWarning = "Recovered package state from backup";
                } else {
                    m_LastLoadWarning += "; recovered from backup";
                }
            } else if (m_LastLoadWarning.empty()) {
                m_LastLoadWarning = "Package state backup could not be read";
            }
        }

        if (!document.IsValid()) {
            return true;
        }

        yyjson_val *root = document.Root();

        m_SchemaVersion = static_cast<int>(utils::JsonGetInt(root, "schema_version", 1));

        if (yyjson_val *installedNode = yyjson_obj_get(root, "installed");
            installedNode && yyjson_is_obj(installedNode)) {
            yyjson_obj_iter iter = yyjson_obj_iter_with(installedNode);
            yyjson_val *key = nullptr;
            while ((key = yyjson_obj_iter_next(&iter)) != nullptr) {
                yyjson_val *value = yyjson_obj_iter_get_val(key);
                if (!value || !yyjson_is_obj(value)) {
                    continue;
                }

                const char *packageId = yyjson_get_str(key);
                if (!packageId) {
                    continue;
                }

                InstalledRecord record;
                record.package_id = packageId;
                record.current_hash = utils::JsonGetString(value, "current_hash");
                record.version = utils::JsonGetString(value, "version");
                record.installed_path = utils::JsonGetWString(value, "installed_path");
                record.entry = utils::JsonGetString(value, "entry");
                record.updated_at_utc = utils::JsonGetString(value, "updated_at_utc");
                m_Installed[record.package_id] = std::move(record);
            }
        }

        if (yyjson_val *packagesNode = yyjson_obj_get(root, "packages");
            packagesNode && yyjson_is_obj(packagesNode)) {
            yyjson_obj_iter iter = yyjson_obj_iter_with(packagesNode);
            yyjson_val *key = nullptr;
            while ((key = yyjson_obj_iter_next(&iter)) != nullptr) {
                yyjson_val *value = yyjson_obj_iter_get_val(key);
                if (!value || !yyjson_is_obj(value)) {
                    continue;
                }

                const char *contentHash = yyjson_get_str(key);
                if (!contentHash) {
                    continue;
                }

                PackageRecord record;
                record.package_id = utils::JsonGetString(value, "package_id");
                record.version = utils::JsonGetString(value, "version");
                record.content_hash = utils::JsonGetString(value, "content_hash", contentHash);
                record.archive_path = utils::JsonGetWString(value, "archive_path");
                record.status = utils::JsonGetString(value, "status");
                record.entry = utils::JsonGetString(value, "entry");
                record.manifest_hash = utils::JsonGetString(value, "manifest_hash");
                record.installed_at_utc = utils::JsonGetString(value, "installed_at_utc");
                record.last_seen_at_utc = utils::JsonGetString(value, "last_seen_at_utc");
                m_Packages[record.content_hash] = std::move(record);
            }
        }

        if (yyjson_val *eventsNode = yyjson_obj_get(root, "events");
            eventsNode && yyjson_is_arr(eventsNode)) {
            size_t index = 0;
            size_t max = 0;
            yyjson_val *value = nullptr;
            yyjson_arr_foreach(eventsNode, index, max, value) {
                if (!value || !yyjson_is_obj(value)) {
                    continue;
                }

                PackageEvent event;
                event.time_utc = utils::JsonGetString(value, "time_utc");
                event.type = utils::JsonGetString(value, "type");
                event.package_id = utils::JsonGetString(value, "package_id");
                event.content_hash = utils::JsonGetString(value, "content_hash");
                event.message = utils::JsonGetString(value, "message");
                m_Events.emplace_back(std::move(event));
            }
        }

        return true;
    }

    bool PackageInstallState::Save(const std::wstring &state_path, std::string &error) const {
        error.clear();
        utils::MutableJsonDocument document;
        if (!document.IsValid()) {
            error = "Unable to allocate JSON document";
            return false;
        }

        yyjson_mut_val *root = document.CreateObject();
        if (!root) {
            error = "Unable to create JSON root object";
            return false;
        }
        document.SetRoot(root);
        if (!document.AddInt(root, "schema_version", m_SchemaVersion)) {
            error = "Unable to write JSON schema_version";
            return false;
        }

        yyjson_mut_val *installedNode = document.CreateObject();
        yyjson_mut_val *packagesNode = document.CreateObject();
        yyjson_mut_val *eventsNode = document.CreateArray();
        if (!installedNode || !packagesNode || !eventsNode) {
            error = "Unable to allocate JSON containers";
            return false;
        }
        for (const auto &[packageId, record] : m_Installed) {
            yyjson_mut_val *entry = document.CreateObject();
            if (!entry
                || !document.AddString(entry, "current_hash", record.current_hash)
                || !document.AddString(entry, "version", record.version)
                || !document.AddString(entry, "installed_path", utils::Utf16ToUtf8(record.installed_path))
                || !document.AddString(entry, "entry", record.entry)
                || !document.AddString(entry, "updated_at_utc", record.updated_at_utc)
                || !document.AddValue(installedNode, packageId, entry)) {
                error = "Unable to serialize installed package state";
                return false;
            }
        }

        for (const auto &[contentHash, record] : m_Packages) {
            yyjson_mut_val *entry = document.CreateObject();
            if (!entry
                || !document.AddString(entry, "package_id", record.package_id)
                || !document.AddString(entry, "version", record.version)
                || !document.AddString(entry, "content_hash", record.content_hash)
                || !document.AddString(entry, "archive_path", utils::Utf16ToUtf8(record.archive_path))
                || !document.AddString(entry, "status", record.status)
                || !document.AddString(entry, "entry", record.entry)
                || !document.AddString(entry, "manifest_hash", record.manifest_hash)
                || !document.AddString(entry, "installed_at_utc", record.installed_at_utc)
                || !document.AddString(entry, "last_seen_at_utc", record.last_seen_at_utc)
                || !document.AddValue(packagesNode, contentHash, entry)) {
                error = "Unable to serialize package history";
                return false;
            }
        }

        for (const auto &event : m_Events) {
            yyjson_mut_val *entry = document.CreateObject();
            if (!entry
                || !document.AddString(entry, "time_utc", event.time_utc)
                || !document.AddString(entry, "type", event.type)
                || !document.AddString(entry, "package_id", event.package_id)
                || !document.AddString(entry, "content_hash", event.content_hash)
                || !document.AddString(entry, "message", event.message)
                || !document.AddValue(eventsNode, entry)) {
                error = "Unable to serialize package events";
                return false;
            }
        }

        if (!document.AddValue(root, "installed", installedNode)
            || !document.AddValue(root, "packages", packagesNode)
            || !document.AddValue(root, "events", eventsNode)) {
            error = "Unable to attach JSON containers";
            return false;
        }

        const std::string payload = document.Write(true, error);
        if (payload.empty()) {
            return false;
        }

        std::filesystem::path path(state_path);
        const std::filesystem::path tempPath = BuildSiblingPath(path, L".tmp");
        const std::filesystem::path backupPath = BuildSiblingPath(path, L".bak");
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            error = ec.message();
            return false;
        }

        std::ofstream ofs(tempPath, std::ios::binary | std::ios::trunc);
        if (!ofs.is_open()) {
            error = "Unable to write package state file";
            return false;
        }

        try {
            ofs << payload;
        } catch (const std::exception &ex) {
            ofs.close();
            std::filesystem::remove(tempPath, ec);
            error = ex.what();
            return false;
        }

        ofs.flush();
        if (!ofs.good()) {
            ofs.close();
            std::filesystem::remove(tempPath, ec);
            error = "Unable to flush package state file";
            return false;
        }
        ofs.close();

        std::filesystem::remove(backupPath, ec);
        ec.clear();
        if (std::filesystem::exists(path)) {
            std::filesystem::rename(path, backupPath, ec);
            if (ec) {
                std::filesystem::remove(tempPath, ec);
                error = "Unable to prepare package state backup";
                return false;
            }
        }

        ec.clear();
        std::filesystem::rename(tempPath, path, ec);
        if (ec) {
            std::error_code restoreEc;
            if (std::filesystem::exists(backupPath)) {
                std::filesystem::rename(backupPath, path, restoreEc);
            }
            std::filesystem::remove(tempPath, ec);
            error = "Unable to replace package state file";
            return false;
        }

        std::filesystem::remove(backupPath, ec);
        return true;
    }

    const std::string &PackageInstallState::GetLastLoadWarning() const {
        return m_LastLoadWarning;
    }

    const InstalledRecord *PackageInstallState::FindInstalled(std::string_view package_id) const {
        auto it = m_Installed.find(std::string(package_id));
        return it == m_Installed.end() ? nullptr : &it->second;
    }

    const PackageRecord *PackageInstallState::FindPackageByHash(std::string_view content_hash) const {
        auto it = m_Packages.find(std::string(content_hash));
        return it == m_Packages.end() ? nullptr : &it->second;
    }

    void PackageInstallState::UpsertInstalled(InstalledRecord record) {
        m_Installed[record.package_id] = std::move(record);
    }

    void PackageInstallState::UpsertPackage(PackageRecord record) {
        m_Packages[record.content_hash] = std::move(record);
    }

    void PackageInstallState::AppendEvent(PackageEvent event) {
        m_Events.emplace_back(std::move(event));
    }
} // namespace BML::Core
