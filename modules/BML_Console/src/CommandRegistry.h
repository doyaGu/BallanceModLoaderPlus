#ifndef BML_CONSOLE_COMMAND_REGISTRY_H
#define BML_CONSOLE_COMMAND_REGISTRY_H

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "bml_console.h"
#include "bml_interface.h"
#include "imgui.h"
#include "StringUtils.h"

namespace BML::Console {

struct CommandRecord {
    std::string name;
    std::string alias;
    std::string description;
    uint32_t flags = BML_CONSOLE_COMMAND_FLAG_NONE;
    PFN_BML_ConsoleCommandExecute execute = nullptr;
    PFN_BML_ConsoleCommandComplete complete = nullptr;
    void *userData = nullptr;
    BML_InterfaceRegistration registration = nullptr;
};

struct ParsedToken {
    std::string value;
    size_t start = 0;
    size_t end = 0;
};

inline std::string ToLowerAscii(std::string_view value) {
    return utils::ToLower(std::string(value));
}

inline std::string TrimCopy(std::string_view text) {
    return utils::TrimStringCopy(std::string(text));
}

inline bool HasTrailingWhitespace(std::string_view text) {
    return !text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0;
}

inline bool ValidateCommandName(std::string_view name) {
    if (name.empty() || !std::isalpha(static_cast<unsigned char>(name.front()))) return false;
    for (size_t i = 1; i < name.size(); ++i) {
        if (!std::isalnum(static_cast<unsigned char>(name[i]))) return false;
    }
    return true;
}

inline std::vector<ParsedToken> ParseTokens(std::string_view text) {
    std::vector<ParsedToken> tokens;
    size_t index = 0;
    while (index < text.size()) {
        while (index < text.size() && std::isspace(static_cast<unsigned char>(text[index])) != 0) ++index;
        const size_t start = index;
        while (index < text.size() && std::isspace(static_cast<unsigned char>(text[index])) == 0) ++index;
        if (index > start) {
            tokens.push_back({std::string(text.substr(start, index - start)), start, index});
        }
    }
    return tokens;
}

inline std::string FormatCommandSummary(const CommandRecord &record) {
    std::string line = record.name;
    if (!record.alias.empty()) {
        line += " (alias: ";
        line += record.alias;
        line += ')';
    }
    if (!record.description.empty()) {
        line += " - ";
        line += record.description;
    }
    return line;
}

inline std::string LongestCommonPrefix(const std::vector<std::string> &values) {
    if (values.empty()) return {};
    std::string prefix = values.front();
    for (size_t i = 1; i < values.size() && !prefix.empty(); ++i) {
        const std::string &value = values[i];
        size_t length = 0;
        const size_t bound = (std::min)(prefix.size(), value.size());
        while (length < bound && std::tolower(static_cast<unsigned char>(prefix[length])) == std::tolower(static_cast<unsigned char>(value[length]))) ++length;
        prefix.resize(length);
    }
    return prefix;
}

inline void AddCompletionCandidate(std::vector<std::string> &candidates, std::string candidate) {
    if (candidate.empty()) return;
    const std::string normalized = ToLowerAscii(candidate);
    const bool exists = std::any_of(candidates.begin(), candidates.end(), [&](const std::string &existing) {
        return ToLowerAscii(existing) == normalized;
    });
    if (!exists) candidates.push_back(std::move(candidate));
}

inline void CompletionCollector(const char *candidateUtf8, void *userData) {
    if (!userData || !candidateUtf8 || candidateUtf8[0] == '\0') return;
    auto *candidates = static_cast<std::vector<std::string> *>(userData);
    AddCompletionCandidate(*candidates, candidateUtf8);
}

inline const char *ResolveArgsUtf8(const std::string &commandLine, const std::vector<ParsedToken> &tokens) {
    if (tokens.size() <= 1) return commandLine.c_str() + commandLine.size();
    return commandLine.c_str() + tokens[1].start;
}

inline void AddCompletionIfMatches(const char *candidate, std::string_view prefix,
    PFN_BML_ConsoleCompletionSink sink, void *sinkUserData) {
    if (!candidate || !sink) return;
    const std::string loweredCandidate = ToLowerAscii(candidate);
    const std::string loweredPrefix = ToLowerAscii(prefix);
    if (loweredPrefix.empty() || loweredCandidate.rfind(loweredPrefix, 0) == 0) {
        sink(candidate, sinkUserData);
    }
}

inline void ReplaceWordAtCursor(ImGuiInputTextCallbackData *data, const char *replacement) {
    if (!data || !replacement) return;

    const char *cursor = data->Buf + data->CursorPos;
    const char *wordStart = cursor;
    while (wordStart > data->Buf && std::isspace(static_cast<unsigned char>(wordStart[-1])) == 0) --wordStart;
    const int leftCount = static_cast<int>(cursor - wordStart);

    const char *textEnd = data->Buf + data->BufTextLen;
    const char *p = cursor;
    while (p < textEnd && std::isspace(static_cast<unsigned char>(*p)) == 0) ++p;
    const int rightCount = static_cast<int>(p - cursor);

    const int totalDelete = leftCount + rightCount;
    const int deletePos = static_cast<int>(wordStart - data->Buf);

    data->DeleteChars(deletePos, totalDelete);
    data->InsertChars(deletePos, replacement);
    const bool hasSpaceAfter = (p < textEnd) && std::isspace(static_cast<unsigned char>(*p)) != 0;
    if (!hasSpaceAfter) {
        const int insertedLen = static_cast<int>(std::strlen(replacement));
        data->InsertChars(deletePos + insertedLen, " ");
    }
}

class CommandRegistry {
public:
    BML_Result Register(const BML_ConsoleCommandDesc *desc, BML_InterfaceRegistration registration = nullptr) {
        if (!desc || desc->struct_size < sizeof(BML_ConsoleCommandDesc) || !desc->name_utf8 || !desc->execute)
            return BML_RESULT_INVALID_ARGUMENT;
        const std::string name = desc->name_utf8;
        const std::string alias = desc->alias_utf8 ? desc->alias_utf8 : "";
        if (!ValidateCommandName(name) || (!alias.empty() && !ValidateCommandName(alias)))
            return BML_RESULT_INVALID_ARGUMENT;
        if (Find(name) != nullptr || (!alias.empty() && Find(alias) != nullptr))
            return BML_RESULT_ALREADY_EXISTS;
        CommandRecord record;
        record.name = name;
        record.alias = alias;
        record.description = desc->description_utf8 ? desc->description_utf8 : "";
        record.flags = desc->flags;
        record.execute = desc->execute;
        record.complete = desc->complete;
        record.userData = desc->user_data;
        record.registration = registration;
        m_Commands.push_back(std::move(record));
        RebuildLookup();
        return BML_RESULT_OK;
    }

    BML_Result Unregister(const char *nameUtf8, BML_InterfaceRegistration *outRegistration = nullptr) {
        if (!nameUtf8 || nameUtf8[0] == '\0') return BML_RESULT_INVALID_ARGUMENT;
        CommandRecord *record = Find(nameUtf8);
        if (!record) return BML_RESULT_NOT_FOUND;
        const std::string canonicalName = record->name;
        if (outRegistration) {
            *outRegistration = record->registration;
        }
        m_Commands.erase(std::remove_if(m_Commands.begin(), m_Commands.end(), [&](const CommandRecord &entry) {
            return entry.name == canonicalName;
        }), m_Commands.end());
        RebuildLookup();
        return BML_RESULT_OK;
    }

    CommandRecord *Find(std::string_view name) {
        const auto it = m_Lookup.find(ToLowerAscii(name));
        if (it == m_Lookup.end() || it->second >= m_Commands.size()) return nullptr;
        return &m_Commands[it->second];
    }

    const CommandRecord *Find(std::string_view name) const {
        const auto it = m_Lookup.find(ToLowerAscii(name));
        if (it == m_Lookup.end() || it->second >= m_Commands.size()) return nullptr;
        return &m_Commands[it->second];
    }

    std::vector<const CommandRecord *> FindVisible() const {
        std::vector<const CommandRecord *> commands;
        commands.reserve(m_Commands.size());
        for (const CommandRecord &record : m_Commands) {
            if ((record.flags & BML_CONSOLE_COMMAND_FLAG_HIDDEN) == 0) commands.push_back(&record);
        }
        std::sort(commands.begin(), commands.end(), [](const CommandRecord *lhs, const CommandRecord *rhs) {
            return lhs->name < rhs->name;
        });
        return commands;
    }

    BML_Result Execute(const std::string &commandLine) {
        const std::vector<ParsedToken> tokens = ParseTokens(commandLine);
        if (tokens.empty()) return BML_RESULT_INVALID_ARGUMENT;
        CommandRecord *record = Find(tokens[0].value);
        if (!record) return BML_RESULT_NOT_FOUND;
        std::vector<const char *> argv;
        argv.reserve(tokens.size());
        for (const ParsedToken &token : tokens) argv.push_back(token.value.c_str());
        BML_ConsoleCommandInvocation invocation = BML_CONSOLE_COMMAND_INVOCATION_INIT;
        invocation.command_line_utf8 = commandLine.c_str();
        invocation.args_utf8 = ResolveArgsUtf8(commandLine, tokens);
        invocation.name_utf8 = tokens[0].value.c_str();
        invocation.argv_utf8 = argv.empty() ? nullptr : argv.data();
        invocation.argc = static_cast<uint32_t>(argv.size());
        return record->execute(&invocation, record->userData);
    }

    void Clear() {
        m_Commands.clear();
        m_Lookup.clear();
    }

    const std::vector<CommandRecord> &Commands() const { return m_Commands; }

private:
    void RebuildLookup() {
        m_Lookup.clear();
        for (size_t index = 0; index < m_Commands.size(); ++index) {
            const CommandRecord &record = m_Commands[index];
            m_Lookup[ToLowerAscii(record.name)] = index;
            if (!record.alias.empty()) m_Lookup[ToLowerAscii(record.alias)] = index;
        }
    }

    std::vector<CommandRecord> m_Commands;
    std::unordered_map<std::string, size_t> m_Lookup;
};

} // namespace BML::Console

#endif // BML_CONSOLE_COMMAND_REGISTRY_H
