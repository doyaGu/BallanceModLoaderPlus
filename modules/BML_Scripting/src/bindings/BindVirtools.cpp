#include "BindVirtools.h"

#include <cassert>
#include <string>

#include "CKAll.h"
#ifdef GetObject
#undef GetObject
#endif

#include "bml_logging.h"
#include "bml_virtools.h"
#include "../ModuleScope.h"

namespace BML::Scripting {

namespace {

static bool g_VirtoolsReady = false;
static bool g_VirtoolsWarnedOnce = false;

CKContext *GetCK() {
    if (!g_VirtoolsReady) {
        if (!g_VirtoolsWarnedOnce) {
            g_VirtoolsWarnedOnce = true;
            const BML_Mod owner = CurrentScriptOwner();
            if (g_Services && g_Services->Logging && g_Services->Logging->Log && owner) {
                g_Services->Logging->Log(owner, BML_LOG_WARN, "script",
                    "Virtools bindings called before engine init - returning nullptr");
            }
        }
        return nullptr;
    }
    if (!g_Services || !g_Services->Context) return nullptr;
    BML_Context ctx = g_Services->Context->Context;
    if (!ctx) return nullptr;
    void *ptr = nullptr;
    if (g_Services->Context->GetUserData(ctx, BML_VIRTOOLS_KEY_CKCONTEXT, &ptr) != BML_RESULT_OK)
        return nullptr;
    return static_cast<CKContext *>(ptr);
}

static CKContext *Script_GetCKContext() { return GetCK(); }

static CK3dObject *Script_Find3dObject(const std::string &name) {
    auto *ck = GetCK(); if (!ck) return nullptr;
    return static_cast<CK3dObject *>(ck->GetObjectByNameAndClass(
        const_cast<CKSTRING>(name.c_str()), CKCID_3DOBJECT, nullptr));
}

static CKGroup *Script_FindGroup(const std::string &name) {
    auto *ck = GetCK(); if (!ck) return nullptr;
    return static_cast<CKGroup *>(ck->GetObjectByNameAndClass(
        const_cast<CKSTRING>(name.c_str()), CKCID_GROUP, nullptr));
}

static CKMaterial *Script_FindMaterial(const std::string &name) {
    auto *ck = GetCK(); if (!ck) return nullptr;
    return static_cast<CKMaterial *>(ck->GetObjectByNameAndClass(
        const_cast<CKSTRING>(name.c_str()), CKCID_MATERIAL, nullptr));
}

static CKTexture *Script_FindTexture(const std::string &name) {
    auto *ck = GetCK(); if (!ck) return nullptr;
    return static_cast<CKTexture *>(ck->GetObjectByNameAndClass(
        const_cast<CKSTRING>(name.c_str()), CKCID_TEXTURE, nullptr));
}

static CKMesh *Script_FindMesh(const std::string &name) {
    auto *ck = GetCK(); if (!ck) return nullptr;
    return static_cast<CKMesh *>(ck->GetObjectByNameAndClass(
        const_cast<CKSTRING>(name.c_str()), CKCID_MESH, nullptr));
}

static CK2dEntity *Script_Find2dEntity(const std::string &name) {
    auto *ck = GetCK(); if (!ck) return nullptr;
    return static_cast<CK2dEntity *>(ck->GetObjectByNameAndClass(
        const_cast<CKSTRING>(name.c_str()), CKCID_2DENTITY, nullptr));
}

static CKDataArray *Script_FindArray(const std::string &name) {
    auto *ck = GetCK(); if (!ck) return nullptr;
    return static_cast<CKDataArray *>(ck->GetObjectByNameAndClass(
        const_cast<CKSTRING>(name.c_str()), CKCID_DATAARRAY, nullptr));
}

static CKSound *Script_FindSound(const std::string &name) {
    auto *ck = GetCK(); if (!ck) return nullptr;
    return static_cast<CKSound *>(ck->GetObjectByNameAndClass(
        const_cast<CKSTRING>(name.c_str()), CKCID_WAVESOUND, nullptr));
}

} // namespace

void RegisterVirtoolsBindings(asIScriptEngine *engine) {
    int r;
    r = engine->RegisterGlobalFunction("CKContext@ GetCKContext()",
        asFUNCTION(Script_GetCKContext), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("CK3dObject@ Find3dObject(const string &in)",
        asFUNCTION(Script_Find3dObject), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("CKGroup@ FindGroup(const string &in)",
        asFUNCTION(Script_FindGroup), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("CKMaterial@ FindMaterial(const string &in)",
        asFUNCTION(Script_FindMaterial), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("CKTexture@ FindTexture(const string &in)",
        asFUNCTION(Script_FindTexture), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("CKMesh@ FindMesh(const string &in)",
        asFUNCTION(Script_FindMesh), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("CK2dEntity@ Find2dEntity(const string &in)",
        asFUNCTION(Script_Find2dEntity), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("CKDataArray@ FindArray(const string &in)",
        asFUNCTION(Script_FindArray), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("CKSound@ FindSound(const string &in)",
        asFUNCTION(Script_FindSound), asCALL_CDECL); assert(r >= 0);
}

void SetVirtoolsBindingsReady(bool ready) {
    g_VirtoolsReady = ready;
    if (ready) g_VirtoolsWarnedOnce = false;
}

} // namespace BML::Scripting
