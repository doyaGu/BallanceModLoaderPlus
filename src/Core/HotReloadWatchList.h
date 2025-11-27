#ifndef BML_CORE_HOT_RELOAD_WATCH_LIST_H
#define BML_CORE_HOT_RELOAD_WATCH_LIST_H

#include <filesystem>
#include <string>
#include <vector>

namespace BML::Core {

class HotReloadWatchList {
public:
    void Reset(const std::vector<std::wstring> &paths);
    bool Empty() const { return m_Entries.empty(); }
    bool DetectChanges();

private:
    struct Entry {
        std::wstring path;
        std::filesystem::file_time_type timestamp{};
        bool exists{false};
    };

    static std::filesystem::file_time_type SampleTimestamp(const std::wstring &path, bool &exists);

    std::vector<Entry> m_Entries;
};

} // namespace BML::Core

#endif // BML_CORE_HOT_RELOAD_WATCH_LIST_H
