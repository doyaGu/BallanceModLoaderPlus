#include "BindCoroutine.h"

#include <cassert>

#include "../CoroutineManager.h"

namespace BML::Scripting {

static CoroutineManager *g_CoMgr = nullptr;

static void Script_Yield() {
    if (g_CoMgr) g_CoMgr->YieldCurrent();
}

static void Script_WaitFrames(int frames) {
    if (g_CoMgr && frames > 0) g_CoMgr->WaitFrames(static_cast<uint32_t>(frames));
}

static void Script_WaitSeconds(float seconds) {
    if (g_CoMgr && seconds > 0.0f) g_CoMgr->WaitSeconds(seconds);
}

static int Script_GetFrame() {
    return g_CoMgr ? static_cast<int>(g_CoMgr->GetFrame()) : 0;
}

void RegisterCoroutineBindings(asIScriptEngine *engine, CoroutineManager *mgr) {
    g_CoMgr = mgr;

    int r;

    r = engine->RegisterGlobalFunction(
        "void bmlYield()",
        asFUNCTION(Script_Yield), asCALL_CDECL);
    assert(r >= 0);

    r = engine->RegisterGlobalFunction(
        "void bmlWaitFrames(int frames)",
        asFUNCTION(Script_WaitFrames), asCALL_CDECL);
    assert(r >= 0);

    r = engine->RegisterGlobalFunction(
        "void bmlWaitSeconds(float seconds)",
        asFUNCTION(Script_WaitSeconds), asCALL_CDECL);
    assert(r >= 0);

    r = engine->RegisterGlobalFunction(
        "int bmlGetFrame()",
        asFUNCTION(Script_GetFrame), asCALL_CDECL);
    assert(r >= 0);
}

} // namespace BML::Scripting
