#include "ScriptCKManagers.h"

#include <string>

#include "CKAll.h"

#include "ScriptUtils.h"
#include "ScriptNativePointer.h"

void RegisterCKPluginManager(asIScriptEngine *engine) {
    assert(engine != nullptr);

    int r = 0;

    r = engine->RegisterObjectMethod("CKPluginManager", "int ParsePlugins(const string &in directory)", asFUNCTIONPR([](CKPluginManager *self, const std::string &directory) { return self->ParsePlugins(const_cast<CKSTRING>(directory.c_str())); }, (CKPluginManager *, const std::string &), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPluginManager", "CKERROR RegisterPlugin(const string &in plugin)", asFUNCTIONPR([](CKPluginManager *self, const std::string &plugin) { return self->RegisterPlugin(const_cast<CKSTRING>(plugin.c_str())); }, (CKPluginManager *, const std::string &), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPluginManager", "CKPluginEntry &FindComponent(CKGUID component, int catIdx = -1)", asMETHODPR(CKPluginManager, FindComponent, (CKGUID, int), CKPluginEntry *), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKPluginManager", "int AddCategory(const string &in category)", asFUNCTIONPR([](CKPluginManager *self, const std::string &category) { return self->AddCategory(const_cast<CKSTRING>(category.c_str())); }, (CKPluginManager *, const std::string &), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPluginManager", "CKERROR RemoveCategory(int catIdx)", asMETHODPR(CKPluginManager, RemoveCategory, (int), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPluginManager", "int GetCategoryCount() const", asMETHODPR(CKPluginManager, GetCategoryCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPluginManager", "string GetCategoryName(int index)", asFUNCTIONPR([](CKPluginManager *self, int index) -> std::string { return self->GetCategoryName(index); }, (CKPluginManager *, int), std::string), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPluginManager", "int GetCategoryIndex(const string &in category)", asFUNCTIONPR([](CKPluginManager *self, const std::string &category) { return self->GetCategoryIndex(const_cast<CKSTRING>(category.c_str())); }, (CKPluginManager *, const std::string &), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPluginManager", "CKERROR RenameCategory(int catIdx, const string &in newName)", asFUNCTIONPR([](CKPluginManager *self, int catIdx, const std::string &newName) { return self->RenameCategory(catIdx, const_cast<CKSTRING>(newName.c_str())); }, (CKPluginManager *, int, const std::string &), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKPluginManager", "int GetPluginDllCount() const", asMETHODPR(CKPluginManager, GetPluginDllCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPluginManager", "CKPluginDll &GetPluginDllInfo(int idx)", asMETHODPR(CKPluginManager, GetPluginDllInfo, (int), CKPluginDll*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPluginManager", "CKPluginDll &GetPluginDllInfo(const string &in name, int &out idx = void)", asFUNCTIONPR([](CKPluginManager *self, const std::string &name, int *idx) { return self->GetPluginDllInfo(const_cast<CKSTRING>(name.c_str()), idx); }, (CKPluginManager *, const std::string &, int *), CKPluginDll *), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPluginManager", "CKERROR UnLoadPluginDll(int idx)", asMETHODPR(CKPluginManager, UnLoadPluginDll, (int), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPluginManager", "CKERROR ReLoadPluginDll(int idx)", asMETHODPR(CKPluginManager, ReLoadPluginDll, (int), CKERROR), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKPluginManager", "int GetPluginCount(int catIdx)", asMETHODPR(CKPluginManager, GetPluginCount, (int), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPluginManager", "CKPluginEntry &GetPluginInfo(int catIdx, int pluginIdx)", asMETHODPR(CKPluginManager, GetPluginInfo, (int, int), CKPluginEntry*), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKPluginManager", "bool SetReaderOptionData(CKContext@ context, NativePointer data, CKParameterOut @param, CKFileExtension ext, CKGUID &in guid = void)", asFUNCTIONPR([](CKPluginManager *self, CKContext *context, NativePointer data, CKParameterOut *param, CKFileExtension ext, const CKGUID &guid) -> bool { return self->SetReaderOptionData(context, data.Get(), param, ext, const_cast<CKGUID *>(&guid)); }, (CKPluginManager *, CKContext *, NativePointer, CKParameterOut *, CKFileExtension, const CKGUID &), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPluginManager", "CKParameterOut@ GetReaderOptionData(CKContext@ context, NativePointer data, CKFileExtension ext, CKGUID &in guid = void)", asFUNCTIONPR([](CKPluginManager *self, CKContext *context, NativePointer data, CKFileExtension ext, CKGUID *guid) { return self->GetReaderOptionData(context, data.Get(), ext, guid); }, (CKPluginManager *, CKContext *, NativePointer, CKFileExtension, CKGUID *), CKParameterOut *), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKPluginManager", "CKBitmapReader@ GetBitmapReader(CKFileExtension &in ext, CKGUID &in preferredGUID = void)",asMETHODPR(CKPluginManager, GetBitmapReader, (CKFileExtension &, CKGUID *), CKBitmapReader *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPluginManager", "CKSoundReader@ GetSoundReader(CKFileExtension &in ext, CKGUID &in preferredGUID = void)",asMETHODPR(CKPluginManager, GetSoundReader, (CKFileExtension &, CKGUID *), CKSoundReader *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPluginManager", "CKModelReader@ GetModelReader(CKFileExtension &in ext, CKGUID &in preferredGUID = void)",asMETHODPR(CKPluginManager, GetModelReader, (CKFileExtension &, CKGUID *), CKModelReader *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPluginManager", "CKMovieReader@ GetMovieReader(CKFileExtension &in ext, CKGUID &in preferredGUID = void)",asMETHODPR(CKPluginManager, GetMovieReader, (CKFileExtension &, CKGUID *), CKMovieReader *), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKPluginManager", "CKERROR Load(CKContext@ context, const string &in fileName, CKObjectArray@ objects, CK_LOAD_FLAGS loadFlags, CKCharacter@ carac = null, CKGUID &readerGuid = void)", asFUNCTIONPR([](CKPluginManager *self, CKContext *context, const std::string &fileName, CKObjectArray *objects, CK_LOAD_FLAGS loadFlags, CKCharacter *carac, CKGUID *readerGuid) { return self->Load(context, const_cast<CKSTRING>(fileName.c_str()), objects, loadFlags, carac, readerGuid); }, (CKPluginManager *, CKContext *, const std::string &, CKObjectArray *, CK_LOAD_FLAGS, CKCharacter *, CKGUID *), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPluginManager", "CKERROR Save(CKContext@ context, const string &in fileName, CKObjectArray@ objects, CK_LOAD_FLAGS saveFlags, CKGUID &readerGuid = void)", asFUNCTIONPR([](CKPluginManager *self, CKContext *context, const std::string &fileName, CKObjectArray *objects, CKDWORD saveFlags, CKGUID *readerGuid) { return self->Save(context, const_cast<CKSTRING>(fileName.c_str()), objects, saveFlags, readerGuid); }, (CKPluginManager *, CKContext *, const std::string &, CKObjectArray *, CKDWORD, CKGUID *), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // Not existing in Virtools 2.1
    // r = engine->RegisterObjectMethod("CKPluginManager", "void ReleaseAllPlugins()", asMETHODPR(CKPluginManager, ReleaseAllPlugins, (), void), asCALL_THISCALL); assert(r >= 0);
    // r = engine->RegisterObjectMethod("CKPluginManager", "void InitializePlugins(CKContext@ context)", asMETHODPR(CKPluginManager, InitializePlugins, (CKContext *), void), asCALL_THISCALL); assert(r >= 0);
    // r = engine->RegisterObjectMethod("CKPluginManager", "void ComputeDependenciesList(CKFile@ file)", asMETHODPR(CKPluginManager, ComputeDependenciesList, (CKFile *), void), asCALL_THISCALL); assert(r >= 0);
    // r = engine->RegisterObjectMethod("CKPluginManager", "void MarkComponentAsNeeded(CKGUID component, int catIdx)", asMETHODPR(CKPluginManager, MarkComponentAsNeeded, (CKGUID, int), void), asCALL_THISCALL); assert(r >= 0);
}

CKERROR CKBaseManager_SequenceAddedToScene(CKBaseManager *man, CKScene *scn, XObjectArray &array) {
    return man->SequenceAddedToScene(scn, array.Begin(), array.Size());
}

CKERROR CKBaseManager_SequenceRemovedFromScene(CKBaseManager *man, CKScene *scn, XObjectArray &array) {
    return man->SequenceRemovedFromScene(scn, array.Begin(), array.Size());
}

CKERROR CKBaseManager_SequenceToBeDeleted(CKBaseManager *man, XObjectArray &array) {
    return man->SequenceToBeDeleted(array.Begin(), array.Size());
}

CKERROR CKBaseManager_SequenceDeleted(CKBaseManager *man, XObjectArray &array) {
    return man->SequenceDeleted(array.Begin(), array.Size());
}

CKERROR CKBaseManager_CKDestroyObjects(CKBaseManager *man, XObjectArray &array, CKDWORD flags, CKDependencies *depoptions) {
    return man->CKDestroyObjects(array.Begin(), array.Size(), flags, depoptions);
}

template <typename T>
static void RegisterCKBaseManagerMembers(asIScriptEngine *engine, const char *name) {
    int r = 0;

    r = engine->RegisterObjectMethod(name, "CKGUID GetGuid() const", asMETHODPR(T, GetGuid, (), CKGUID), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "string GetName() const", asFUNCTIONPR([](T *self) -> std::string { return ScriptStringify(self->GetName()); }, (T *), std::string), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKStateChunk@ SaveData(CKFile@ savedFile) const", asMETHODPR(T, SaveData, (CKFile*), CKStateChunk*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKERROR LoadData(CKStateChunk@ chunk, CKFile@ loadedFile)", asMETHODPR(T, LoadData, (CKStateChunk*, CKFile*), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKERROR PreClearAll()", asMETHODPR(T, PreClearAll, (), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKERROR PostClearAll()", asMETHODPR(T, PostClearAll, (), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKERROR PreProcess()", asMETHODPR(T, PreProcess, (), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKERROR PostProcess()", asMETHODPR(T, PostProcess, (), CKERROR), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "CKERROR SequenceAddedToScene(CKScene@ scene, XObjectArray &in objects)", asFUNCTIONPR(CKBaseManager_SequenceAddedToScene, (CKBaseManager *, CKScene *, XObjectArray &), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKERROR SequenceRemovedFromScene(CKScene@ scene, XObjectArray &in objects)",asFUNCTIONPR(CKBaseManager_SequenceRemovedFromScene, (CKBaseManager *, CKScene *, XObjectArray &), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKERROR PreLaunchScene(CKScene@ oldScene, CKScene@ newScene)", asMETHODPR(T, PreLaunchScene, (CKScene*, CKScene*), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKERROR PostLaunchScene(CKScene@ oldScene, CKScene@ newScene)", asMETHODPR(T, PostLaunchScene, (CKScene*, CKScene*), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKERROR OnCKInit()", asMETHODPR(T, OnCKInit, (), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKERROR OnCKEnd()", asMETHODPR(T, OnCKEnd, (), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKERROR OnCKReset()", asMETHODPR(T, OnCKReset, (), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKERROR OnCKPostReset()", asMETHODPR(T, OnCKPostReset, (), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKERROR OnCKPause()", asMETHODPR(T, OnCKPause, (), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKERROR OnCKPlay()", asMETHODPR(T, OnCKPlay, (), CKERROR), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "CKERROR SequenceToBeDeleted(XObjectArray &in objects)", asFUNCTIONPR(CKBaseManager_SequenceToBeDeleted, (CKBaseManager *, XObjectArray &), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKERROR SequenceDeleted(XObjectArray &in objects)", asFUNCTIONPR(CKBaseManager_SequenceDeleted, (CKBaseManager *, XObjectArray &), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKERROR PreLoad()", asMETHODPR(T, PreLoad, (), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKERROR PostLoad()", asMETHODPR(T, PostLoad, (), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKERROR PreSave()", asMETHODPR(T, PreSave, (), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKERROR PostSave()", asMETHODPR(T, PostSave, (), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKERROR OnPreCopy(CKDependenciesContext &in context)", asMETHODPR(T, OnPreCopy, (CKDependenciesContext &), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKERROR OnPostCopy(CKDependenciesContext &in context)", asMETHODPR(T, OnPostCopy, (CKDependenciesContext &), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKERROR OnPreRender(CKRenderContext@ dev)", asMETHODPR(T, OnPreRender, (CKRenderContext *), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKERROR OnPostRender(CKRenderContext@ dev)", asMETHODPR(T, OnPostRender, (CKRenderContext *), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKERROR OnPostSpriteRender(CKRenderContext@ dev)", asMETHODPR(T, OnPostSpriteRender, (CKRenderContext *), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "int GetFunctionPriority(CKMANAGER_FUNCTIONS functions)", asMETHODPR(T, GetFunctionPriority, (CKMANAGER_FUNCTIONS), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKDWORD GetValidFunctionsMask() const", asMETHODPR(T, GetValidFunctionsMask, (), CKDWORD), asCALL_THISCALL); assert(r >= 0);

#if CKVERSION == 0x13022002
    r = engine->RegisterObjectMethod(name, "CKERROR CKDestroyObject(CKObject@ obj, CKDWORD flags = 0, CKDependencies &in depoptions = void)", asMETHODPR(T, CKDestroyObject, (CKObject *, CKDWORD, CKDependencies *), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKERROR CKDestroyObject(CK_ID id, CKDWORD flags = 0, CKDependencies &in depoptions = void)", asMETHODPR(T, CKDestroyObject, (CK_ID, CKDWORD, CKDependencies *), CKERROR), asCALL_THISCALL); assert(r >= 0);
#else
    r = engine->RegisterObjectMethod(name, "CKERROR CKDestroyObject(CKObject@ obj, CKDWORD flags = 0, CKDependencies &in depoptions = void)", asMETHODPR(T, CKDestroyObject, (CKObject*, DWORD, CKDependencies*), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKERROR CKDestroyObject(CK_ID id, CKDWORD flags = 0, CKDependencies &in depoptions = void)", asMETHODPR(T, CKDestroyObject, (CK_ID, DWORD, CKDependencies*), CKERROR), asCALL_THISCALL); assert(r >= 0);
#endif
    r = engine->RegisterObjectMethod(name, "CKERROR CKDestroyObjects(XObjectArray &in, CKDWORD flags = 0, CKDependencies &in depoptions = void)", asFUNCTIONPR(CKBaseManager_CKDestroyObjects, (CKBaseManager *, XObjectArray &, CKDWORD, CKDependencies *), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKObject@ CKGetObject(CK_ID id)", asMETHODPR(T, CKGetObject, (CK_ID), CKObject*), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void StartProfile()", asMETHODPR(T, StartProfile, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void StopProfile()", asMETHODPR(T, StopProfile, (), void), asCALL_THISCALL); assert(r >= 0);

    if (strcmp(name, "CKBaseManager") != 0) {
        RegisterClassRefCast<T, CKBaseManager>(engine, name, "CKBaseManager");
    }
}

void RegisterCKBaseManager(asIScriptEngine *engine) {
    assert(engine != nullptr);

    RegisterCKBaseManagerMembers<CKBaseManager>(engine, "CKBaseManager");
}

void RegisterCKParameterManager(asIScriptEngine *engine) {
    assert(engine != nullptr);

    int r = 0;

    RegisterCKBaseManagerMembers<CKParameterManager>(engine, "CKParameterManager");

    r = engine->RegisterObjectMethod("CKParameterManager", "CKERROR RegisterParameterType(CKParameterTypeDesc &in type)", asMETHODPR(CKParameterManager, RegisterParameterType, (CKParameterTypeDesc*), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKParameterManager", "CKERROR UnRegisterParameterType(CKGUID guid)", asMETHODPR(CKParameterManager, UnRegisterParameterType, (CKGUID), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKParameterManager", "CKParameterTypeDesc &GetParameterTypeDescription(int type)", asMETHODPR(CKParameterManager, GetParameterTypeDescription, (int), CKParameterTypeDesc*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKParameterManager", "CKParameterTypeDesc &GetParameterTypeDescription(CKGUID guid)", asMETHODPR(CKParameterManager, GetParameterTypeDescription, (CKGUID), CKParameterTypeDesc*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKParameterManager", "int GetParameterSize(CKParameterType type)", asMETHODPR(CKParameterManager, GetParameterSize, (CKParameterType), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKParameterManager", "int GetParameterTypesCount()", asMETHODPR(CKParameterManager, GetParameterTypesCount, (), int), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKParameterManager", "CKParameterType ParameterGuidToType(CKGUID guid)", asMETHODPR(CKParameterManager, ParameterGuidToType, (CKGUID), CKParameterType), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKParameterManager", "string ParameterGuidToName(CKGUID guid)", asFUNCTIONPR([](CKParameterManager *self, CKGUID guid) -> std::string { return ScriptStringify(self->ParameterGuidToName(guid)); }, (CKParameterManager *, CKGUID), std::string), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKParameterManager", "CKGUID ParameterTypeToGuid(CKParameterType type)", asMETHODPR(CKParameterManager, ParameterTypeToGuid, (CKParameterType), CKGUID), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKParameterManager", "string ParameterTypeToName(CKParameterType type)", asFUNCTIONPR([](CKParameterManager *self, CKParameterType type) -> std::string { return ScriptStringify(self->ParameterTypeToName(type)); }, (CKParameterManager *, CKParameterType), std::string), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKParameterManager", "CKGUID ParameterNameToGuid(const string &in name)", asFUNCTIONPR([](CKParameterManager *self, const std::string &name) { return self->ParameterNameToGuid(const_cast<CKSTRING>(name.c_str())); }, (CKParameterManager *, const std::string &), CKGUID), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKParameterManager", "CKParameterType ParameterNameToType(const string &in name)", asFUNCTIONPR([](CKParameterManager *self, const std::string &name) { return self->ParameterNameToType(const_cast<CKSTRING>(name.c_str())); }, (CKParameterManager *, const std::string &), CKParameterType), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKParameterManager", "bool IsDerivedFrom(CKGUID guid1, CKGUID guid2)", asFUNCTIONPR([](CKParameterManager *self, CKGUID guid1, CKGUID guid2) -> bool { return self->IsDerivedFrom(guid1, guid2); }, (CKParameterManager *, CKGUID, CKGUID), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKParameterManager", "bool IsDerivedFrom(CKParameterType child, CKParameterType parent)", asFUNCTIONPR([](CKParameterManager *self, CKParameterType child, CKParameterType parent) -> bool { return self->IsDerivedFrom(child, parent); }, (CKParameterManager *, CKParameterType, CKParameterType), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKParameterManager", "bool IsTypeCompatible(CKGUID guid1, CKGUID guid2)", asFUNCTIONPR([](CKParameterManager *self, CKGUID guid1, CKGUID guid2) -> bool { return self->IsTypeCompatible(guid1, guid2); }, (CKParameterManager *, CKGUID, CKGUID), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKParameterManager", "bool IsTypeCompatible(CKParameterType type1, CKParameterType type2)", asFUNCTIONPR([](CKParameterManager *self, CKParameterType type1, CKParameterType type2) -> bool { return self->IsTypeCompatible(type1, type2); }, (CKParameterManager *, CKParameterType, CKParameterType), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKParameterManager", "int TypeToClassID(CKParameterType type)", asMETHODPR(CKParameterManager, TypeToClassID, (CKParameterType), CK_CLASSID), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKParameterManager", "int GuidToClassID(CKGUID guid)", asMETHODPR(CKParameterManager, GuidToClassID, (CKGUID), CK_CLASSID), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKParameterManager", "CKParameterType ClassIDToType(CK_CLASSID cid)", asMETHODPR(CKParameterManager, ClassIDToType, (CK_CLASSID), CKParameterType), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKParameterManager", "CKGUID ClassIDToGuid(CK_CLASSID cid)", asMETHODPR(CKParameterManager, ClassIDToGuid, (CK_CLASSID), CKGUID), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKParameterManager", "CKERROR RegisterNewFlags(CKGUID flagsGuid, const string &in flagsName, const string &in flagsData)", asFUNCTIONPR([](CKParameterManager *self, CKGUID flagsGuid, const std::string &flagsName, const std::string &flagsData) { return self->RegisterNewFlags(flagsGuid, const_cast<CKSTRING>(flagsName.c_str()), const_cast<CKSTRING>(flagsData.c_str())); }, (CKParameterManager *, CKGUID, const std::string &, const std::string &), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKParameterManager", "CKERROR RegisterNewEnum(CKGUID enumGuid, const string &in enumName, const string &in enumData)", asFUNCTIONPR([](CKParameterManager *self, CKGUID enumGuid, const std::string &enumName, const std::string &enumData) { return self->RegisterNewEnum(enumGuid, const_cast<CKSTRING>(enumName.c_str()), const_cast<CKSTRING>(enumData.c_str())); }, (CKParameterManager *, CKGUID, const std::string &, const std::string &), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKParameterManager", "CKERROR ChangeEnumDeclaration(CKGUID enumGuid, const string &in enumData)", asFUNCTIONPR([](CKParameterManager *self, CKGUID enumGuid, const std::string &enumData) { return self->ChangeEnumDeclaration(enumGuid, const_cast<CKSTRING>(enumData.c_str())); }, (CKParameterManager *, CKGUID, const std::string &), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKParameterManager", "CKERROR ChangeFlagsDeclaration(CKGUID flagsGuid, const string &in flagsData)", asFUNCTIONPR([](CKParameterManager *self, CKGUID flagsGuid, const std::string &flagsData) { return self->ChangeFlagsDeclaration(flagsGuid, const_cast<CKSTRING>(flagsData.c_str())); }, (CKParameterManager *, CKGUID, const std::string &), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKParameterManager", "CKERROR RegisterNewStructure(const CKGUID &in, const string &in, const string &in, XGUIDArray &in)", asFUNCTIONPR([](CKParameterManager *self, const CKGUID &guid, const std::string &name, const std::string &desc, XArray<CKGUID> &array) { return self->RegisterNewStructure(guid, const_cast<CKSTRING>(name.c_str()), const_cast<CKSTRING>(desc.c_str()), array); }, (CKParameterManager *, const CKGUID &, const std::string &, const std::string &, XArray<CKGUID> &), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKParameterManager", "int GetNbFlagDefined()", asMETHODPR(CKParameterManager, GetNbFlagDefined, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKParameterManager", "int GetNbEnumDefined()", asMETHODPR(CKParameterManager, GetNbEnumDefined, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKParameterManager", "int GetNbStructDefined()", asMETHODPR(CKParameterManager, GetNbStructDefined, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKParameterManager", "CKFlagsStruct &GetFlagsDescByType(CKParameterType type)", asMETHODPR(CKParameterManager, GetFlagsDescByType, (CKParameterType), CKFlagsStruct*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKParameterManager", "CKEnumStruct &GetEnumDescByType(CKParameterType type)", asMETHODPR(CKParameterManager, GetEnumDescByType, (CKParameterType), CKEnumStruct*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKParameterManager", "CKStructStruct &GetStructDescByType(CKParameterType type)", asMETHODPR(CKParameterManager, GetStructDescByType, (CKParameterType), CKStructStruct*), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKParameterManager", "CKOperationType RegisterOperationType(CKGUID opGuid, const string &in name)", asFUNCTIONPR([](CKParameterManager *self, CKGUID opGuid, const std::string &name) { return self->RegisterOperationType(opGuid, const_cast<CKSTRING>(name.c_str())); }, (CKParameterManager *, CKGUID, const std::string &), CKOperationType), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKParameterManager", "CKERROR UnRegisterOperationType(CKGUID opGuid)", asMETHODPR(CKParameterManager, UnRegisterOperationType, (CKGUID), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKParameterManager", "CKERROR UnRegisterOperationType(CKOperationType opCode)", asMETHODPR(CKParameterManager, UnRegisterOperationType, (CKOperationType), CKERROR), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKParameterManager", "CKERROR RegisterOperationFunction(const CKGUID &in operation, const CKGUID &in paramRes, const CKGUID &in param1, const CKGUID &in param2, NativePointer op)", asFUNCTIONPR([](CKParameterManager* self, const CKGUID &operation, const CKGUID &paramRes, const CKGUID &param1, const CKGUID &param2, NativePointer op) { return self->RegisterOperationFunction(const_cast<CKGUID&>(operation), const_cast<CKGUID&>(paramRes), const_cast<CKGUID&>(param1), const_cast<CKGUID&>(param2), reinterpret_cast<CK_PARAMETEROPERATION>(op.Get())); }, (CKParameterManager *, const CKGUID &, const CKGUID &, const CKGUID &, const CKGUID &, NativePointer), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKParameterManager", "NativePointer GetOperationFunction(const CKGUID &in operation, const CKGUID &in paramRes, const CKGUID &in param1, const CKGUID &in param2)", asFUNCTIONPR([](CKParameterManager* self, const CKGUID &operation, const CKGUID &paramRes, const CKGUID &param1, const CKGUID &param2) { return NativePointer(self->GetOperationFunction(const_cast<CKGUID&>(operation), const_cast<CKGUID&>(paramRes), const_cast<CKGUID&>(param1), const_cast<CKGUID&>(param2))); }, (CKParameterManager *, const CKGUID &, const CKGUID &, const CKGUID &, const CKGUID &), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKParameterManager", "CKERROR UnRegisterOperationFunction(const CKGUID &in operation, const CKGUID &in paramRes, const CKGUID &in param1, const CKGUID &in param2)", asMETHODPR(CKParameterManager, UnRegisterOperationFunction, (CKGUID &, CKGUID &, CKGUID &, CKGUID &), CKERROR), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKParameterManager", "CKGUID OperationCodeToGuid(CKOperationType type)", asMETHODPR(CKParameterManager, OperationCodeToGuid, (CKOperationType), CKGUID), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKParameterManager", "string OperationCodeToName(CKOperationType type)", asFUNCTIONPR([](CKParameterManager *self, CKOperationType type) -> std::string { return ScriptStringify(self->OperationCodeToName(type)); }, (CKParameterManager *, CKOperationType), std::string), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKParameterManager", "int OperationGuidToCode(CKGUID guid)", asMETHODPR(CKParameterManager, OperationGuidToCode, (CKGUIDCONSTREF), CKOperationType), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKParameterManager", "string OperationGuidToName(CKGUID guid)", asFUNCTIONPR([](CKParameterManager *self, CKGUID guid) -> std::string { return ScriptStringify(self->OperationGuidToName(guid)); }, (CKParameterManager *, CKGUID), std::string), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKParameterManager", "CKGUID OperationNameToGuid(const string &in name)", asFUNCTIONPR([](CKParameterManager *self, const std::string &name) { return self->OperationNameToGuid(const_cast<CKSTRING>(name.c_str())); }, (CKParameterManager *, const std::string &), CKGUID), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKParameterManager", "int OperationNameToCode(const string &in name)", asFUNCTIONPR([](CKParameterManager *self, const std::string &name) { return self->OperationNameToCode(const_cast<CKSTRING>(name.c_str())); }, (CKParameterManager *, const std::string &), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKParameterManager", "int GetAvailableOperationsDesc(const CKGUID &in opGuid, CKParameterOut@ res, CKParameterIn@ p1, CKParameterIn@ p2, CKOperationDesc &out list)", asMETHODPR(CKParameterManager, GetAvailableOperationsDesc, (const CKGUID&, CKParameterOut *, CKParameterIn*, CKParameterIn*, CKOperationDesc*), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKParameterManager", "int GetParameterOperationCount()", asMETHODPR(CKParameterManager, GetParameterOperationCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKParameterManager", "bool IsParameterTypeToBeShown(CKParameterType type)", asFUNCTIONPR([](CKParameterManager *self, CKParameterType type) -> bool { return self->IsParameterTypeToBeShown(type); }, (CKParameterManager *, CKParameterType), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKParameterManager", "bool IsParameterTypeToBeShown(CKGUID guid)", asFUNCTIONPR([](CKParameterManager *self, CKGUID guid) -> bool { return self->IsParameterTypeToBeShown(guid); }, (CKParameterManager *, CKGUID), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // Not existing in Virtools 2.1
    // r = engine->RegisterObjectMethod("CKParameterManager", "void UpdateParameterEnum()", asMETHODPR(CKParameterManager, UpdateParameterEnum, (), void), asCALL_THISCALL); assert(r >= 0);
    // r = engine->RegisterObjectProperty("CKParameterManager", "bool m_ParameterTypeEnumUpToDate", asOFFSET(CKParameterManager, m_ParameterTypeEnumUpToDate)); assert(r >= 0);
}

void RegisterCKAttributeManager(asIScriptEngine *engine) {
    assert(engine != nullptr);

    int r = 0;

    RegisterCKBaseManagerMembers<CKAttributeManager>(engine, "CKAttributeManager");

    r = engine->RegisterObjectMethod("CKAttributeManager", "CKAttributeType RegisterNewAttributeType(const string &in name, CKGUID type, CK_CLASSID compatibleCid = CKCID_BEOBJECT, CK_ATTRIBUT_FLAGS flags = CK_ATTRIBUT_SYSTEM)", asFUNCTIONPR([](CKAttributeManager *self, const std::string &name, CKGUID type, CK_CLASSID compatibleCid, CK_ATTRIBUT_FLAGS flags) { return self->RegisterNewAttributeType(const_cast<CKSTRING>(name.c_str()), type, compatibleCid, flags); }, (CKAttributeManager *, const std::string &, CKGUID, CK_CLASSID, CK_ATTRIBUT_FLAGS), CKAttributeType), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKAttributeManager", "void UnRegisterAttribute(CKAttributeType attribType)", asMETHODPR(CKAttributeManager, UnRegisterAttribute, (CKAttributeType), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKAttributeManager", "void UnRegisterAttribute(const string &in name)", asFUNCTIONPR([](CKAttributeManager *self, const std::string &name) { self->UnRegisterAttribute(const_cast<CKSTRING>(name.c_str())); }, (CKAttributeManager *, const std::string &), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKAttributeManager", "string GetAttributeNameByType(CKAttributeType attribType)", asFUNCTIONPR([](CKAttributeManager *self, CKAttributeType attribType) -> std::string { return ScriptStringify(self->GetAttributeNameByType(attribType)); }, (CKAttributeManager *, CKAttributeType), std::string), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKAttributeManager", "int GetAttributeTypeByName(const string &in attribName)", asFUNCTIONPR([](CKAttributeManager *self, const std::string &attribName) { return self->GetAttributeTypeByName(const_cast<CKSTRING>(attribName.c_str())); }, (CKAttributeManager *, const std::string &), CKAttributeType), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKAttributeManager", "void SetAttributeNameByType(CKAttributeType attribType, const string &in name)", asFUNCTIONPR([](CKAttributeManager *self, CKAttributeType attribType, const std::string &name) { self->SetAttributeNameByType(attribType, const_cast<CKSTRING>(name.c_str())); }, (CKAttributeManager *, CKAttributeType, const std::string &), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKAttributeManager", "int GetAttributeCount()", asMETHODPR(CKAttributeManager, GetAttributeCount, (), int), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKAttributeManager", "CKGUID GetAttributeParameterGUID(CKAttributeType attribType)", asMETHODPR(CKAttributeManager, GetAttributeParameterGUID, (CKAttributeType), CKGUID), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKAttributeManager", "CKParameterType GetAttributeParameterType(CKAttributeType attribType)", asMETHODPR(CKAttributeManager, GetAttributeParameterType, (CKAttributeType), CKParameterType), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKAttributeManager", "CK_CLASSID GetAttributeCompatibleClassId(CKAttributeType attribType)", asMETHODPR(CKAttributeManager, GetAttributeCompatibleClassId, (CKAttributeType), CK_CLASSID), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKAttributeManager", "bool IsAttributeIndexValid(CKAttributeType index)", asFUNCTIONPR([](CKAttributeManager *self, CKAttributeType index) -> bool { return self->IsAttributeIndexValid(index); }, (CKAttributeManager *, CKAttributeType), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKAttributeManager", "bool IsCategoryIndexValid(CKAttributeCategory index)", asFUNCTIONPR([](CKAttributeManager *self, CKAttributeCategory index) -> bool { return self->IsCategoryIndexValid(index); }, (CKAttributeManager *, CKAttributeCategory), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKAttributeManager", "CK_ATTRIBUT_FLAGS GetAttributeFlags(CKAttributeType attribType)", asMETHODPR(CKAttributeManager, GetAttributeFlags, (CKAttributeType), CK_ATTRIBUT_FLAGS), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKAttributeManager", "void SetAttributeCallbackFunction(CKAttributeType attribType, NativePointer callback, NativePointer arg)", asFUNCTIONPR([](CKAttributeManager* self, CKAttributeType attribType, NativePointer callback, NativePointer arg) { self->SetAttributeCallbackFunction(attribType, reinterpret_cast<CKATTRIBUTECALLBACK>(callback.Get()), arg.Get()); }, (CKAttributeManager *, CKAttributeType, NativePointer, NativePointer), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKAttributeManager", "void SetAttributeDefaultValue(CKAttributeType attribType, const string &in defaultVal)", asFUNCTIONPR([](CKAttributeManager *self, CKAttributeType attribType, const std::string &defaultVal) { self->SetAttributeDefaultValue(attribType, const_cast<CKSTRING>(defaultVal.c_str())); }, (CKAttributeManager *, CKAttributeType, const std::string &), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKAttributeManager", "string GetAttributeDefaultValue(CKAttributeType attribType)", asFUNCTIONPR([](CKAttributeManager *self, CKAttributeType attribType) -> std::string { return ScriptStringify(self->GetAttributeDefaultValue(attribType)); }, (CKAttributeManager *, CKAttributeType), std::string), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKAttributeManager", "const XObjectPointerArray &GetAttributeListPtr(CKAttributeType attribType)", asMETHODPR(CKAttributeManager, GetAttributeListPtr, (CKAttributeType), const XObjectPointerArray &), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKAttributeManager", "const XObjectPointerArray &GetGlobalAttributeListPtr(CKAttributeType attribType)", asMETHODPR(CKAttributeManager, GetGlobalAttributeListPtr, (CKAttributeType), const XObjectPointerArray &), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKAttributeManager", "const XObjectPointerArray &FillListByAttributes(NativePointer attribList, int count)", asFUNCTIONPR([](CKAttributeManager *self, NativePointer attribList, int count) -> const XObjectPointerArray & { return self->FillListByAttributes(reinterpret_cast<CKAttributeType *>(attribList.Get()), count); }, (CKAttributeManager *, NativePointer, int), const XObjectPointerArray &), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKAttributeManager", "const XObjectPointerArray &FillListByGlobalAttributes(NativePointer attribList, int count)", asFUNCTIONPR([](CKAttributeManager *self, NativePointer attribList, int count) -> const XObjectPointerArray & { return self->FillListByGlobalAttributes(reinterpret_cast<CKAttributeType *>(attribList.Get()), count); }, (CKAttributeManager  *, NativePointer, int), const XObjectPointerArray &), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKAttributeManager", "int GetCategoriesCount()", asMETHODPR(CKAttributeManager, GetCategoriesCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKAttributeManager", "string GetCategoryName(CKAttributeCategory index)", asFUNCTIONPR([](CKAttributeManager *self, CKAttributeCategory index) -> std::string { return ScriptStringify(self->GetCategoryName(index)); }, (CKAttributeManager *, CKAttributeCategory), std::string), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKAttributeManager", "CKAttributeCategory GetCategoryByName(const string &in name)", asFUNCTIONPR([](CKAttributeManager *self, const std::string &name) { return self->GetCategoryByName(const_cast<CKSTRING>(name.c_str())); }, (CKAttributeManager *, const std::string &), CKAttributeCategory), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKAttributeManager", "void SetCategoryName(CKAttributeCategory catType, const string &in name)", asFUNCTIONPR([](CKAttributeManager *self, CKAttributeCategory catType, const std::string &name) { self->SetCategoryName(catType, const_cast<CKSTRING>(name.c_str())); }, (CKAttributeManager *, CKAttributeCategory, const std::string &), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKAttributeManager", "CKAttributeCategory AddCategory(const string &in category, CKDWORD flags = 0)", asFUNCTIONPR([](CKAttributeManager *self, const std::string &category, CKDWORD flags) { return self->AddCategory(const_cast<CKSTRING>(category.c_str()), flags); }, (CKAttributeManager *, const std::string &, CKDWORD), CKAttributeCategory), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKAttributeManager", "void RemoveCategory(const string &in category)", asFUNCTIONPR([](CKAttributeManager *self, const std::string &category) { self->RemoveCategory(const_cast<CKSTRING>(category.c_str())); }, (CKAttributeManager *, const std::string &), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKAttributeManager", "CKDWORD GetCategoryFlags(CKAttributeCategory cat)", asMETHODPR(CKAttributeManager, GetCategoryFlags, (CKAttributeCategory), CKDWORD), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKAttributeManager", "CKDWORD GetCategoryFlags(const string &in cat)", asFUNCTIONPR([](CKAttributeManager *self, const std::string &cat) { return self->GetCategoryFlags(const_cast<CKSTRING>(cat.c_str())); }, (CKAttributeManager *, const std::string &), CKDWORD), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKAttributeManager", "void SetAttributeCategory(CKAttributeType attribType, const string &in category)", asFUNCTIONPR([](CKAttributeManager *self, CKAttributeType attribType, const std::string &category) { self->SetAttributeCategory(attribType, const_cast<CKSTRING>(category.c_str())); }, (CKAttributeManager *, CKAttributeType, const std::string &), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKAttributeManager", "string GetAttributeCategory(CKAttributeType attribType)", asFUNCTIONPR([](CKAttributeManager *self, CKAttributeType attribType) -> std::string { return ScriptStringify(self->GetAttributeCategory(attribType)); }, (CKAttributeManager *, CKAttributeType), std::string), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKAttributeManager", "CKAttributeCategory GetAttributeCategoryIndex(CKAttributeType attribType)", asMETHODPR(CKAttributeManager, GetAttributeCategoryIndex, (CKAttributeType), CKAttributeCategory), asCALL_THISCALL); assert(r >= 0);
}

void RegisterCKTimeManager(asIScriptEngine *engine) {
    assert(engine != nullptr);

    int r = 0;

    RegisterCKBaseManagerMembers<CKTimeManager>(engine, "CKTimeManager");

    r = engine->RegisterObjectMethod("CKTimeManager", "CKDWORD GetMainTickCount()", asMETHOD(CKTimeManager, GetMainTickCount), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKTimeManager", "float GetTime()", asMETHOD(CKTimeManager, GetTime), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKTimeManager", "float GetLastDeltaTime()", asMETHOD(CKTimeManager, GetLastDeltaTime), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKTimeManager", "float GetLastDeltaTimeFree()", asMETHOD(CKTimeManager, GetLastDeltaTimeFree), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKTimeManager", "float GetAbsoluteTime()", asMETHOD(CKTimeManager, GetAbsoluteTime), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKTimeManager", "void SetTimeScaleFactor(float factor)", asMETHOD(CKTimeManager, SetTimeScaleFactor), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKTimeManager", "float GetTimeScaleFactor()", asMETHOD(CKTimeManager, GetTimeScaleFactor), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKTimeManager", "CKDWORD GetLimitOptions()", asMETHOD(CKTimeManager, GetLimitOptions), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKTimeManager", "float GetFrameRateLimit()", asMETHOD(CKTimeManager, GetFrameRateLimit), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKTimeManager", "float GetBehavioralRateLimit()", asMETHOD(CKTimeManager, GetBehavioralRateLimit), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKTimeManager", "float GetMinimumDeltaTime()", asMETHOD(CKTimeManager, GetMinimumDeltaTime), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKTimeManager", "float GetMaximumDeltaTime()", asMETHOD(CKTimeManager, GetMaximumDeltaTime), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKTimeManager", "void ChangeLimitOptions(CK_FRAMERATE_LIMITS fpsOptions, CK_FRAMERATE_LIMITS behOptions = CK_RATE_NOP)", asMETHOD(CKTimeManager, ChangeLimitOptions), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKTimeManager", "void SetFrameRateLimit(float limit)", asMETHOD(CKTimeManager, SetFrameRateLimit), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKTimeManager", "void SetBehavioralRateLimit(float limit)", asMETHOD(CKTimeManager, SetBehavioralRateLimit), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKTimeManager", "void SetMinimumDeltaTime(float limit)", asMETHOD(CKTimeManager, SetMinimumDeltaTime), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKTimeManager", "void SetMaximumDeltaTime(float limit)", asMETHOD(CKTimeManager, SetMaximumDeltaTime), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKTimeManager", "void SetLastDeltaTime(float delta)", asMETHOD(CKTimeManager, SetLastDeltaTime), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKTimeManager", "void GetTimeToWaitForLimits(float &out timeBeforeRender, float &out timeBeforeBeh)", asMETHOD(CKTimeManager, GetTimeToWaitForLimits), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKTimeManager", "void ResetChronos(bool renderChrono, bool behavioralChrono)", asFUNCTIONPR([](CKTimeManager *self, bool renderChrono, bool behavioralChrono) { self->ResetChronos(renderChrono, behavioralChrono); }, (CKTimeManager *, bool, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
}

void RegisterCKMessageManager(asIScriptEngine *engine) {
    assert(engine != nullptr);

    int r = 0;

    RegisterCKBaseManagerMembers<CKMessageManager>(engine, "CKMessageManager");

    r = engine->RegisterObjectMethod("CKMessageManager", "CKMessageType AddMessageType(const string &in msgName)", asFUNCTIONPR([](CKMessageManager *self, const std::string &msgName) { return self->AddMessageType(const_cast<CKSTRING>(msgName.c_str())); }, (CKMessageManager *, const std::string &), CKMessageType), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMessageManager", "string GetMessageTypeName(CKMessageType msgType)", asFUNCTIONPR([](CKMessageManager *self, CKMessageType msgType) -> std::string { return ScriptStringify(self->GetMessageTypeName(msgType)); }, (CKMessageManager *, CKMessageType), std::string), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMessageManager", "int GetMessageTypeCount()", asMETHODPR(CKMessageManager, GetMessageTypeCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMessageManager", "void RenameMessageType(CKMessageType msgType, const string &in newName)", asFUNCTIONPR([](CKMessageManager *self, CKMessageType msgType, const std::string &newName) { self->RenameMessageType(msgType, const_cast<CKSTRING>(newName.c_str())); }, (CKMessageManager *, CKMessageType, const std::string &), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMessageManager", "void RenameMessageType(const string &in oldName, const string &in newName)", asFUNCTIONPR([](CKMessageManager *self, const std::string &oldName, const std::string &newName) { self->RenameMessageType(const_cast<CKSTRING>(oldName.c_str()), const_cast<CKSTRING>(newName.c_str())); }, (CKMessageManager *, const std::string &, const std::string &), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

#if CKVERSION == 0x13022002
    r = engine->RegisterObjectMethod("CKMessageManager", "CKERROR SendMessage(CKMessage@ msg)", asMETHODPR(CKMessageManager, SendMessage, (CKMessage*), CKERROR), asCALL_THISCALL); assert(r >= 0);
#endif
    r = engine->RegisterObjectMethod("CKMessageManager", "CKMessage@ SendMessageSingle(int MsgType, CKBeObject@ dest, CKBeObject@ sender = null)", asMETHODPR(CKMessageManager, SendMessageSingle, (CKMessageType, CKBeObject *, CKBeObject *), CKMessage *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMessageManager", "CKMessage@ SendMessageGroup(int MsgType, CKGroup@ group, CKBeObject@ sender = null)", asMETHODPR(CKMessageManager, SendMessageGroup, (CKMessageType, CKGroup *, CKBeObject *), CKMessage*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMessageManager", "CKMessage@ SendMessageBroadcast(int MsgType, int id = 0, CKBeObject@ sender = null)", asMETHODPR(CKMessageManager, SendMessageBroadcast, (CKMessageType, CK_CLASSID, CKBeObject *), CKMessage *), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKMessageManager", "CKERROR RegisterWait(CKMessageType msgType, CKBehavior@ beh, int outputToActivate, CKBeObject@ obj)", asMETHODPR(CKMessageManager, RegisterWait, (CKMessageType, CKBehavior *, int, CKBeObject *), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMessageManager", "CKERROR RegisterWait(const string &in msgName, CKBehavior@ beh, int OutputToActivate, CKBeObject@ obj)", asFUNCTIONPR([](CKMessageManager *self, const std::string &msgName, CKBehavior *behav, int OutputToActivate, CKBeObject *obj) { return self->RegisterWait(const_cast<CKSTRING>(msgName.c_str()), behav, OutputToActivate, obj); }, (CKMessageManager *, const std::string &, CKBehavior *, int, CKBeObject *), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMessageManager", "CKERROR UnRegisterWait(CKMessageType msgType, CKBehavior@ beh, int OutputToActivate)", asMETHODPR(CKMessageManager, UnRegisterWait, (CKMessageType, CKBehavior *, int), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMessageManager", "CKERROR UnRegisterWait(const string &in msgName, CKBehavior@ beh, int OutputToActivate)", asFUNCTIONPR([](CKMessageManager *self, const std::string &msgName, CKBehavior *behav, int OutputToActivate) { return self->UnRegisterWait(const_cast<CKSTRING>(msgName.c_str()), behav, OutputToActivate); }, (CKMessageManager *, const std::string &, CKBehavior *, int), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMessageManager", "CKERROR RegisterDefaultMessages()", asMETHODPR(CKMessageManager, RegisterDefaultMessages, (), CKERROR), asCALL_THISCALL); assert(r >= 0);
}

void RegisterCKBehaviorManager(asIScriptEngine *engine) {
    assert(engine != nullptr);

    int r = 0;

    RegisterCKBaseManagerMembers<CKBehaviorManager>(engine, "CKBehaviorManager");

    r = engine->RegisterObjectMethod("CKBehaviorManager", "CKERROR Execute(float delta)", asMETHODPR(CKBehaviorManager, Execute, (float), CKERROR), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKBehaviorManager", "int GetObjectsCount()", asMETHODPR(CKBehaviorManager, GetObjectsCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehaviorManager", "CKBeObject@ GetObject(int pos)", asMETHODPR(CKBehaviorManager, GetObject, (int), CKBeObject *), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKBehaviorManager", "int GetBehaviorMaxIteration()", asMETHODPR(CKBehaviorManager, GetBehaviorMaxIteration, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehaviorManager", "void SetBehaviorMaxIteration(int n)", asMETHODPR(CKBehaviorManager, SetBehaviorMaxIteration, (int), void), asCALL_THISCALL); assert(r >= 0);
}

void RegisterCKPathManager(asIScriptEngine *engine) {
    assert(engine != nullptr);

    int r = 0;

    RegisterCKBaseManagerMembers<CKPathManager>(engine, "CKPathManager");

    r = engine->RegisterObjectMethod("CKPathManager", "int AddCategory(XString &in cat)", asMETHODPR(CKPathManager, AddCategory, (XString &), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPathManager", "CKERROR RemoveCategory(int catIdx)", asMETHODPR(CKPathManager, RemoveCategory, (int), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPathManager", "int GetCategoryCount()", asMETHODPR(CKPathManager, GetCategoryCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPathManager", "CKERROR GetCategoryName(int catIdx, XString &out catName)", asMETHODPR(CKPathManager, GetCategoryName, (int, XString &), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPathManager", "int GetCategoryIndex(XString &in cat)", asMETHODPR(CKPathManager, GetCategoryIndex, (XString &), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPathManager", "CKERROR RenameCategory(int catIdx, const XString &in newName)", asMETHODPR(CKPathManager, RenameCategory, (int, XString &), CKERROR), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKPathManager", "int AddPath(int catIdx, XString &in path)", asMETHODPR(CKPathManager, AddPath, (int, XString &), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPathManager", "CKERROR RemovePath(int catIdx, int pathIdx)", asMETHODPR(CKPathManager, RemovePath, (int, int), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPathManager", "CKERROR SwapPaths(int catIdx, int pathIdx1, int pathIdx2)", asMETHODPR(CKPathManager, SwapPaths, (int, int, int), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPathManager", "int GetPathCount(int catIdx)", asMETHODPR(CKPathManager, GetPathCount, (int), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPathManager", "CKERROR GetPathName(int catIdx, int pathIdx, XString &out path)", asMETHODPR(CKPathManager, GetPathName, (int, int, XString &), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPathManager", "int GetPathIndex(int catIdx, const XString &in path)", asMETHODPR(CKPathManager, GetPathIndex, (int, XString &), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPathManager", "CKERROR RenamePath(int catIdx, int pathIdx, const XString &in path)", asMETHODPR(CKPathManager, RenamePath, (int, int, XString &), CKERROR), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKPathManager", "CKERROR ResolveFileName(const XString &in file, XString &out resolvedFile, int catIdx, int startIdx = -1)", asFUNCTIONPR([](CKPathManager *self, XString &file, XString &resolvedFile, int catIdx, int startIdx) { resolvedFile = file; return self->ResolveFileName(resolvedFile, catIdx, startIdx); }, (CKPathManager *, XString &, XString &, int, int), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKPathManager", "bool PathIsAbsolute(const XString &in file)", asFUNCTIONPR([](CKPathManager *self, XString &file) -> bool { return self->PathIsAbsolute(file); }, (CKPathManager *, XString &), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPathManager", "bool PathIsUNC(const XString &in file)", asFUNCTIONPR([](CKPathManager *self, XString &file) -> bool { return self->PathIsUNC(file); }, (CKPathManager *, XString &), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPathManager", "bool PathIsURL(const XString &in file)", asFUNCTIONPR([](CKPathManager *self, XString &file) -> bool { return self->PathIsURL(file); }, (CKPathManager *, XString &), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPathManager", "bool PathIsFile(const XString &in file)", asFUNCTIONPR([](CKPathManager *self, XString &file) -> bool { return self->PathIsFile(file); }, (CKPathManager *, XString &), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKPathManager", "void RemoveEscapedSpace(const XString &in input, XString &out output)", asFUNCTIONPR([](CKPathManager *self, XString &input, XString &output) { output = input; self->RemoveEscapedSpace(output.Str()); }, (CKPathManager *, XString &, XString &), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPathManager", "void AddEscapedSpace(const XString &in input, XString &out output)", asFUNCTIONPR([](CKPathManager *self, XString &input, XString &output) { output = input; self->AddEscapedSpace(output); }, (CKPathManager *, XString &, XString &), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKPathManager", "XString GetVirtoolsTemporaryFolder()", asMETHODPR(CKPathManager, GetVirtoolsTemporaryFolder, (), XString), asCALL_THISCALL); assert(r >= 0);
}

void RegisterCKRenderManager(asIScriptEngine *engine) {
    assert(engine != nullptr);

    int r = 0;

    RegisterCKBaseManagerMembers<CKRenderManager>(engine, "CKRenderManager");

    r = engine->RegisterObjectMethod("CKRenderManager", "int GetRenderDriverCount()", asMETHODPR(CKRenderManager, GetRenderDriverCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderManager", "VxDriverDesc &GetRenderDriverDescription(int driver)", asMETHODPR(CKRenderManager, GetRenderDriverDescription, (int), VxDriverDesc *), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKRenderManager", "void GetDesiredTexturesVideoFormat(VxImageDescEx &out format)", asMETHODPR(CKRenderManager, GetDesiredTexturesVideoFormat, (VxImageDescEx &), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderManager", "void SetDesiredTexturesVideoFormat(VxImageDescEx &in format)", asMETHODPR(CKRenderManager, SetDesiredTexturesVideoFormat, (VxImageDescEx &), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKRenderManager", "CKRenderContext@ GetRenderContext(int pos)", asMETHODPR(CKRenderManager, GetRenderContext, (int), CKRenderContext *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderManager", "CKRenderContext@ GetRenderContextFromPoint(CKPOINT &in pt)", asMETHODPR(CKRenderManager, GetRenderContextFromPoint, (CKPOINT &), CKRenderContext *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderManager", "int GetRenderContextCount()", asMETHODPR(CKRenderManager, GetRenderContextCount, (), int), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKRenderManager", "void Process()", asMETHODPR(CKRenderManager, Process, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderManager", "void FlushTextures()", asMETHODPR(CKRenderManager, FlushTextures, (), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKRenderManager", "CKRenderContext@ CreateRenderContext(WIN_HANDLE window, int driver = 0, CKRECT &in rect = void, bool fullscreen = false, int bpp = -1, int zbpp = -1, int stencilBpp = -1, int refreshRate = 0)", asFUNCTIONPR([](CKRenderManager *self, void *window, int driver, CKRECT &rect, bool fullscreen, int bpp, int zbpp, int stencilBpp, int refreshRate) { return self->CreateRenderContext(window, driver, &rect, fullscreen, bpp, zbpp, stencilBpp, refreshRate); }, (CKRenderManager *, void *, int, CKRECT &, bool, int, int, int, int), CKRenderContext *), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderManager", "CKERROR DestroyRenderContext(CKRenderContext@ context)", asMETHODPR(CKRenderManager, DestroyRenderContext, (CKRenderContext *), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderManager", "void RemoveRenderContext(CKRenderContext@ context)", asMETHODPR(CKRenderManager, RemoveRenderContext, (CKRenderContext *), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKRenderManager", "CKVertexBuffer@ CreateVertexBuffer()", asMETHODPR(CKRenderManager, CreateVertexBuffer, (), CKVertexBuffer *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderManager", "void DestroyVertexBuffer(CKVertexBuffer@ vb)", asMETHODPR(CKRenderManager, DestroyVertexBuffer, (CKVertexBuffer *), void), asCALL_THISCALL); assert(r >= 0);

#if CKVERSION == 0x13022002
    r = engine->RegisterObjectMethod("CKRenderManager", "void SetRenderOptions(const string &in option, CKDWORD value)", asFUNCTIONPR([](CKRenderManager *self, const std::string &option, CKDWORD value) { self->SetRenderOptions(const_cast<CKSTRING>(option.c_str()), value); }, (CKRenderManager *, const std::string &, CKDWORD), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
#endif

    r = engine->RegisterObjectMethod("CKRenderManager", "const VxEffectDescription &GetEffectDescription(int effectIndex)", asMETHODPR(CKRenderManager, GetEffectDescription, (int), const VxEffectDescription &), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderManager", "int GetEffectCount()", asMETHODPR(CKRenderManager, GetEffectCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderManager", "int AddEffect(const VxEffectDescription &in effect)", asMETHODPR(CKRenderManager, AddEffect, (const VxEffectDescription &), int), asCALL_THISCALL); assert(r >= 0);
}

void RegisterCKFloorManager(asIScriptEngine *engine) {
    assert(engine != nullptr);

    int r = 0;

    RegisterCKBaseManagerMembers<CKFloorManager>(engine, "CKFloorManager");

    r = engine->RegisterObjectMethod("CKFloorManager", "CK_FLOORNEAREST GetNearestFloors(const VxVector &in position, CKFloorPoint &out floorPoint, CK3dEntity@ excludeFloor = null)", asMETHODPR(CKFloorManager, GetNearestFloors, (const VxVector&, CKFloorPoint*, CK3dEntity *), CK_FLOORNEAREST), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKFloorManager", "CK_FLOORNEAREST GetNearestFloor(const VxVector &in position, CK3dEntity@ &out floor, int &out faceIndex, VxVector &out normal = void, float &out distance = void, CK3dEntity@ excludeFloor = null)", asMETHODPR(CKFloorManager, GetNearestFloor, (const VxVector&, CK3dEntity **, int*, VxVector*, float*, CK3dEntity *), CK_FLOORNEAREST), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKFloorManager", "void SetLimitAngle(float angle)", asMETHODPR(CKFloorManager, SetLimitAngle, (float), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKFloorManager", "float GetLimitAngle()", asMETHODPR(CKFloorManager, GetLimitAngle, (), float), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKFloorManager", "int AddFloorObjectsByName(CKLevel@ level, const string &in floorName, CK_FLOORGEOMETRY geo, bool moving, int type, bool hiera, bool first)", asFUNCTIONPR([](CKFloorManager *self, CKLevel *level, const std::string &floorName, CK_FLOORGEOMETRY geo, bool moving, int type, bool hiera, bool first) -> int { return self->AddFloorObjectsByName(level, const_cast<CKSTRING>(floorName.c_str()), geo, moving, type, hiera, first); }, (CKFloorManager *, CKLevel *, const std::string &, CK_FLOORGEOMETRY, bool, int, bool, bool), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKFloorManager", "void AddFloorObject(CK3dEntity@ ent, CK_FLOORGEOMETRY geo, bool moving, int type, bool hiera, bool first)", asFUNCTIONPR([](CKFloorManager *self, CK3dEntity *ent, CK_FLOORGEOMETRY geo, bool moving, int type, bool hiera, bool first) { self->AddFloorObject(ent, geo, moving, type, hiera, first); }, (CKFloorManager *, CK3dEntity *, CK_FLOORGEOMETRY, bool, int, bool, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKFloorManager", "void RemoveFloorObject(CK3dEntity@ ent)", asMETHODPR(CKFloorManager, RemoveFloorObject, (CK3dEntity *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKFloorManager", "int GetFloorObjectCount()", asMETHODPR(CKFloorManager, GetFloorObjectCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKFloorManager", "CK3dEntity@ GetFloorObject(int pos)", asMETHODPR(CKFloorManager, GetFloorObject, (int), CK3dEntity *), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKFloorManager", "float GetCacheThreshold()", asMETHODPR(CKFloorManager, GetCacheThreshold, (), float), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKFloorManager", "void SetCacheThreshold(float t)", asMETHODPR(CKFloorManager, SetCacheThreshold, (float), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKFloorManager", "int GetCacheSize()", asMETHODPR(CKFloorManager, GetCacheSize, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKFloorManager", "void SetCacheSize(int size)", asMETHODPR(CKFloorManager, SetCacheSize, (int), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKFloorManager", "bool ReadAttributeValues(CK3dEntity@ ent, CKDWORD &out geo = void, bool &out moving = void, int &out type = void, bool &out hiera = void, bool &out first = void)", asFUNCTIONPR([](CKFloorManager *self, CK3dEntity *ent, CKDWORD *geo, bool *moving, int *type, bool *hiera, bool *first) -> bool { CKBOOL m; CKBOOL h; CKBOOL f; bool ret = self->ReadAttributeValues(ent, geo, &m, type, &h, &f); *moving = m; *hiera = h; *first = f; return ret; }, (CKFloorManager *, CK3dEntity *, CKDWORD *, bool *, int *, bool *, bool *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKFloorManager", "int GetFloorAttribute()", asMETHODPR(CKFloorManager, GetFloorAttribute, (), int), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKFloorManager", "bool ConstrainToFloor(const VxVector &in oldPos, const VxVector &in position, float radius, VxVector &out oPosition, int excludeAttribute = -1)", asFUNCTIONPR([](CKFloorManager *self, const VxVector &oldPos, const VxVector &position, float radius, VxVector &oPosition, int excludeAttribute) -> bool { return self->ConstrainToFloor(oldPos, position, radius, &oPosition, static_cast<CKAttributeType>(excludeAttribute)); }, (CKFloorManager *, const VxVector &, const VxVector &, float, VxVector &, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
}

void RegisterCKGridManager(asIScriptEngine *engine) {
    assert(engine != nullptr);

    int r = 0;

    RegisterCKBaseManagerMembers<CKGridManager>(engine, "CKGridManager");

    r = engine->RegisterObjectMethod("CKGridManager", "int GetTypeFromName(const string &in name)", asFUNCTIONPR([](CKGridManager *self, const std::string &name) { return self->GetTypeFromName(const_cast<CKSTRING>(name.c_str())); }, (CKGridManager *, const std::string &), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKGridManager", "string GetTypeName(int type)", asFUNCTIONPR([](CKGridManager *self, int type) -> std::string { return ScriptStringify(self->GetTypeName(type)); }, (CKGridManager *, int), std::string), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKGridManager", "CKERROR SetTypeName(int type, const string &in name)", asFUNCTIONPR([](CKGridManager *self, int type, const std::string &name) { return self->SetTypeName(type, const_cast<CKSTRING>(name.c_str())); }, (CKGridManager *, int, const std::string &), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKGridManager", "int RegisterType(const string &in name)", asFUNCTIONPR([](CKGridManager *self, const std::string &name) { return self->RegisterType(const_cast<CKSTRING>(name.c_str())); }, (CKGridManager *, const std::string &), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKGridManager", "int UnRegisterType(const string &in name)", asFUNCTIONPR([](CKGridManager *self, const std::string &name) { return self->UnRegisterType(const_cast<CKSTRING>(name.c_str())); }, (CKGridManager *, const std::string &), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKGridManager", "CKERROR SetAssociatedParam(int type, CKGUID param)", asMETHODPR(CKGridManager, SetAssociatedParam, (int, CKGUID), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKGridManager", "CKGUID GetAssociatedParam(int type)", asMETHODPR(CKGridManager, GetAssociatedParam, (int), CKGUID), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKGridManager", "CKERROR SetAssociatedColor(int type, VxColor &in color)", asMETHODPR(CKGridManager, SetAssociatedColor, (int, VxColor*), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKGridManager", "CKERROR GetAssociatedColor(int type, VxColor &out color)", asMETHODPR(CKGridManager, GetAssociatedColor, (int, VxColor*), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKGridManager", "int GetLayerTypeCount()", asMETHODPR(CKGridManager, GetLayerTypeCount, (), int), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKGridManager", "int GetClassificationFromName(const string &in name)", asFUNCTIONPR([](CKGridManager *self, const std::string &name) { return self->GetClassificationFromName(const_cast<CKSTRING>(name.c_str())); }, (CKGridManager *, const std::string &), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKGridManager", "string GetClassificationName(int classification)", asFUNCTIONPR([](CKGridManager *self, int classification) -> std::string { return ScriptStringify(self->GetClassificationName(classification)); }, (CKGridManager *, int), std::string), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKGridManager", "int RegisterClassification(const string &in name)", asFUNCTIONPR([](CKGridManager *self, const std::string &name) { return self->RegisterClassification(const_cast<CKSTRING>(name.c_str())); }, (CKGridManager *, const std::string &), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);
#if CKVERSION == 0x13022002
    r = engine->RegisterObjectMethod("CKGridManager", "int GetGridClassificationCategory()", asMETHODPR(CKGridManager, GetGridClassificationCategory, (), int), asCALL_THISCALL); assert(r >= 0);
#endif

    r = engine->RegisterObjectMethod("CKGridManager", "const XObjectPointerArray &GetGridArray(int flag = 0)", asMETHODPR(CKGridManager, GetGridArray, (int), const XObjectPointerArray&), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKGridManager", "CKGrid@ GetNearestGrid(VxVector &in pos, CK3dEntity@ ref = null)", asMETHODPR(CKGridManager, GetNearestGrid, (VxVector*, CK3dEntity *), CKGrid*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKGridManager", "CKGrid@ GetPreferredGrid(VxVector &in pos, CK3dEntity@ ref = null)", asMETHODPR(CKGridManager, GetPreferredGrid, (VxVector*, CK3dEntity *), CKGrid*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKGridManager", "bool IsInGrid(CKGrid@ grid, VxVector &in pos, CK3dEntity@ ref = null)", asFUNCTIONPR([](CKGridManager *self, CKGrid *grid, const VxVector &pos, CK3dEntity *ref) -> bool { return self->IsInGrid(grid, const_cast<VxVector*>(&pos), ref); }, (CKGridManager *, CKGrid *, const VxVector &, CK3dEntity *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKGridManager", "int GetGridObjectCount(int flag = 0)", asMETHODPR(CKGridManager, GetGridObjectCount, (int), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKGridManager", "CKGrid@ GetGridObject(int pos, int flag = 0)", asMETHODPR(CKGridManager, GetGridObject, (int, int), CKGrid*), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKGridManager", "void FillGridWithObjectShape(CK3dEntity@ ent, int layerType, int &in fillVal)", asMETHODPR(CKGridManager, FillGridWithObjectShape, (CK3dEntity *, int, void *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKGridManager", "void FillGridWithObjectShape(CK3dEntity@ ent, int solidLayerType, int shapeLayerType, int &in fillVal)", asMETHODPR(CKGridManager, FillGridWithObjectShape, (CK3dEntity *, int, int, void *), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKGridManager", "void Init()", asMETHODPR(CKGridManager, Init, (), void), asCALL_THISCALL); assert(r >= 0);
}

void RegisterCKInterfaceManager(asIScriptEngine *engine) {
    assert(engine != nullptr);

    int r = 0;

    RegisterCKBaseManagerMembers<CKInterfaceManager>(engine, "CKInterfaceManager");

    r = engine->RegisterObjectMethod("CKInterfaceManager", "NativePointer GetEditorFunctionForParameterType(CKParameterTypeDesc &in param)", asFUNCTIONPR([](CKInterfaceManager *self, CKParameterTypeDesc *param) { return NativePointer(self->GetEditorFunctionForParameterType(param)); }, (CKInterfaceManager *, CKParameterTypeDesc *), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKInterfaceManager", "int CallBehaviorEditionFunction(CKBehavior@ beh, NativePointer arg)", asFUNCTIONPR([](CKInterfaceManager *self, CKBehavior *beh, NativePointer arg) { return self->CallBehaviorEditionFunction(beh, arg.Get()); }, (CKInterfaceManager *, CKBehavior *, NativePointer), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKInterfaceManager", "int CallBehaviorSettingsEditionFunction(CKBehavior@ beh, NativePointer arg)", asFUNCTIONPR([](CKInterfaceManager *self, CKBehavior *beh, NativePointer arg) { return self->CallBehaviorSettingsEditionFunction(beh, arg.Get()); }, (CKInterfaceManager *, CKBehavior *, NativePointer), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKInterfaceManager", "int CallEditionFunction(CK_CLASSID cid, NativePointer arg)", asFUNCTIONPR([](CKInterfaceManager *self, CK_CLASSID cid, NativePointer arg) { return self->CallEditionFunction(cid, arg.Get()); }, (CKInterfaceManager *, CK_CLASSID, NativePointer), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKInterfaceManager", "int DoRenameDialog(const string &in name, CK_CLASSID cid)", asFUNCTIONPR([](CKInterfaceManager *self, const std::string &name, CK_CLASSID cid) { return self->DoRenameDialog(const_cast<CKSTRING>(name.c_str()), cid); }, (CKInterfaceManager *, const std::string &, CK_CLASSID), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);
}

void RegisterCKSoundManager(asIScriptEngine *engine) {
    assert(engine != nullptr);

    int r = 0;

    RegisterCKBaseManagerMembers<CKSoundManager>(engine, "CKSoundManager");

    r = engine->RegisterObjectMethod("CKSoundManager", "CK_SOUNDMANAGER_CAPS GetCaps()", asMETHODPR(CKSoundManager, GetCaps, (), CK_SOUNDMANAGER_CAPS), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKSoundManager", "NativePointer CreateSource(CK_WAVESOUND_TYPE flags, CKWaveFormat &in wf, CKDWORD bytes, bool streamed)", asFUNCTIONPR([](CKSoundManager *self, CK_WAVESOUND_TYPE flags, CKWaveFormat &wf, CKDWORD bytes, bool streamed) { return NativePointer(self->CreateSource(flags, &wf, bytes, streamed)); }, (CKSoundManager *, CK_WAVESOUND_TYPE, CKWaveFormat &, CKDWORD, bool), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSoundManager", "NativePointer DuplicateSource(NativePointer source)", asFUNCTIONPR([](CKSoundManager *self, NativePointer source) { return NativePointer(self->DuplicateSource(source.Get())); }, (CKSoundManager *, NativePointer), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSoundManager", "void ReleaseSource(NativePointer source)", asFUNCTIONPR([](CKSoundManager *self, NativePointer source) { self->ReleaseSource(source.Get()); }, (CKSoundManager *, NativePointer), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKSoundManager", "void Play(CKWaveSound@ sound, NativePointer source, bool loop)", asFUNCTIONPR([](CKSoundManager *self, CKWaveSound *sound, NativePointer source, bool loop) { self->Play(sound, source.Get(), loop); }, (CKSoundManager *, CKWaveSound *, NativePointer, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSoundManager", "void Pause(CKWaveSound@ sound, NativePointer source)", asFUNCTIONPR([](CKSoundManager *self, CKWaveSound *sound, NativePointer source) { self->Pause(sound, source.Get()); }, (CKSoundManager *, CKWaveSound *, NativePointer), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSoundManager", "void SetPlayPosition(NativePointer source, int pos)", asFUNCTIONPR([](CKSoundManager *self, NativePointer source, int pos) { self->SetPlayPosition(source.Get(), pos); }, (CKSoundManager *, NativePointer, int), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSoundManager", "int GetPlayPosition(NativePointer source)", asFUNCTIONPR([](CKSoundManager *self, NativePointer source) { return self->GetPlayPosition(source.Get()); }, (CKSoundManager *, NativePointer), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSoundManager", "void Stop(CKWaveSound@ sound, NativePointer source)", asFUNCTIONPR([](CKSoundManager *self, CKWaveSound *sound, NativePointer source) { self->Stop(sound, source.Get()); }, (CKSoundManager *, CKWaveSound *, NativePointer), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSoundManager", "bool IsPlaying(NativePointer source)", asFUNCTIONPR([](CKSoundManager *self, NativePointer source) -> bool { return self->IsPlaying(source.Get()); }, (CKSoundManager *, NativePointer), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKSoundManager", "CKERROR SetWaveFormat(NativePointer source, CKWaveFormat &in wf)", asFUNCTIONPR([](CKSoundManager *self, NativePointer source, CKWaveFormat &wf) { return self->SetWaveFormat(source.Get(), wf); }, (CKSoundManager *, NativePointer, CKWaveFormat &), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSoundManager", "CKERROR GetWaveFormat(NativePointer source, CKWaveFormat &out wf)", asFUNCTIONPR([](CKSoundManager *self, NativePointer source, CKWaveFormat &wf) { return self->GetWaveFormat(source.Get(), wf); }, (CKSoundManager *, NativePointer, CKWaveFormat &), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSoundManager", "int GetWaveSize(NativePointer source)", asFUNCTIONPR([](CKSoundManager *self, NativePointer source) { return self->GetWaveSize(source.Get()); }, (CKSoundManager *, NativePointer), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKSoundManager", "CKERROR Lock(NativePointer source, CKDWORD writeCursor, CKDWORD numBytes, NativePointer &out audioPtr1, CKDWORD &out audioBytes1, NativePointer &out audioPtr2, CKDWORD &out audioBytes2, CK_WAVESOUND_LOCKMODE flags)", asFUNCTIONPR([](CKSoundManager *self, NativePointer source, CKDWORD writeCursor, CKDWORD numBytes, NativePointer *audioPtr1, CKDWORD *audioBytes1, NativePointer *audioPtr2, CKDWORD *audioBytes2, CK_WAVESOUND_LOCKMODE flags) {
        return self->Lock(source.Get(), writeCursor, numBytes, reinterpret_cast<void **>(audioPtr1), audioBytes1, reinterpret_cast<void **>(audioPtr2), audioBytes2, flags);
    }, (CKSoundManager *, NativePointer, CKDWORD, CKDWORD, NativePointer *, CKDWORD *, NativePointer *, CKDWORD *, CK_WAVESOUND_LOCKMODE), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKSoundManager", "CKERROR Unlock(NativePointer source, NativePointer audioPtr1, CKDWORD numBytes1, NativePointer audioPtr2, CKDWORD audioBytes2)", asFUNCTIONPR([](CKSoundManager *self, NativePointer source, NativePointer audioPtr1, CKDWORD numBytes1, NativePointer audioPtr2, CKDWORD audioBytes2) {
        return self->Unlock(source.Get(), audioPtr1.Get(), numBytes1, audioPtr2.Get(), audioBytes2);
    }, (CKSoundManager *, NativePointer, NativePointer, CKDWORD, NativePointer, CKDWORD), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKSoundManager", "void SetType(NativePointer source, CK_WAVESOUND_TYPE type)", asFUNCTIONPR([](CKSoundManager *self, NativePointer source, CK_WAVESOUND_TYPE type) { self->SetType(source.Get(), type); }, (CKSoundManager *, NativePointer, CK_WAVESOUND_TYPE), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSoundManager", "CK_WAVESOUND_TYPE GetType(NativePointer source)", asFUNCTIONPR([](CKSoundManager *self, NativePointer source) { return self->GetType(source.Get()); }, (CKSoundManager *, NativePointer), CK_WAVESOUND_TYPE), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKSoundManager", "void UpdateSettings(NativePointer source, CK_SOUNDMANAGER_CAPS settingsoptions, CKWaveSoundSettings &out settings, bool set = true)", asFUNCTIONPR([](CKSoundManager *self, NativePointer source, CK_SOUNDMANAGER_CAPS settingsoptions, CKWaveSoundSettings &settings, bool set) { self->UpdateSettings(source.Get(), settingsoptions, settings, set); }, (CKSoundManager *, NativePointer, CK_SOUNDMANAGER_CAPS, CKWaveSoundSettings &, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSoundManager", "void Update3DSettings(NativePointer source, CK_SOUNDMANAGER_CAPS settingsoptions, CKWaveSound3DSettings &out settings, bool set = true)", asFUNCTIONPR([](CKSoundManager *self, NativePointer source, CK_SOUNDMANAGER_CAPS settingsoptions, CKWaveSound3DSettings &settings, bool set) { self->Update3DSettings(source.Get(), settingsoptions, settings, set); }, (CKSoundManager *, NativePointer, CK_SOUNDMANAGER_CAPS, CKWaveSound3DSettings &, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKSoundManager", "void UpdateListenerSettings(CK_SOUNDMANAGER_CAPS settingsoptions, CKListenerSettings &out settings, bool set = true)", asFUNCTIONPR([](CKSoundManager *self, CK_SOUNDMANAGER_CAPS settingsoptions, CKListenerSettings &settings, bool set) { self->UpdateListenerSettings(settingsoptions, settings, set); }, (CKSoundManager *, CK_SOUNDMANAGER_CAPS, CKListenerSettings &, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSoundManager", "void SetListener(CK3dEntity@ listener)", asMETHODPR(CKSoundManager, SetListener, (CK3dEntity *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSoundManager", "CK3dEntity@ GetListener()", asMETHODPR(CKSoundManager, GetListener, (), CK3dEntity *), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKSoundManager", "void SetStreamedBufferSize(CKDWORD size)", asMETHODPR(CKSoundManager, SetStreamedBufferSize, (CKDWORD), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSoundManager", "CKDWORD GetStreamedBufferSize()", asMETHODPR(CKSoundManager, GetStreamedBufferSize, (), CKDWORD), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKSoundManager", "bool IsInitialized()", asFUNCTIONPR([](CKSoundManager *self) -> bool { return self->IsInitialized(); }, (CKSoundManager *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
}

void RegisterCKMidiManager(asIScriptEngine *engine) {
    assert(engine != nullptr);

    int r = 0;

    RegisterCKBaseManagerMembers<CKMidiManager>(engine, "CKMidiManager");

    r = engine->RegisterObjectMethod("CKMidiManager", "NativePointer Create(NativePointer hwnd)", asFUNCTIONPR([](CKMidiManager *self, NativePointer hwnd) { return NativePointer(self->Create(hwnd.Get())); }, (CKMidiManager *, NativePointer), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMidiManager", "void Release(NativePointer source)", asFUNCTIONPR([](CKMidiManager *self, NativePointer source) { self->Release(source.Get()); }, (CKMidiManager *, NativePointer), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKMidiManager", "CKERROR SetSoundFileName(NativePointer source, const string &in filename)", asFUNCTIONPR([](CKMidiManager *self, NativePointer source, const std::string &filename) { return self->SetSoundFileName(source.Get(), const_cast<CKSTRING>(filename.c_str())); }, (CKMidiManager *, NativePointer, const std::string &), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMidiManager", "string GetSoundFileName(NativePointer source)", asFUNCTIONPR([](CKMidiManager *self, NativePointer source) -> std::string { return ScriptStringify(self->GetSoundFileName(source.Get())); }, (CKMidiManager *, NativePointer), std::string), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKMidiManager", "CKERROR Play(NativePointer source)", asFUNCTIONPR([](CKMidiManager *self, NativePointer source) { return self->Play(source.Get()); }, (CKMidiManager *, NativePointer), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMidiManager", "CKERROR Restart(NativePointer source)", asFUNCTIONPR([](CKMidiManager *self, NativePointer source) { return self->Restart(source.Get()); }, (CKMidiManager *, NativePointer), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMidiManager", "CKERROR Stop(NativePointer source)", asFUNCTIONPR([](CKMidiManager *self, NativePointer source) { return self->Stop(source.Get()); }, (CKMidiManager *, NativePointer), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMidiManager", "CKERROR Pause(NativePointer source, bool pause = true)", asFUNCTIONPR([](CKMidiManager *self, NativePointer source, bool pause) { return self->Pause(source.Get(), pause); }, (CKMidiManager *, NativePointer, bool), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMidiManager", "bool IsPlaying(NativePointer source)", asFUNCTIONPR([](CKMidiManager *self, NativePointer source) -> bool { return self->IsPlaying(source.Get()); }, (CKMidiManager *, NativePointer), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMidiManager", "bool IsPaused(NativePointer source)", asFUNCTIONPR([](CKMidiManager *self, NativePointer source) -> bool { return self->IsPaused(source.Get()); }, (CKMidiManager *, NativePointer), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKMidiManager", "CKERROR OpenFile(NativePointer source)", asFUNCTIONPR([](CKMidiManager *self, NativePointer source) { return self->OpenFile(source.Get()); }, (CKMidiManager *, NativePointer), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMidiManager", "CKERROR CloseFile(NativePointer source)", asFUNCTIONPR([](CKMidiManager *self, NativePointer source) { return self->CloseFile(source.Get()); }, (CKMidiManager *, NativePointer), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMidiManager", "CKERROR Preroll(NativePointer source)", asFUNCTIONPR([](CKMidiManager *self, NativePointer source) { return self->Preroll(source.Get()); }, (CKMidiManager *, NativePointer), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMidiManager", "CKERROR Time(NativePointer source, CKDWORD &out ticks)", asFUNCTIONPR([](CKMidiManager *self, NativePointer source, CKDWORD *ticks) { return self->Time(source.Get(), ticks); }, (CKMidiManager *, NativePointer, CKDWORD *), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMidiManager", "CKDWORD MillisecsToTicks(NativePointer source, CKDWORD msOffset)", asFUNCTIONPR([](CKMidiManager *self, NativePointer source, CKDWORD msOffset) { return self->MillisecsToTicks(source.Get(), msOffset); }, (CKMidiManager *, NativePointer, CKDWORD), CKDWORD), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMidiManager", "CKDWORD TicksToMillisecs(NativePointer source, CKDWORD tkOffset)", asFUNCTIONPR([](CKMidiManager *self, NativePointer source, CKDWORD tkOffset) { return self->TicksToMillisecs(source.Get(), tkOffset); }, (CKMidiManager *, NativePointer, CKDWORD), CKDWORD), asCALL_CDECL_OBJFIRST); assert(r >= 0);
}

void RegisterCKInputManager(asIScriptEngine *engine) {
    assert(engine != nullptr);

    int r = 0;

    RegisterCKBaseManagerMembers<CKInputManager>(engine, "CKInputManager");

    r = engine->RegisterObjectMethod("CKInputManager", "void EnableKeyboardRepetition(bool enable = true)", asFUNCTIONPR([](CKInputManager *self, bool enable) { self->EnableKeyboardRepetition(enable); }, (CKInputManager *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKInputManager", "bool IsKeyboardRepetitionEnabled()", asFUNCTIONPR([](CKInputManager *self) -> bool { return self->IsKeyboardRepetitionEnabled(); }, (CKInputManager *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKInputManager", "bool IsKeyDown(CKDWORD key, CKDWORD &out stamp = 0)", asFUNCTIONPR([](CKInputManager *self, CKDWORD key, CKDWORD &stamp) -> bool { return self->IsKeyDown(key, &stamp); }, (CKInputManager *, CKDWORD, CKDWORD &), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKInputManager", "bool IsKeyUp(CKDWORD key)", asFUNCTIONPR([](CKInputManager *self, CKDWORD key) -> bool { return self->IsKeyUp(key); }, (CKInputManager *, CKDWORD), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKInputManager", "bool IsKeyToggled(CKDWORD key, CKDWORD &out stamp = 0)", asFUNCTIONPR([](CKInputManager *self, CKDWORD key, CKDWORD &stamp) -> bool { return self->IsKeyToggled(key, &stamp); }, (CKInputManager *, CKDWORD, CKDWORD &), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
#if CKVERSION == 0x13022002
    r = engine->RegisterObjectMethod("CKInputManager", "int GetKeyName(CKDWORD key, string &out keyName)", asFUNCTIONPR([](CKInputManager *self, CKDWORD key, std::string &keyName) { return self->GetKeyName(key, const_cast<CKSTRING>(keyName.c_str())); }, (CKInputManager *, CKDWORD, std::string &), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKInputManager", "CKDWORD GetKeyFromName(const string &in keyName)", asFUNCTIONPR([](CKInputManager *self, const std::string &keyName) { return self->GetKeyFromName(const_cast<CKSTRING>(keyName.c_str())); }, (CKInputManager *, const std::string &), CKDWORD), asCALL_CDECL_OBJFIRST); assert(r >= 0);
#endif
    r = engine->RegisterObjectMethod("CKInputManager", "NativePointer GetKeyboardState()", asFUNCTIONPR([](CKInputManager *self) { return NativePointer(self->GetKeyboardState()); }, (CKInputManager *), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKInputManager", "bool IsKeyboardAttached()", asFUNCTIONPR([](CKInputManager *self) -> bool { return self->IsKeyboardAttached(); }, (CKInputManager *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKInputManager", "int GetNumberOfKeyInBuffer()", asMETHODPR(CKInputManager, GetNumberOfKeyInBuffer, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKInputManager", "int GetKeyFromBuffer(int index, CKDWORD &out key, CKDWORD &out timeStamp = void)", asMETHODPR(CKInputManager, GetKeyFromBuffer, (int, CKDWORD&, CKDWORD *), int), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKInputManager", "bool IsMouseButtonDown(CK_MOUSEBUTTON button)", asFUNCTIONPR([](CKInputManager *self, CK_MOUSEBUTTON button) -> bool { return self->IsMouseButtonDown(button); }, (CKInputManager *, CK_MOUSEBUTTON), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKInputManager", "bool IsMouseClicked(CK_MOUSEBUTTON button)", asFUNCTIONPR([](CKInputManager *self, CK_MOUSEBUTTON button) -> bool { return self->IsMouseClicked(button); }, (CKInputManager *, CK_MOUSEBUTTON), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKInputManager", "bool IsMouseToggled(CK_MOUSEBUTTON button)", asFUNCTIONPR([](CKInputManager *self, CK_MOUSEBUTTON button) -> bool { return self->IsMouseToggled(button); }, (CKInputManager *, CK_MOUSEBUTTON), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKInputManager", "void GetMouseButtonsState(CKDWORD &out states)", asMETHODPR(CKInputManager, GetMouseButtonsState, (CKBYTE[4]), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKInputManager", "void GetMousePosition(Vx2DVector &out position, bool absolute = true)", asFUNCTIONPR([](CKInputManager *self, Vx2DVector &position, bool absolute) { self->GetMousePosition(position, absolute); }, (CKInputManager *, Vx2DVector &, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKInputManager", "void GetMouseRelativePosition(VxVector &out position)", asMETHODPR(CKInputManager, GetMouseRelativePosition, (VxVector&), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKInputManager", "bool IsMouseAttached()", asFUNCTIONPR([](CKInputManager *self) -> bool { return self->IsMouseAttached(); }, (CKInputManager *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKInputManager", "bool IsJoystickAttached(int joystick)", asFUNCTIONPR([](CKInputManager *self, int joystick) -> bool { return self->IsJoystickAttached(joystick); }, (CKInputManager *, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKInputManager", "void GetJoystickPosition(int joystick, VxVector &out position)", asMETHODPR(CKInputManager, GetJoystickPosition, (int, VxVector*), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKInputManager", "void GetJoystickRotation(int joystick, VxVector &out rotation)", asMETHODPR(CKInputManager, GetJoystickRotation, (int, VxVector*), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKInputManager", "void GetJoystickSliders(int joystick, Vx2DVector &out position)", asMETHODPR(CKInputManager, GetJoystickSliders, (int, Vx2DVector*), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKInputManager", "void GetJoystickPointOfViewAngle(int joystick, float &out angle)", asMETHODPR(CKInputManager, GetJoystickPointOfViewAngle, (int, float*), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKInputManager", "CKDWORD GetJoystickButtonsState(int joystick)", asMETHODPR(CKInputManager, GetJoystickButtonsState, (int), CKDWORD), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKInputManager", "bool IsJoystickButtonDown(int joystick, int button)", asFUNCTIONPR([](CKInputManager *self, int joystick, int button) -> bool { return self->IsJoystickButtonDown(joystick, button); }, (CKInputManager *, int, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKInputManager", "void Pause(bool pause)", asFUNCTIONPR([](CKInputManager *self, bool pause) { self->Pause(pause); }, (CKInputManager *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKInputManager", "void ShowCursor(bool show)", asFUNCTIONPR([](CKInputManager *self, bool show) { self->ShowCursor(show); }, (CKInputManager *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKInputManager", "bool GetCursorVisibility()", asFUNCTIONPR([](CKInputManager *self) -> bool { return self->GetCursorVisibility(); }, (CKInputManager *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKInputManager", "VXCURSOR_POINTER GetSystemCursor()", asMETHODPR(CKInputManager, GetSystemCursor, (), VXCURSOR_POINTER), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKInputManager", "void SetSystemCursor(VXCURSOR_POINTER cursor)", asMETHODPR(CKInputManager, SetSystemCursor, (VXCURSOR_POINTER), void), asCALL_THISCALL); assert(r >= 0);
}

void RegisterCKCollisionManager(asIScriptEngine *engine) {
    assert(engine != nullptr);

    int r = 0;

    RegisterCKBaseManagerMembers<CKCollisionManager>(engine, "CKCollisionManager");

    r = engine->RegisterObjectMethod("CKCollisionManager", "void AddObstacle(CK3dEntity@ ent, bool moving = false, CK_GEOMETRICPRECISION precision = CKCOLLISION_BOX, bool hiera = false)", asFUNCTIONPR([](CKCollisionManager *self, CK3dEntity *ent, bool moving, CK_GEOMETRICPRECISION precision, bool hiera) { self->AddObstacle(ent, moving, precision, hiera); }, (CKCollisionManager *, CK3dEntity *, bool, CK_GEOMETRICPRECISION, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCollisionManager", "int AddObstaclesByName(CKLevel@ level, const string &in name, bool moving = false, CK_GEOMETRICPRECISION precision = CKCOLLISION_BOX, bool hiera = false)", asFUNCTIONPR([](CKCollisionManager *self, CKLevel *level, const std::string &name, bool moving, CK_GEOMETRICPRECISION precision, bool hiera) { return self->AddObstaclesByName(level, const_cast<CKSTRING>(name.c_str()), moving, precision, hiera); }, (CKCollisionManager *, CKLevel *, const std::string &, bool, CK_GEOMETRICPRECISION, bool), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCollisionManager", "void RemoveObstacle(CK3dEntity@ ent)", asMETHODPR(CKCollisionManager, RemoveObstacle, (CK3dEntity *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCollisionManager", "void RemoveAllObstacles(bool level = true)", asFUNCTIONPR([](CKCollisionManager *self, bool level) { self->RemoveAllObstacles(level); }, (CKCollisionManager *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKCollisionManager", "bool IsObstacle(CK3dEntity@ ent, bool moving = false)", asFUNCTIONPR([](CKCollisionManager *self, CK3dEntity *ent, bool moving) -> bool { return self->IsObstacle(ent, moving); }, (CKCollisionManager *, CK3dEntity *, bool), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCollisionManager", "int GetFixedObstacleCount(bool level = false)", asFUNCTIONPR([](CKCollisionManager *self, bool level) -> int { return self->GetFixedObstacleCount(level); }, (CKCollisionManager *, bool), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCollisionManager", "CK3dEntity@ GetFixedObstacle(int pos, bool level = false)", asFUNCTIONPR([](CKCollisionManager *self, int pos, bool level) -> CK3dEntity * { return self->GetFixedObstacle(pos, level); }, (CKCollisionManager *, int, bool), CK3dEntity *), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCollisionManager", "int GetMovingObstacleCount(bool level = false)", asFUNCTIONPR([](CKCollisionManager *self, bool level) -> int { return self->GetMovingObstacleCount(level); }, (CKCollisionManager *, bool), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCollisionManager", "CK3dEntity@ GetMovingObstacle(int pos, bool level = false)", asFUNCTIONPR([](CKCollisionManager *self, int pos, bool level) -> CK3dEntity * { return self->GetMovingObstacle(pos, level); }, (CKCollisionManager *, int, bool), CK3dEntity *), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCollisionManager", "int GetObstacleCount(bool level = false)", asFUNCTIONPR([](CKCollisionManager *self, bool level) -> int { return self->GetObstacleCount(level); }, (CKCollisionManager *, bool), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCollisionManager", "CK3dEntity@ GetObstacle(int pos, bool level = false)", asFUNCTIONPR([](CKCollisionManager *self, int pos, bool level) -> CK3dEntity * { return self->GetObstacle(pos, level); }, (CKCollisionManager *, int, bool), CK3dEntity *), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKCollisionManager", "bool DetectCollision(CK3dEntity@ ent, CK_GEOMETRICPRECISION precisionLevel, int replacementPrecision, int detectionPrecision, CK_IMPACTINFO impactFlags, ImpactDesc &out imp)", asFUNCTIONPR([](CKCollisionManager *self, CK3dEntity *ent, CK_GEOMETRICPRECISION precisionLevel, int replacementPrecision, int detectionPrecision, CK_IMPACTINFO impactFlags, ImpactDesc &imp) -> bool { return self->DetectCollision(ent, precisionLevel, replacementPrecision, detectionPrecision, impactFlags, &imp); }, (CKCollisionManager *, CK3dEntity *, CK_GEOMETRICPRECISION, int, int, CK_IMPACTINFO, ImpactDesc &), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCollisionManager", "bool ObstacleBetween(const VxVector &in pos, const VxVector &in endpos, float width, float height)", asFUNCTIONPR([](CKCollisionManager *self, const VxVector &pos, const VxVector &endpos, float width, float height) -> bool { return self->ObstacleBetween(pos, endpos, width, height); }, (CKCollisionManager *, const VxVector &, const VxVector &, float, float), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKCollisionManager", "bool BoxBoxIntersection(CK3dEntity@ ent1, bool hiera1, bool local1, CK3dEntity@ ent2, bool hiera2, bool local2)", asFUNCTIONPR([](CKCollisionManager *self, CK3dEntity *ent1, bool hiera1, bool local1, CK3dEntity *ent2, bool hiera2, bool local2) -> bool { return self->BoxBoxIntersection(ent1, hiera1, local1, ent2, hiera2, local2); }, (CKCollisionManager *, CK3dEntity *, bool, bool, CK3dEntity *, bool, bool), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCollisionManager", "bool BoxFaceIntersection(CK3dEntity@ ent1, bool hiera1, bool local1, CK3dEntity@ ent2)", asFUNCTIONPR([](CKCollisionManager *self, CK3dEntity *ent1, bool hiera1, bool local1, CK3dEntity *ent2) -> bool { return self->BoxFaceIntersection(ent1, hiera1, local1, ent2); }, (CKCollisionManager *, CK3dEntity *, bool, bool, CK3dEntity *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCollisionManager", "bool FaceFaceIntersection(CK3dEntity@ ent1, CK3dEntity@ ent2)", asFUNCTIONPR([](CKCollisionManager *self, CK3dEntity *ent1, CK3dEntity *ent2) -> bool { return self->FaceFaceIntersection(ent1, ent2); }, (CKCollisionManager *, CK3dEntity *, CK3dEntity *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCollisionManager", "bool IsInCollision(CK3dEntity@ ent, CK_GEOMETRICPRECISION precisionLevel1, CK3dEntity@ ent2, CK_GEOMETRICPRECISION precisionLevel2)", asFUNCTIONPR([](CKCollisionManager *self, CK3dEntity *ent, CK_GEOMETRICPRECISION precisionLevel1, CK3dEntity *ent2, CK_GEOMETRICPRECISION precisionLevel2) -> bool { return self->IsInCollision(ent, precisionLevel1, ent2, precisionLevel2); }, (CKCollisionManager *, CK3dEntity *, CK_GEOMETRICPRECISION, CK3dEntity *, CK_GEOMETRICPRECISION), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCollisionManager", "CK3dEntity@ IsInCollisionWithHierarchy(CK3dEntity@ ent, CK_GEOMETRICPRECISION precisionLevel1, CK3dEntity@ ent2, CK_GEOMETRICPRECISION precisionLevel2)", asMETHODPR(CKCollisionManager, IsInCollisionWithHierarchy, (CK3dEntity *, CK_GEOMETRICPRECISION, CK3dEntity *, CK_GEOMETRICPRECISION), CK3dEntity *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCollisionManager", "bool IsHierarchyInCollisionWithHierarchy(CK3dEntity@ ent1, CK_GEOMETRICPRECISION precisionLevel1, CK3dEntity@ ent2, CK_GEOMETRICPRECISION precisionLevel2, CK3dEntity@ &out sub, CK3dEntity@ &out subob)", asFUNCTIONPR([](CKCollisionManager *self, CK3dEntity *ent1, CK_GEOMETRICPRECISION precisionLevel1, CK3dEntity *ent2, CK_GEOMETRICPRECISION precisionLevel2, CK3dEntity *& sub, CK3dEntity *& subob) -> bool { return self->IsHierarchyInCollisionWithHierarchy(ent1, precisionLevel1, ent2, precisionLevel2, &sub, &subob); }, (CKCollisionManager *, CK3dEntity *, CK_GEOMETRICPRECISION, CK3dEntity *, CK_GEOMETRICPRECISION, CK3dEntity *&, CK3dEntity *&), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
}