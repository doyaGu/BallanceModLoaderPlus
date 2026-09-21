// Type-safe C++ authoring for Command.h. The C interface remains the complete
// ABI seam; this layer owns C++ strings, views, exception containment, and
// registration lifetime without introducing another command vtable.
#ifndef BML_COMMAND_HPP
#define BML_COMMAND_HPP

#include "BML/Command.h"
#include "BML/Interface.hpp"

#include <atomic>
#include <cstddef>
#include <functional>
#include <new>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace BML::Command {

struct Definition {
    std::string Name;
    std::string Alias;
    std::string Description;
    std::string Usage;
    std::string Category;
    bool Cheat = false;
    bool Hidden = false;
    bool Enabled = true;
};

struct Info;
using Visitor = std::function<int(const Info &)>;

[[nodiscard]] inline int Visit(const Visitor &visitor) noexcept;
[[nodiscard]] inline int Find(std::string_view name, Info &out) noexcept;
[[nodiscard]] inline int ExecuteLine(
    std::string_view line, int &commandStatus) noexcept;

struct Info {
    Info() = default;

    BML_CommandHandle Handle = BML_COMMAND_INVALID_HANDLE;
    std::string Name;
    std::string Alias;
    std::string Description;
    std::string Usage;
    std::string Category;
    bool Cheat = false;
    bool Hidden = false;
    bool Enabled = true;

private:
    explicit Info(const BML_CommandInfo &value)
        : Handle(value.Handle),
          Name(value.Name ? value.Name : ""),
          Alias(value.Alias ? value.Alias : ""),
          Description(value.Description ? value.Description : ""),
          Usage(value.Usage ? value.Usage : ""),
          Category(value.Category ? value.Category : ""),
          Cheat((value.Flags & BML_COMMAND_CHEAT) != 0),
          Hidden((value.Flags & BML_COMMAND_HIDDEN) != 0),
          Enabled((value.Flags & BML_COMMAND_DISABLED) == 0) {}

    friend int Visit(const Visitor &visitor) noexcept;
    friend int Find(std::string_view name, Info &out) noexcept;
};

class Invocation {
public:
    [[nodiscard]] std::string_view Name() const noexcept {
        return m_Value && m_Value->Name ? m_Value->Name : "";
    }

    [[nodiscard]] std::string_view InvokedAs() const noexcept {
        return m_Value && m_Value->InvokedAs ? m_Value->InvokedAs : "";
    }

    [[nodiscard]] std::span<const char *const> Arguments() const noexcept {
        if (!m_Value || !m_Value->Arguments)
            return {};
        return {m_Value->Arguments, m_Value->ArgumentCount};
    }

    [[nodiscard]] std::string_view Argument(std::size_t index) const noexcept {
        const auto arguments = Arguments();
        return index < arguments.size() && arguments[index] ? arguments[index] : "";
    }

    [[nodiscard]] std::string_view Input() const noexcept {
        if (!m_Value || !m_Value->Input)
            return {};
        return {m_Value->Input, m_Value->InputLength};
    }

    [[nodiscard]] bool HasInput() const noexcept {
        return m_Value && m_Value->Input;
    }

    [[nodiscard]] int Write(std::string_view text) const noexcept {
        if (!m_Value || !m_Value->Write)
            return BML_ERROR_UNAVAILABLE;
        return m_Value->Write(m_Value->OutputContext, text.data(), text.size());
    }

private:
    friend class Registration;
    explicit Invocation(const BML_CommandInvocation *value) noexcept : m_Value(value) {}
    const BML_CommandInvocation *m_Value = nullptr;
};

class Completion {
public:
    [[nodiscard]] int Add(std::string_view value) const noexcept {
        if (!m_Value || !m_Value->Add)
            return BML_ERROR_UNAVAILABLE;
        return m_Value->Add(m_Value->Context, value.data(), value.size());
    }

private:
    friend class Registration;
    explicit Completion(const BML_CommandCompletion *value) noexcept : m_Value(value) {}
    const BML_CommandCompletion *m_Value = nullptr;
};

class CompletionRequest {
public:
    [[nodiscard]] std::string_view Name() const noexcept {
        return m_Value && m_Value->Name ? m_Value->Name : "";
    }

    [[nodiscard]] std::string_view InvokedAs() const noexcept {
        return m_Value && m_Value->InvokedAs ? m_Value->InvokedAs : "";
    }

    [[nodiscard]] std::span<const char *const> Arguments() const noexcept {
        if (!m_Value || !m_Value->Arguments)
            return {};
        return {m_Value->Arguments, m_Value->ArgumentCount};
    }

    [[nodiscard]] std::string_view Argument(std::size_t index) const noexcept {
        const auto arguments = Arguments();
        return index < arguments.size() && arguments[index] ? arguments[index] : "";
    }

    [[nodiscard]] std::size_t ActiveArgument() const noexcept {
        return m_Value ? m_Value->ActiveArgument : 0;
    }

    [[nodiscard]] std::string_view Prefix() const noexcept {
        return m_Value && m_Value->Prefix ? m_Value->Prefix : "";
    }

private:
    friend class Registration;
    explicit CompletionRequest(
        const BML_CommandCompletionRequest *value) noexcept : m_Value(value) {}
    const BML_CommandCompletionRequest *m_Value = nullptr;
};

using ExecuteCallback = std::function<int(const Invocation &)>;
// Return BML_OK after adding every candidate, or a BML error to discard this
// completion result.
using CompleteCallback =
    std::function<int(const CompletionRequest &, const Completion &)>;

// Register, Unregister, SetEnabled, and destruction belong on the game thread.
// Keep a Registration alive while it is registered. Destruction requests
// removal. The loader owns callback state after successful registration and
// keeps it alive until every in-progress command dispatch has finished.
class Registration final {
private:
    BML_DECLARE_INTERFACE_TRAITS(
        InterfaceTraits, BML_CommandInterface, BML_COMMAND_INTERFACE_ID,
        BML_COMMAND_INTERFACE_MAJOR, ExecuteLine);

    struct CallbackState {
        CallbackState(ExecuteCallback execute, CompleteCallback complete) noexcept
            : Execute(std::move(execute)), Complete(std::move(complete)) {}

        void Retain() noexcept {
            References.fetch_add(1, std::memory_order_relaxed);
        }

        void Release() noexcept {
            if (References.fetch_sub(1, std::memory_order_acq_rel) == 1)
                delete this;
        }

        std::atomic<std::size_t> References{1};
        std::atomic_bool Abandoned{false};
        ExecuteCallback Execute;
        CompleteCallback Complete;
    };

    class CallbackReference {
    public:
        explicit CallbackReference(CallbackState *state) noexcept : m_State(state) {}
        ~CallbackReference() {
            m_State->Release();
        }

        CallbackReference(const CallbackReference &) = delete;
        CallbackReference &operator=(const CallbackReference &) = delete;

    private:
        CallbackState *m_State;
    };

public:
    Registration() noexcept = default;
    ~Registration() {
        Dispose();
    }

    Registration(const Registration &) = delete;
    Registration &operator=(const Registration &) = delete;

    Registration(Registration &&other) noexcept
        : m_Interface(std::move(other.m_Interface)),
          m_OwnerId(std::move(other.m_OwnerId)),
          m_Handle(std::exchange(
              other.m_Handle, BML_COMMAND_INVALID_HANDLE)) {}

    Registration &operator=(Registration &&other) noexcept {
        if (this != &other) {
            Dispose();
            m_Interface = std::move(other.m_Interface);
            m_OwnerId = std::move(other.m_OwnerId);
            m_Handle = std::exchange(
                other.m_Handle, BML_COMMAND_INVALID_HANDLE);
        }
        return *this;
    }

    [[nodiscard]] int Register(const Definition &definition,
                               ExecuteCallback execute,
                               CompleteCallback complete = {},
                               const char *ownerId = nullptr) noexcept {
        if (m_Handle != BML_COMMAND_INVALID_HANDLE)
            return BML_ERROR_ALREADY_EXISTS;
        if (!execute || definition.Name.empty() ||
            definition.Name.size() > BML_COMMAND_MAX_NAME_BYTES ||
            definition.Alias.size() > BML_COMMAND_MAX_NAME_BYTES ||
            ContainsNull(definition.Name) || ContainsNull(definition.Alias) ||
            ContainsNull(definition.Description) ||
            ContainsNull(definition.Usage) || ContainsNull(definition.Category))
            return BML_ERROR_INVALID_PARAMETER;

        int status = m_Interface.Open();
        if (status != BML_OK)
            return status;
        if (!BML_IFACE_HAS(m_Interface.Get(), BML_CommandInterface, Register)) {
            m_Interface.Reset();
            return BML_ERROR_NOT_FOUND;
        }

        CallbackState *state = nullptr;
        try {
            std::string owner = ownerId ? ownerId : "";
            state = new CallbackState(std::move(execute), std::move(complete));
            const BML_CommandDefinition wire = {
                sizeof(BML_CommandDefinition),
                definition.Name.c_str(),
                definition.Alias.c_str(),
                definition.Description.c_str(),
                definition.Usage.c_str(),
                definition.Category.c_str(),
                static_cast<BML_CommandFlags>(
                    (definition.Cheat ? BML_COMMAND_CHEAT : 0) |
                    (definition.Hidden ? BML_COMMAND_HIDDEN : 0) |
                    (definition.Enabled ? 0 : BML_COMMAND_DISABLED)),
                state,
                &Execute,
                state->Complete ? &CompleteCandidates : nullptr,
                &ReleaseState,
            };
            BML_CommandHandle handle = BML_COMMAND_INVALID_HANDLE;
            status = m_Interface->Register(ownerId, &wire, &handle);
            if (status == BML_OK && handle == BML_COMMAND_INVALID_HANDLE) {
                state->Abandoned.store(true, std::memory_order_release);
                state = nullptr;
                Reset();
                return BML_ERROR_MALFORMED_MESSAGE;
            }
            if (status == BML_OK) {
                state = nullptr;
                m_OwnerId.swap(owner);
                m_Handle = handle;
            } else {
                state->Release();
                state = nullptr;
                Reset();
            }
            return status;
        } catch (const std::bad_alloc &) {
            if (state)
                state->Release();
            Reset();
            return BML_ERROR_OUT_OF_MEMORY;
        } catch (...) {
            if (state)
                state->Release();
            Reset();
            return BML_ERROR_FAIL;
        }
    }

    [[nodiscard]] int Unregister() noexcept {
        if (m_Handle == BML_COMMAND_INVALID_HANDLE)
            return BML_OK;
        if (!m_Interface)
            return BML_ERROR_NOT_FOUND;
        if (!BML_IFACE_HAS(m_Interface.Get(), BML_CommandInterface, Unregister))
            return BML_ERROR_NOT_FOUND;

        const char *ownerId = m_OwnerId.empty() ? nullptr : m_OwnerId.c_str();
        const int status = m_Interface->Unregister(ownerId, m_Handle);
        if (status == BML_OK || status == BML_ERROR_NOT_FOUND ||
            status == BML_ERROR_INVALID_HANDLE)
            Reset();
        return status;
    }

    [[nodiscard]] bool IsRegistered() const noexcept {
        return m_Handle != BML_COMMAND_INVALID_HANDLE;
    }

    [[nodiscard]] BML_CommandHandle Handle() const noexcept { return m_Handle; }

    [[nodiscard]] int SetEnabled(bool enabled) noexcept {
        if (m_Handle == BML_COMMAND_INVALID_HANDLE || !m_Interface)
            return BML_ERROR_INVALID_HANDLE;
        if (!BML_IFACE_HAS(m_Interface.Get(), BML_CommandInterface, SetEnabled))
            return BML_ERROR_NOT_FOUND;
        const char *ownerId = m_OwnerId.empty() ? nullptr : m_OwnerId.c_str();
        return m_Interface->SetEnabled(ownerId, m_Handle, enabled ? 1 : 0);
    }

private:
    static int BML_CDECL Execute(
        void *userData, const BML_CommandInvocation *invocation) noexcept {
        if (!userData || !invocation ||
            invocation->StructSize < BML_COMMAND_INVOCATION_1_0_SIZE)
            return 1;
        CallbackState *state = static_cast<CallbackState *>(userData);
        state->Retain();
        CallbackReference reference(state);
        if (state->Abandoned.load(std::memory_order_acquire))
            return BML_COMMAND_STATUS_FAILURE;
        try {
            return state->Execute
                ? state->Execute(Invocation(invocation))
                : BML_COMMAND_STATUS_FAILURE;
        } catch (...) {
            return BML_COMMAND_STATUS_FAILURE;
        }
    }

    static int BML_CDECL CompleteCandidates(
        void *userData, const BML_CommandCompletionRequest *request,
        const BML_CommandCompletion *completion) noexcept {
        if (!userData || !request || !completion ||
            request->StructSize < BML_COMMAND_COMPLETION_REQUEST_1_0_SIZE ||
            completion->StructSize < BML_COMMAND_COMPLETION_1_0_SIZE)
            return BML_ERROR_INVALID_PARAMETER;
        CallbackState *state = static_cast<CallbackState *>(userData);
        state->Retain();
        CallbackReference reference(state);
        if (state->Abandoned.load(std::memory_order_acquire))
            return BML_ERROR_COMMAND_EXECUTION;
        try {
            return state->Complete
                ? state->Complete(CompletionRequest(request), Completion(completion))
                : BML_OK;
        } catch (const std::bad_alloc &) {
            return BML_ERROR_OUT_OF_MEMORY;
        } catch (...) {
            return BML_ERROR_COMMAND_EXECUTION;
        }
    }

    static void BML_CDECL ReleaseState(void *userData) noexcept {
        if (userData)
            static_cast<CallbackState *>(userData)->Release();
    }

    void Reset() noexcept {
        m_Interface.Reset();
        m_OwnerId.clear();
        m_Handle = BML_COMMAND_INVALID_HANDLE;
    }

    void Abandon() noexcept {
        if (m_Handle != BML_COMMAND_INVALID_HANDLE && m_Interface &&
            BML_IFACE_HAS(m_Interface.Get(), BML_CommandInterface, SetEnabled)) {
            const char *ownerId = m_OwnerId.empty() ? nullptr : m_OwnerId.c_str();
            (void) m_Interface->SetEnabled(ownerId, m_Handle, 0);
        }
        m_Interface.Reset();
        m_OwnerId.clear();
        m_Handle = BML_COMMAND_INVALID_HANDLE;
    }

    void Dispose() noexcept {
        if (Unregister() != BML_OK)
            Abandon();
    }

    static bool ContainsNull(std::string_view value) noexcept {
        return value.find('\0') != std::string_view::npos;
    }

    friend int Visit(const Visitor &visitor) noexcept;
    friend int Find(std::string_view name, Info &out) noexcept;
    friend int ExecuteLine(
        std::string_view line, int &commandStatus) noexcept;

    Interfaces::Reference<InterfaceTraits> m_Interface;
    std::string m_OwnerId;
    BML_CommandHandle m_Handle = BML_COMMAND_INVALID_HANDLE;
};

[[nodiscard]] inline int Visit(const Visitor &visitor) noexcept {
    if (!visitor)
        return BML_ERROR_INVALID_PARAMETER;
    Interfaces::Reference<Registration::InterfaceTraits> interfaceReference;
    int status = interfaceReference.Open();
    if (status != BML_OK)
        return status;
    if (!BML_IFACE_HAS(interfaceReference.Get(), BML_CommandInterface, Visit))
        return BML_ERROR_NOT_FOUND;

    struct State {
        const Visitor *Callback;
    } state = {&visitor};
    const auto visit = [](void *userData, const BML_CommandInfo *wire) noexcept {
        if (!userData || !wire ||
            wire->StructSize < BML_COMMAND_INFO_1_0_SIZE)
            return BML_ERROR_MALFORMED_MESSAGE;
        State *state = static_cast<State *>(userData);
        try {
            const Info info(*wire);
            return (*state->Callback)(info);
        } catch (const std::bad_alloc &) {
            return BML_ERROR_OUT_OF_MEMORY;
        } catch (...) {
            return BML_ERROR_FAIL;
        }
    };
    return interfaceReference->Visit(visit, &state);
}

[[nodiscard]] inline int Find(std::string_view name, Info &out) noexcept {
    if (name.empty() || name.size() > BML_COMMAND_MAX_NAME_BYTES ||
        name.find('\0') != std::string_view::npos)
        return BML_ERROR_INVALID_PARAMETER;

    Interfaces::Reference<Registration::InterfaceTraits> interfaceReference;
    int status = interfaceReference.Open();
    if (status != BML_OK)
        return status;
    if (!BML_IFACE_HAS(interfaceReference.Get(), BML_CommandInterface, Find))
        return BML_ERROR_NOT_FOUND;

    try {
        const std::string terminated(name);
        const auto store = [](void *userData,
                              const BML_CommandInfo *wire) noexcept {
            if (!userData || !wire ||
                wire->StructSize < BML_COMMAND_INFO_1_0_SIZE)
                return BML_ERROR_MALFORMED_MESSAGE;
            try {
                *static_cast<Info *>(userData) = Info(*wire);
                return BML_OK;
            } catch (const std::bad_alloc &) {
                return BML_ERROR_OUT_OF_MEMORY;
            } catch (...) {
                return BML_ERROR_FAIL;
            }
        };
        return interfaceReference->Find(terminated.c_str(), store, &out);
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
}

[[nodiscard]] inline int ExecuteLine(std::string_view line,
                                     int &commandStatus) noexcept {
    commandStatus = BML_COMMAND_STATUS_FAILURE;
    if (line.size() > BML_COMMAND_MAX_LINE_BYTES ||
        line.find('\0') != std::string_view::npos)
        return BML_ERROR_INVALID_PARAMETER;

    Interfaces::Reference<Registration::InterfaceTraits> interfaceReference;
    int status = interfaceReference.Open();
    if (status != BML_OK)
        return status;
    if (!BML_IFACE_HAS(interfaceReference.Get(), BML_CommandInterface, ExecuteLine))
        return BML_ERROR_NOT_FOUND;

    try {
        const std::string terminated(line);
        return interfaceReference->ExecuteLine(terminated.c_str(), &commandStatus);
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
}

} // namespace BML::Command

#endif // BML_COMMAND_HPP
