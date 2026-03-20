#include "AnsiSegments.h"

namespace BML {
    namespace UI {
        ParsedAnsiSegments::ParsedAnsiSegments(const char *source)
            : m_Text(source ? source : "") {
        }

        void ParsedAnsiSegments::SetText(const char *text) {
            m_Text.SetText(text ? text : "");
        }

        ParsedAnsiSegments *CreateParsedAnsiSegments(const char *text) {
            if (!text) {
                return nullptr;
            }

            return new ParsedAnsiSegments(text);
        }

        void DestroyParsedAnsiSegments(ParsedAnsiSegments *segments) {
            delete segments;
        }
    } // namespace UI
} // namespace BML
