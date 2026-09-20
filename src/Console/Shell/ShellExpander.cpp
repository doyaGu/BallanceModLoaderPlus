#include "Console/Shell/ShellExpander.h"

#include "Console/Shell/ShellTypes.h"

namespace BML::Shell {
    namespace {
        bool ExpandPart(const WordPart &part, const ExpandContext &context, std::string &out, std::string &error) {
            switch (part.kind) {
                case WordPart::Kind::Literal:
                case WordPart::Kind::SingleQuoted:
                case WordPart::Kind::AnsiC:
                    out += part.text;
                    return true;
                case WordPart::Kind::DoubleQuoted:
                    for (const WordPart &child : part.children) {
                        if (!ExpandPart(child, context, out, error))
                            return false;
                    }
                    return true;
                case WordPart::Kind::Variable: {
                    if (part.text == "?") {
                        out += std::to_string(context.lastStatus);
                        return true;
                    }
                    std::string value;
                    if (context.variables && context.variables->LookupVariable(part.text, value))
                        out += value;
                    return true;
                }
                case WordPart::Kind::Substitution: {
                    if (!context.runSubstitution) {
                        error = "command substitution is not available here";
                        return false;
                    }
                    std::string captured;
                    if (!context.runSubstitution(part.text, captured, error))
                        return false;
                    out += captured;
                    return true;
                }
            }
            return true;
        }
    }

    bool ExpandWord(const Word &word, const ExpandContext &context, std::string &out, std::string &error) {
        out.clear();
        for (const WordPart &part : word.parts) {
            if (!ExpandPart(part, context, out, error))
                return false;
            if (out.size() > Limits::MaxWordBytes) {
                error = "expanded word is too long";
                return false;
            }
        }
        return true;
    }

    bool ExpandWords(const std::vector<Word> &words, const ExpandContext &context,
                     std::vector<std::string> &out, std::string &error) {
        out.clear();
        out.reserve(words.size());
        for (const Word &word : words) {
            std::string value;
            if (!ExpandWord(word, context, value, error))
                return false;
            out.push_back(std::move(value));
        }
        return true;
    }
}
