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

// Returns false when the engine cannot represent the UTF-8 input.
bool EncodeNativeText(std::string_view utf8, std::string &native,
                      unsigned codePage = ActiveCodePage);

// Decodes engine text according to its runtime, even if ACP bytes happen to
// form a valid UTF-8 sequence.
std::string_view Utf8Text(std::string_view native, std::string &storage,
                          unsigned codePage = ActiveCodePage);

} // namespace BML::Api::Behavior

#endif // BML_API_BEHAVIOR_TEXT_H
