#ifndef BML_SCRIPTHOOKBLOCKSERVICE_H
#define BML_SCRIPTHOOKBLOCKSERVICE_H

#include <cstddef>
#include <memory>
#include <string>

#include "CKAll.h"
#include "ScriptDiagnostic.h"

class ModContext;
class asIScriptFunction;

namespace BML {

class ScriptHookBlockServiceState;
class ScriptMod;
class ScriptModContextView;

class ScriptHookBlockEventView {
public:
    ScriptHookBlockEventView() = default;
    ScriptHookBlockEventView(const CKBehaviorContext *context, CKBehavior *ownerScript);

    bool IsValid() const { return m_Block != nullptr; }
    int GetBlockId() const;
    std::string GetBlockName() const;
    float GetDeltaTime() const { return m_DeltaTime; }
    int GetInputCount() const;
    int GetOutputCount() const;
    CKBehavior *BorrowBlock() const { return m_Block; }
    CKBehavior *BorrowOwnerScript() const { return m_OwnerScript; }
    bool ActivateOutput(int index) const;
    void ActivateAllOutputs() const;

private:
    CKBehavior *m_Block = nullptr;
    CKBehavior *m_OwnerScript = nullptr;
    float m_DeltaTime = 0.0f;
};

class ScriptHookBlockRef {
public:
    ScriptHookBlockRef(std::weak_ptr<ScriptHookBlockServiceState> state,
                       unsigned int id,
                       unsigned int generation);

    void AddRef();
    void Release();

    bool IsValid() const;
    bool IsInstalled() const;
    bool IsEnabled() const;
    bool SetEnabled(bool enabled);
    bool GetAutoActivateOutputs() const;
    bool SetAutoActivateOutputs(bool enabled);
    int GetBlockId() const;
    std::string GetName() const;
    CKBehavior *BorrowBlock() const;
    CKBehavior *BorrowOwnerScript() const;
    bool Uninstall();

private:
    int m_RefCount = 1;
    std::weak_ptr<ScriptHookBlockServiceState> m_State;
    unsigned int m_Id = 0;
    unsigned int m_Generation = 0;
};

class ScriptHookBlockService {
public:
    ScriptHookBlockService();
    ~ScriptHookBlockService();

    void Bind(ModContext *context, ScriptMod *owner, ScriptModContextView *contextView);
    ScriptHookBlockRef *Create(CKBehavior *ownerScript,
                               asIScriptFunction *callback,
                               const std::string &name,
                               int inputCount,
                               int outputCount);
    ScriptHookBlockRef *InsertAfter(CKBehavior *ownerScript,
                                    CKBehavior *source,
                                    asIScriptFunction *callback,
                                    const std::string &name,
                                    int sourceOutput,
                                    int targetInput);
    ScriptHookBlockRef *InsertBefore(CKBehavior *ownerScript,
                                     CKBehavior *target,
                                     asIScriptFunction *callback,
                                     const std::string &name,
                                     int sourceOutput,
                                     int targetInput);
    ScriptHookBlockRef *InsertBetween(CKBehavior *ownerScript,
                                      CKBehavior *source,
                                      CKBehavior *target,
                                      asIScriptFunction *callback,
                                      const std::string &name,
                                      int sourceOutput,
                                      int targetInput);
    void Release(ScriptDiagnostic *diagnostic = nullptr);
    size_t GetActiveCount() const;

private:
    std::shared_ptr<ScriptHookBlockServiceState> m_State;
};

} // namespace BML

#endif
