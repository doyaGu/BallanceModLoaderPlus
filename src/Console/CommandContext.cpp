#include "Console/CommandContext.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <new>

#include <utf8.h>

#include "BML/Command.h"
#include "HashUtils.h"
#include "Logging/Logger.h"
#include "StringUtils.h"

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
            const std::size_t bytes = utf8codepointsize(folded);
            char encoded[4] = {};
            utf8catcodepoint(reinterpret_cast<utf8_int8_t *>(encoded), folded, bytes);
            for (std::size_t index = 0; index < bytes; ++index) {
                if (!visitor(static_cast<unsigned char>(encoded[index]))) {
                    return false;
                }
            }
        }

        return true;
    }

    std::size_t HashCommandKey(const char *name) {
        std::size_t hash = utils::Fnv1aSizeInit();
        VisitNormalizedCommandKeyBytes(name, [&hash](unsigned char byte) {
            hash = utils::Fnv1aSizeAppendByte(hash, byte);
            return true;
        });
        return hash;
    }

    bool EqualsCommandKey(std::string_view normalized, const char *name) {
        std::size_t index = 0;
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

    std::size_t MeasureNormalizedCommandKeyBytes(const char *name) {
        std::size_t size = 0;
        VisitNormalizedCommandKeyBytes(name, [&size](unsigned char) {
            ++size;
            return true;
        });
        return size;
    }

    void WriteNormalizedCommandKey(std::string &normalized, const char *name) {
        std::size_t index = 0;
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

std::size_t CommandContext::CommandKeyHash::operator()(const std::string &key) const noexcept {
    return utils::Fnv1aSize(key);
}

std::size_t CommandContext::CommandKeyHash::operator()(const char *key) const noexcept {
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

    CommandInfo info;
    info.Name = cmd->GetName();
    info.Alias = cmd->GetAlias();
    info.Description = cmd->GetDescription();
    info.Cheat = cmd->IsCheat();
    return RegisterCommand(registrar, cmd, std::move(info));
}

bool CommandContext::RegisterCommand(const void *registrar, ICommand *cmd, CommandInfo info) {
    return RegisterCommand(registrar, cmd, std::move(info), {});
}

bool CommandContext::RegisterCommand(const void *registrar, ICommand *cmd, CommandInfo info,
                                     std::shared_ptr<void> lifetime) {
    if (!registrar || !cmd)
        return false;

    const std::string name = info.Name;
    if (!IsValidCommandName(name)) {
        Logger::GetDefault()->Error("Command name %s is invalid.", name.c_str());
        return false;
    }

    std::string nameKey = NormalizeCommandName(name.c_str());
    if (m_CommandMap.find(nameKey) != m_CommandMap.end()) {
        Logger::GetDefault()->Error("Command %s has already been registered.", name.c_str());
        return false;
    }

    std::string aliasKey;
    const std::string alias = info.Alias;
    if (!alias.empty()) {
        if (!IsValidCommandAlias(alias)) {
            Logger::GetDefault()->Error("Command alias %s is invalid.", alias.c_str());
            return false;
        }

        aliasKey = NormalizeCommandName(alias.c_str());
        if (aliasKey == nameKey || m_CommandMap.find(aliasKey) != m_CommandMap.end()) {
            Logger::GetDefault()->Warn("Command Alias Conflict: %s is redefined.", alias.c_str());
            aliasKey.clear();
            info.Alias.clear();
        }
    }

    Entry entry;
    entry.Command = cmd;
    entry.Registrar = registrar;
    entry.Info = std::move(info);
    entry.NameKey = nameKey;
    entry.AliasKey = aliasKey;
    entry.Lifetime = std::move(lifetime);

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
            [](const Entry &item, const std::string &value) { return item.Info.Name < value; });
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

    std::size_t normalizedBytes = 0;
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

std::size_t CommandContext::GetCommandCount() const {
    return m_Commands.size();
}

ICommand *CommandContext::GetCommandByIndex(std::size_t index) const {
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

bool CommandContext::AcquireCommand(const char *name, CommandCall &call) const {
    call = {};
    ICommand *command = GetCommandByName(name);
    if (!command)
        return false;

    const auto entry = std::find_if(m_Commands.begin(), m_Commands.end(),
                                    [command](const Entry &item) {
                                        return item.Command == command;
                                    });
    if (entry == m_Commands.end()) {
        return false;
    }

    call.Command = command;
    call.Info = entry->Info;
    call.Lifetime = entry->Lifetime;
    return true;
}

std::vector<CommandContext::CommandInfo> CommandContext::GetCommandSnapshot() const {
    std::vector<CommandInfo> snapshot;
    snapshot.reserve(m_Commands.size());
    for (const Entry &entry : m_Commands)
        snapshot.push_back(entry.Info);
    return snapshot;
}

bool CommandContext::GetCommandInfoByIndex(
    std::size_t index, CommandInfo &info) const {
    if (index >= m_Commands.size())
        return false;
    info = m_Commands[index].Info;
    return true;
}

bool CommandContext::GetCommandInfoByName(const char *name, CommandInfo &info) const {
    ICommand *command = GetCommandByName(name);
    if (!command)
        return false;

    const auto entry = std::find_if(m_Commands.begin(), m_Commands.end(),
                                    [command](const Entry &item) {
                                        return item.Command == command;
                                    });
    if (entry == m_Commands.end())
        return false;
    info = entry->Info;
    return true;
}

bool CommandContext::SetCommandEnabled(ICommand *command, bool enabled) {
    if (!command)
        return false;
    const auto entry = std::find_if(m_Commands.begin(), m_Commands.end(),
                                    [command](const Entry &item) {
                                        return item.Command == command;
                                    });
    if (entry == m_Commands.end())
        return false;
    entry->Info.Enabled = enabled;
    return true;
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

    const std::size_t bufferSize = message.size() + 1;
    auto *string = new char[bufferSize];
    std::memcpy(string, message.c_str(), bufferSize);
    return string;
}

bool CommandContext::IsValidCommandName(const char *name) {
    if (!name || name[0] == '\0')
        return false;

    const std::size_t size = strnlen(name, BML_COMMAND_MAX_NAME_BYTES + 1);
    if (size > BML_COMMAND_MAX_NAME_BYTES)
        return false;

    return IsValidCommandName(std::string_view(name, size));
}

bool CommandContext::IsValidCommandName(std::string_view name) {
    if (name.empty() || name.size() > BML_COMMAND_MAX_NAME_BYTES ||
        !utils::IsValidUtf8(name))
        return false;

    const auto *cursor = reinterpret_cast<const utf8_int8_t *>(name.data());
    const auto *end = cursor + name.size();
    bool first = true;
    while (cursor < end) {
        utf8_int32_t codepoint = 0;
        const utf8_int8_t *next = utf8codepoint(cursor, &codepoint);

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

    const std::size_t size = strnlen(alias, BML_COMMAND_MAX_NAME_BYTES + 1);
    if (size > BML_COMMAND_MAX_NAME_BYTES)
        return false;

    return IsValidCommandAlias(std::string_view(alias, size));
}

bool CommandContext::IsValidCommandAlias(std::string_view alias) {
    if (alias.empty() || alias.size() > BML_COMMAND_MAX_NAME_BYTES ||
        !utils::IsValidUtf8(alias))
        return false;

    const auto *cursor = reinterpret_cast<const utf8_int8_t *>(alias.data());
    const auto *end = cursor + alias.size();
    while (cursor < end) {
        utf8_int32_t codepoint = 0;
        const utf8_int8_t *next = utf8codepoint(cursor, &codepoint);

        if (codepoint <= 0x20 || codepoint == 0x7F)
            return false;

        cursor = next;
    }

    return true;
}
