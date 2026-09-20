// Command history of the console: the persisted list of lines, bash-style
// event designators (!!, !n, !-n, !prefix), Up/Down navigation that keeps the
// draft and filters by prefix, reverse search, and fish-style autosuggestion.
#ifndef BML_SHELL_HISTORY_H
#define BML_SHELL_HISTORY_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace BML::Shell {
    class History {
    public:
        // One logical command per entry, oldest first. Commands may contain
        // physical newlines. An equal entry moves to the newest position.
        const std::vector<std::string> &Entries() const { return m_Entries; }
        std::size_t Size() const { return m_Entries.size(); }
        bool Empty() const { return m_Entries.empty(); }
        // Changes whenever the entry sequence changes. Consumers may use this
        // to keep derived views without rescanning an unchanged history.
        std::uint64_t Revision() const { return m_Revision; }

        // Adds an entry and saves when a path is set.
        void Add(std::string entry);
        // Removes the entry with the given 1-based number, oldest being 1.
        bool Erase(std::size_t number);
        void Clear();

        void SetPath(std::wstring path) { m_Path = std::move(path); }
        const std::wstring &Path() const { return m_Path; }
        void Load();
        bool Save() const;

        // Applies event designators. Outside single quotes and not escaped, "!!"
        // is the newest entry, "!n" entry number n, "!-n" the n-th newest, and
        // "!text" the newest entry starting with text; a "!" followed by nothing,
        // whitespace, "=", or "(" is left alone. Returns false with error set for
        // an event that does not exist. changed says whether anything expanded.
        bool Expand(std::string_view line, std::string &out, bool &changed, std::string &error) const;

        // Index of the newest entry before `from` containing query, or -1.
        int SearchBackward(std::string_view query, int from) const;

        // Newest entry that starts with prefix and is longer than it, or null.
        const std::string *Suggest(std::string_view prefix) const;

        // Length-prefixed text form used by the file and by tests. FromText also
        // accepts the legacy one-command-per-line representation.
        void FromText(std::string_view text);
        std::string ToText() const;

    private:
        std::vector<std::string> m_Entries;
        std::wstring m_Path;
        std::uint64_t m_Revision = 0;
    };

    // Up/Down over a History. Leaving the draft saves it; walking back past the
    // newest entry restores it. A non-empty draft restricts the walk to entries
    // that start with it, the way fish does.
    class HistoryNavigator {
    public:
        // Forget the current walk; call when the bar opens, a line runs, or the
        // player edits the text.
        void Reset();

        bool Browsing() const { return m_Index >= 0; }

        // Both return true and fill text when the buffer should change.
        bool Up(const History &history, std::string_view current, std::string &text);
        bool Down(const History &history, std::string_view current, std::string &text);

    private:
        static bool Matches(const std::string &entry, const std::string &filter);

        int m_Index = -1;
        std::string m_Draft;
        std::string m_Filter;
    };
}

#endif // BML_SHELL_HISTORY_H
