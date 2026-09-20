// Pure helpers for the command bar's logical multi-line input. The visible
// editor owns one current row while earlier continuation rows are retained
// separately; these functions keep conversions between both representations
// lossless and testable without ImGui.
#ifndef BML_COMMANDINPUT_H
#define BML_COMMANDINPUT_H

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace CommandInput {
    struct Rows {
        std::vector<std::string> pending;
        std::string current;
        std::size_t currentOffset = 0;
    };

    std::size_t PendingBytes(const std::vector<std::string> &pending) noexcept;
    std::string Join(const std::vector<std::string> &pending, std::string_view current);
    bool Equals(const std::vector<std::string> &pending, std::string_view current,
                std::string_view logical) noexcept;
    Rows Split(std::string_view logical);

    // Makes multi-line history/search text safe to draw inside a single rail.
    std::string SingleLinePreview(std::string_view text);
}

#endif // BML_COMMANDINPUT_H
