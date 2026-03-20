#ifndef BML_ANSI_SEGMENTS_H
#define BML_ANSI_SEGMENTS_H

#include "AnsiText.h"

namespace BML {
    namespace UI {
        class ParsedAnsiSegments {
        public:
            explicit ParsedAnsiSegments(const char *source);

            void SetText(const char *text);
            const AnsiText::AnsiString &GetText() const { return m_Text; }

        private:
            AnsiText::AnsiString m_Text;
        };

        ParsedAnsiSegments *CreateParsedAnsiSegments(const char *text);
        void DestroyParsedAnsiSegments(ParsedAnsiSegments *segments);
    } // namespace UI
} // namespace BML

#endif // BML_ANSI_SEGMENTS_H
