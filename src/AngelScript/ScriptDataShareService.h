#ifndef BML_SCRIPTDATASHARESERVICE_H
#define BML_SCRIPTDATASHARESERVICE_H

#include <memory>
#include <string>

#include "BML/DataShare.h"
#include "ScriptDiagnostic.h"
#include "ScriptModRuntime.h"

class ModContext;
class asIScriptObject;

namespace BML {

class ScriptMod;
class ScriptModContextView;
class ScriptDataShareServiceState;

enum class ScriptDataShareRequestType {
    String,
    Bool,
    Int,
    Float,
};

class ScriptDataShareEventView {
public:
    ScriptDataShareEventView(const std::string *key = nullptr,
                             ScriptDataShareRequestType type = ScriptDataShareRequestType::String,
                             bool exists = false,
                             const void *data = nullptr,
                             size_t size = 0);

    bool Exists() const { return m_Exists; }
    std::string GetKey() const;
    int GetType() const;
    std::string GetString() const;
    bool GetBool() const;
    int GetInt() const;
    float GetFloat() const;

private:
    const std::string *m_Key = nullptr;
    ScriptDataShareRequestType m_Type = ScriptDataShareRequestType::String;
    bool m_Exists = false;
    const void *m_Data = nullptr;
    size_t m_Size = 0;
};

class ScriptDataShareRequestRef {
public:
    ScriptDataShareRequestRef(std::weak_ptr<ScriptDataShareServiceState> state, int id, unsigned int generation);

    void AddRef();
    void Release();

    bool IsValid() const;
    std::string GetKey() const;
    int GetType() const;
    bool Cancel();

private:
    int m_RefCount = 1;
    std::weak_ptr<ScriptDataShareServiceState> m_State;
    int m_Id = 0;
    unsigned int m_Generation = 0;
};

class ScriptDataShareService {
public:
    ScriptDataShareService();
    ~ScriptDataShareService();

    void Bind(ModContext *context, ScriptMod *owner, ScriptModRuntime *runtime, ScriptModContextView *contextView);
    ScriptDataShareRequestRef *Request(asIScriptObject *request);
    void Release(ScriptDiagnostic *diagnostic = nullptr);

private:
    std::shared_ptr<ScriptDataShareServiceState> m_State;
};

} // namespace BML

#endif
