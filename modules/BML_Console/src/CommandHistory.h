#ifndef BML_CONSOLE_COMMAND_HISTORY_H
#define BML_CONSOLE_COMMAND_HISTORY_H

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "CommandRegistry.h"

namespace BML::Console {

class CommandHistory {
public:
    void Load(const std::filesystem::path &path) {
        m_Entries.clear();
        std::error_code ec;
        if (!std::filesystem::exists(path, ec))
            return;

        std::ifstream stream(path);
        if (!stream.is_open())
            return;

        std::string line;
        while (std::getline(stream, line)) {
            line = TrimCopy(line);
            if (line.empty())
                continue;
            m_Entries.erase(std::remove(m_Entries.begin(), m_Entries.end(), line), m_Entries.end());
            m_Entries.push_back(line);
        }
    }

    void Save(const std::filesystem::path &path) const {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);

        if (m_Entries.empty()) {
            std::filesystem::remove(path, ec);
            return;
        }

        std::ofstream stream(path, std::ios::trunc);
        if (!stream.is_open())
            return;

        for (const std::string &entry : m_Entries)
            stream << entry << '\n';
    }

    void Append(const std::string &command) {
        m_Entries.erase(std::remove(m_Entries.begin(), m_Entries.end(), command), m_Entries.end());
        m_Entries.push_back(command);
        m_Index = -1;
    }

    void Clear() {
        m_Entries.clear();
        m_Index = -1;
    }

    void NavigateUp() {
        if (m_Entries.empty()) return;
        if (m_Index == -1) {
            m_Index = static_cast<int>(m_Entries.size()) - 1;
        } else if (m_Index > 0) {
            --m_Index;
        }
    }

    void NavigateDown() {
        if (m_Index != -1 && ++m_Index >= static_cast<int>(m_Entries.size())) {
            m_Index = -1;
        }
    }

    const std::string &Current() const {
        static const std::string kEmpty;
        if (m_Index >= 0 && m_Index < static_cast<int>(m_Entries.size()))
            return m_Entries[static_cast<size_t>(m_Index)];
        return kEmpty;
    }

    void ResetIndex() { m_Index = -1; }
    void SetIndexToEnd() { m_Index = static_cast<int>(m_Entries.size()); }
    int Index() const { return m_Index; }

    const std::vector<std::string> &Entries() const { return m_Entries; }
    std::vector<std::string> &Entries() { return m_Entries; }

private:
    std::vector<std::string> m_Entries;
    int m_Index = -1;
};

} // namespace BML::Console

#endif // BML_CONSOLE_COMMAND_HISTORY_H
