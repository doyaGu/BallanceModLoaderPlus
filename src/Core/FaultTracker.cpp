#include "FaultTracker.h"

#include "KernelServices.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <unordered_map>

#include "JsonUtils.h"
#include "Logging.h"
#include "StringUtils.h"
#include "TimeUtils.h"

namespace BML::Core {
    namespace {
        constexpr char kLogCategory[] = "fault.tracker";
        constexpr int kFaultDisableThreshold = 3;

        std::wstring GetFaultLogPath(const std::wstring &base_dir) {
            std::filesystem::path dir(base_dir);
            return (dir / L"ModLoader" / L"fault_log.json").wstring();
        }

        std::string ExceptionCodeToHex(unsigned long code) {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "0x%08lX", code);
            return buf;
        }

        void WriteJson(const std::wstring &path,
                       const std::unordered_map<std::string, FaultTracker::FaultRecord> &records) {
            std::filesystem::path p(path);
            std::error_code ec;
            std::filesystem::create_directories(p.parent_path(), ec);

            utils::MutableJsonDocument document;
            yyjson_mut_val *root = document.CreateObject();
            if (!root) {
                return;
            }
            document.SetRoot(root);

            for (const auto &[id, rec] : records) {
                yyjson_mut_val *entry = document.CreateObject();
                if (!entry
                    || !document.AddInt(entry, "fault_count", rec.fault_count)
                    || !document.AddString(entry, "last_fault", rec.last_fault)
                    || !document.AddString(entry, "last_code", rec.last_code)
                    || !document.AddBool(entry, "disabled", rec.disabled)
                    || !document.AddValue(root, id, entry)) {
                    CoreLog(BML_LOG_WARN, kLogCategory,
                            "Failed to serialize fault log entry for '%s'",
                            id.c_str());
                    return;
                }
            }

            std::string error;
            const std::string payload = document.Write(true, error);
            if (payload.empty()) {
                CoreLog(BML_LOG_WARN, kLogCategory,
                        "Failed to write fault log JSON: %s",
                        error.c_str());
                return;
            }

            std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
            if (!ofs.is_open()) {
                CoreLog(BML_LOG_WARN, kLogCategory,
                        "Failed to write fault log: %s",
                        utils::Utf16ToUtf8(path).c_str());
                return;
            }
            ofs << payload;
        }
    } // namespace

    struct FaultTracker::Impl {
        std::mutex mutex;
        std::unordered_map<std::string, FaultTracker::FaultRecord> records;
        std::wstring base_dir;
        bool loaded = false;
    };

    FaultTracker::FaultTracker() : m_Impl(std::make_unique<Impl>()) {}
    FaultTracker::~FaultTracker() = default;

    void FaultTracker::Load(const std::wstring &base_dir) {
        auto &impl = *m_Impl;
        std::lock_guard lock(impl.mutex);

        impl.base_dir = base_dir;
        impl.loaded = true;
        std::unordered_map<std::string, FaultTracker::FaultRecord> loadedRecords;

        auto path = GetFaultLogPath(base_dir);
        std::error_code ec;
        if (!std::filesystem::exists(path, ec)) {
            impl.records.clear();
            return;
        }

        std::string error;
        auto document = utils::JsonDocument::ParseFile(path, error);
        if (!document.IsValid()) {
            impl.records.clear();
            CoreLog(BML_LOG_WARN, kLogCategory,
                    "Failed to parse fault log JSON: %s",
                    error.c_str());
            return;
        }

        yyjson_val *root = document.Root();
        if (!root || !yyjson_is_obj(root)) {
            impl.records.clear();
            return;
        }

        yyjson_obj_iter iter = yyjson_obj_iter_with(root);
        yyjson_val *key = nullptr;
        while ((key = yyjson_obj_iter_next(&iter)) != nullptr) {
            yyjson_val *value = yyjson_obj_iter_get_val(key);
            if (!value || !yyjson_is_obj(value)) {
                continue;
            }

            const char *moduleId = yyjson_get_str(key);
            if (!moduleId) {
                continue;
            }

            FaultTracker::FaultRecord record;
            record.fault_count = static_cast<int>(utils::JsonGetInt(value, "fault_count", 0));
            record.last_fault = utils::JsonGetString(value, "last_fault");
            record.last_code = utils::JsonGetString(value, "last_code");
            record.disabled = utils::JsonGetBool(value, "disabled", false);
            loadedRecords[moduleId] = std::move(record);
        }

        impl.records = std::move(loadedRecords);

        for (const auto &[id, rec] : impl.records) {
            if (rec.disabled) {
                CoreLog(BML_LOG_WARN, kLogCategory,
                        "Module '%s' disabled due to %d previous crash(es) (last: %s)",
                        id.c_str(), rec.fault_count, rec.last_code.c_str());
            }
        }
    }

    void FaultTracker::RecordFault(const std::string &module_id,
                                   unsigned long exception_code) {
        auto &impl = *m_Impl;
        std::lock_guard lock(impl.mutex);

        auto &rec = impl.records[module_id];
        rec.fault_count++;
        rec.last_fault = utils::GetCurrentLocalIso8601();
        rec.last_code = ExceptionCodeToHex(exception_code);
        if (rec.fault_count >= kFaultDisableThreshold) {
            rec.disabled = true;
        }

        CoreLog(BML_LOG_ERROR, kLogCategory,
                "Fault recorded for '%s': code=%s, total=%d%s",
                module_id.c_str(), rec.last_code.c_str(), rec.fault_count,
                rec.disabled ? " [DISABLED]" : "");

        if (!impl.base_dir.empty()) {
            WriteJson(GetFaultLogPath(impl.base_dir), impl.records);
        }
    }

    bool FaultTracker::IsDisabled(const std::string &module_id) const {
        auto &impl = *m_Impl;
        std::lock_guard lock(impl.mutex);

        auto it = impl.records.find(module_id);
        return it != impl.records.end() && it->second.disabled;
    }

    void FaultTracker::Enable(const std::string &module_id) {
        auto &impl = *m_Impl;
        std::lock_guard lock(impl.mutex);

        auto it = impl.records.find(module_id);
        if (it != impl.records.end()) {
            it->second.disabled = false;
            it->second.fault_count = 0;
            CoreLog(BML_LOG_INFO, kLogCategory,
                    "Module '%s' re-enabled", module_id.c_str());

            if (!impl.base_dir.empty()) {
                WriteJson(GetFaultLogPath(impl.base_dir), impl.records);
            }
        }
    }

    int FaultTracker::GetFaultCount(const std::string &module_id) const {
        auto &impl = *m_Impl;
        std::lock_guard lock(impl.mutex);

        auto it = impl.records.find(module_id);
        return it != impl.records.end() ? it->second.fault_count : 0;
    }

    void FaultTracker::Shutdown() {
        auto &impl = *m_Impl;
        std::lock_guard lock(impl.mutex);
        impl.records.clear();
        impl.base_dir.clear();
        impl.loaded = false;
    }
} // namespace BML::Core
