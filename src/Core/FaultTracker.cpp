#include "FaultTracker.h"

#include "KernelServices.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <unordered_map>

#include "Logging.h"
#include "StringUtils.h"

namespace BML::Core {
    namespace {
        constexpr char kLogCategory[] = "fault.tracker";
        constexpr int kFaultDisableThreshold = 3;

        struct FaultTrackerImpl {
            std::mutex mutex;
            std::unordered_map<std::string, FaultTracker::FaultRecord> records;
            std::wstring base_dir;
            bool loaded = false;
        };

        FaultTrackerImpl &Impl() {
            static FaultTrackerImpl impl;
            return impl;
        }

        std::wstring GetFaultLogPath(const std::wstring &base_dir) {
            std::filesystem::path dir(base_dir);
            return (dir / L"ModLoader" / L"fault_log.json").wstring();
        }

        std::string NowIso8601() {
            auto now = std::chrono::system_clock::now();
            auto time_t_now = std::chrono::system_clock::to_time_t(now);
            std::tm tm_buf{};
#if defined(_WIN32)
            localtime_s(&tm_buf, &time_t_now);
#else
            localtime_r(&time_t_now, &tm_buf);
#endif
            char buf[64];
            std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm_buf);
            return buf;
        }

        std::string ExceptionCodeToHex(unsigned long code) {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "0x%08lX", code);
            return buf;
        }

        // Minimal JSON parser/writer for fault_log.json
        // Format:
        // {
        //   "module.id": {
        //     "fault_count": N,
        //     "last_fault": "...",
        //     "last_code": "0x...",
        //     "disabled": true|false
        //   }
        // }

        std::string EscapeJsonString(const std::string &s) {
            std::string result;
            result.reserve(s.size() + 8);
            for (char c : s) {
                switch (c) {
                    case '"': result += "\\\""; break;
                    case '\\': result += "\\\\"; break;
                    case '\n': result += "\\n"; break;
                    case '\r': result += "\\r"; break;
                    case '\t': result += "\\t"; break;
                    default: result += c; break;
                }
            }
            return result;
        }

        void WriteJson(const std::wstring &path,
                       const std::unordered_map<std::string, FaultTracker::FaultRecord> &records) {
            // Ensure directory exists
            std::filesystem::path p(path);
            std::error_code ec;
            std::filesystem::create_directories(p.parent_path(), ec);

            std::ofstream ofs(path);
            if (!ofs.is_open()) {
                CoreLog(BML_LOG_WARN, kLogCategory,
                        "Failed to write fault log: %s",
                        utils::Utf16ToUtf8(path).c_str());
                return;
            }

            ofs << "{\n";
            size_t i = 0;
            for (const auto &[id, rec] : records) {
                ofs << "  \"" << EscapeJsonString(id) << "\": {\n"
                    << "    \"fault_count\": " << rec.fault_count << ",\n"
                    << "    \"last_fault\": \"" << EscapeJsonString(rec.last_fault) << "\",\n"
                    << "    \"last_code\": \"" << EscapeJsonString(rec.last_code) << "\",\n"
                    << "    \"disabled\": " << (rec.disabled ? "true" : "false") << "\n"
                    << "  }";
                if (++i < records.size())
                    ofs << ",";
                ofs << "\n";
            }
            ofs << "}\n";
        }

        // Simple JSON tokenizer for fault_log.json loading
        void ParseFaultLog(const std::string &json,
                           std::unordered_map<std::string, FaultTracker::FaultRecord> &out) {
            out.clear();
            // Very minimal: scan for patterns like "key": { ... }
            // We rely on our own well-formatted output
            size_t pos = 0;
            auto skipWs = [&]() {
                while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\n' ||
                       json[pos] == '\r' || json[pos] == '\t'))
                    ++pos;
            };
            auto readString = [&]() -> std::string {
                skipWs();
                if (pos >= json.size() || json[pos] != '"')
                    return {};
                ++pos;
                std::string result;
                while (pos < json.size() && json[pos] != '"') {
                    if (json[pos] == '\\' && pos + 1 < json.size()) {
                        ++pos;
                    }
                    result += json[pos++];
                }
                if (pos < json.size()) ++pos; // skip closing "
                return result;
            };
            auto readInt = [&]() -> int {
                skipWs();
                int val = 0;
                bool neg = false;
                if (pos < json.size() && json[pos] == '-') { neg = true; ++pos; }
                while (pos < json.size() && json[pos] >= '0' && json[pos] <= '9') {
                    val = val * 10 + (json[pos] - '0');
                    ++pos;
                }
                return neg ? -val : val;
            };
            auto readBool = [&]() -> bool {
                skipWs();
                if (pos + 4 <= json.size() && json.substr(pos, 4) == "true") {
                    pos += 4;
                    return true;
                }
                if (pos + 5 <= json.size() && json.substr(pos, 5) == "false") {
                    pos += 5;
                    return false;
                }
                return false;
            };
            auto skipChar = [&](char c) {
                skipWs();
                if (pos < json.size() && json[pos] == c) ++pos;
            };

            skipChar('{');
            while (pos < json.size()) {
                skipWs();
                if (pos >= json.size() || json[pos] == '}') break;

                std::string module_id = readString();
                if (module_id.empty()) break;
                skipChar(':');
                skipChar('{');

                FaultTracker::FaultRecord rec;
                // Read fields
                for (int field = 0; field < 4; ++field) {
                    skipWs();
                    if (pos >= json.size() || json[pos] == '}') break;
                    std::string key = readString();
                    skipChar(':');
                    if (key == "fault_count") {
                        rec.fault_count = readInt();
                    } else if (key == "last_fault") {
                        rec.last_fault = readString();
                    } else if (key == "last_code") {
                        rec.last_code = readString();
                    } else if (key == "disabled") {
                        rec.disabled = readBool();
                    }
                    skipChar(',');
                }
                skipChar('}');
                skipChar(',');

                out[module_id] = rec;
            }
        }
    } // namespace

    FaultTracker::FaultTracker() = default;

    void FaultTracker::Load(const std::wstring &base_dir) {
        auto &impl = Impl();
        std::lock_guard lock(impl.mutex);

        impl.base_dir = base_dir;
        impl.loaded = true;

        auto path = GetFaultLogPath(base_dir);
        std::error_code ec;
        if (!std::filesystem::exists(path, ec))
            return;

        std::ifstream ifs(path);
        if (!ifs.is_open())
            return;

        std::string content((std::istreambuf_iterator<char>(ifs)),
                             std::istreambuf_iterator<char>());
        ifs.close();

        ParseFaultLog(content, impl.records);

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
        auto &impl = Impl();
        std::lock_guard lock(impl.mutex);

        auto &rec = impl.records[module_id];
        rec.fault_count++;
        rec.last_fault = NowIso8601();
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
        auto &impl = Impl();
        std::lock_guard lock(impl.mutex);

        auto it = impl.records.find(module_id);
        return it != impl.records.end() && it->second.disabled;
    }

    void FaultTracker::Enable(const std::string &module_id) {
        auto &impl = Impl();
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
        auto &impl = Impl();
        std::lock_guard lock(impl.mutex);

        auto it = impl.records.find(module_id);
        return it != impl.records.end() ? it->second.fault_count : 0;
    }

    void FaultTracker::Shutdown() {
        auto &impl = Impl();
        std::lock_guard lock(impl.mutex);
        impl.records.clear();
        impl.base_dir.clear();
        impl.loaded = false;
    }
} // namespace BML::Core
