#ifndef BML_SCRIPTCOMMANDSERVICE_H
#define BML_SCRIPTCOMMANDSERVICE_H

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "BML/ICommand.h"
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

class ScriptCommandCompletion {
public:
    explicit ScriptCommandCompletion(std::vector<std::string> *items = nullptr) : m_Items(items) {}
    void Add(const std::string &value) const;
    int GetCount() const;
    std::string At(int index) const;

private:
    std::vector<std::string> *m_Items = nullptr;
};

class ScriptCommandRef {
public:
    ScriptCommandRef(std::weak_ptr<ScriptCommandServiceState> state, std::string key, unsigned int generation);

    void AddRef();
    void Release();

    bool IsValid() const;
    std::string GetName() const;
    std::string GetAlias() const;
    bool IsCheat() const;
    bool IsEnabled() const;
    bool SetEnabled(bool enabled);
    bool Unregister();

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

    void Bind(ModContext *context, ScriptMod *owner, ScriptModContextView *contextView);
    ScriptCommandRef *Register(asIScriptObject *command);
    ScriptCommandRef *Register(const ScriptCommandDefinition &definition,
                               asIScriptFunction *execute,
                               asIScriptFunction *complete);
    bool Unregister(const std::string &name);
    void Release(ScriptDiagnostic *diagnostic = nullptr);
    size_t GetActiveCount() const;

private:
    std::shared_ptr<ScriptCommandServiceState> m_State;
};

} // namespace BML

#endif
