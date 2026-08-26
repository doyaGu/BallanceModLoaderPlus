#include "ScriptModDefinition.h"

#include <cctype>
#include <cstdlib>
#include <limits>

#include "Utils/StringUtils.h"

namespace BML {

static void SkipMetadataSpaces(const std::string &text, size_t &pos) {
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos])))
        ++pos;
}

static bool IsMetadataIdentStart(char ch) {
    return std::isalpha(static_cast<unsigned char>(ch)) || ch == '_';
}

static bool IsMetadataIdentChar(char ch) {
    return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '.';
}

static bool ParseMetadataIdentifier(const std::string &text, size_t &pos, std::string &out) {
    SkipMetadataSpaces(text, pos);
    if (pos >= text.size() || !IsMetadataIdentStart(text[pos]))
        return false;

    const size_t start = pos++;
    while (pos < text.size() && IsMetadataIdentChar(text[pos]))
        ++pos;
    out.assign(text, start, pos - start);
    return true;
}

static bool ParseMetadataQuotedString(const std::string &text, size_t &pos, std::string &out, std::string &diagnostic) {
    SkipMetadataSpaces(text, pos);
    if (pos >= text.size() || text[pos] != '"') {
        diagnostic = "metadata argument value must be quoted.";
        return false;
    }

    ++pos;
    out.clear();
    while (pos < text.size()) {
        const char ch = text[pos++];
        if (ch == '"')
            return true;
        if (ch != '\\') {
            out.push_back(ch);
            continue;
        }
        if (pos >= text.size()) {
            diagnostic = "metadata string escape is truncated.";
            return false;
        }
        const char escaped = text[pos++];
        switch (escaped) {
        case 'n':
            out.push_back('\n');
            break;
        case 'r':
            out.push_back('\r');
            break;
        case 't':
            out.push_back('\t');
            break;
        case '\\':
        case '"':
            out.push_back(escaped);
            break;
        default:
            out.push_back(escaped);
            break;
        }
    }

    diagnostic = "metadata string is missing closing quote.";
    return false;
}

static bool ParseVersionPart(const std::string &value, size_t &pos, int &part) {
    if (pos >= value.size() || !std::isdigit(static_cast<unsigned char>(value[pos])))
        return false;
    if (value[pos] == '0' && pos + 1 < value.size() &&
        std::isdigit(static_cast<unsigned char>(value[pos + 1]))) {
        return false;
    }

    int parsed = 0;
    do {
        const int digit = value[pos] - '0';
        if (parsed > ((std::numeric_limits<int>::max)() - digit) / 10)
            return false;
        parsed = parsed * 10 + digit;
        ++pos;
    } while (pos < value.size() &&
             std::isdigit(static_cast<unsigned char>(value[pos])));

    part = parsed;
    return true;
}

bool ParseScriptModVersion(const std::string &value, BMLVersion &version) {
    size_t pos = 0;
    int parts[3] = {};
    for (int index = 0; index < 3; ++index) {
        if (index != 0 && (pos >= value.size() || value[pos++] != '.'))
            return false;
        if (!ParseVersionPart(value, pos, parts[index]))
            return false;
    }
    if (pos != value.size())
        return false;

    version = BMLVersion(parts[0], parts[1], parts[2]);
    return true;
}

ScriptModReloadPolicy ParseScriptModReloadPolicy(const std::string &value) {
    const std::string normalized = utils::ToLower(utils::TrimStringCopy(value));
    if (normalized == "auto")
        return ScriptModReloadPolicy::Auto;
    if (normalized == "manual" || normalized == "off" || normalized == "false")
        return ScriptModReloadPolicy::Manual;
    return ScriptModReloadPolicy::Default;
}

const char *ToString(ScriptModReloadPolicy policy) {
    switch (policy) {
    case ScriptModReloadPolicy::Auto:
        return "auto";
    case ScriptModReloadPolicy::Manual:
        return "manual";
    case ScriptModReloadPolicy::Default:
    default:
        return "default";
    }
}

bool ParseScriptMetadataTag(const std::string &metadata,
                            ScriptMetadataTag &tag,
                            std::string &diagnostic) {
    tag = ScriptMetadataTag();
    diagnostic.clear();

    size_t pos = 0;
    if (!ParseMetadataIdentifier(metadata, pos, tag.Name)) {
        diagnostic = "metadata tag name is missing.";
        return false;
    }

    while (true) {
        SkipMetadataSpaces(metadata, pos);
        if (pos >= metadata.size())
            return true;

        std::string key;
        if (!ParseMetadataIdentifier(metadata, pos, key)) {
            diagnostic = "metadata argument name is invalid.";
            return false;
        }
        SkipMetadataSpaces(metadata, pos);
        if (pos >= metadata.size() || metadata[pos] != '=') {
            diagnostic = "metadata argument '" + key + "' is missing '='.";
            return false;
        }
        ++pos;

        std::string value;
        if (!ParseMetadataQuotedString(metadata, pos, value, diagnostic))
            return false;
        if (!tag.Args.emplace(key, value).second) {
            diagnostic = "metadata argument '" + key + "' is duplicated.";
            return false;
        }
    }
}

} // namespace BML
