#ifndef BML_CUSTOMMAPLOAD_H
#define BML_CUSTOMMAPLOAD_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>

#include "BML/DataShare.h"

namespace CustomMapLoad {

constexpr std::uint32_t ProtocolVersion = 1;
constexpr char RequestKey[] = "BML.CustomMapLoad.Request";
constexpr char ResultKey[] = "BML.CustomMapLoad.Result";
constexpr char NameKey[] = "CustomMapName";

enum class Outcome : std::uint32_t {
    Loaded = 1,
    Failed = 2,
};

struct Request {
    std::uint32_t Version = ProtocolVersion;
    std::uint32_t Reserved = 0;
    std::uint64_t Attempt = 0;
};

struct Result {
    std::uint32_t Version = ProtocolVersion;
    Outcome Value = Outcome::Failed;
    std::uint64_t Attempt = 0;
};

static_assert(std::is_trivially_copyable_v<Request> && sizeof(Request) == 16);
static_assert(std::is_trivially_copyable_v<Result> && sizeof(Result) == 16);

inline bool ReadRequest(const BML_DataShare *share, Request &request) {
    std::size_t size = 0;
    const int copied = BML_DataShare_CopyEx(
        share, RequestKey, &request, sizeof(request), &size);
    return copied == 1 && size == sizeof(request) &&
           request.Version == ProtocolVersion && request.Attempt != 0;
}

inline bool WriteRequest(BML_DataShare *share, std::uint64_t attempt) {
    Request request;
    request.Attempt = attempt;
    return BML_DataShare_Set(
        share, RequestKey, &request, sizeof(request)) != 0;
}

inline bool ReadResult(const BML_DataShare *share, Result &result) {
    std::size_t size = 0;
    const int copied = BML_DataShare_CopyEx(
        share, ResultKey, &result, sizeof(result), &size);
    return copied == 1 && size == sizeof(result) &&
           result.Version == ProtocolVersion && result.Attempt != 0;
}

inline bool WriteResult(BML_DataShare *share, std::uint64_t attempt,
                        Outcome outcome) {
    Result result;
    result.Value = outcome;
    result.Attempt = attempt;
    return BML_DataShare_Set(
        share, ResultKey, &result, sizeof(result)) != 0;
}

inline std::uint64_t HashPath(std::wstring_view path) {
    std::uint64_t hash = 14695981039346656037ull;
    for (wchar_t character : path) {
        const std::uint16_t value = character == L'/'
            ? static_cast<std::uint16_t>(L'\\')
            : static_cast<std::uint16_t>(character);
        hash ^= value & 0xffu;
        hash *= 1099511628211ull;
        hash ^= value >> 8u;
        hash *= 1099511628211ull;
    }
    return hash;
}

inline void AppendHex(std::wstring &text, std::uint64_t value) {
    constexpr wchar_t Digits[] = L"0123456789ABCDEF";
    for (int shift = 60; shift >= 0; shift -= 4)
        text.push_back(Digits[(value >> shift) & 0x0fu]);
}

inline std::wstring MakeTempFileName(std::wstring_view sourcePath,
                                     std::wstring_view extension,
                                     std::uint64_t attempt) {
    std::wstring name;
    name.reserve(16u + 1u + 16u + extension.size());
    AppendHex(name, HashPath(sourcePath));
    name.push_back(L'-');
    AppendHex(name, attempt);
    name.append(extension);
    return name;
}

} // namespace CustomMapLoad

#endif // BML_CUSTOMMAPLOAD_H
