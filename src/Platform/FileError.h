#pragma once
#include <cstdint>

// Stable file-result values, also used by existing Windows diagnostics.
namespace BML::Platform::FileError {
inline constexpr std::uint32_t Success = 0;
inline constexpr std::uint32_t NotFound = 2;
inline constexpr std::uint32_t PathNotFound = 3;
inline constexpr std::uint32_t AccessDenied = 5;
inline constexpr std::uint32_t InvalidData = 13;
inline constexpr std::uint32_t WriteFault = 29;
inline constexpr std::uint32_t ReadFault = 30;
inline constexpr std::uint32_t FileExists = 80;
inline constexpr std::uint32_t InvalidName = 123;
inline constexpr std::uint32_t AlreadyExists = 183;
}
