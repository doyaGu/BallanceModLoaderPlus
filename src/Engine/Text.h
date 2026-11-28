#ifndef BML_ENGINE_TEXT_H
#define BML_ENGINE_TEXT_H

#include <string>
#include <string_view>

// SDL uses UTF-8 for CK strings. Retail Virtools uses the active code page.
// Convert at the CK boundary so the rest of BML stays in UTF-8.
namespace BML::CKText {

// CP_ACP. Tests pass an explicit code page.
inline constexpr unsigned ActiveCodePage = 0;

// Returns false when the engine cannot represent the UTF-8 input.
bool EncodeNativeText(std::string_view utf8, std::string &native,
                      unsigned codePage = ActiveCodePage);

// Decodes engine text according to its runtime, even if ACP bytes happen to
// form a valid UTF-8 sequence.
std::string_view Utf8Text(std::string_view native, std::string &storage,
                          unsigned codePage = ActiveCodePage);

} // namespace BML::CKText

#endif // BML_ENGINE_TEXT_H
