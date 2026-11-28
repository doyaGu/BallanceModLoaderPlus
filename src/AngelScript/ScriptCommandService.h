#ifndef BML_SCRIPTCOMMANDSERVICE_H
#define BML_SCRIPTCOMMANDSERVICE_H

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Console/CommandRecord.h"
#include "ScriptRuntimeScope.h"
#include "ScriptDiagnostic.h"

class ModContext;
class asIScriptFunction;
class asIScriptObject;

namespace BML {

class ScriptMod;
class ScriptModContextView;
class ScriptCommandServiceState;

struct ScriptCommandDefinition {
    std::string Name;
    std::string Alias;
    std::string Description;
    std::string Usage;
    std::string Category;
    bool Cheat = false;
    bool Hidden = false;
    bool Enabled = true;
};

struct ScriptCommandFrame {
    bool Active = true;
    std::string Name, InvokedAs, Input, Prefix;
    bool HasInput = false;
    std::vector<std::string> Arguments;
    unsigned ActiveArgument = 0;
    ModContext *Context = nullptr;
    std::vector<std::string> *Items = nullptr;
    int Status = BML_OK;
};

class ScriptCommandInvocation {
public:
    std::shared_ptr<ScriptCommandFrame> Frame;
    std::string GetName() const;
    std::string GetInvokedAs() const;
    unsigned GetArgumentCount() const;
    int ReadArgument(unsigned index, std::string &out) const;
    bool HasInput() const;
    std::string GetInput() const;
    int Write(const std::string &text) const;
};

class ScriptCommandCompletionRequest : public ScriptCommandInvocation {
public:
    unsigned GetActiveArgument() const;
    std::string GetPrefix() const;
};

class ScriptCommandCompletion {
public:
    ScriptCommandCompletion() = default;
    explicit ScriptCommandCompletion(std::vector<std::string> *items);
    std::shared_ptr<ScriptCommandFrame> Frame;
    int Add(const std::string &value) const;
    int ReadCount(unsigned &out) const;
    int ReadCandidate(unsigned index, std::string &out) const;
    int GetCount() const;
    std::string At(int index) const;
};

class ScriptCommandRef {
public:
    ScriptCommandRef(std::weak_ptr<ScriptCommandServiceState> state, std::string key,
                     unsigned int generation);

    void AddRef();
    void Release();

    bool IsValid() const;
    std::string GetName() const;
    std::string GetAlias() const;
    bool IsCheat() const;
    bool IsEnabled() const;
    int Read(CommandInfo &out) const;
    int SetEnabled(bool enabled);
    int Unregister();
    std::shared_ptr<ScriptControlTicket> Ticket;

private:
    int m_RefCount = 1;
    std::weak_ptr<ScriptCommandServiceState> m_State;
    std::string m_Key;
    unsigned int m_Generation = 0;
};

class ScriptCommandService {
public:
    ScriptCommandService();
    ~ScriptCommandService();

    bool Bind(ModContext *context, ScriptMod *owner, ScriptModContextView *contextView);
    int Register(const ScriptCommandDefinition &definition, asIScriptFunction *execute,
                 asIScriptFunction *complete, ScriptCommandRef *&out);
    bool Unregister(const std::string &name);
    void Release(ScriptDiagnostic *diagnostic = nullptr);
    size_t GetActiveCount() const;

#ifdef BML_TEST
    ScriptCommandRef *
    AddTestCommandForRelease(const std::string &name, const std::string &alias = std::string(),
                             std::function<void(const std::vector<std::string> &)> execute = {},
                             std::function<bool()> registerHost = {});
    bool InvokeTestCommandForRelease(const std::string &name,
                                     const std::vector<std::string> &args = {});
#endif

private:
    std::shared_ptr<ScriptCommandServiceState> m_State;
};

} // namespace BML

#endif
