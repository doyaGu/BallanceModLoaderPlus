#include "CommandContext.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <new>

#include <utf8.h>

#include "HashUtils.h"
#include "Logger.h"
#include "StringUtils.h"

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

bool CommandContext::RegisterCommand(const void *registrar, ICommand *cmd) {
    if (!registrar || !cmd)
        return false;

    const auto name = cmd->GetName();
    if (!IsValidCommandName(name.c_str())) {
        Logger::GetDefault()->Error("Command name %s is invalid.", name.c_str());
        return false;
    }

    std::string nameKey = NormalizeCommandName(name.c_str());
    if (m_CommandMap.find(nameKey) != m_CommandMap.end()) {
        Logger::GetDefault()->Error("Command %s has already been registered.", name.c_str());
        return false;
    }

    std::string aliasKey;
    const auto alias = cmd->GetAlias();
    if (!alias.empty()) {
        if (!IsValidCommandAlias(alias.c_str())) {
            Logger::GetDefault()->Error("Command alias %s is invalid.", alias.c_str());
            return false;
        }

        aliasKey = NormalizeCommandName(alias.c_str());
        if (aliasKey == nameKey || m_CommandMap.find(aliasKey) != m_CommandMap.end()) {
            Logger::GetDefault()->Warn("Command Alias Conflict: %s is redefined.", alias.c_str());
            aliasKey.clear();
        }
    }

    Entry entry;
    entry.Command = cmd;
    entry.Registrar = registrar;
    entry.Name = name;
    entry.NameKey = nameKey;
    entry.AliasKey = aliasKey;

    try {
        const auto [nameIt, nameInserted] = m_CommandMap.emplace(nameKey, cmd);
        if (!nameInserted)
            return false;

        if (!aliasKey.empty()) {
            const bool aliasInserted = m_CommandMap.emplace(aliasKey, cmd).second;
            if (!aliasInserted) {
                m_CommandMap.erase(nameIt);
                return false;
            }
        }

        const auto position = std::lower_bound(
            m_Commands.begin(), m_Commands.end(), name,
            [](const Entry &item, const std::string &value) { return item.Name < value; });
        m_Commands.insert(position, std::move(entry));
    } catch (const std::bad_alloc &) {
        m_CommandMap.erase(nameKey);
        if (!aliasKey.empty())
            m_CommandMap.erase(aliasKey);
        return false;
    }

    return true;
}

std::string CommandContext::NormalizeCommandName(const char *name) {
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

CommandContext::UnregisterResult CommandContext::UnregisterCommand(const void *registrar,
                                                                   const char *name) {
    if (!name || name[0] == '\0')
        return UnregisterResult::InvalidName;

    const auto it = m_CommandMap.find(name);
    if (it == m_CommandMap.end())
        return UnregisterResult::NotFound;

    auto *cmd = it->second;
    const auto entry = std::find_if(m_Commands.begin(), m_Commands.end(),
                                    [cmd](const Entry &item) { return item.Command == cmd; });
    if (entry == m_Commands.end())
        return UnregisterResult::InternalError;
    if (!registrar || entry->Registrar != registrar)
        return UnregisterResult::AccessDenied;

    m_CommandMap.erase(entry->NameKey);
    if (!entry->AliasKey.empty())
        m_CommandMap.erase(entry->AliasKey);
    m_Commands.erase(entry);
    return UnregisterResult::Success;
}

void CommandContext::UnregisterCommands(const void *registrar) {
    if (!registrar)
        return;

    for (const Entry &entry : m_Commands) {
        if (entry.Registrar != registrar)
            continue;

        m_CommandMap.erase(entry.NameKey);
        if (!entry.AliasKey.empty())
            m_CommandMap.erase(entry.AliasKey);
    }

    m_Commands.erase(std::remove_if(m_Commands.begin(), m_Commands.end(),
                                    [registrar](const Entry &entry) {
                                        return entry.Registrar == registrar;
                                    }),
                     m_Commands.end());
}

size_t CommandContext::GetCommandCount() const {
    return m_Commands.size();
}

ICommand *CommandContext::GetCommandByIndex(size_t index) const {
    if (index >= m_Commands.size())
        return nullptr;
    return m_Commands[index].Command;
}

ICommand *CommandContext::GetCommandByName(const char *name) const {
    if (!name || name[0] == '\0')
        return nullptr;

    const auto it = m_CommandMap.find(name);
    if (it == m_CommandMap.end())
        return nullptr;

    return it->second;
}

bool CommandContext::SetCheatEnabled(bool enabled) noexcept {
    const bool changed = m_CheatEnabled != enabled;
    m_CheatEnabled = enabled;
    return changed;
}

void CommandContext::ClearCommands() {
    m_CommandMap.clear();
    m_Commands.clear();
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
    std::string message;
    utils::FormatStringV(format, args, message);

    const size_t bufferSize = message.size() + 1;
    auto *string = new char[bufferSize];
    std::memcpy(string, message.c_str(), bufferSize);
    return string;
}

bool CommandContext::IsValidCommandName(const char *name) {
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

bool CommandContext::IsValidCommandAlias(const char *alias) {
    if (!alias || alias[0] == '\0')
        return false;

    size_t size = strnlen(alias, MAX_CMD_NAME_LENGTH + 1);
    if (size > MAX_CMD_NAME_LENGTH)
        return false;

    if (utf8valid(reinterpret_cast<const utf8_int8_t *>(alias)) != nullptr)
        return false;

    const auto *cursor = reinterpret_cast<const utf8_int8_t *>(alias);
    while (*cursor != '\0') {
        utf8_int32_t codepoint = 0;
        const utf8_int8_t *next = utf8codepoint(cursor, &codepoint);
        if (!next)
            return false;

        if (codepoint <= 0x20 || codepoint == 0x7F)
            return false;

        cursor = next;
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
