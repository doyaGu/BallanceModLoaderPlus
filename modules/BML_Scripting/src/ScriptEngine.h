#ifndef BML_SCRIPTING_SCRIPT_ENGINE_H
#define BML_SCRIPTING_SCRIPT_ENGINE_H

#include <angelscript.h>

#include "bml_export.h"

namespace BML::Scripting {

class ScriptEngine {
public:
    bool Initialize();
    void Shutdown();

    /// Set the API resolver for compile error forwarding to Console.
    /// Call after Initialize(), before any script compilation.
    void SetGetProc(PFN_BML_GetProcAddress get_proc);

    asIScriptEngine *Get() const { return m_Engine; }
    explicit operator bool() const { return m_Engine != nullptr; }

private:
    static void MessageCallback(const asSMessageInfo *msg, void *param);

    asIScriptEngine *m_Engine = nullptr;
};

} // namespace BML::Scripting

#endif // BML_SCRIPTING_SCRIPT_ENGINE_H
