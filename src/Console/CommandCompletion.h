#ifndef BML_COMMANDCOMPLETION_H
#define BML_COMMANDCOMPLETION_H

#include <cstddef>
#include <string>
#include <vector>

namespace CommandCompletion {
    // Length in bytes of the prefix every candidate shares, comparing codepoints
    // without regard to case.
    std::size_t CommonPrefixLength(const std::vector<std::string> &candidates) noexcept;
}

#endif // BML_COMMANDCOMPLETION_H
