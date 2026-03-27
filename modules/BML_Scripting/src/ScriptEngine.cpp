#include "ScriptEngine.h"
#include "ScriptExceptionHelper.h"

#include <cstdio>
#include <string>

#include "add_on/scriptstdstring/scriptstdstring.h"
#include "add_on/scriptarray/scriptarray.h"
#include "add_on/scriptmath/scriptmath.h"
#include "add_on/scripthelper/scripthelper.h"

namespace BML::Scripting {

static BML_Mod s_Owner = nullptr;

bool ScriptEngine::Initialize() {
    if (m_Engine)
        return false;

    m_Engine = asCreateScriptEngine();
    if (!m_Engine)
        return false;

    m_Engine->SetMessageCallback(asFUNCTION(MessageCallback), this, asCALL_CDECL);

    // Engine properties (following CKAngelScript conventions)
    m_Engine->SetEngineProperty(asEP_USE_CHARACTER_LITERALS, true);
    m_Engine->SetEngineProperty(asEP_ALLOW_UNSAFE_REFERENCES, true);
    m_Engine->SetEngineProperty(asEP_ALLOW_IMPLICIT_HANDLE_TYPES, true);
    m_Engine->SetEngineProperty(asEP_PROPERTY_ACCESSOR_MODE, 1);
    // Keep line cues enabled for timeout callback support
    m_Engine->SetEngineProperty(asEP_BUILD_WITHOUT_LINE_CUES, false);
    m_Engine->SetEngineProperty(asEP_MAX_STACK_SIZE, 1024 * 1024); // 1MB

    // Standard add-ons
    RegisterStdString(m_Engine);
    RegisterScriptArray(m_Engine, true);
    RegisterScriptMath(m_Engine);
    RegisterExceptionRoutines(m_Engine);

    return true;
}

void ScriptEngine::SetServices(BML_Mod owner, const BML_Services * /*services*/) {
    s_Owner = owner;
}

void ScriptEngine::Shutdown() {
    if (m_Engine) {
        m_Engine->ShutDownAndRelease();
        m_Engine = nullptr;
    }
    s_Owner = nullptr;
}

void ScriptEngine::MessageCallback(const asSMessageInfo *msg, void * /*param*/) {
    if (!msg || !s_Owner) return;

    const char *severity = "INFO";
    if (msg->type == asMSGTYPE_WARNING) severity = "WARN";
    if (msg->type == asMSGTYPE_ERROR)   severity = "ERROR";

    char buf[1024];
    std::snprintf(buf, sizeof(buf), "[AS %s] %s (%d, %d): %s",
                  severity,
                  msg->section ? msg->section : "<unknown>",
                  msg->row, msg->col,
                  msg->message ? msg->message : "");

    PublishConsoleOutputMessage(s_Owner, buf, 0);
}

} // namespace BML::Scripting
