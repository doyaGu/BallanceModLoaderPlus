#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string_view>

namespace BML::Platform {
inline bool MatchesPeImage(const std::filesystem::path &path, std::string_view architecture,
                           bool executable) {
    const auto word = [](const unsigned char *bytes) {
        return std::uint32_t(bytes[0]) | (std::uint32_t(bytes[1]) << 8);
    };
    std::ifstream file(path, std::ios::binary);
    std::array<unsigned char, 64> dos{};
    if (!file.read(reinterpret_cast<char *>(dos.data()), dos.size()) ||
        dos[0] != 'M' || dos[1] != 'Z')
        return false;
    const auto offset = word(dos.data() + 0x3c) | (word(dos.data() + 0x3e) << 16);
    std::array<unsigned char, 26> pe{};
    if (offset < dos.size() || !file.seekg(offset, std::ios::beg) ||
        !file.read(reinterpret_cast<char *>(pe.data()), pe.size()) ||
        pe[0] != 'P' || pe[1] != 'E' || pe[2] != 0 || pe[3] != 0)
        return false;
    const auto expectedMachine = architecture == "x86" ? 0x14cu :
        architecture == "x64" ? 0x8664u : architecture == "arm64" ? 0xaa64u : 0u;
    const auto expectedMagic = architecture == "x86" ? 0x10bu : 0x20bu;
    const bool dll = (word(pe.data() + 22) & 0x2000u) != 0;
    return expectedMachine != 0 && word(pe.data() + 4) == expectedMachine &&
        word(pe.data() + 24) == expectedMagic && dll != executable;
}
}
