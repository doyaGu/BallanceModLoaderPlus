#ifndef BML_API_BEHAVIOR_TEXT_H
#define BML_API_BEHAVIOR_TEXT_H

#include <cstddef>
#include <string>
#include <string_view>

// Virtools keeps names and string parameters in the active code page, and the
// Behavior internals keep that text as the engine stores it. bml.behavior
// carries UTF-8, so the Codec converts text at the boundary.
namespace BML::Api::Behavior {

// CP_ACP. Tests pass an explicit code page.
inline constexpr unsigned ActiveCodePage = 0;

bool IsUtf8(const char *data, std::size_t size) noexcept;

// Text the code page cannot represent keeps its UTF-8 bytes.
std::string NativeText(std::string_view utf8,
                       unsigned codePage = ActiveCodePage);

// Returns the text itself when it is already UTF-8, as names set by UTF-8
// callers are, and otherwise the converted text held in storage.
std::string_view Utf8Text(std::string_view native, std::string &storage,
                          unsigned codePage = ActiveCodePage);

} // namespace BML::Api::Behavior

#endif // BML_API_BEHAVIOR_TEXT_H
