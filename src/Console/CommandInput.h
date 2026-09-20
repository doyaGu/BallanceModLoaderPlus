// Console-owned representation of logical multi-line input. ImGui owns the
// editable current row while this module retains earlier continuation rows and
// keeps conversions between both representations lossless and testable.
#ifndef BML_COMMANDINPUT_H
#define BML_COMMANDINPUT_H

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace CommandInput {
    // Owns the physical rows that precede ImGui's editable current row. It is
    // the single owner of their logical byte offset and lossless conversions.
    class Continuations {
    public:
        bool Empty() const noexcept { return m_Rows.empty(); }
        std::size_t Size() const noexcept { return m_Rows.size(); }
        std::size_t Bytes() const noexcept { return m_Bytes; }
        const std::vector<std::string> &Rows() const noexcept { return m_Rows; }

        std::string Join(std::string_view current) const;
        void Push(std::string row);
        void Clear() noexcept;

        // Replaces the logical command, retaining every completed row and
        // returning the final row for ImGui's editor.
        std::string Replace(std::string_view logical);

    private:
        std::vector<std::string> m_Rows;
        std::size_t m_Bytes = 0;
    };

    // Makes multi-line history/search text safe to draw inside a single rail.
    std::string SingleLinePreview(std::string_view text);
}

#endif // BML_COMMANDINPUT_H
