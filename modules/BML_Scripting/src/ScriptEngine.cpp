#include "ScriptEngine.h"

#include <cstdio>
#include <string>

#include "add_on/scriptstdstring/scriptstdstring.h"
#include "add_on/scriptarray/scriptarray.h"
#include "add_on/scriptmath/scriptmath.h"
#include "add_on/scripthelper/scripthelper.h"

#include "bml_core.h"
#include "bml_imc.h"
#include "bml_topics.h"

namespace BML::Scripting {

// Resolved once at init, used by MessageCallback to forward errors to Console
static PFN_BML_ImcGetTopicId s_GetTopicId = nullptr;
static PFN_BML_ImcPublish s_Publish = nullptr;
static PFN_BML_GetHostModule s_GetHostModule = nullptr;
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

void ScriptEngine::SetGetProc(PFN_BML_GetProcAddress get_proc) {
    if (!get_proc) return;
    s_GetTopicId = reinterpret_cast<PFN_BML_ImcGetTopicId>(get_proc("bmlImcGetTopicId"));
    s_Publish = reinterpret_cast<PFN_BML_ImcPublish>(get_proc("bmlImcPublish"));
    s_GetHostModule = reinterpret_cast<PFN_BML_GetHostModule>(get_proc("bmlGetHostModule"));
    s_Owner = s_GetHostModule ? s_GetHostModule() : nullptr;
}

void ScriptEngine::Shutdown() {
    if (m_Engine) {
        m_Engine->ShutDownAndRelease();
        m_Engine = nullptr;
    }
    s_GetTopicId = nullptr;
    s_Publish = nullptr;
    s_GetHostModule = nullptr;
    s_Owner = nullptr;
}

void ScriptEngine::MessageCallback(const asSMessageInfo *msg, void * /*param*/) {
    if (!msg) return;

    const char *severity = "INFO";
    if (msg->type == asMSGTYPE_WARNING) severity = "WARN";
    if (msg->type == asMSGTYPE_ERROR)   severity = "ERROR";

    // Format: [SEVERITY] file (line, col): message
    char buf[1024];
    std::snprintf(buf, sizeof(buf), "[AS %s] %s (%d, %d): %s",
                  severity,
                  msg->section ? msg->section : "<unknown>",
                  msg->row, msg->col,
                  msg->message ? msg->message : "");

    // Forward to Console output if IMC is available
    if (s_GetTopicId && s_Publish && s_Owner) {
        BML_TopicId topic_id = BML_TOPIC_ID_INVALID;
        if (s_GetTopicId(BML_TOPIC_CONSOLE_OUTPUT, &topic_id) == BML_RESULT_OK) {
            struct {
                size_t struct_size;
                const char *message_utf8;
                uint32_t flags;
            } event;
            event.struct_size = sizeof(event);
            event.message_utf8 = buf;
            event.flags = 0;
            s_Publish(s_Owner, topic_id, &event, sizeof(event));
        }
    }
}

} // namespace BML::Scripting
