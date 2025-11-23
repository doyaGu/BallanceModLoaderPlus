#include "CommandContext.h"

#include <utf8.h>

#include "Logger.h"
#include "StringUtils.h"

#define MAX_CMD_NAME_LENGTH 256

using namespace BML;

CommandContext::CommandContext() = default;

CommandContext::~CommandContext() = default;

bool CommandContext::RegisterCommand(ICommand *cmd) {
    if (!cmd)
        return false;

    auto name = std::move(cmd->GetName());
    if (!ValidateCommandName(name.c_str())) {
        Logger::GetDefault()->Error("Command name %s is invalid.", name);
        return false;
    }

    auto [it, inserted] = m_CommandMap.insert({name, cmd});
    if (!inserted) {
        Logger::GetDefault()->Error("Command %s has already been registered.", name);
        return false;
    }
    m_Commands.push_back(cmd);

    const auto alias = cmd->GetAlias();
    if (!alias.empty()) {
        it = m_CommandMap.find(alias);
        if (it == m_CommandMap.end()) {
            m_CommandMap[alias] = cmd;
        } else {
            Logger::GetDefault()->Warn("Command Alias Conflict: %s is redefined.", alias.c_str());
        }
    }
    return true;
}

bool CommandContext::UnregisterCommand(const char *name) {
    if (!name || name[0] == '\0')
        return false;

    const auto it = m_CommandMap.find(name);
    if (it == m_CommandMap.end()) {
        return false;
    }

    auto *cmd = it->second;

    m_CommandMap.erase(it);
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
        char *message = AllocPrintf(format, args);
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
    if (size > MAX_CMD_NAME_LENGTH) {
        return false;
    }

    if (!isalpha(name[0])) {
        return false;
    }

    bool valid = true;
    for (size_t i = 1; i < size; ++i)
        if (!isalnum(name[i]))
            valid = false;

    return valid;
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
        if (utf8codepointsize(*rp) == 1 && std::isspace(*rp) || *rp == '\0') {
            size_t len = rp - lp;
            if (len != 0) {
                char bk = *rp;
                *rp = '\0';
                args.emplace_back(lp);
                *rp = bk;
            }

            if (*rp != '\0') {
                while (utf8codepointsize(*rp) == 1 && std::isspace(*rp))
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