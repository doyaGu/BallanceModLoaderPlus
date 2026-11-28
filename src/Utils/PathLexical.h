#pragma once

#include "StringUtils.h"
#include <algorithm>
#include <vector>

namespace utils::path {
inline std::string Slashes(std::string path) { std::replace(path.begin(), path.end(), '\\', '/'); return path; }
inline std::string Path(const std::wstring &path) { return Slashes(Utf16ToUtf8(path)); }
inline std::wstring NativePath(std::string path) {
#ifdef _WIN32
    std::replace(path.begin(), path.end(), '/', '\\');
#endif
    return Utf8ToUtf16(path);
}
inline size_t RootLength(const std::string &path) {
#ifdef _WIN32
    if (path.size() >= 3 && path[1] == ':' && path[2] == '/') return 3;
    if (path.starts_with("//")) {
        auto server = path.find('/', 2);
        if (server == std::string::npos) return path.size();
        auto share = path.find('/', server + 1);
        return share == std::string::npos ? path.size() : share + 1;
    }
#endif
    return path.starts_with('/') ? 1 : 0;
}
inline std::string Lexical(const std::string &input) {
    auto path = Slashes(input);
    size_t rootLength = RootLength(path);
    std::vector<std::string> parts;
    for (size_t begin = rootLength; begin < path.size();) {
        size_t end = path.find('/', begin);
        if (end == std::string::npos) end = path.size();
        auto part = path.substr(begin, end - begin);
        if (part == ".." && !parts.empty() && parts.back() != "..") parts.pop_back();
        else if (part != "." && !part.empty() && (part != ".." || !rootLength)) parts.push_back(part);
        begin = end + 1;
    }
    std::string out = path.substr(0, rootLength);
    for (const auto &part : parts) { if (!out.empty() && out.back() != '/') out += '/'; out += part; }
    return out;
}
inline std::string Join(const std::string &a, const std::string &b) {
    if (a.empty()) return Slashes(b);
    if (b.empty()) return Slashes(a);
    auto result = Slashes(a), tail = Slashes(b);
    if (result.back() != '/') result += '/';
    size_t begin = tail.find_first_not_of('/');
    if (begin != std::string::npos) result += tail.substr(begin);
    return result;
}
}
