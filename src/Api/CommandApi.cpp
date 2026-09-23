#include "Api/CommandApi.h"

#include <cstring>
#include <exception>
#include <string_view>
#include <utility>
#include <vector>

#include <intrin.h>

#include "BML/IBML.h"
#include "BML/ICommand.h"
#include "Console/CommandContext.h"
#include "Console/Shell/ShellIo.h"
#include "Console/Shell/ShellTypes.h"
#include "Loader/ModContext.h"
#include "Logging/Logger.h"
#include "StringUtils.h"

namespace BML::Api {
namespace {

template <typename Body>
int ServeOnMainThread(Body &&body) {
    ModContextLease context;
    if (!context || !context->AreModsLoaded())
        return BML_ERROR_FAIL;
    if (!context->IsMainThread())
        return BML_ERROR_WRONG_THREAD;

    try {
        return body(*context);
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
}

class CallbackCommand final : public ICommand {
public:
    explicit CallbackCommand(const BML_CommandDefinition &definition)
        : m_Name(definition.Name),
          m_Alias(definition.Alias ? definition.Alias : ""),
          m_Description(definition.Description ? definition.Description : ""),
          m_Cheat((definition.Flags & BML_COMMAND_CHEAT) != 0),
          m_UserData(definition.UserData),
          m_Execute(definition.Execute),
          m_Complete(definition.Complete),
          m_Release(definition.Release) {}

    ~CallbackCommand() {
        if (!m_OwnsUserData || !m_Release)
            return;
        try {
            m_Release(m_UserData);
        } catch (...) {
        }
    }

    std::string GetName() override { return m_Name; }
    std::string GetAlias() override { return m_Alias; }
    std::string GetDescription() override { return m_Description; }
    bool IsCheat() override { return m_Cheat; }

    const std::string &Name() const noexcept { return m_Name; }
    bool IsActive() const noexcept { return m_ActiveCalls != 0; }
    bool CanDeferRemoval() const noexcept {
        return !m_UserData || m_Release;
    }
    void AdoptUserData() noexcept { m_OwnsUserData = true; }

    void Execute(IBML *bml, const std::vector<std::string> &args) override;
    const std::vector<std::string> GetTabCompletion(
        IBML *bml, const std::vector<std::string> &args) override;

private:
    class ActiveCall {
    public:
        explicit ActiveCall(CallbackCommand &command) noexcept
            : m_Command(command) {
            ++m_Command.m_ActiveCalls;
        }

        ~ActiveCall() {
            --m_Command.m_ActiveCalls;
        }

        ActiveCall(const ActiveCall &) = delete;
        ActiveCall &operator=(const ActiveCall &) = delete;

    private:
        CallbackCommand &m_Command;
    };

    std::string m_Name;
    std::string m_Alias;
    std::string m_Description;
    bool m_Cheat = false;
    void *m_UserData = nullptr;
    BML_CommandExecute m_Execute = nullptr;
    BML_CommandComplete m_Complete = nullptr;
    BML_CommandRelease m_Release = nullptr;
    unsigned int m_ActiveCalls = 0;
    bool m_OwnsUserData = false;
};

struct OutputContext {
    IBML *Bml = nullptr;
};

struct CompletionContext {
    std::vector<std::string> *Items = nullptr;
    int Status = BML_OK;
};

int BML_CDECL WriteOutput(
    void *context, const char *text, std::size_t length) {
    if (!context || (!text && length != 0))
        return BML_ERROR_INVALID_PARAMETER;
    const std::string_view value(text ? text : "", length);
    if ((length != 0 && std::memchr(text, '\0', length)) ||
        !utils::IsValidUtf8(value))
        return BML_ERROR_INVALID_PARAMETER;

    try {
        OutputContext *output = static_cast<OutputContext *>(context);
        if (!output->Bml)
            return BML_ERROR_UNAVAILABLE;
        const std::string terminated(value);
        output->Bml->SendIngameMessage(terminated.c_str());
        return BML_OK;
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_COMMAND_EXECUTION;
    }
}

int BML_CDECL AddCompletion(
    void *context, const char *text, std::size_t length) {
    if (!context)
        return BML_ERROR_INVALID_PARAMETER;

    CompletionContext *completion = static_cast<CompletionContext *>(context);
    if (completion->Status != BML_OK)
        return completion->Status;
    if (!completion->Items)
        return completion->Status = BML_ERROR_UNAVAILABLE;
    if (!text || length == 0 || length > BML_COMMAND_MAX_COMPLETION_BYTES)
        return completion->Status = BML_ERROR_INVALID_PARAMETER;

    try {
        const std::string_view value(text, length);
        if (std::memchr(text, '\0', length) || !utils::IsValidUtf8(value))
            return completion->Status = BML_ERROR_INVALID_PARAMETER;
        if (completion->Items->size() >= BML_COMMAND_MAX_COMPLETIONS) {
            completion->Status = BML_ERROR_WOULD_BLOCK;
            return completion->Status;
        }
        completion->Items->emplace_back(value);
        return BML_OK;
    } catch (const std::bad_alloc &) {
        completion->Status = BML_ERROR_OUT_OF_MEMORY;
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        completion->Status = BML_ERROR_COMMAND_EXECUTION;
        return BML_ERROR_COMMAND_EXECUTION;
    }
}

std::vector<const char *> BuildArguments(const std::vector<std::string> &args) {
    std::vector<const char *> values;
    if (args.size() <= 1)
        return values;

    values.reserve(args.size() - 1);
    for (std::size_t index = 1; index < args.size(); ++index)
        values.push_back(args[index].c_str());
    return values;
}

int BML_CDECL RegisterCommand(
    const char *ownerId, const BML_CommandDefinition *definition,
    BML_CommandHandle *outHandle) {
    const void *const callerAddress = _ReturnAddress();
    if (outHandle)
        *outHandle = BML_COMMAND_INVALID_HANDLE;
    if (!definition || !outHandle ||
        definition->StructSize < BML_COMMAND_DEFINITION_1_0_SIZE)
        return BML_ERROR_INVALID_PARAMETER;

    return ServeOnMainThread([=](ModContext &context) {
        const std::string owner =
            context.GetNativeModOwnerId(callerAddress, ownerId);
        if (owner.empty() || (ownerId && owner != ownerId))
            return BML_ERROR_ACCESS_DENIED;
        if (!definition->Execute ||
            !context.NativeModOwnsAddress(
                owner, reinterpret_cast<const void *>(definition->Execute)) ||
            (definition->Complete &&
             !context.NativeModOwnsAddress(
                 owner, reinterpret_cast<const void *>(definition->Complete))) ||
            (definition->Release &&
             !context.NativeModOwnsAddress(
                 owner, reinterpret_cast<const void *>(definition->Release))))
            return BML_ERROR_ACCESS_DENIED;
        return context.GetCommandApi().Register(owner, *definition, *outHandle);
    });
}

int BML_CDECL UnregisterCommand(
    const char *ownerId, BML_CommandHandle handle) {
    const void *const callerAddress = _ReturnAddress();
    if (handle == BML_COMMAND_INVALID_HANDLE)
        return BML_ERROR_INVALID_PARAMETER;

    return ServeOnMainThread([=](ModContext &context) {
        const std::string owner =
            context.GetNativeModOwnerId(callerAddress, ownerId);
        if (owner.empty() || (ownerId && owner != ownerId))
            return BML_ERROR_ACCESS_DENIED;
        return context.GetCommandApi().Unregister(owner, handle);
    });
}

int BML_CDECL SetCommandEnabled(
    const char *ownerId, BML_CommandHandle handle, int enabled) {
    const void *const callerAddress = _ReturnAddress();
    if (handle == BML_COMMAND_INVALID_HANDLE)
        return BML_ERROR_INVALID_PARAMETER;

    return ServeOnMainThread([=](ModContext &context) {
        const std::string owner =
            context.GetNativeModOwnerId(callerAddress, ownerId);
        if (owner.empty() || (ownerId && owner != ownerId))
            return BML_ERROR_ACCESS_DENIED;
        return context.GetCommandApi().SetEnabled(owner, handle, enabled != 0);
    });
}

int VisitCommandInfo(const CommandContext::CommandInfo &source,
                     BML_CommandVisitor visitor, void *userData) {
    const BML_CommandInfo info = {
        sizeof(BML_CommandInfo),
        source.Handle,
        source.Name.c_str(),
        source.Alias.c_str(),
        source.Description.c_str(),
        source.Usage.c_str(),
        source.Category.c_str(),
        static_cast<BML_CommandFlags>(
            (source.Cheat ? BML_COMMAND_CHEAT : 0) |
            (source.Hidden ? BML_COMMAND_HIDDEN : 0) |
            (source.Enabled ? 0 : BML_COMMAND_DISABLED)),
    };
    return visitor(userData, &info);
}

int BML_CDECL VisitCommands(BML_CommandVisitor visitor, void *userData) {
    if (!visitor)
        return BML_ERROR_INVALID_PARAMETER;

    return ServeOnMainThread([=](ModContext &context) {
        const auto commands = context.GetCommandSnapshot();
        for (const auto &command : commands) {
            const int status = VisitCommandInfo(command, visitor, userData);
            if (status != BML_OK)
                return status;
        }
        return BML_OK;
    });
}

int BML_CDECL FindCommand(
    const char *name, BML_CommandVisitor visitor, void *userData) {
    if (!visitor || !CommandContext::IsValidCommandAlias(name))
        return BML_ERROR_INVALID_PARAMETER;

    return ServeOnMainThread([=](ModContext &context) {
        CommandContext::CommandInfo command;
        if (!context.FindCommandInfo(name, command))
            return BML_ERROR_NOT_FOUND;
        return VisitCommandInfo(command, visitor, userData);
    });
}

int BML_CDECL ExecuteCommandLine(const char *line, int *outStatus) {
    if (outStatus)
        *outStatus = Shell::Status::Failure;
    if (!line || !outStatus)
        return BML_ERROR_INVALID_PARAMETER;

    const std::size_t length =
        strnlen(line, Shell::Limits::MaxLineBytes + 1);
    if (length > Shell::Limits::MaxLineBytes ||
        !utils::IsValidUtf8(std::string_view(line, length)))
        return BML_ERROR_INVALID_PARAMETER;

    return ServeOnMainThread([=](ModContext &context) {
        *outStatus = context.ExecuteCommandLine(line);
        return BML_OK;
    });
}

const BML_CommandInterface CommandInterfaceTable = {
    BML_IFACE_HEADER(BML_CommandInterface, BML_COMMAND_INTERFACE_ID,
                     BML_COMMAND_INTERFACE_MAJOR, BML_COMMAND_INTERFACE_MINOR),
    &RegisterCommand,
    &UnregisterCommand,
    &SetCommandEnabled,
    &VisitCommands,
    &FindCommand,
    &ExecuteCommandLine,
};

BML_CommandInvocation BuildInvocation(
    const std::string &name, const std::vector<std::string> &args,
    const std::vector<const char *> &arguments, OutputContext &output) {
    const std::string *input = Shell::GetInput();
    return BML_CommandInvocation{
        sizeof(BML_CommandInvocation),
        name.c_str(),
        args.empty() ? name.c_str() : args.front().c_str(),
        arguments.size(),
        arguments.empty() ? nullptr : arguments.data(),
        input ? input->data() : nullptr,
        input ? input->size() : 0,
        &output,
        &WriteOutput,
    };
}

BML_CommandCompletionRequest BuildCompletionRequest(
    const std::string &name, const std::vector<std::string> &args,
    const std::vector<const char *> &arguments) {
    return BML_CommandCompletionRequest{
        sizeof(BML_CommandCompletionRequest),
        name.c_str(),
        args.empty() ? name.c_str() : args.front().c_str(),
        arguments.size(),
        arguments.empty() ? nullptr : arguments.data(),
        arguments.empty() ? 0 : arguments.size() - 1,
        arguments.empty() ? "" : arguments.back(),
    };
}

void CallbackCommand::Execute(IBML *bml, const std::vector<std::string> &args) {
    ActiveCall active(*this);
    int status = Shell::Status::Failure;
    try {
        const std::vector<const char *> arguments = BuildArguments(args);
        OutputContext output = {bml};
        const BML_CommandInvocation invocation =
            BuildInvocation(m_Name, args, arguments, output);
        status = m_Execute ? m_Execute(m_UserData, &invocation)
                           : Shell::Status::Failure;
    } catch (const std::exception &error) {
        Logger::GetDefault()->Error("Exception in command '%s': %s",
                                    m_Name.c_str(), error.what());
        status = Shell::Status::Failure;
    } catch (...) {
        Logger::GetDefault()->Error("Unknown exception in command '%s'", m_Name.c_str());
        status = Shell::Status::Failure;
    }
    Shell::SetStatus(status);
}

const std::vector<std::string> CallbackCommand::GetTabCompletion(
    IBML *, const std::vector<std::string> &args) {
    if (!m_Complete)
        return {};

    ActiveCall active(*this);
    try {
        const std::vector<const char *> arguments = BuildArguments(args);
        const BML_CommandCompletionRequest request =
            BuildCompletionRequest(m_Name, args, arguments);
        std::vector<std::string> items;
        CompletionContext completionContext = {&items, BML_OK};
        const BML_CommandCompletion completion = {
            sizeof(BML_CommandCompletion),
            &completionContext,
            &AddCompletion,
        };
        const int status = m_Complete(m_UserData, &request, &completion);
        return status == BML_OK && completionContext.Status == BML_OK
            ? std::move(items)
            : std::vector<std::string>();
    } catch (const std::exception &error) {
        Logger::GetDefault()->Error("Exception completing command '%s': %s",
                                    m_Name.c_str(), error.what());
    } catch (...) {
        Logger::GetDefault()->Error("Unknown exception completing command '%s'",
                                    m_Name.c_str());
    }
    return {};
}

} // namespace

struct CommandApi::Entry {
    Entry(std::string owner, const BML_CommandDefinition &definition)
        : Owner(std::move(owner)), Command(definition) {}

    std::string Owner;
    CallbackCommand Command;
    bool PendingRemoval = false;
};

CommandApi::CommandApi(ModContext &context) noexcept : m_Context(context) {}

CommandApi::~CommandApi() = default;

BML_CommandHandle CommandApi::NextHandle() noexcept {
    if (m_NextHandle == BML_COMMAND_INVALID_HANDLE)
        return BML_COMMAND_INVALID_HANDLE;
    return m_NextHandle++;
}

int CommandApi::Register(const std::string &owner,
                         const BML_CommandDefinition &definition,
                         BML_CommandHandle &handle) {
    handle = BML_COMMAND_INVALID_HANDLE;
    if (owner.empty() || !definition.Name || !definition.Execute ||
        !CommandContext::IsValidCommandName(definition.Name) ||
        (definition.Alias && definition.Alias[0] != '\0' &&
         !CommandContext::IsValidCommandAlias(definition.Alias)) ||
        (definition.Description && !utils::IsValidUtf8(definition.Description)) ||
        (definition.Usage && !utils::IsValidUtf8(definition.Usage)) ||
        (definition.Category && !utils::IsValidUtf8(definition.Category)) ||
        (definition.Flags & ~(BML_COMMAND_CHEAT | BML_COMMAND_HIDDEN |
                              BML_COMMAND_DISABLED)) != 0)
        return BML_ERROR_INVALID_PARAMETER;

    if (!m_Context.IsMainThread())
        return BML_ERROR_WRONG_THREAD;
    if (m_CleaningOwner)
        return BML_ERROR_BUSY;

    try {
        const BML_CommandHandle candidate = NextHandle();
        if (candidate == BML_COMMAND_INVALID_HANDLE)
            return BML_ERROR_BUSY;

        auto entry = std::make_shared<Entry>(owner, definition);
        Entry *registrar = entry.get();
        const auto inserted = m_Entries.emplace(candidate, entry);
        if (!inserted.second)
            return BML_ERROR_BUSY;

        CommandContext::CommandInfo info;
        info.Name = definition.Name;
        info.Alias = definition.Alias ? definition.Alias : "";
        info.Description = definition.Description ? definition.Description : "";
        info.Usage = definition.Usage ? definition.Usage : "";
        info.Category = definition.Category ? definition.Category : "";
        info.Cheat = (definition.Flags & BML_COMMAND_CHEAT) != 0;
        info.Hidden = (definition.Flags & BML_COMMAND_HIDDEN) != 0;
        info.Enabled = (definition.Flags & BML_COMMAND_DISABLED) == 0;
        info.Handle = candidate;
        int result = BML_ERROR_FAIL;
        try {
            result = m_Context.RegisterCallbackCommand(
                registrar, &registrar->Command, std::move(info), entry);
        } catch (...) {
            m_Entries.erase(candidate);
            throw;
        }
        if (result != BML_OK) {
            m_Entries.erase(candidate);
            return result;
        }

        registrar->Command.AdoptUserData();
        handle = candidate;
        return BML_OK;
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
}

int CommandApi::Unregister(const std::string &owner,
                           BML_CommandHandle handle) {
    if (owner.empty() || handle == BML_COMMAND_INVALID_HANDLE)
        return BML_ERROR_INVALID_PARAMETER;
    if (!m_Context.IsMainThread())
        return BML_ERROR_WRONG_THREAD;
    const bool defer = m_Context.IsCommandInvocationActiveOnCurrentThread();
    const int status = UnregisterLocked(owner, handle, defer);
    if (status == BML_OK && !defer)
        FlushPending();
    return status;
}

int CommandApi::UnregisterLocked(
    const std::string &owner, BML_CommandHandle handle, bool defer) {
    const auto position = m_Entries.find(handle);
    if (position == m_Entries.end() || position->second->PendingRemoval)
        return BML_ERROR_INVALID_HANDLE;
    Entry *entry = position->second.get();
    if (entry->Owner != owner)
        return BML_ERROR_ACCESS_DENIED;
    if (defer && !entry->Command.IsActive() &&
        !entry->Command.CanDeferRemoval())
        return BML_ERROR_BUSY;

    const CommandContext::UnregisterResult result =
        m_Context.UnregisterCallbackCommand(entry, entry->Command.Name().c_str());
    if (result != CommandContext::UnregisterResult::Success)
        return BML_ERROR_FAIL;
    entry->PendingRemoval = true;
    return BML_OK;
}

int CommandApi::SetEnabled(const std::string &owner,
                           BML_CommandHandle handle, bool enabled) {
    if (owner.empty() || handle == BML_COMMAND_INVALID_HANDLE)
        return BML_ERROR_INVALID_PARAMETER;
    if (!m_Context.IsMainThread())
        return BML_ERROR_WRONG_THREAD;

    const auto position = m_Entries.find(handle);
    if (position == m_Entries.end() || position->second->PendingRemoval)
        return BML_ERROR_INVALID_HANDLE;
    Entry *entry = position->second.get();
    if (entry->Owner != owner)
        return BML_ERROR_ACCESS_DENIED;
    return m_Context.SetCommandEnabled(&entry->Command, enabled)
        ? BML_OK
        : BML_ERROR_INVALID_HANDLE;
}

bool CommandApi::CleanupOwner(const std::string &owner) noexcept {
    if (owner.empty())
        return true;
    if (m_CleaningOwner)
        return false;

    m_CleaningOwner = true;
    try {
        for (;;) {
            std::shared_ptr<Entry> removed;
            auto position = m_Entries.begin();
            while (position != m_Entries.end() &&
                   position->second->Owner != owner)
                ++position;
            if (position == m_Entries.end())
                break;
            Entry *entry = position->second.get();
            if (!m_Context.RetireCallbackCommands(entry)) {
                m_CleaningOwner = false;
                return false;
            }
            removed = std::move(position->second);
            m_Entries.erase(position);
            removed.reset();
        }
        m_CleaningOwner = false;
        return true;
    } catch (...) {
        m_CleaningOwner = false;
        if (Logger::GetDefault())
            Logger::GetDefault()->Error(
                "Failed to clean command registrations for Mod %s.", owner.c_str());
        return false;
    }
}

void CommandApi::FlushPending() noexcept {
    if (!m_Context.IsMainThread())
        return;

    try {
        for (;;) {
            std::shared_ptr<Entry> removed;
            auto position = m_Entries.begin();
            while (position != m_Entries.end() &&
                   (!position->second->PendingRemoval ||
                    position->second->Command.IsActive()))
                ++position;
            if (position == m_Entries.end())
                break;
            removed = std::move(position->second);
            m_Entries.erase(position);
            removed.reset();
        }
    } catch (...) {
    }
}

const BML_CommandInterface &CommandInterface() noexcept {
    return CommandInterfaceTable;
}

} // namespace BML::Api
