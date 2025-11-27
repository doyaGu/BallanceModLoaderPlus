#include "HotReloadWatchList.h"

#include <system_error>
#include <unordered_set>

namespace BML::Core {
    namespace {
        std::wstring NormalizePath(const std::wstring &value) {
            if (value.empty())
                return value;
            std::filesystem::path p(value);
            auto normalized = p.lexically_normal();
            return normalized.wstring();
        }
    }

    void HotReloadWatchList::Reset(const std::vector<std::wstring> &paths) {
        m_Entries.clear();
        std::unordered_set<std::wstring> seen;
        seen.reserve(paths.size());

        for (const auto &path : paths) {
            if (path.empty())
                continue;
            auto normalized = NormalizePath(path);
            if (normalized.empty())
                continue;
            if (!seen.insert(normalized).second)
                continue;

            Entry entry;
            entry.path = normalized;
            entry.timestamp = SampleTimestamp(entry.path, entry.exists);
            m_Entries.emplace_back(std::move(entry));
        }
    }

    bool HotReloadWatchList::DetectChanges() {
        bool changed = false;
        for (auto &entry : m_Entries) {
            bool exists = false;
            auto ts = SampleTimestamp(entry.path, exists);
            if (entry.exists != exists) {
                entry.exists = exists;
                entry.timestamp = ts;
                changed = true;
                continue;
            }
            if (exists && ts != entry.timestamp) {
                entry.timestamp = ts;
                changed = true;
            }
        }
        return changed;
    }

    std::filesystem::file_time_type HotReloadWatchList::SampleTimestamp(const std::wstring &path, bool &exists) {
        exists = false;
        std::error_code ec;
        auto status = std::filesystem::status(path, ec);
        if (ec)
            return std::filesystem::file_time_type::min();
        exists = std::filesystem::exists(status);
        if (!exists)
            return std::filesystem::file_time_type::min();
        auto timestamp = std::filesystem::last_write_time(path, ec);
        if (ec)
            return std::filesystem::file_time_type::min();
        return timestamp;
    }
} // namespace BML::Core
