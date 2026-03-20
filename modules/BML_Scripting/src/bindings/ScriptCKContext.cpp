#include "ScriptCKContext.h"

#include <cassert>
#include <string>

#include "CKContext.h"

#include "ScriptUtils.h"
#include "ScriptNativePointer.h"

void RegisterCKContext(asIScriptEngine *engine) {
    int r = 0;

    // Objects Management
    r = engine->RegisterObjectMethod("CKContext", "CKObject@ CreateObject(CK_CLASSID cid, const string &in name = void, CK_OBJECTCREATION_OPTIONS options = CK_OBJECTCREATION_NONAMECHECK, CK_LOADMODE &out res = void)", asFUNCTIONPR([](CKContext *self, CK_CLASSID cid, const std::string &name, CK_OBJECTCREATION_OPTIONS options, CK_CREATIONMODE &res) { return self->CreateObject(cid, name.empty() ? nullptr : const_cast<CKSTRING>(name.c_str()), options, &res); }, (CKContext *, CK_CLASSID, const std::string &, CK_OBJECTCREATION_OPTIONS, CK_CREATIONMODE &), CKObject*), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "CKObject@ CopyObject(CKObject@ src, CKDependencies &in dependencies = void, const string &in appendName = void, CK_OBJECTCREATION_OPTIONS options = CK_OBJECTCREATION_NONAMECHECK)", asFUNCTIONPR([](CKContext *self, CKObject *src, CKDependencies *dependencies, const std::string &appendName, CK_OBJECTCREATION_OPTIONS options) { return self->CopyObject(src, dependencies, appendName.empty() ? nullptr : const_cast<CKSTRING>(appendName.c_str()), options); }, (CKContext *, CKObject *, CKDependencies *, const std::string &, CK_OBJECTCREATION_OPTIONS), CKObject*), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "const XObjectArray &CopyObjects(const XObjectArray &in srcObjects, CKDependencies &in dependencies = void, CK_OBJECTCREATION_OPTIONS options = CK_OBJECTCREATION_NONAMECHECK, const string &in appendName = void)", asMETHODPR(CKContext, CopyObjects, (const XObjectArray&, CKDependencies*, CK_OBJECTCREATION_OPTIONS, CKSTRING), const XObjectArray&), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKContext", "CKObject@ GetObject(CK_ID id)", asMETHODPR(CKContext, GetObject, (CK_ID), CKObject*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "int GetObjectCount()", asMETHODPR(CKContext, GetObjectCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "int GetObjectSize(CKObject@ obj)", asMETHODPR(CKContext, GetObjectSize, (CKObject*), int), asCALL_THISCALL); assert(r >= 0);
#if CKVERSION == 0x13022002
    r = engine->RegisterObjectMethod("CKContext", "CKERROR DestroyObject(CKObject@ obj, CKDWORD flags = 0, CKDependencies &in depoptions = void)", asMETHODPR(CKContext, DestroyObject, (CKObject*, CKDWORD, CKDependencies*), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "CKERROR DestroyObject(CK_ID id, CKDWORD flags = 0, CKDependencies &in depoptions = void)", asMETHODPR(CKContext, DestroyObject, (CK_ID, CKDWORD, CKDependencies*), CKERROR), asCALL_THISCALL); assert(r >= 0);
#else
    r = engine->RegisterObjectMethod("CKContext", "CKERROR DestroyObject(CKObject@ obj, CKDWORD flags = 0, CKDependencies &in depoptions = void)", asMETHODPR(CKContext, DestroyObject, (CKObject*, DWORD, CKDependencies*), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "CKERROR DestroyObject(CK_ID id, CKDWORD flags = 0, CKDependencies &in depoptions = void)", asMETHODPR(CKContext, DestroyObject, (CK_ID, DWORD, CKDependencies*), CKERROR), asCALL_THISCALL); assert(r >= 0);
#endif
    r = engine->RegisterObjectMethod("CKContext", "CKERROR DestroyObjects(const XObjectArray &in ids, CKDWORD flags = 0, CKDependencies &in depoptions = void)", asFUNCTIONPR([](CKContext *self, const XObjectArray &objects, CKDWORD flags, CKDependencies *deps) { return self->DestroyObjects(objects.Begin(), objects.Size(), flags, deps); }, (CKContext *, const XObjectArray &, CKDWORD, CKDependencies *), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
#if CKVERSION == 0x13022002 
    r = engine->RegisterObjectMethod("CKContext", "void DestroyAllDynamicObjects()", asMETHODPR(CKContext, DestroyAllDynamicObjects, (), void), asCALL_THISCALL); assert(r >= 0);
#else
    r = engine->RegisterObjectMethod("CKContext", "void DestroyAllDynamicObjects(CKScene@ scene = null)", asMETHODPR(CKContext, DestroyAllDynamicObjects, (CKScene *), void), asCALL_THISCALL); assert(r >= 0);
#endif
    r = engine->RegisterObjectMethod("CKContext", "void ChangeObjectDynamic(CKObject@ obj, bool setDynamic = true)", asFUNCTIONPR([](CKContext *self, CKObject *obj, bool setDynamic) { self->ChangeObjectDynamic(obj, setDynamic); }, (CKContext *, CKObject *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKContext", "const XObjectPointerArray &CKFillObjectsUnused()", asMETHODPR(CKContext, CKFillObjectsUnused, (), const XObjectPointerArray &), asCALL_THISCALL); assert(r >= 0);

    // Object Access
    r = engine->RegisterObjectMethod("CKContext", "CKObject@ GetObjectByName(const string &in name, CKObject@ previous = null)", asFUNCTIONPR([](CKContext *self, const std::string &name, CKObject *previous) { return self->GetObjectByName(const_cast<CKSTRING>(name.c_str()), previous); }, (CKContext *, const std::string &, CKObject *), CKObject*), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "CKObject@ GetObjectByNameAndClass(const string &in name, CK_CLASSID cid, CKObject@ previous = null)", asFUNCTIONPR([](CKContext *self, const std::string &name, CK_CLASSID cid, CKObject *previous) { return self->GetObjectByNameAndClass(const_cast<CKSTRING>(name.c_str()), cid, previous); }, (CKContext *, const std::string &, CK_CLASSID, CKObject *), CKObject*), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "CKObject@ GetObjectByNameAndParentClass(const string &in name, CK_CLASSID pcid, CKObject@ previous)", asFUNCTIONPR([](CKContext *self, const std::string &name, CK_CLASSID pcid, CKObject *previous) { return self->GetObjectByNameAndParentClass(const_cast<CKSTRING>(name.c_str()), pcid, previous); }, (CKContext *, const std::string &, CK_CLASSID, CKObject *), CKObject*), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "const XObjectPointerArray &GetObjectListByType(CK_CLASSID cid, bool derived)", asFUNCTIONPR([](CKContext *self, CK_CLASSID cid, bool derived) -> const XObjectPointerArray & { return self->GetObjectListByType(cid, derived); }, (CKContext *, CK_CLASSID, bool), const XObjectPointerArray &), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKContext", "int GetObjectsCountByClassID(CK_CLASSID cid)", asMETHODPR(CKContext, GetObjectsCountByClassID, (CK_CLASSID), int), asCALL_THISCALL); assert(r >= 0);
    // r = engine->RegisterObjectMethod("CKContext", "uintptr_t GetObjectsListByClassID(int)", asMETHODPR(CKContext, GetObjectsListByClassID, (CK_CLASSID), CK_ID*), asCALL_THISCALL); assert(r >= 0);

    // Engine runtime
    r = engine->RegisterObjectMethod("CKContext", "CKERROR Play()", asMETHODPR(CKContext, Play, (), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "CKERROR Pause()", asMETHODPR(CKContext, Pause, (), CKERROR), asCALL_THISCALL); assert(r >= 0);
#if CKVERSION == 0x13022002
    r = engine->RegisterObjectMethod("CKContext", "CKERROR Reset()", asMETHODPR(CKContext, Reset, (), CKERROR), asCALL_THISCALL); assert(r >= 0);
#else
    r = engine->RegisterObjectMethod("CKContext", "CKERROR Reset(bool toLevelScene = false)", asFUNCTIONPR([](CKContext *self, bool toLevelScene) { return self->Reset(toLevelScene); }, (CKContext *, bool), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
#endif
    r = engine->RegisterObjectMethod("CKContext", "bool IsPlaying()", asFUNCTIONPR([](CKContext *self) -> bool { return self->IsPlaying(); }, (CKContext *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "bool IsReseted()", asFUNCTIONPR([](CKContext *self) -> bool { return self->IsReseted(); }, (CKContext *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "CKERROR Process()", asMETHODPR(CKContext, Process, (), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "CKERROR ClearAll()", asMETHODPR(CKContext, ClearAll, (), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "bool IsInClearAll()", asFUNCTIONPR([](CKContext *self) -> bool { return self->IsInClearAll(); }, (CKContext *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // Current Level & Scene functions
    r = engine->RegisterObjectMethod("CKContext", "CKLevel@ GetCurrentLevel()", asMETHODPR(CKContext, GetCurrentLevel, (), CKLevel*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "CKRenderContext@ GetPlayerRenderContext()", asMETHODPR(CKContext, GetPlayerRenderContext, (), CKRenderContext*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "CKScene@ GetCurrentScene()", asMETHODPR(CKContext, GetCurrentScene, (), CKScene*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "void SetCurrentLevel(CKLevel@ level)", asMETHODPR(CKContext, SetCurrentLevel, (CKLevel*), void), asCALL_THISCALL); assert(r >= 0);

    // Object Management functions
    r = engine->RegisterObjectMethod("CKContext", "CKParameterIn@ CreateCKParameterIn(const string &in name, CKParameterType type, bool dynamic = false)", asFUNCTIONPR([](CKContext *self, const std::string &name, CKParameterType type, bool dynamic) { return self->CreateCKParameterIn(const_cast<CKSTRING>(name.c_str()), type, dynamic); }, (CKContext *, const std::string &, CKParameterType, bool), CKParameterIn*), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "CKParameterIn@ CreateCKParameterIn(const string &in name, CKGUID guid, bool dynamic = false)", asFUNCTIONPR([](CKContext *self, const std::string &name, CKGUID guid, bool dynamic) { return self->CreateCKParameterIn(const_cast<CKSTRING>(name.c_str()), guid, dynamic); }, (CKContext *, const std::string &, CKGUID, bool), CKParameterIn*), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "CKParameterIn@ CreateCKParameterIn(const string &in name, const string &in typeName, bool dynamic = false)", asFUNCTIONPR([](CKContext *self, const std::string &name, const std::string &typeName, bool dynamic) { return self->CreateCKParameterIn(const_cast<CKSTRING>(name.c_str()), const_cast<CKSTRING>(typeName.c_str()), dynamic); }, (CKContext *, const std::string &, const std::string &, bool), CKParameterIn*), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "CKParameterOut@ CreateCKParameterOut(const string &in name, CKParameterType type, bool dynamic = false)", asFUNCTIONPR([](CKContext *self, const std::string &name, CKParameterType type, bool dynamic) { return self->CreateCKParameterOut(const_cast<CKSTRING>(name.c_str()), type, dynamic); }, (CKContext *, const std::string &, CKParameterType, bool), CKParameterOut*), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "CKParameterOut@ CreateCKParameterOut(const string &in name, CKGUID guid, bool dynamic = false)", asFUNCTIONPR([](CKContext *self, const std::string &name, CKGUID guid, bool dynamic) { return self->CreateCKParameterOut(const_cast<CKSTRING>(name.c_str()), guid, dynamic); }, (CKContext *, const std::string &, CKGUID, bool), CKParameterOut*), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "CKParameterOut@ CreateCKParameterOut(const string &in name, const string &in typeName, bool dynamic = false)", asFUNCTIONPR([](CKContext *self, const std::string &name, const std::string &typeName, bool dynamic) { return self->CreateCKParameterOut(const_cast<CKSTRING>(name.c_str()), const_cast<CKSTRING>(typeName.c_str()), dynamic); }, (CKContext *, const std::string &, const std::string &, bool), CKParameterOut*), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "CKParameterLocal@ CreateCKParameterLocal(const string &in name, CKParameterType type, bool dynamic = false)", asFUNCTIONPR([](CKContext *self, const std::string &name, CKParameterType type, bool dynamic) { return self->CreateCKParameterLocal(const_cast<CKSTRING>(name.c_str()), type, dynamic); }, (CKContext *, const std::string &, CKParameterType, bool), CKParameterLocal*), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "CKParameterLocal@ CreateCKParameterLocal(const string &in name, CKGUID guid, bool dynamic = false)", asFUNCTIONPR([](CKContext *self, const std::string &name, CKGUID guid, bool dynamic) { return self->CreateCKParameterLocal(const_cast<CKSTRING>(name.c_str()), guid, dynamic); }, (CKContext *, const std::string &, CKGUID, bool), CKParameterLocal*), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "CKParameterLocal@ CreateCKParameterLocal(const string &in name, const string &in typeName, bool dynamic = false)", asFUNCTIONPR([](CKContext *self, const std::string &name, const std::string &typeName, bool dynamic) { return self->CreateCKParameterLocal(const_cast<CKSTRING>(name.c_str()), const_cast<CKSTRING>(typeName.c_str()), dynamic); }, (CKContext *, const std::string &, const std::string &, bool), CKParameterLocal*), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "CKParameterOperation@ CreateCKParameterOperation(const string &in name, CKGUID opguid, CKGUID resGuid, CKGUID p1Guid, CKGUID p2Guid)", asFUNCTIONPR([](CKContext *self, const std::string &name, CKGUID opguid, CKGUID resGuid, CKGUID p1Guid, CKGUID p2Guid) { return self->CreateCKParameterOperation(const_cast<CKSTRING>(name.c_str()), opguid, resGuid, p1Guid, p2Guid); }, (CKContext *, const std::string &, CKGUID, CKGUID, CKGUID, CKGUID), CKParameterOperation*), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKContext", "CKFile@ CreateCKFile()", asMETHODPR(CKContext, CreateCKFile, (), CKFile*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "CKERROR DeleteCKFile(CKFile@ file)", asMETHODPR(CKContext, DeleteCKFile, (CKFile*), CKERROR), asCALL_THISCALL); assert(r >= 0);

    // IHM
    // r = engine->RegisterObjectMethod("CKContext", "void SetInterfaceMode(bool mode = true, uintptr_t = 0, uintptr_t = 0)", asMETHODPR(CKContext, SetInterfaceMode, (CKBOOL, CKUICALLBACKFCT, void *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "bool IsInInterfaceMode()", asFUNCTIONPR([](CKContext *self) -> bool { return self->IsInInterfaceMode(); }, (CKContext *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKContext", "CKERROR OutputToConsole(const string &in str, bool beep = true)", asFUNCTIONPR([](CKContext *self, const std::string &str, bool beep) { return self->OutputToConsole(const_cast<CKSTRING>(str.c_str()), beep); }, (CKContext *, const std::string &, bool), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "CKERROR OutputToInfo(const string &in format)", asFUNCTIONPR([](CKContext *self, const std::string &info) { return self->OutputToInfo(const_cast<CKSTRING>(info.c_str())); }, (CKContext *, const std::string &), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "CKERROR RefreshBuildingBlocks(const XGUIDArray &in guids)", asMETHODPR(CKContext, RefreshBuildingBlocks, (const XArray<CKGUID>&), CKERROR), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKContext", "CKERROR ShowSetup(int)", asMETHODPR(CKContext, ShowSetup, (CK_ID), CKERROR), asCALL_THISCALL); assert(r >= 0);
    // r = engine->RegisterObjectMethod("CKContext", "int ChooseObject(uintptr_t dialogParentWnd)", asMETHODPR(CKContext, ChooseObject, (void *), CK_ID), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "CKERROR Select(const XObjectArray &in objects, bool clearSelection = true)", asFUNCTIONPR([](CKContext *self, const XObjectArray &objects, bool clearSelection) { return self->Select(objects, clearSelection); }, (CKContext *, const XObjectArray &, bool), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
#if CKVERSION != 0x26052005
    r = engine->RegisterObjectMethod("CKContext", "CKDWORD SendInterfaceMessage(CKDWORD reason, CKDWORD param1, CKDWORD param2)", asMETHODPR(CKContext, SendInterfaceMessage, (CKDWORD, CKDWORD, CKDWORD), CKDWORD), asCALL_THISCALL); assert(r >= 0);
#else
    r = engine->RegisterObjectMethod("CKContext", "CKDWORD SendInterfaceMessage(CKDWORD reason, CKDWORD param1, CKDWORD param2, CKDWORD param3 = 0)", asMETHODPR(CKContext, SendInterfaceMessage, (CKDWORD, CKDWORD, CKDWORD, CKDWORD), CKDWORD), asCALL_THISCALL); assert(r >= 0);
#endif
    r = engine->RegisterObjectMethod("CKContext", "CKERROR UICopyObjects(const XObjectArray &in, bool iClearClipboard = true)", asFUNCTIONPR([](CKContext *self, const XObjectArray &objects, bool iClearClipboard) { return self->UICopyObjects(objects, iClearClipboard); }, (CKContext *, const XObjectArray &, bool), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "CKERROR UIPasteObjects(const XObjectArray &in)", asMETHODPR(CKContext, UIPasteObjects, (const XObjectArray&), CKERROR), asCALL_THISCALL); assert(r >= 0);

    // Managers
    r = engine->RegisterObjectMethod("CKContext", "CKRenderManager@ GetRenderManager()", asMETHODPR(CKContext, GetRenderManager, (), CKRenderManager*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "CKBehaviorManager@ GetBehaviorManager()", asMETHODPR(CKContext, GetBehaviorManager, (), CKBehaviorManager*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "CKParameterManager@ GetParameterManager()", asMETHODPR(CKContext, GetParameterManager, (), CKParameterManager*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "CKMessageManager@ GetMessageManager()", asMETHODPR(CKContext, GetMessageManager, (), CKMessageManager*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "CKTimeManager@ GetTimeManager()", asMETHODPR(CKContext, GetTimeManager, (), CKTimeManager*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "CKAttributeManager@ GetAttributeManager()", asMETHODPR(CKContext, GetAttributeManager, (), CKAttributeManager*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "CKPathManager@ GetPathManager()", asMETHODPR(CKContext, GetPathManager, (), CKPathManager*), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKContext", "XManagerHashTableIt GetManagers()", asMETHODPR(CKContext, GetManagers, (), XManagerHashTableIt), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "CKBaseManager@ GetManagerByGuid(const CKGUID &in guid)", asMETHODPR(CKContext, GetManagerByGuid, (CKGUID), CKBaseManager*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "CKBaseManager@ GetManagerByName(const string &in name)", asFUNCTIONPR([](CKContext *self, const std::string &name) { return self->GetManagerByName(const_cast<CKSTRING>(name.c_str())); }, (CKContext *, const std::string &), CKBaseManager*), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKContext", "int GetManagerCount()", asMETHODPR(CKContext, GetManagerCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "CKBaseManager@ GetManager(int index)", asMETHODPR(CKContext, GetManager, (int), CKBaseManager*), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKContext", "bool IsManagerActive(CKBaseManager@ manager)", asFUNCTIONPR([](CKContext *self, CKBaseManager *manager) -> bool { return self->IsManagerActive(manager); }, (CKContext *, CKBaseManager *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "bool HasManagerDuplicates(CKBaseManager@ manager)", asFUNCTIONPR([](CKContext *self, CKBaseManager *manager) -> bool { return self->HasManagerDuplicates(manager); }, (CKContext *, CKBaseManager *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "void ActivateManager(CKBaseManager@ manager, bool active)", asFUNCTIONPR([](CKContext *self, CKBaseManager *manager, bool active) { self->ActivateManager(manager, active); }, (CKContext *, CKBaseManager *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "int GetInactiveManagerCount()", asMETHODPR(CKContext, GetInactiveManagerCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "CKBaseManager@ GetInactiveManager(int index)", asMETHODPR(CKContext, GetInactiveManager, (int), CKBaseManager*), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKContext", "CKERROR RegisterNewManager(CKBaseManager@ manager)", asMETHODPR(CKContext, RegisterNewManager, (CKBaseManager*), CKERROR), asCALL_THISCALL); assert(r >= 0);

    // Profiling Functions
    r = engine->RegisterObjectMethod("CKContext", "void EnableProfiling(bool enable)", asFUNCTIONPR([](CKContext *self, bool enable) { self->EnableProfiling(enable); }, (CKContext *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "bool IsProfilingEnable()", asFUNCTIONPR([](CKContext *self) -> bool { return self->IsProfilingEnable(); }, (CKContext *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "void GetProfileStats(CKStats &out stats)", asMETHODPR(CKContext, GetProfileStats, (CKStats*), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "void UserProfileStart(CKDWORD slot)", asMETHODPR(CKContext, UserProfileStart, (CKDWORD), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "float UserProfileEnd(CKDWORD slot)", asMETHODPR(CKContext, UserProfileEnd, (CKDWORD), float), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "float GetLastUserProfileTime(CKDWORD slot)", asMETHODPR(CKContext, GetLastUserProfileTime, (CKDWORD), float), asCALL_THISCALL); assert(r >= 0);

    // Utilities
    r = engine->RegisterObjectMethod("CKContext", "CKGUID GetSecureGuid()", asMETHODPR(CKContext, GetSecureGuid, (), CKGUID), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "CKDWORD GetStartOptions()", asMETHODPR(CKContext, GetStartOptions, (), CKDWORD), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "WIN_HANDLE GetMainWindow()", asMETHODPR(CKContext, GetMainWindow, (), WIN_HANDLE), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "int GetSelectedRenderEngine()", asMETHODPR(CKContext, GetSelectedRenderEngine, (), int), asCALL_THISCALL); assert(r >= 0);

    // File Save/Load Options
    r = engine->RegisterObjectMethod("CKContext", "void SetCompressionLevel(int level)", asMETHODPR(CKContext, SetCompressionLevel, (int), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "int GetCompressionLevel()", asMETHODPR(CKContext, GetCompressionLevel, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "void SetFileWriteMode(CK_FILE_WRITEMODE mode)", asMETHODPR(CKContext, SetFileWriteMode, (CK_FILE_WRITEMODE), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "CK_FILE_WRITEMODE GetFileWriteMode()", asMETHODPR(CKContext, GetFileWriteMode, (), CK_FILE_WRITEMODE), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "CK_TEXTURE_SAVEOPTIONS GetGlobalImagesSaveOptions()", asMETHODPR(CKContext, GetGlobalImagesSaveOptions, (), CK_TEXTURE_SAVEOPTIONS), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "void SetGlobalImagesSaveOptions(CK_TEXTURE_SAVEOPTIONS options)", asMETHODPR(CKContext, SetGlobalImagesSaveOptions, (CK_TEXTURE_SAVEOPTIONS), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "CKBitmapProperties@ GetGlobalImagesSaveFormat()", asMETHODPR(CKContext, GetGlobalImagesSaveFormat, (), CKBitmapProperties *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "void SetGlobalImagesSaveFormat(CKBitmapProperties@ format)", asMETHODPR(CKContext, SetGlobalImagesSaveFormat, (CKBitmapProperties *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "CK_SOUND_SAVEOPTIONS GetGlobalSoundsSaveOptions()", asMETHODPR(CKContext, GetGlobalSoundsSaveOptions, (), CK_SOUND_SAVEOPTIONS), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "void SetGlobalSoundsSaveOptions(CK_SOUND_SAVEOPTIONS options)", asMETHODPR(CKContext, SetGlobalSoundsSaveOptions, (CK_SOUND_SAVEOPTIONS), void), asCALL_THISCALL); assert(r >= 0);

    // Save/Load functions
    r = engine->RegisterObjectMethod("CKContext", "CKERROR Load(const string &in fileName, CKObjectArray@ objects, CK_LOAD_FLAGS loadFlags = CK_LOAD_DEFAULT, CKGUID &in readerGuid = void)", asFUNCTIONPR([](CKContext *self, const std::string &fileName, CKObjectArray *objects, CK_LOAD_FLAGS loadFlags, CKGUID *readerGuid) { return self->Load(const_cast<CKSTRING>(fileName.c_str()), objects, loadFlags, readerGuid); }, (CKContext *, const std::string &, CKObjectArray *, CK_LOAD_FLAGS, CKGUID *), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "CKERROR Load(int size, NativePointer buffer, CKObjectArray@ objects, CK_LOAD_FLAGS loadFlags = CK_LOAD_DEFAULT)", asFUNCTIONPR([](CKContext *self, int size, NativePointer buffer, CKObjectArray *objects, CK_LOAD_FLAGS loadFlags) { return self->Load(size, buffer.Get(), objects, loadFlags); }, (CKContext *, int, NativePointer, CKObjectArray *, CK_LOAD_FLAGS), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "string GetLastFileLoaded()", asFUNCTIONPR([](CKContext *self) -> std::string { return ScriptStringify(self->GetLastFileLoaded()); }, (CKContext *), std::string), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "string GetLastCmoLoaded()", asFUNCTIONPR([](CKContext *self) -> std::string { return ScriptStringify(self->GetLastCmoLoaded()); }, (CKContext *), std::string), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "void SetLastCmoLoaded(const string &in str)", asFUNCTIONPR([](CKContext *self, const std::string &str) { self->SetLastCmoLoaded(const_cast<CKSTRING>(str.c_str())); }, (CKContext *, const std::string &), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "CKERROR GetFileInfo(const string &in fileName, CKFileInfo &out fileInfo)", asFUNCTIONPR([](CKContext *self, const std::string &fileName, CKFileInfo *fileInfo) { return self->GetFileInfo(const_cast<CKSTRING>(fileName.c_str()), fileInfo); }, (CKContext *, const std::string &, CKFileInfo *), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "CKERROR GetFileInfo(int size, NativePointer buffer, CKFileInfo &out fileInfo)", asFUNCTIONPR([](CKContext *self, int size, NativePointer buffer, CKFileInfo* fileInfo) { return self->GetFileInfo(size, buffer.Get(), fileInfo); }, (CKContext *, int, NativePointer, CKFileInfo *), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "CKERROR Save(const string &in fileName, CKObjectArray@ objects, CKDWORD saveFlags, CKDependencies &in dependencies = void, CKGUID &in readerGuid = void)", asFUNCTIONPR([](CKContext *self, const std::string &fileName, CKObjectArray *objects, CKDWORD saveFlags, CKDependencies *dependencies, CKGUID *readerGuid) { return self->Save(const_cast<CKSTRING>(fileName.c_str()), objects, saveFlags, dependencies, readerGuid); }, (CKContext *, const std::string &, CKObjectArray *, CKDWORD, CKDependencies *, CKGUID *), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "CKERROR LoadAnimationOnCharacter(const string &in fileName, CKObjectArray@ objects, CKCharacter@ carac, CKGUID &in readerGuid = void, bool asDynamicObjects = false)", asFUNCTIONPR([](CKContext *self, const std::string &fileName, CKObjectArray *objects, CKCharacter *carac, CKGUID *readerGuid, bool asDynamicObjects) { return self->LoadAnimationOnCharacter(const_cast<CKSTRING>(fileName.c_str()), objects, carac, readerGuid, asDynamicObjects); }, (CKContext *, const std::string &, CKObjectArray *, CKCharacter *, CKGUID *, bool), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "CKERROR LoadAnimationOnCharacter(int size, NativePointer buffer, CKObjectArray@ objects, CKCharacter@ carac, bool asDynamicObjects = false)", asFUNCTIONPR([](CKContext *self, int size, NativePointer buffer, CKObjectArray *objects, CKCharacter *carac, bool asDynamicObjects) { return self->LoadAnimationOnCharacter(size, buffer.Get(), objects, carac, asDynamicObjects); }, (CKContext *, int, NativePointer, CKObjectArray *, CKCharacter *, bool), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // r = engine->RegisterObjectMethod("CKContext", "void SetAutomaticLoadMode(CK_LOADMODE generalMode, CK_LOADMODE _3DObjectsMode, CK_LOADMODE meshMode, CK_LOADMODE matTexturesMode)", asMETHODPR(CKContext, SetAutomaticLoadMode, (CK_LOADMODE, CK_LOADMODE, CK_LOADMODE, CK_LOADMODE), void), asCALL_THISCALL); assert(r >= 0);
    // r = engine->RegisterObjectMethod("CKContext", "void SetUserLoadCallback(uintptr_t, uintptr_t)", asMETHODPR(CKContext, SetUserLoadCallback, (CK_USERLOADCALLBACK, void *), void), asCALL_THISCALL); assert(r >= 0);
    // r = engine->RegisterObjectMethod("CKContext", "CK_LOADMODE LoadVerifyObjectUnicity(const string &in oldName, CK_CLASSID cid, const string &in newName, CKObject@ &out newObj)", asFUNCTIONPR([](CKContext *self, const std::string &oldName, CK_CLASSID cid, const std::string &newName, CKObject **newObj) { return self->LoadVerifyObjectUnicity(const_cast<CKSTRING>(oldName.c_str()), cid, const_cast<CKSTRING>(newName.c_str()), newObj); }, (CKContext *, const std::string &, CK_CLASSID, const std::string &, CKObject **), CK_LOADMODE), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKContext", "bool IsInLoad()", asFUNCTIONPR([](CKContext *self) -> bool { return self->IsInLoad(); }, (CKContext *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "bool IsInSave()", asFUNCTIONPR([](CKContext *self) -> bool { return self->IsInSave(); }, (CKContext *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "bool IsRunTime()", asFUNCTIONPR([](CKContext *self) -> bool { return self->IsRunTime(); }, (CKContext *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // Render Engine Implementation Specific
    r = engine->RegisterObjectMethod("CKContext", "void ExecuteManagersOnPreRender(CKRenderContext@ dev)", asMETHODPR(CKContext, ExecuteManagersOnPreRender, (CKRenderContext*), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "void ExecuteManagersOnPostRender(CKRenderContext@ dev)", asMETHODPR(CKContext, ExecuteManagersOnPostRender, (CKRenderContext*), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "void ExecuteManagersOnPostSpriteRender(CKRenderContext@ dev)", asMETHODPR(CKContext, ExecuteManagersOnPostSpriteRender, (CKRenderContext*), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKContext", "void AddProfileTime(CK_PROFILE_CATEGORY category, float time)", asMETHODPR(CKContext, AddProfileTime, (CK_PROFILE_CATEGORY, float), void), asCALL_THISCALL); assert(r >= 0);

    // Runtime Debug Mode
    r = engine->RegisterObjectMethod("CKContext", "CKERROR ProcessDebugStart(float deltaTime = 20.0)", asMETHODPR(CKContext, ProcessDebugStart, (float), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "bool ProcessDebugStep()", asFUNCTIONPR([](CKContext *self) -> bool { return self->ProcessDebugStep(); }, (CKContext *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "CKERROR ProcessDebugEnd()", asMETHODPR(CKContext, ProcessDebugEnd, (), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "CKDebugContext@ GetDebugContext()", asMETHODPR(CKContext, GetDebugContext, (), CKDebugContext *), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKContext", "void SetVirtoolsVersion(CK_VIRTOOLS_VERSION ver, CKDWORD build)", asMETHODPR(CKContext, SetVirtoolsVersion, (CK_VIRTOOLS_VERSION, CKDWORD), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "int GetPVInformation()", asMETHODPR(CKContext, GetPVInformation, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKContext", "bool IsInDynamicCreationMode()", asFUNCTIONPR([](CKContext *self) -> bool { return self->IsInDynamicCreationMode(); }, (CKContext *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
}
