#include "CommandContext.h"

#include <algorithm>
#include <cctype>
#include <cstring>

#include <utf8.h>

#include "HashUtils.h"
#include "Logger.h"

#define MAX_CMD_NAME_LENGTH 256

using namespace BML;

namespace {
    enum class CommandKeyEncoding {
        kAscii,
        kUtf8,
        kInvalidUtf8,
    };

    bool IsValidFirstAsciiCommandChar(unsigned char ch) {
        return std::isalpha(ch) || ch == '_';
    }

    bool IsValidAsciiCommandChar(unsigned char ch) {
        return std::isalnum(ch) || ch == '_' || ch == '-' || ch == '.';
    }

    CommandKeyEncoding ClassifyCommandKey(const char *name) {
        if (!name || name[0] == '\0') {
            return CommandKeyEncoding::kAscii;
        }

        const unsigned char *cursor = reinterpret_cast<const unsigned char *>(name);
        while (*cursor != '\0') {
            if (*cursor >= 0x80) {
                return utf8valid(reinterpret_cast<const utf8_int8_t *>(name)) == nullptr
                    ? CommandKeyEncoding::kUtf8
                    : CommandKeyEncoding::kInvalidUtf8;
            }
            ++cursor;
        }

        return CommandKeyEncoding::kAscii;
    }

    template <typename Visitor>
    bool VisitNormalizedCommandKeyBytes(const char *name, Visitor &&visitor) {
        if (!name) {
            return false;
        }

        if (ClassifyCommandKey(name) != CommandKeyEncoding::kUtf8) {
            const unsigned char *cursor = reinterpret_cast<const unsigned char *>(name);
            while (*cursor != '\0') {
                if (!visitor(static_cast<unsigned char>(std::tolower(*cursor)))) {
                    return false;
                }
                ++cursor;
            }

            return true;
        }

        const utf8_int8_t *cursor = reinterpret_cast<const utf8_int8_t *>(name);
        while (*cursor != '\0') {
            utf8_int32_t codepoint = 0;
            cursor = utf8codepoint(cursor, &codepoint);

            const utf8_int32_t folded = utf8lwrcodepoint(codepoint);
            const size_t bytes = utf8codepointsize(folded);
            char encoded[4] = {};
            utf8catcodepoint(reinterpret_cast<utf8_int8_t *>(encoded), folded, bytes);
            for (size_t index = 0; index < bytes; ++index) {
                if (!visitor(static_cast<unsigned char>(encoded[index]))) {
                    return false;
                }
            }
        }

        return true;
    }

    size_t HashCommandKey(const char *name) {
        size_t hash = utils::Fnv1aSizeInit();
        VisitNormalizedCommandKeyBytes(name, [&hash](unsigned char byte) {
            hash = utils::Fnv1aSizeAppendByte(hash, byte);
            return true;
        });
        return hash;
    }

    bool EqualsCommandKey(std::string_view normalized, const char *name) {
        size_t index = 0;
        const bool matched = VisitNormalizedCommandKeyBytes(name, [&normalized, &index](unsigned char byte) {
            if (index >= normalized.size()) {
                return false;
            }

            if (static_cast<unsigned char>(normalized[index]) != byte) {
                return false;
            }

            ++index;
            return true;
        });
        return matched && index == normalized.size();
    }

    size_t MeasureNormalizedCommandKeyBytes(const char *name) {
        size_t size = 0;
        VisitNormalizedCommandKeyBytes(name, [&size](unsigned char) {
            ++size;
            return true;
        });
        return size;
    }

    void WriteNormalizedCommandKey(std::string &normalized, const char *name) {
        size_t index = 0;
        VisitNormalizedCommandKeyBytes(name, [&normalized, &index](unsigned char byte) {
            if (index < normalized.size()) {
                normalized[index++] = static_cast<char>(byte);
                return true;
            }

            return false;
        });
    }
}

CommandContext::CommandContext() = default;

CommandContext::~CommandContext() = default;

size_t CommandContext::CommandKeyHash::operator()(const std::string &key) const noexcept {
    return utils::Fnv1aSize(key);
}

size_t CommandContext::CommandKeyHash::operator()(const char *key) const noexcept {
    return HashCommandKey(key);
}

bool CommandContext::CommandKeyEqual::operator()(const std::string &lhs, const std::string &rhs) const noexcept {
    return lhs == rhs;
}

bool CommandContext::CommandKeyEqual::operator()(const std::string &lhs, const char *rhs) const noexcept {
    return EqualsCommandKey(lhs, rhs);
}

bool CommandContext::CommandKeyEqual::operator()(const char *lhs, const std::string &rhs) const noexcept {
    return EqualsCommandKey(rhs, lhs);
}

bool CommandContext::RegisterCommand(ICommand *cmd) {
    if (!cmd)
        return false;

    const auto name = cmd->GetName();
    if (!ValidateCommandName(name.c_str())) {
        Logger::GetDefault()->Error("Command name %s is invalid.", name.c_str());
        return false;
    }

    const std::string nameKey = NormalizeCommandKey(name.c_str());
    auto [it, inserted] = m_CommandMap.emplace(nameKey, cmd);
    if (!inserted) {
        Logger::GetDefault()->Error("Command %s has already been registered.", name.c_str());
        return false;
    }
    m_Commands.push_back(cmd);

    const auto alias = cmd->GetAlias();
    if (!alias.empty()) {
        if (!ValidateCommandName(alias.c_str())) {
            Logger::GetDefault()->Warn("Command alias %s is invalid and will be ignored.", alias.c_str());
        } else {
            auto aliasIt = m_CommandMap.find(alias.c_str());
            if (aliasIt == m_CommandMap.end()) {
                if (!CommandKeyEqual{}(nameKey, alias.c_str())) {
                    m_CommandMap.emplace(NormalizeCommandKey(alias.c_str()), cmd);
                }
            } else {
                Logger::GetDefault()->Warn("Command Alias Conflict: %s is redefined.", alias.c_str());
            }
        }
    }

    return true;
}

std::string CommandContext::NormalizeCommandKey(const char *name) {
    if (!name || name[0] == '\0')
        return {};

    const CommandKeyEncoding encoding = ClassifyCommandKey(name);
    if (encoding != CommandKeyEncoding::kUtf8) {
        std::string normalized(name);
        std::transform(normalized.begin(), normalized.end(), normalized.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        return normalized;
    }

    size_t normalizedBytes = 0;
    normalizedBytes = MeasureNormalizedCommandKeyBytes(name);

    std::string normalized(normalizedBytes, '\0');
    WriteNormalizedCommandKey(normalized, name);

    return normalized;
}

bool CommandContext::UnregisterCommand(const char *name) {
    if (!name || name[0] == '\0')
        return false;

    const auto it = m_CommandMap.find(name);
    if (it == m_CommandMap.end())
        return false;

    auto *cmd = it->second;
    for (auto eraseIt = m_CommandMap.begin(); eraseIt != m_CommandMap.end();) {
        if (eraseIt->second == cmd) {
            eraseIt = m_CommandMap.erase(eraseIt);
        } else {
            ++eraseIt;
        }
    }

    m_Commands.erase(std::remove(m_Commands.begin(), m_Commands.end(), cmd), m_Commands.end());
    return true;
}

size_t CommandContext::GetCommandCount() const {
    return m_Commands.size();
}

ICommand *CommandContext::GetCommandByIndex(size_t index) const {
    if (index >= m_Commands.size())
        return nullptr;
    return m_Commands[index];
}

ICommand *CommandContext::GetCommandByName(const char *name) const {
    if (!name || name[0] == '\0')
        return nullptr;

    const auto it = m_CommandMap.find(name);
    if (it == m_CommandMap.end())
        return nullptr;

    return it->second;
}

void CommandContext::SortCommands() {
    std::sort(m_Commands.begin(), m_Commands.end(),
          [](ICommand *a, ICommand *b) { return a->GetName() < b->GetName(); });
}

void CommandContext::ClearCommands() {
    m_CommandMap.clear();
    m_Commands.clear();
}

const char *CommandContext::GetVariable(const char *key) const {
    if (!key || key[0] == '\0')
        return nullptr;

    auto it = m_Variables.find(key);
    if (it == m_Variables.end())
        return nullptr;
    return it->second.c_str();
}

bool CommandContext::AddVariable(const char *key, const char *value) {
    if (!key || key[0] == '\0')
        return false;

    if (!value)
        value = "";

    bool inserted;
    VariableMap::iterator it;
    std::tie(it, inserted) = m_Variables.insert({key, value});
    return inserted;
}

bool CommandContext::RemoveVariable(const char *key) {
    if (!key || key[0] == '\0')
        return false;

    auto it = m_Variables.find(key);
    if (it == m_Variables.end())
        return false;

    m_Variables.erase(it);
    return true;
}

bool CommandContext::SetOutputCallback(CommandOutputCallback callback, void *userdata) {
    if (!callback || m_OutputCallback)
        return false;

    m_OutputCallback = callback;
    m_OutputCallbackData = userdata;
    return true;
}

void CommandContext::ClearOutputCallback() {
    m_OutputCallback = nullptr;
    m_OutputCallbackData = nullptr;
}

void CommandContext::Output(const char *message) {
    if (m_OutputCallback) {
        m_OutputCallback(message, m_OutputCallbackData);
    }
}

void CommandContext::OutputV(const char *format, va_list args) {
    if (m_OutputCallback) {
        char *message = AllocPrintfV(format, args);
        m_OutputCallback(message, m_OutputCallbackData);
        delete[] message;
    }
}

char *CommandContext::AllocPrintfV(const char *format, va_list args) {
    va_list args2;
    va_copy(args2, args);
    int len = vsnprintf(nullptr, 0, format, args2);
    va_end(args2);

    auto *string = new char[len + 2];
    vsnprintf(string, len + 1, format, args);
    return string;
}

char *CommandContext::AllocPrintf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    char *string = AllocPrintfV(format, args);
    va_end(args);
    return string;
}

bool CommandContext::ValidateCommandName(const char *name) {
    if (!name || name[0] == '\0')
        return false;

    size_t size = strnlen(name, MAX_CMD_NAME_LENGTH + 1);
    if (size > MAX_CMD_NAME_LENGTH)
        return false;

    if (utf8valid(reinterpret_cast<const utf8_int8_t *>(name)) != nullptr)
        return false;

    const auto *cursor = reinterpret_cast<const utf8_int8_t *>(name);
    bool first = true;
    while (*cursor != '\0') {
        utf8_int32_t codepoint = 0;
        const utf8_int8_t *next = utf8codepoint(cursor, &codepoint);
        if (!next)
            return false;

        if (codepoint <= 0x20 || codepoint == 0x7F)
            return false;

        if (codepoint < 0x80) {
            const unsigned char ch = static_cast<unsigned char>(codepoint);
            if (first) {
                if (!IsValidFirstAsciiCommandChar(ch))
                    return false;
            } else if (!IsValidAsciiCommandChar(ch)) {
                return false;
            }
        }

        cursor = next;
        first = false;
    }

    return true;
}

std::vector<std::string> CommandContext::ParseCommandLine(const char *cmd) {
    if (!cmd || cmd[0] == '\0')
        return {};

    std::vector<std::string> args;
    size_t size = utf8size(cmd);
    char *buf = new char[size + 1];
    utf8ncpy(buf, cmd, size);

    char *lp = &buf[0];
    char *rp = lp;
    char *end = lp + size;
    utf8_int32_t cp, temp;
    utf8codepoint(rp, &cp);
    while (rp != end) {
        if ((utf8codepointsize(*rp) == 1 && std::isspace(static_cast<unsigned char>(*rp))) || *rp == '\0') {
            size_t len = rp - lp;
            if (len != 0) {
                char bk = *rp;
                *rp = '\0';
                args.emplace_back(lp);
                *rp = bk;
            }

            if (*rp != '\0') {
                while (utf8codepointsize(*rp) == 1 && std::isspace(static_cast<unsigned char>(*rp)))
                    ++rp;
                --rp;
            }

            lp = utf8codepoint(rp, &temp);
        }

        rp = utf8codepoint(rp, &cp);
    }

    delete[] buf;
    return args;
}
