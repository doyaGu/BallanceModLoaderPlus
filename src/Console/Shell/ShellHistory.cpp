#include "Console/Shell/ShellHistory.h"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <cstdlib>

#include "Console/Shell/ShellQuoting.h"
#include "Console/Shell/ShellTypes.h"
#include "PathUtils.h"

namespace BML::Shell {
    namespace {
        constexpr std::string_view kHistoryFormatHeader = "BMLHIST2\n";

        bool StartsWith(std::string_view text, std::string_view prefix) {
            return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
        }

        void MergeEntry(std::vector<std::string> &entries, std::string entry) {
            if (entry.empty() || entry.size() > Limits::MaxLineBytes)
                return;
            const auto existing = std::find(entries.begin(), entries.end(), entry);
            if (existing != entries.end())
                entries.erase(existing);
            entries.push_back(std::move(entry));
        }

        bool DecodeLengthPrefixed(std::string_view text, std::vector<std::string> &entries) {
            if (!StartsWith(text, kHistoryFormatHeader))
                return false;

            std::vector<std::string> decoded;
            std::size_t cursor = kHistoryFormatHeader.size();
            while (cursor < text.size()) {
                const std::size_t colon = text.find(':', cursor);
                if (colon == std::string_view::npos || colon == cursor)
                    return false;

                std::size_t length = 0;
                const char *first = text.data() + cursor;
                const char *last = text.data() + colon;
                const auto parsed = std::from_chars(first, last, length);
                if (parsed.ec != std::errc() || parsed.ptr != last)
                    return false;

                cursor = colon + 1;
                if (length > text.size() - cursor)
                    return false;
                MergeEntry(decoded, std::string(text.substr(cursor, length)));
                cursor += length;
                if (cursor == text.size())
                    break;
                if (text[cursor] != '\n')
                    return false;
                ++cursor;
            }
            entries = std::move(decoded);
            return true;
        }

        // Characters that end the word of a "!prefix" designator.
        bool EndsEventWord(char c) {
            switch (c) {
                case ' ':
                case '\t':
                case '\r':
                case '\n':
                case ';':
                case '|':
                case '&':
                case '(':
                case ')':
                case '\'':
                case '"':
                case '`':
                case '$':
                case '!':
                    return true;
                default:
                    return false;
            }
        }
    }

    void History::Add(std::string entry) {
        MergeEntry(m_Entries, std::move(entry));
        if (!m_Path.empty())
            Save();
    }

    bool History::Erase(std::size_t number) {
        if (number == 0 || number > m_Entries.size())
            return false;
        m_Entries.erase(m_Entries.begin() + static_cast<std::ptrdiff_t>(number - 1));
        if (!m_Path.empty())
            Save();
        return true;
    }

    void History::Clear() {
        m_Entries.clear();
        if (!m_Path.empty())
            Save();
    }

    void History::FromText(std::string_view text) {
        m_Entries.clear();
        if (StartsWith(text, kHistoryFormatHeader)) {
            DecodeLengthPrefixed(text, m_Entries);
            return;
        }

        // Legacy files stored one command per physical line. Keep accepting
        // them so upgrading does not discard an existing history.
        std::size_t start = 0;
        while (start < text.size()) {
            std::size_t end = text.find('\n', start);
            if (end == std::string_view::npos)
                end = text.size();
            std::string_view line = text.substr(start, end - start);
            if (!line.empty() && line.back() == '\r')
                line.remove_suffix(1);
            start = end + 1;
            if (line.empty() || line[0] == '\0')
                continue;
            MergeEntry(m_Entries, std::string(line));
        }
    }

    std::string History::ToText() const {
        std::string content(kHistoryFormatHeader);
        for (const std::string &entry : m_Entries) {
            content += std::to_string(entry.size());
            content.push_back(':');
            content += entry;
            content.push_back('\n');
        }
        return content;
    }

    void History::Load() {
        m_Entries.clear();
        if (m_Path.empty())
            return;
        const std::vector<std::uint8_t> bytes = utils::ReadBinaryFileW(m_Path);
        if (bytes.empty())
            return;
        FromText(std::string_view(reinterpret_cast<const char *>(bytes.data()), bytes.size()));
    }

    bool History::Save() const {
        if (m_Path.empty())
            return false;
        const std::wstring tempPath = m_Path + L".tmp";
        if (m_Entries.empty()) {
            utils::DeleteFileW(tempPath);
            utils::DeleteFileW(m_Path);
            return true;
        }
        const std::string content = ToText();
        const std::vector<std::uint8_t> bytes(content.begin(), content.end());
        if (!utils::WriteBinaryFileW(tempPath, bytes)) {
            utils::DeleteFileW(tempPath);
            return false;
        }
        if (!utils::MoveFileW(tempPath, m_Path)) {
            utils::DeleteFileW(tempPath);
            return false;
        }
        return true;
    }

    bool History::Expand(std::string_view line, std::string &out, bool &changed, std::string &error) const {
        out.clear();
        changed = false;
        error.clear();
        bool singleQuoted = false;
        std::size_t i = 0;
        while (i < line.size()) {
            const char c = line[i];
            if (singleQuoted) {
                out.push_back(c);
                if (c == '\'')
                    singleQuoted = false;
                ++i;
                continue;
            }
            if (c == '\\' && i + 1 < line.size()) {
                out.push_back(c);
                out.push_back(line[i + 1]);
                i += 2;
                continue;
            }
            if (c == '\'') {
                singleQuoted = true;
                out.push_back(c);
                ++i;
                continue;
            }
            if (c != '!') {
                out.push_back(c);
                ++i;
                continue;
            }

            // At an unescaped '!'.
            const std::size_t designatorStart = i;
            ++i;
            if (i >= line.size() || IsWhitespace(line[i]) || line[i] == '\n' || line[i] == '=' || line[i] == '(') {
                out.push_back('!');
                continue;
            }

            const std::string *entry = nullptr;
            std::string designator;
            if (line[i] == '!') {
                ++i;
                designator = "!!";
                if (!m_Entries.empty())
                    entry = &m_Entries.back();
            } else if (line[i] == '-' || (line[i] >= '0' && line[i] <= '9')) {
                const bool relative = line[i] == '-';
                std::size_t j = relative ? i + 1 : i;
                const std::size_t digitsBegin = j;
                while (j < line.size() && line[j] >= '0' && line[j] <= '9')
                    ++j;
                if (j == digitsBegin) {
                    // "!-" without digits is a literal.
                    out.push_back('!');
                    continue;
                }
                designator = std::string(line.substr(designatorStart, j - designatorStart));
                const long number = std::strtol(std::string(line.substr(digitsBegin, j - digitsBegin)).c_str(), nullptr, 10);
                i = j;
                if (number > 0 && static_cast<std::size_t>(number) <= m_Entries.size()) {
                    if (relative)
                        entry = &m_Entries[m_Entries.size() - static_cast<std::size_t>(number)];
                    else
                        entry = &m_Entries[static_cast<std::size_t>(number) - 1];
                }
            } else {
                std::size_t j = i;
                while (j < line.size() && !EndsEventWord(line[j]))
                    ++j;
                const std::string_view prefix = line.substr(i, j - i);
                designator = std::string(line.substr(designatorStart, j - designatorStart));
                i = j;
                for (auto it = m_Entries.rbegin(); it != m_Entries.rend(); ++it) {
                    if (StartsWith(*it, prefix)) {
                        entry = &*it;
                        break;
                    }
                }
            }

            if (!entry) {
                error = designator + ": event not found";
                out.clear();
                changed = false;
                return false;
            }
            out += *entry;
            changed = true;
        }
        return true;
    }

    int History::SearchBackward(std::string_view query, int from) const {
        int index = std::min(from, static_cast<int>(m_Entries.size()));
        while (index-- > 0) {
            if (query.empty() || m_Entries[static_cast<std::size_t>(index)].find(query) != std::string::npos)
                return index;
        }
        return -1;
    }

    const std::string *History::Suggest(std::string_view prefix) const {
        if (prefix.empty())
            return nullptr;
        for (auto it = m_Entries.rbegin(); it != m_Entries.rend(); ++it) {
            if (it->size() > prefix.size() && StartsWith(*it, prefix))
                return &*it;
        }
        return nullptr;
    }

    void HistoryNavigator::Reset() {
        m_Index = -1;
        m_Draft.clear();
        m_Filter.clear();
    }

    bool HistoryNavigator::Matches(const std::string &entry, const std::string &filter) {
        return filter.empty() || StartsWith(entry, filter);
    }

    bool HistoryNavigator::Up(const History &history, std::string_view current, std::string &text) {
        const auto &entries = history.Entries();
        if (entries.empty())
            return false;
        int index = m_Index;
        if (index < 0) {
            m_Draft.assign(current.data(), current.size());
            m_Filter = m_Draft;
            // A draft that matches nothing falls back to walking everything, so
            // Up always does something useful.
            const bool anyMatch = std::any_of(entries.begin(), entries.end(), [&](const std::string &entry) {
                return Matches(entry, m_Filter) && entry != current;
            });
            if (!anyMatch)
                m_Filter.clear();
            index = static_cast<int>(entries.size());
        }
        while (index-- > 0) {
            const std::string &entry = entries[static_cast<std::size_t>(index)];
            if (Matches(entry, m_Filter) && entry != current) {
                m_Index = index;
                text = entry;
                return true;
            }
        }
        return false;
    }

    bool HistoryNavigator::Down(const History &history, std::string_view current, std::string &text) {
        if (m_Index < 0)
            return false;
        const auto &entries = history.Entries();
        for (int index = m_Index + 1; index < static_cast<int>(entries.size()); ++index) {
            const std::string &entry = entries[static_cast<std::size_t>(index)];
            if (Matches(entry, m_Filter) && entry != current) {
                m_Index = index;
                text = entry;
                return true;
            }
        }
        m_Index = -1;
        text = m_Draft;
        return true;
    }
}
