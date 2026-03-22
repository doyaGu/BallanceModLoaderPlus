#include "ScriptCKObjects.h"

#include <type_traits>

#include "CKAll.h"

// Win32 GetObject macro conflicts with asIScriptGeneric::GetObject
#ifdef GetObject
#undef GetObject
#endif

// #include "ScriptManager.h" // CKAngelScript-specific, not available
#include "ScriptUtils.h"
#include "ScriptNativePointer.h"

template<typename B, typename D>
B *CKObjectUpcast(D *derived) {
    static_assert(std::is_base_of_v<CKObject, B> == true);
    static_assert(std::is_base_of_v<CKObject, D> == true);

    if (!derived)
        return nullptr;

    return derived;
}

template<typename B, typename D>
D *CKObjectDowncast(B *base) {
    static_assert(std::is_base_of_v<CKObject, B> == true);
    static_assert(std::is_base_of_v<CKObject, D> == true);

    if (!base)
        return nullptr;

    return D::Cast(base);
}

template <typename D, typename B>
static void RegisterCKObjectCast(asIScriptEngine *engine, const char *derived, const char *base) {
    int r = 0;

    std::string decl = derived;
    decl.append("@ opCast()");
    r = engine->RegisterObjectMethod(base, decl.c_str(), asFUNCTIONPR((CKObjectDowncast<B, D>), (B *), D *), asCALL_CDECL_OBJLAST); assert( r >= 0 );

    decl = base;
    decl.append("@ opImplCast()");
    r = engine->RegisterObjectMethod(derived, decl.c_str(), asFUNCTIONPR((CKObjectUpcast<B, D>), (D *), B *), asCALL_CDECL_OBJLAST); assert( r >= 0 );

    decl = "const ";
    decl.append(derived).append("@ opCast() const");
    r = engine->RegisterObjectMethod(base, decl.c_str(), asFUNCTIONPR((CKObjectDowncast<B, D>), (B *), D *), asCALL_CDECL_OBJLAST); assert( r >= 0 );

    decl = "const ";
    decl.append(base).append("@ opImplCast() const");
    r = engine->RegisterObjectMethod(derived, decl.c_str(), asFUNCTIONPR((CKObjectUpcast<B, D>), (D *), B *), asCALL_CDECL_OBJLAST); assert( r >= 0 );
}

template <typename T>
static void RegisterCKObjectMembers(asIScriptEngine *engine, const char *name) {
    int r = 0;

    r = engine->RegisterObjectMethod(name, "void SetName(const string &in name, bool shared = false)", asFUNCTIONPR([](T *self, const std::string &name, bool shared) { self->SetName(const_cast<CKSTRING>(name.c_str()), shared); }, (T *, const std::string &, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "NativePointer GetAppData()", asFUNCTIONPR([](T *self) { return NativePointer(self->GetAppData()); }, (T *), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetAppData(NativePointer data)", asFUNCTIONPR([](T *self, NativePointer data) { self->SetAppData(data.Get()); }, (T *, NativePointer), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void Show(CK_OBJECT_SHOWOPTION show = CKSHOW)", asMETHODPR(T, Show, (CK_OBJECT_SHOWOPTION), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool IsHiddenByParent()", asFUNCTIONPR([](T *self) -> bool { return self->IsHiddenByParent(); }, (T *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "int CanBeHide()", asMETHODPR(T, CanBeHide, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool IsVisible()", asFUNCTIONPR([](T *self) -> bool { return self->IsVisible(); }, (T *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool IsHierarchicallyHide()", asFUNCTIONPR([](T *self) -> bool { return self->IsHierarchicallyHide(); }, (T *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "CKContext@ GetCKContext()", asMETHODPR(T, GetCKContext, (), CKContext*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CK_ID GetID()", asMETHODPR(T, GetID, (), CK_ID), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "string GetName()", asFUNCTIONPR([](T *self) -> std::string { return ScriptStringify(self->GetName()); }, (T *), std::string), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKDWORD GetObjectFlags()", asMETHODPR(T, GetObjectFlags, (), CKDWORD), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool IsDynamic()", asFUNCTIONPR([](T *self) -> bool { return self->IsDynamic(); }, (T *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool IsToBeDeleted()", asFUNCTIONPR([](T *self) -> bool { return self->IsToBeDeleted(); }, (T *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void ModifyObjectFlags(CKDWORD add, CKDWORD remove)", asMETHODPR(T, ModifyObjectFlags, (CKDWORD, CKDWORD), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "CK_CLASSID GetClassID()", asMETHODPR(T, GetClassID, (), CK_CLASSID), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void PreSave(CKFile@ file, CKDWORD flags)", asMETHODPR(T, PreSave, (CKFile *, CKDWORD), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKStateChunk@ Save(CKFile@ file, CKDWORD flags)", asMETHODPR(T, Save, (CKFile *, CKDWORD), CKStateChunk *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKERROR Load(CKStateChunk@ chunk, CKFile@ file)", asMETHODPR(T, Load, (CKStateChunk *, CKFile *), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void PostLoad()", asMETHODPR(T, PostLoad, (), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void PreDelete()", asMETHODPR(T, PreDelete, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void CheckPreDeletion()", asMETHODPR(T, CheckPreDeletion, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void CheckPostDeletion()", asMETHODPR(T, CheckPostDeletion, (), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "int GetMemoryOccupation()", asMETHODPR(T, GetMemoryOccupation, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool IsObjectUsed(CKObject@ obj, CK_CLASSID cid)", asFUNCTIONPR([](T *self, CKObject *obj, CK_CLASSID cid) -> bool { return self->IsObjectUsed(obj, cid); }, (T *, CKObject *, CK_CLASSID), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

#if CKVERSION == 0x13022002
    r = engine->RegisterObjectMethod(name, "CKERROR PrepareDependencies(CKDependenciesContext &in context)", asMETHODPR(T, PrepareDependencies, (CKDependenciesContext &), CKERROR), asCALL_THISCALL); assert(r >= 0);
#else
    r = engine->RegisterObjectMethod(name, "CKERROR PrepareDependencies(CKDependenciesContext &in context, bool caller = true)", asFUNCTIONPR([](T *self, CKDependenciesContext &context, bool caller) -> CKERROR { return self->PrepareDependencies(context, caller); }, (T *, CKDependenciesContext &, bool), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
#endif
    r = engine->RegisterObjectMethod(name, "CKERROR RemapDependencies(CKDependenciesContext &in context)", asMETHODPR(T, RemapDependencies, (CKDependenciesContext &), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKERROR Copy(CKObject@ obj, CKDependenciesContext &in context)", asMETHODPR(T, Copy, (CKObject&, CKDependenciesContext &), CKERROR), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "bool IsUpToDate()", asFUNCTIONPR([](T *self) -> bool { return self->IsUpToDate(); }, (T *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool IsPrivate()", asFUNCTIONPR([](T *self) -> bool { return self->IsPrivate(); }, (T *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool IsNotToBeSaved()", asFUNCTIONPR([](T *self) -> bool { return self->IsNotToBeSaved(); }, (T *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool IsInterfaceObj()", asFUNCTIONPR([](T *self) -> bool { return self->IsInterfaceObj(); }, (T *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

#if CKVERSION == 0x13022002
    r = engine->RegisterObjectMethod(name, "CKERROR CKDestroyObject(CKObject@ obj, CKDWORD flags = 0, CKDependencies &in depoptions = void)", asMETHODPR(T, CKDestroyObject, (CKObject*, CKDWORD, CKDependencies*), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKERROR CKDestroyObject(CK_ID id, CKDWORD flags = 0, CKDependencies &in depoptions = void)", asMETHODPR(T, CKDestroyObject, (CK_ID, CKDWORD, CKDependencies*), CKERROR), asCALL_THISCALL); assert(r >= 0);
#else
    r = engine->RegisterObjectMethod(name, "CKERROR CKDestroyObject(CKObject@ obj, CKDWORD flags = 0, CKDependencies &in depoptions = void)", asMETHODPR(T, CKDestroyObject, (CKObject*, DWORD, CKDependencies*), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKERROR CKDestroyObject(CK_ID id, CKDWORD flags = 0, CKDependencies &in depoptions = void)", asMETHODPR(T, CKDestroyObject, (CK_ID, DWORD, CKDependencies*), CKERROR), asCALL_THISCALL); assert(r >= 0);
#endif
    r = engine->RegisterObjectMethod(name, "CKERROR CKDestroyObjects(const XObjectArray &in ids, CKDWORD flags = 0, CKDependencies &in depoptions = void)", asFUNCTIONPR([](T *obj, const XObjectArray &objects, CKDWORD flags, CKDependencies *deps) { return obj->CKDestroyObjects(objects.Begin(), objects.Size(), flags, deps); }, (T *, const XObjectArray &, CKDWORD, CKDependencies *), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKObject@ CKGetObject(CK_ID id)", asMETHODPR(T, CKGetObject, (CK_ID), CKObject*), asCALL_THISCALL); assert(r >= 0);

    if (strcmp(name, "CKObject") != 0) {
        RegisterCKObjectCast<T, CKObject>(engine, name, "CKObject");
    }
}

template <>
static void RegisterCKObjectMembers<CKScene>(asIScriptEngine *engine, const char *name) {
    int r = 0;

    r = engine->RegisterObjectMethod(name, "void SetName(const string &in name, bool shared = false)", asFUNCTIONPR([](CKScene *self, const std::string &name, bool shared) { self->SetName(const_cast<CKSTRING>(name.c_str()), shared); }, (CKScene *, const std::string &, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "NativePointer GetAppData()", asFUNCTIONPR([](CKScene *self) { return NativePointer(self->GetAppData()); }, (CKScene *), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetAppData(NativePointer data)", asFUNCTIONPR([](CKScene *self, NativePointer data) { self->SetAppData(data.Get()); }, (CKScene *, NativePointer), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void Show(CK_OBJECT_SHOWOPTION show = CKSHOW)", asMETHODPR(CKScene, Show, (CK_OBJECT_SHOWOPTION), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool IsHiddenByParent()", asFUNCTIONPR([](CKScene *self) -> bool { return self->IsHiddenByParent(); }, (CKScene *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "int CanBeHide()", asMETHODPR(CKScene, CanBeHide, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool IsVisible()", asFUNCTIONPR([](CKScene *self) -> bool { return self->IsVisible(); }, (CKScene *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool IsHierarchicallyHide()", asFUNCTIONPR([](CKScene *self) -> bool { return self->IsHierarchicallyHide(); }, (CKScene *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "CKContext@ GetCKContext()", asMETHODPR(CKScene, GetCKContext, (), CKContext*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CK_ID GetID()", asMETHODPR(CKScene, GetID, (), CK_ID), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "string GetName()", asFUNCTIONPR([](CKScene *self) -> std::string { return ScriptStringify(self->GetName()); }, (CKScene *), std::string), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKDWORD GetObjectFlags()", asMETHODPR(CKObject, GetObjectFlags, (), CKDWORD), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool IsDynamic()", asFUNCTIONPR([](CKScene *self) -> bool { return self->IsDynamic(); }, (CKScene *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool IsToBeDeleted()", asFUNCTIONPR([](CKScene *self) -> bool { return self->IsToBeDeleted(); }, (CKScene *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void ModifyObjectFlags(CKDWORD add, CKDWORD remove)", asMETHODPR(CKObject, ModifyObjectFlags, (CKDWORD, CKDWORD), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "CK_CLASSID GetClassID()", asMETHODPR(CKScene, GetClassID, (), CK_CLASSID), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void PreSave(CKFile@ file, CKDWORD flags)", asMETHODPR(CKScene, PreSave, (CKFile *, CKDWORD), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKStateChunk@ Save(CKFile@ file, CKDWORD flags)", asMETHODPR(CKScene, Save, (CKFile *, CKDWORD), CKStateChunk *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKERROR Load(CKStateChunk@ chunk, CKFile@ file)", asMETHODPR(CKScene, Load, (CKStateChunk *, CKFile *), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void PostLoad()", asMETHODPR(CKScene, PostLoad, (), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void PreDelete()", asMETHODPR(CKScene, PreDelete, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void CheckPreDeletion()", asMETHODPR(CKScene, CheckPreDeletion, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void CheckPostDeletion()", asMETHODPR(CKScene, CheckPostDeletion, (), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "int GetMemoryOccupation()", asMETHODPR(CKScene, GetMemoryOccupation, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool IsObjectUsed(CKObject@ obj, CK_CLASSID cid)", asFUNCTIONPR([](CKScene *self, CKObject *obj, CK_CLASSID cid) -> bool { return self->IsObjectUsed(obj, cid); }, (CKScene *, CKObject *, CK_CLASSID), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

#if CKVERSION == 0x13022002
    r = engine->RegisterObjectMethod(name, "CKERROR PrepareDependencies(CKDependenciesContext &in context)", asMETHODPR(CKScene, PrepareDependencies, (CKDependenciesContext &), CKERROR), asCALL_THISCALL); assert(r >= 0);
#else
    r = engine->RegisterObjectMethod(name, "CKERROR PrepareDependencies(CKDependenciesContext &in context, bool caller = true)", asFUNCTIONPR([](CKScene *obj, CKDependenciesContext &context, bool caller) { return obj->PrepareDependencies(context, caller); }, (CKScene *, CKDependenciesContext &, bool), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
#endif
    r = engine->RegisterObjectMethod(name, "CKERROR RemapDependencies(CKDependenciesContext &in context)", asMETHODPR(CKScene, RemapDependencies, (CKDependenciesContext &), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKERROR Copy(CKObject@ obj, CKDependenciesContext &in context)", asMETHODPR(CKScene, Copy, (CKObject&, CKDependenciesContext &), CKERROR), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "bool IsUpToDate()", asFUNCTIONPR([](CKScene *self) -> bool { return self->IsUpToDate(); }, (CKScene *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool IsPrivate()", asFUNCTIONPR([](CKScene *self) -> bool { return self->IsPrivate(); }, (CKScene *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool IsNotToBeSaved()", asFUNCTIONPR([](CKScene *self) -> bool { return self->IsNotToBeSaved(); }, (CKScene *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool IsInterfaceObj()", asFUNCTIONPR([](CKScene *self) -> bool { return self->IsInterfaceObj(); }, (CKScene *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

#if CKVERSION == 0x13022002
    r = engine->RegisterObjectMethod(name, "CKERROR CKDestroyObject(CKObject@ obj, CKDWORD flags = 0, CKDependencies &in depoptions = void)", asMETHODPR(CKScene, CKDestroyObject, (CKObject*, CKDWORD, CKDependencies*), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKERROR CKDestroyObject(CK_ID id, CKDWORD flags = 0, CKDependencies &in depoptions = void)", asMETHODPR(CKScene, CKDestroyObject, (CK_ID, CKDWORD, CKDependencies*), CKERROR), asCALL_THISCALL); assert(r >= 0);
#else
    r = engine->RegisterObjectMethod(name, "CKERROR CKDestroyObject(CKObject@ obj, CKDWORD flags = 0, CKDependencies &in depoptions = void)", asMETHODPR(CKScene, CKDestroyObject, (CKObject*, DWORD, CKDependencies*), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKERROR CKDestroyObject(CK_ID id, CKDWORD flags = 0, CKDependencies &in depoptions = void)", asMETHODPR(CKScene, CKDestroyObject, (CK_ID, DWORD, CKDependencies*), CKERROR), asCALL_THISCALL); assert(r >= 0);
#endif
    r = engine->RegisterObjectMethod(name, "CKERROR CKDestroyObjects(const XObjectArray &in ids, CKDWORD flags = 0, CKDependencies &in depoptions = void)", asFUNCTIONPR([](CKScene *obj, const XObjectArray &objects, CKDWORD flags, CKDependencies *deps) { return obj->CKDestroyObjects(objects.Begin(), objects.Size(), flags, deps); }, (CKScene *, const XObjectArray &, CKDWORD, CKDependencies *), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKObject@ CKGetObject(CK_ID id)", asMETHODPR(CKScene, CKGetObject, (CK_ID), CKObject*), asCALL_THISCALL); assert(r >= 0);

    RegisterCKObjectCast<CKScene, CKObject>(engine, name, "CKObject");
}

void RegisterCKObject(asIScriptEngine *engine) {
    assert(engine != nullptr);

    RegisterCKObjectMembers<CKObject>(engine, "CKObject");
}

void RegisterCKInterfaceObjectManager(asIScriptEngine *engine) {
    assert(engine != nullptr);

    RegisterCKObjectMembers<CKInterfaceObjectManager>(engine, "CKInterfaceObjectManager");
}

static void CKParameterInGetValueGeneric(asIScriptGeneric *gen) {
    asIScriptEngine *engine = gen->GetEngine();
    asIScriptContext *ctx = asGetActiveContext();

    auto *self = static_cast<CKParameterIn *>(gen->GetObject());
    const int typeId = gen->GetArgTypeId(0);
    void *buf = *static_cast<void **>(gen->GetAddressOfArg(0));

    CKParameter *p = self->GetRealSource();
    if (!p) {
        gen->SetReturnDWord(CKERR_NOTINITIALIZED);
        return;
    }

    CKERROR err = CK_OK;

    if (typeId & asTYPEID_SCRIPTOBJECT) {
        ctx->SetException("Cannot read script objects from buffer");
        gen->SetReturnDWord(CKERR_INVALIDPARAMETER);
        return;
    }

    if (typeId & asTYPEID_APPOBJECT) {
        asITypeInfo *type = engine->GetTypeInfoById(typeId);
        if (!type) {
            gen->SetReturnDWord(CKERR_INVALIDPARAMETER);
            return;
        }

        if (typeId & asTYPEID_OBJHANDLE) {
            ctx->SetException("Cannot read object handle from buffer");
            gen->SetReturnDWord(CKERR_INVALIDPARAMETER);
            return;
        }

        if (strcmp(type->GetName(), "string") == 0) {
            std::string &str = *static_cast<std::string *>(buf);
            CKSTRING value = (CKSTRING) p->GetReadDataPtr();
            if (value)
                str = value;
            else
                err = CKERR_INVALIDPARAMETER;
        } else {
            int size = engine->GetSizeOfPrimitiveType(typeId);
            if (size == 0) {
                if (type->GetFlags() & asOBJ_POD) {
                    err = p->GetValue(buf);
                } else {
                    ctx->SetException("Cannot read non-POD objects from buffer");
                    gen->SetReturnDWord(CKERR_INVALIDPARAMETER);
                    return;
                }
            }
        }
    } else {
        int size = engine->GetSizeOfPrimitiveType(typeId);
        if (size != 0) {
            err = p->GetValue(buf);
        }
    }

    gen->SetReturnDWord(err);
}

template <typename T>
static void RegisterCKParameterInMembers(asIScriptEngine *engine, const char *name) {
    int r = 0;

    RegisterCKObjectMembers<T>(engine, name);

    // r = engine->RegisterObjectMethod(name, "CKERROR GetValue(uintptr_t buf)", asMETHODPR(T, GetValue, (void *), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKERROR GetValue(?&out value)", asFUNCTION(CKParameterInGetValueGeneric), asCALL_GENERIC); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "NativePointer GetReadDataPtr()", asFUNCTIONPR([](T *self) { return NativePointer(self->GetReadDataPtr()); }, (T *), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKParameterIn@ GetSharedSource()", asMETHODPR(T, GetSharedSource, (), CKParameterIn *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKParameter@ GetRealSource()", asMETHODPR(T, GetRealSource, (), CKParameter*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKParameter@ GetDirectSource()", asMETHODPR(T, GetDirectSource, (), CKParameter*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKERROR SetDirectSource(CKParameter@ param)", asMETHODPR(T, SetDirectSource, (CKParameter*), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKERROR ShareSourceWith(CKParameterIn@ pin)", asMETHODPR(T, ShareSourceWith, (CKParameterIn *), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetType(CKParameterType type, bool updateSource = false, const string &in newName = void)", asFUNCTIONPR([](T *self, CKParameterType type, bool updateSource, const std::string &newName) { self->SetType(type, updateSource, const_cast<CKSTRING>(newName.c_str())); }, (T *, CKParameterType, bool, const std::string &), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetGUID(CKGUID guid, bool updateSource = false, const string &in newName = void)", asFUNCTIONPR([](T *self, CKGUID guid, bool updateSource, const std::string &newName) { self->SetGUID(guid, updateSource, const_cast<CKSTRING>(newName.c_str())); }, (T *, CKGUID, bool, const std::string &), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKParameterType GetType()", asMETHODPR(T, GetType, (), CKParameterType), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKGUID GetGUID()", asMETHODPR(T, GetGUID, (), CKGUID), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetOwner(CKObject@ obj)", asMETHODPR(T, SetOwner, (CKObject*), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKObject@ GetOwner()", asMETHODPR(T, GetOwner, (), CKObject*), asCALL_THISCALL); assert(r >= 0);

    if (strcmp(name, "CKParameterIn") != 0) {
        RegisterCKObjectCast<T, CKParameterIn>(engine, name, "CKParameterIn");
    }
}

void RegisterCKParameterIn(asIScriptEngine *engine) {
    assert(engine != nullptr);

    RegisterCKParameterInMembers<CKParameterIn>(engine, "CKParameterIn");
}

static void CKParameterGetValueGeneric(asIScriptGeneric *gen) {
    asIScriptEngine *engine = gen->GetEngine();
    asIScriptContext *ctx = asGetActiveContext();

    auto *self = static_cast<CKParameter *>(gen->GetObject());
    const int typeId = gen->GetArgTypeId(0);
    void *buf = *static_cast<void **>(gen->GetAddressOfArg(0));
    bool update = gen->GetArgByte(1);

    CKERROR err = CK_OK;

    if (typeId & asTYPEID_SCRIPTOBJECT) {
        ctx->SetException("Cannot read script objects from buffer");
        gen->SetReturnDWord(CKERR_INVALIDPARAMETER);
        return;
    }

    if (typeId & asTYPEID_APPOBJECT) {
        asITypeInfo *type = engine->GetTypeInfoById(typeId);
        if (!type) {
            gen->SetReturnDWord(CKERR_INVALIDPARAMETER);
            return;
        }

        if (typeId & asTYPEID_OBJHANDLE) {
            ctx->SetException("Cannot read object handle from buffer");
            gen->SetReturnDWord(CKERR_INVALIDPARAMETER);
            return;
        }

        if (strcmp(type->GetName(), "string") == 0) {
            std::string &str = *static_cast<std::string *>(buf);
            CKSTRING value = (CKSTRING) self->GetReadDataPtr();
            if (value)
                str = value;
            else
                err = CKERR_INVALIDPARAMETER;
        } else {
            int size = engine->GetSizeOfPrimitiveType(typeId);
            if (size == 0) {
                if (type->GetFlags() & asOBJ_POD) {
                    err = self->GetValue(buf, update);
                } else {
                    ctx->SetException("Cannot read non-POD objects from buffer");
                    gen->SetReturnDWord(CKERR_INVALIDPARAMETER);
                    return;
                }
            }
        }
    } else {
        int size = engine->GetSizeOfPrimitiveType(typeId);
        if (size != 0) {
            err = self->GetValue(buf, update);
        }
    }

    gen->SetReturnDWord(err);
}

static void CKParameterSetValueGeneric(asIScriptGeneric *gen) {
    asIScriptEngine *engine = gen->GetEngine();
    asIScriptContext *ctx = asGetActiveContext();

    auto *self = static_cast<CKParameter *>(gen->GetObject());
    const int typeId = gen->GetArgTypeId(0);
    void *buf = *static_cast<void **>(gen->GetAddressOfArg(0));

    CKERROR err = CK_OK;

    if (typeId & asTYPEID_SCRIPTOBJECT) {
        ctx->SetException("Cannot write script objects to buffer");
        gen->SetReturnDWord(CKERR_INVALIDPARAMETER);
        return;
    }

    if (typeId & asTYPEID_APPOBJECT) {
        asITypeInfo *type = engine->GetTypeInfoById(typeId);
        if (!type) {
            gen->SetReturnDWord(CKERR_INVALIDPARAMETER);
            return;
        }

        if (typeId & asTYPEID_OBJHANDLE) {
            ctx->SetException("Cannot write object handle to buffer");
            gen->SetReturnDWord(CKERR_INVALIDPARAMETER);
            return;
        }

        if (strcmp(type->GetName(), "string") == 0) {
            std::string &str = *static_cast<std::string *>(buf);
            err = self->SetStringValue(str.data());
        } else {
            int size = engine->GetSizeOfPrimitiveType(typeId);
            if (size == 0) {
                if (type->GetFlags() & asOBJ_POD) {
                    err = self->SetValue(buf, size);
                } else {
                    ctx->SetException("Cannot write non-POD objects to buffer");
                    gen->SetReturnDWord(CKERR_INVALIDPARAMETER);
                    return;
                }
            }
        }
    } else {
        int size = engine->GetSizeOfPrimitiveType(typeId);
        if (size != 0) {
            err = self->SetValue(buf, size);
        }
    }

    gen->SetReturnDWord(err);
}

template <typename T>
static void RegisterCKParameterMembers(asIScriptEngine *engine, const char *name) {
    int r = 0;

    RegisterCKObjectMembers<T>(engine, name);

    r = engine->RegisterObjectMethod(name, "CKObject@ GetValueObject(bool update = true)", asFUNCTIONPR([](T *self, bool update) { return self->GetValueObject(update); }, (T *, bool), CKObject*), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // r = engine->RegisterObjectMethod(name, "CKERROR GetValue(NativePointer buf, bool update = true)", asFUNCTIONPR([](T *self, NativePointer buf, bool update) { return self->GetValue(buf.Get(), update); }, (T *, NativePointer, bool), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    // r = engine->RegisterObjectMethod(name, "CKERROR SetValue(NativePointer buf, int size = 0)", asFUNCTIONPR([](T *self, NativePointer buf, int size) { return self->SetValue(buf.Get(), size); }, (T *, NativePointer, int), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKERROR GetValue(?&out value, bool update = true)", asFUNCTION(CKParameterGetValueGeneric), asCALL_GENERIC); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKERROR SetValue(?&in value)", asFUNCTION(CKParameterSetValueGeneric), asCALL_GENERIC); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKERROR CopyValue(CKParameter@ param, bool updateParam = true)", asFUNCTIONPR([](T *self, CKParameter *param, bool updateParam) { return self->CopyValue(param, updateParam); }, (T *, CKParameter *, bool), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "bool IsCompatibleWith(CKParameter@ param)", asFUNCTIONPR([](T *self, CKParameter* param) -> bool { return self->IsCompatibleWith(param); }, (T *, CKParameter*), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "int GetDataSize()", asMETHODPR(T, GetDataSize, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "NativePointer GetReadDataPtr(bool update = true)", asFUNCTIONPR([](T *self, bool update) { return NativePointer(self->GetReadDataPtr(update)); }, (T *, bool), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "NativePointer GetWriteDataPtr()", asFUNCTIONPR([](T *self) { return NativePointer(self->GetWriteDataPtr()); }, (T *), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "CKERROR SetStringValue(const string &in value)", asFUNCTIONPR([](T *self, const std::string &value) { return self->SetStringValue(const_cast<CKSTRING>(value.c_str())); }, (T *, const std::string &), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "int GetStringValue(const string &in, bool = true)", asFUNCTIONPR([](T *self, const std::string &value, bool flag) { return self->GetStringValue(const_cast<CKSTRING>(value.c_str()), flag); }, (T *, const std::string &, bool), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "CKParameterType GetType()", asMETHODPR(T, GetType, (), CKParameterType), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetType(CKParameterType type)", asMETHODPR(T, SetType, (CKParameterType), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKGUID GetGUID()", asMETHODPR(T, GetGUID, (), CKGUID), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetGUID(CKGUID guid)", asMETHODPR(T, SetGUID, (CKGUID), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CK_CLASSID GetParameterClassID()", asMETHODPR(T, GetParameterClassID, (), CK_CLASSID), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void SetOwner(CKObject@ obj)", asMETHODPR(T, SetOwner, (CKObject*), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKObject@ GetOwner()", asMETHODPR(T, GetOwner, (), CKObject*), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void Enable(bool act = true)", asFUNCTIONPR([](T *self, bool act) { self->Enable(act); }, (T *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool IsEnabled()", asFUNCTIONPR([](T *self) -> bool { return self->IsEnabled(); }, (T *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // r = engine->RegisterObjectMethod(name, "CKERROR CreateDefaultValue()", asMETHODPR(T, CreateDefaultValue, (), CKERROR), asCALL_THISCALL); assert(r >= 0);
    // r = engine->RegisterObjectMethod(name, "void MessageDeleteAfterUse(bool act)", asFUNCTIONPR([](T *obj, bool act) { obj->MessageDeleteAfterUse(act); }, (T *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKParameterTypeDesc &GetParameterType()", asMETHODPR(T, GetParameterType, (), CKParameterTypeDesc*), asCALL_THISCALL); assert(r >= 0);
    // r = engine->RegisterObjectMethod(name, "bool IsCandidateForFixedSize(CKParameterTypeDesc &in type)", asFUNCTIONPR([](T *obj, CKParameterTypeDesc &type) { return obj->IsCandidateForFixedSize(&type); }, (T *, CKParameterTypeDesc &), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    if (strcmp(name, "CKParameter") != 0) {
        RegisterCKObjectCast<T, CKParameter>(engine, name, "CKParameter");
    }
}

void RegisterCKParameter(asIScriptEngine *engine) {
    assert(engine != nullptr);

    RegisterCKParameterMembers<CKParameter>(engine, "CKParameter");
}

template <typename T>
static void RegisterCKParameterOutMembers(asIScriptEngine *engine, const char *name) {
    int r = 0;

    RegisterCKParameterMembers<T>(engine, name);

    r = engine->RegisterObjectMethod(name, "void DataChanged()", asMETHODPR(T, DataChanged, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKERROR AddDestination(CKParameter@ param, bool checkType = true)", asFUNCTIONPR([](T *self, CKParameter *param, bool checkType) { return self->AddDestination(param, checkType); }, (T *, CKParameter *, bool), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void RemoveDestination(CKParameter@ param)", asMETHODPR(T, RemoveDestination, (CKParameter*), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "int GetDestinationCount()", asMETHODPR(T, GetDestinationCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKParameter@ GetDestination(int pos)", asMETHODPR(T, GetDestination, (int), CKParameter*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void RemoveAllDestinations()", asMETHODPR(T, RemoveAllDestinations, (), void), asCALL_THISCALL); assert(r >= 0);

    // r = engine->RegisterObjectMethod(name, "void Update()", asMETHODPR(T, Update, (), void), asCALL_THISCALL); assert(r >= 0);

    if (strcmp(name, "CKParameterOut") != 0) {
        RegisterCKObjectCast<T, CKParameterOut>(engine, name, "CKParameterOut");
    }
}

void RegisterCKParameterOut(asIScriptEngine *engine) {
    assert(engine != nullptr);

    RegisterCKParameterOutMembers<CKParameterOut>(engine, "CKParameterOut");
}

template <typename T>
static void RegisterCKParameterLocalMembers(asIScriptEngine *engine, const char *name) {
    int r = 0;

    RegisterCKParameterMembers<T>(engine, name);

    r = engine->RegisterObjectMethod(name, "void SetAsMyselfParameter(bool act)", asFUNCTIONPR([](T *self, bool act) { self->SetAsMyselfParameter(act); }, (T *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool IsMyselfParameter()", asFUNCTIONPR([](T *self) -> bool { return self->IsMyselfParameter(); }, (T *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    if (strcmp(name, "CKParameterLocal") != 0) {
        RegisterCKObjectCast<T, CKParameterLocal>(engine, name, "CKParameterLocal");
    }
}

void RegisterCKParameterLocal(asIScriptEngine *engine) {
    assert(engine != nullptr);

    RegisterCKParameterLocalMembers<CKParameterLocal>(engine, "CKParameterLocal");
}

template <typename T>
static void RegisterCKParameterOperationMembers(asIScriptEngine *engine, const char *name) {
    int r = 0;

    RegisterCKObjectMembers<T>(engine, name);

    r = engine->RegisterObjectMethod(name, "CKParameterIn@ GetInParameter1()", asMETHODPR(T, GetInParameter1, (), CKParameterIn *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKParameterIn@ GetInParameter2()", asMETHODPR(T, GetInParameter2, (), CKParameterIn *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKParameterOut@ GetOutParameter()", asMETHODPR(T, GetOutParameter, (), CKParameterOut *), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "CKBehavior@ GetOwner()", asMETHODPR(T, GetOwner, (), CKBehavior *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetOwner(CKBehavior@)", asMETHODPR(T, SetOwner, (CKBehavior *), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "CKERROR DoOperation()", asMETHODPR(T, DoOperation, (), CKERROR), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "CKGUID GetOperationGuid()", asMETHODPR(T, GetOperationGuid, (), CKGUID), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void Reconstruct(const string &in name, CKGUID opGuid, CKGUID resGuid, CKGUID p1Guid, CKGUID p2Guid)", asFUNCTIONPR([](T *self, const std::string &name, CKGUID opGuid, CKGUID resGuid, CKGUID p1Guid, CKGUID p2Guid) { self->Reconstruct(const_cast<CKSTRING>(name.c_str()), opGuid, resGuid, p1Guid, p2Guid); }, (T *, const std::string&, CKGUID, CKGUID, CKGUID, CKGUID), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "NativePointer GetOperationFunction()", asFUNCTIONPR([](T *self) { return NativePointer(self->GetOperationFunction()); }, (T *), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    if (strcmp(name, "CKParameterOperation") != 0) {
        RegisterCKObjectCast<T, CKParameterOperation>(engine, name, "CKParameterOperation");
    }
}

void RegisterCKParameterOperation(asIScriptEngine *engine) {
    assert(engine != nullptr);

    RegisterCKParameterOperationMembers<CKParameterOperation>(engine, "CKParameterOperation");
}

void RegisterCKBehaviorLink(asIScriptEngine *engine) {
    assert(engine != nullptr);

    int r = 0;

    RegisterCKObjectMembers<CKBehaviorLink>(engine, "CKBehaviorLink");

    r = engine->RegisterObjectMethod("CKBehaviorLink", "CKERROR SetOutBehaviorIO(CKBehaviorIO@ ckbioin)", asMETHODPR(CKBehaviorLink, SetOutBehaviorIO, (CKBehaviorIO *), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehaviorLink", "CKERROR SetInBehaviorIO(CKBehaviorIO@ ckbioout)", asMETHODPR(CKBehaviorLink, SetInBehaviorIO, (CKBehaviorIO *), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehaviorLink", "CKBehaviorIO@ GetOutBehaviorIO()", asMETHODPR(CKBehaviorLink, GetOutBehaviorIO, (), CKBehaviorIO *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehaviorLink", "CKBehaviorIO@ GetInBehaviorIO()", asMETHODPR(CKBehaviorLink, GetInBehaviorIO, (), CKBehaviorIO *), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKBehaviorLink", "int GetActivationDelay()", asMETHODPR(CKBehaviorLink, GetActivationDelay, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehaviorLink", "void SetActivationDelay(int delay)", asMETHODPR(CKBehaviorLink, SetActivationDelay, (int), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehaviorLink", "void ResetActivationDelay()", asMETHODPR(CKBehaviorLink, ResetActivationDelay, (), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKBehaviorLink", "void SetInitialActivationDelay(int delay)", asMETHODPR(CKBehaviorLink, SetInitialActivationDelay, (int), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehaviorLink", "int GetInitialActivationDelay()", asMETHODPR(CKBehaviorLink, GetInitialActivationDelay, (), int), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKBehaviorLink", "CKDWORD GetFlags()", asMETHODPR(CKBehaviorLink, GetFlags, (), CKDWORD), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehaviorLink", "void SetFlags(CKDWORD flags)", asMETHODPR(CKBehaviorLink, SetFlags, (CKDWORD), void), asCALL_THISCALL); assert(r >= 0);
}

void RegisterCKBehaviorIO(asIScriptEngine *engine) {
    assert(engine != nullptr);

    int r = 0;

    RegisterCKObjectMembers<CKBehaviorIO>(engine, "CKBehaviorIO");

    r = engine->RegisterObjectMethod("CKBehaviorIO", "void SetType(int type)", asMETHODPR(CKBehaviorIO, SetType, (int), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehaviorIO", "int GetType()", asMETHODPR(CKBehaviorIO, GetType, (), int), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKBehaviorIO", "void Activate(bool active = true)", asFUNCTIONPR([](CKBehaviorIO *self, bool active) { self->Activate(active); }, (CKBehaviorIO *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehaviorIO", "bool IsActive()", asFUNCTIONPR([](CKBehaviorIO *self) -> bool { return self->IsActive(); }, (CKBehaviorIO *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKBehaviorIO", "CKBehavior@ GetOwner()", asMETHODPR(CKBehaviorIO, GetOwner, (), CKBehavior *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehaviorIO", "void SetOwner(CKBehavior@ owner)", asMETHODPR(CKBehaviorIO, SetOwner, (CKBehavior *), void), asCALL_THISCALL); assert(r >= 0);

    // r = engine->RegisterObjectMethod("CKBehaviorIO", "void SortLinks()", asMETHODPR(CKBehaviorIO, SortLinks, (), void), asCALL_THISCALL); assert(r >= 0);
}

static void CKRenderContext_RenderCallback(CKRenderContext *dev, void *data) {
    auto *func = static_cast<asIScriptFunction *>(data);
    if (!func)
        return;

    asIScriptEngine *engine = func->GetEngine();
    asIScriptContext *ctx = engine->RequestContext();

    int r = 0;
    if (func->GetFuncType() == asFUNC_DELEGATE) {
        asIScriptFunction *callback = func->GetDelegateFunction();
        void *callbackObject = func->GetDelegateObject();
        r = ctx->Prepare(callback);
        ctx->SetObject(callbackObject);
    } else {
        r = ctx->Prepare(func);
    }

    if (r < 0) {
        engine->ReturnContext(ctx);
        return;
    }

    ctx->SetArgObject(0, dev);

    ctx->Execute();

    engine->ReturnContext(ctx);

    if (IsMarkedAsTemporary(func)) {
        func->Release();
        ClearTemporaryMark(func);
        MarkAsReleasedOnce(func);
    }
}

void RegisterCKRenderContext(asIScriptEngine *engine) {
    assert(engine != nullptr);

    int r = 0;

    RegisterCKObjectMembers<CKRenderContext>(engine, "CKRenderContext");

    r = engine->RegisterObjectMethod("CKRenderContext", "void AddObject(CKRenderObject@ ojb)", asMETHODPR(CKRenderContext, AddObject, (CKRenderObject *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "void AddObjectWithHierarchy(CKRenderObject@ obj)", asMETHODPR(CKRenderContext, AddObjectWithHierarchy, (CKRenderObject *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "void RemoveObject(CKRenderObject@ obj)", asMETHODPR(CKRenderContext, RemoveObject, (CKRenderObject *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "bool IsObjectAttached(CKRenderObject@ obj)", asFUNCTIONPR([](CKRenderContext *self, CKRenderObject *obj) -> bool { return self->IsObjectAttached(obj); }, (CKRenderContext *, CKRenderObject *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKRenderContext", "const XObjectArray &Compute3dRootObjects()", asMETHODPR(CKRenderContext, Compute3dRootObjects, (), const XObjectArray&), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "const XObjectArray &Compute2dRootObjects()", asMETHODPR(CKRenderContext, Compute2dRootObjects, (), const XObjectArray&), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKRenderContext", "CK2dEntity@ Get2dRoot(bool background)", asFUNCTIONPR([](CKRenderContext *self, bool background) { return self->Get2dRoot(background); }, (CKRenderContext *, bool), CK2dEntity *), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKRenderContext", "void DetachAll()", asMETHODPR(CKRenderContext, DetachAll, (), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKRenderContext", "void ForceCameraSettingsUpdate()", asMETHODPR(CKRenderContext, ForceCameraSettingsUpdate, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "void PrepareCameras(CK_RENDER_FLAGS flags = CK_RENDER_USECURRENTSETTINGS)", asMETHODPR(CKRenderContext, PrepareCameras, (CK_RENDER_FLAGS), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKRenderContext", "CKERROR Clear(CK_RENDER_FLAGS flags = CK_RENDER_USECURRENTSETTINGS, CKDWORD = 0)", asMETHODPR(CKRenderContext, Clear, (CK_RENDER_FLAGS, CKDWORD), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "CKERROR DrawScene(CK_RENDER_FLAGS flags = CK_RENDER_USECURRENTSETTINGS)", asMETHODPR(CKRenderContext, DrawScene, (CK_RENDER_FLAGS), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "CKERROR BackToFront(CK_RENDER_FLAGS flags = CK_RENDER_USECURRENTSETTINGS)", asMETHODPR(CKRenderContext, BackToFront, (CK_RENDER_FLAGS), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "CKERROR Render(CK_RENDER_FLAGS flags = CK_RENDER_USECURRENTSETTINGS)", asMETHODPR(CKRenderContext, Render, (CK_RENDER_FLAGS), CKERROR), asCALL_THISCALL); assert(r >= 0);

    // r = engine->RegisterObjectMethod("CKRenderContext", "void AddPreRenderCallBack(uintptr_t, uintptr_t, bool = false)", asMETHODPR(CKRenderContext, AddPreRenderCallBack, (CK_RENDERCALLBACK, void *, CKBOOL), void), asCALL_THISCALL); assert(r >= 0);
    // r = engine->RegisterObjectMethod("CKRenderContext", "void RemovePreRenderCallBack(uintptr_t, uintptr_t)", asMETHODPR(CKRenderContext, RemovePreRenderCallBack, (CK_RENDERCALLBACK, void *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "void AddPreRenderCallBack(CK_RENDERCALLBACK@ callback, bool temporary = false)", asFUNCTIONPR([](CKRenderContext *self, asIScriptFunction *scriptFunc, bool temporary) {
        scriptFunc->AddRef(); if (temporary) MarkAsTemporary(scriptFunc); self->AddPreRenderCallBack(CKRenderContext_RenderCallback, scriptFunc, temporary);
    }, (CKRenderContext *, asIScriptFunction *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "void RemovePreRenderCallBack(CK_RENDERCALLBACK@ callback)", asFUNCTIONPR([](CKRenderContext *self, asIScriptFunction *scriptFunc) {
        self->RemovePreRenderCallBack(CKRenderContext_RenderCallback, scriptFunc); if (IsMarkedAsReleasedOnce(scriptFunc)) ClearReleasedOnceMark(scriptFunc); else scriptFunc->Release();
    }, (CKRenderContext *, asIScriptFunction *), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // r = engine->RegisterObjectMethod("CKRenderContext", "void AddPostRenderCallBack(uintptr_t, uintptr_t, bool = false, bool = false)", asMETHODPR(CKRenderContext, AddPostRenderCallBack, (CK_RENDERCALLBACK, void *, CKBOOL, CKBOOL), void), asCALL_THISCALL); assert(r >= 0);
    // r = engine->RegisterObjectMethod("CKRenderContext", "void RemovePostRenderCallBack(uintptr_t, uintptr_t)", asMETHODPR(CKRenderContext, RemovePostRenderCallBack, (CK_RENDERCALLBACK, void *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "void AddPostRenderCallBack(CK_RENDERCALLBACK@ callback, bool temporary = false)", asFUNCTIONPR([](CKRenderContext *self, asIScriptFunction *scriptFunc, bool temporary) {
        scriptFunc->AddRef(); if (temporary) MarkAsTemporary(scriptFunc); self->AddPostRenderCallBack(CKRenderContext_RenderCallback, scriptFunc, temporary);
    }, (CKRenderContext *, asIScriptFunction *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "void RemovePostRenderCallBack(CK_RENDERCALLBACK@ callback)", asFUNCTIONPR([](CKRenderContext *self, asIScriptFunction *scriptFunc) {
        self->RemovePostRenderCallBack(CKRenderContext_RenderCallback, scriptFunc); if (IsMarkedAsReleasedOnce(scriptFunc)) ClearReleasedOnceMark(scriptFunc); else scriptFunc->Release();
    }, (CKRenderContext *, asIScriptFunction *), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // r = engine->RegisterObjectMethod("CKRenderContext", "void AddPostSpriteRenderCallBack(uintptr_t, uintptr_t, bool = false)", asMETHODPR(CKRenderContext, AddPostSpriteRenderCallBack, (CK_RENDERCALLBACK, void *, CKBOOL), void), asCALL_THISCALL); assert(r >= 0);
    // r = engine->RegisterObjectMethod("CKRenderContext", "void RemovePostSpriteRenderCallBack(uintptr_t, uintptr_t)", asMETHODPR(CKRenderContext, RemovePostSpriteRenderCallBack, (CK_RENDERCALLBACK, void *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "void AddPostSpriteRenderCallBack(CK_RENDERCALLBACK@ callback, bool temporary = false)", asFUNCTIONPR([](CKRenderContext *self, asIScriptFunction *scriptFunc, bool temporary) {
        scriptFunc->AddRef(); if (temporary) MarkAsTemporary(scriptFunc); self->AddPostSpriteRenderCallBack(CKRenderContext_RenderCallback, scriptFunc, temporary);
    }, (CKRenderContext *, asIScriptFunction *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "void RemovePostSpriteRenderCallBack(CK_RENDERCALLBACK@ callback)", asFUNCTIONPR([](CKRenderContext *self, asIScriptFunction *scriptFunc) {
        self->RemovePostSpriteRenderCallBack(CKRenderContext_RenderCallback, scriptFunc); if (IsMarkedAsReleasedOnce(scriptFunc)) ClearReleasedOnceMark(scriptFunc); else scriptFunc->Release();
    }, (CKRenderContext *, asIScriptFunction *), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKRenderContext", "VxDrawPrimitiveData &GetDrawPrimitiveStructure(CKRST_DPFLAGS flags, int vretexCount)", asMETHODPR(CKRenderContext, GetDrawPrimitiveStructure, (CKRST_DPFLAGS, int), VxDrawPrimitiveData *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "CKWORD &GetDrawPrimitiveIndices(int indicesCount)", asMETHODPR(CKRenderContext, GetDrawPrimitiveIndices, (int), CKWORD*), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKRenderContext", "void Transform(VxVector &out dest, const VxVector &in src, CK3dEntity@ ref = null)", asMETHODPR(CKRenderContext, Transform, (VxVector *, VxVector *, CK3dEntity *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "void TransformVertices(int vertexCount, VxTransformData &out data, CK3dEntity@ ref = null)", asMETHODPR(CKRenderContext, TransformVertices, (int, VxTransformData*, CK3dEntity *), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKRenderContext", "CKERROR GoFullScreen(int width = 640, int height = 480, int bpp = -1, int driver = 0, int refreshRate = 0)", asMETHODPR(CKRenderContext, GoFullScreen, (int, int, int, int, int), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "CKERROR StopFullScreen()", asMETHODPR(CKRenderContext, StopFullScreen, (), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "bool IsFullScreen()", asFUNCTIONPR([](CKRenderContext *self) -> bool { return self->IsFullScreen(); }, (CKRenderContext *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKRenderContext", "int GetDriverIndex()", asMETHODPR(CKRenderContext, GetDriverIndex, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "bool ChangeDriver(int newDriver)", asFUNCTIONPR([](CKRenderContext *ctx, int newDriver) -> bool { return ctx->ChangeDriver(newDriver); }, (CKRenderContext *, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKRenderContext", "WIN_HANDLE GetWindowHandle()", asMETHODPR(CKRenderContext, GetWindowHandle, (), WIN_HANDLE), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKRenderContext", "void ScreenToClient(Vx2DVector &in iPoint, Vx2DVector &out oPoint)", asFUNCTIONPR([](CKRenderContext *dev, Vx2DVector *iPoint, Vx2DVector *oPoint) { dev->ScreenToClient(iPoint); *oPoint = *iPoint; }, (CKRenderContext *, Vx2DVector *, Vx2DVector *), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "void ClientToScreen(Vx2DVector &in iPoint, Vx2DVector &out oPoint)", asFUNCTIONPR([](CKRenderContext *dev, Vx2DVector *iPoint, Vx2DVector *oPoint) { dev->ClientToScreen(iPoint); *oPoint = *iPoint; }, (CKRenderContext *, Vx2DVector *, Vx2DVector *), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKRenderContext", "CKERROR SetWindowRect(const VxRect &in rect, CKDWORD flags = 0)", asMETHODPR(CKRenderContext, SetWindowRect, (VxRect&, CKDWORD), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "void GetWindowRect(VxRect &out rect, bool screenRelative = false)", asFUNCTIONPR([](CKRenderContext *self, VxRect &rect, bool screenRelative) { self->GetWindowRect(rect, screenRelative); }, (CKRenderContext*, VxRect&, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKRenderContext", "int GetHeight()", asMETHODPR(CKRenderContext, GetHeight, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "int GetWidth()", asMETHODPR(CKRenderContext, GetWidth, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "CKERROR Resize(int posX = 0, int poxY = 0, int sizeX = 0, int sizeY = 0, CKDWORD flags = 0)", asMETHODPR(CKRenderContext, Resize, (int, int, int, int, CKDWORD), CKERROR), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKRenderContext", "void SetViewRect(const VxRect &in rect)", asMETHODPR(CKRenderContext, SetViewRect, (VxRect&), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "void GetViewRect(VxRect &out rect)", asMETHODPR(CKRenderContext, GetViewRect, (VxRect&), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKRenderContext", "VX_PIXELFORMAT GetPixelFormat(int &in bpp = void, int &in zbpp = void, int &in stencilBpp = void)", asMETHODPR(CKRenderContext, GetPixelFormat, (int*, int*, int*), VX_PIXELFORMAT), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKRenderContext", "void SetState(VXRENDERSTATETYPE state, CKDWORD value)", asMETHODPR(CKRenderContext, SetState, (VXRENDERSTATETYPE, CKDWORD), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "CKDWORD GetState(VXRENDERSTATETYPE state)", asMETHODPR(CKRenderContext, GetState, (VXRENDERSTATETYPE), CKDWORD), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKRenderContext", "bool SetTexture(CKTexture@ tex, bool clamped = false, int stage = 0)", asFUNCTIONPR([](CKRenderContext *self, CKTexture *tex, bool clamped, int stage) -> bool { return self->SetTexture(tex, clamped, stage); }, (CKRenderContext *, CKTexture *, bool, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "bool SetTextureStageState(CKRST_TEXTURESTAGESTATETYPE state, CKDWORD value, int stage = 0)", asFUNCTIONPR([](CKRenderContext *self, CKRST_TEXTURESTAGESTATETYPE state, CKDWORD value, int stage) -> bool { return self->SetTextureStageState(state, value, stage); }, (CKRenderContext *, CKRST_TEXTURESTAGESTATETYPE, CKDWORD, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // r = engine->RegisterObjectMethod("CKRenderContext", "CKRasterizerContext@ GetRasterizerContext()", asMETHODPR(CKRenderContext, GetRasterizerContext, (), CKRasterizerContext*), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKRenderContext", "void SetClearBackground(bool clearBack = true)", asFUNCTIONPR([](CKRenderContext *self, bool clearBack) { self->SetClearBackground(clearBack); }, (CKRenderContext *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "bool GetClearBackground()", asFUNCTIONPR([](CKRenderContext *self) -> bool { return self->GetClearBackground(); }, (CKRenderContext *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKRenderContext", "void SetClearZBuffer(bool clearZ = true)", asFUNCTIONPR([](CKRenderContext *self, bool clearZ) { self->SetClearZBuffer(clearZ); }, (CKRenderContext *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "bool GetClearZBuffer()", asFUNCTIONPR([](CKRenderContext *self) -> bool { return self->GetClearZBuffer(); }, (CKRenderContext *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKRenderContext", "void GetGlobalRenderMode(VxShadeType &out shading, bool &out texture, bool &out wireframe)", asFUNCTIONPR([](CKRenderContext *self, VxShadeType *shading, bool *texture, bool *wireframe) { CKBOOL t; CKBOOL w; self->GetGlobalRenderMode(shading, &t, &w); *texture = t; *wireframe = w; }, (CKRenderContext *, VxShadeType *, bool *, bool *), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "void SetGlobalRenderMode(VxShadeType shading = GouraudShading, bool texture = true, bool wireframe = false)", asFUNCTIONPR([](CKRenderContext *self, VxShadeType shading, bool texture, bool wireframe) { self->SetGlobalRenderMode(shading, texture, wireframe); }, (CKRenderContext *, VxShadeType, bool, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKRenderContext", "void SetCurrentRenderOptions(CKDWORD flags)", asMETHODPR(CKRenderContext, SetCurrentRenderOptions, (CKDWORD), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "CKDWORD GetCurrentRenderOptions()", asMETHODPR(CKRenderContext, GetCurrentRenderOptions, (), CKDWORD), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "void ChangeCurrentRenderOptions(CKDWORD add, CKDWORD remove)", asMETHODPR(CKRenderContext, ChangeCurrentRenderOptions, (CKDWORD, CKDWORD), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKRenderContext", "void SetCurrentExtents(const VxRect &in extents)", asMETHODPR(CKRenderContext, SetCurrentExtents, (VxRect&), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "void GetCurrentExtents(VxRect &out extents)", asMETHODPR(CKRenderContext, GetCurrentExtents, (VxRect&), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKRenderContext", "void SetAmbientLight(float r, float g, float b)", asMETHODPR(CKRenderContext, SetAmbientLight, (float, float, float), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "void SetAmbientLight(CKDWORD color)", asMETHODPR(CKRenderContext, SetAmbientLight, (CKDWORD), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "CKDWORD GetAmbientLight()", asMETHODPR(CKRenderContext, GetAmbientLight, (), CKDWORD), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKRenderContext", "void SetFogMode(VXFOG_MODE mode)", asMETHODPR(CKRenderContext, SetFogMode, (VXFOG_MODE), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "void SetFogStart(float start)", asMETHODPR(CKRenderContext, SetFogStart, (float), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "void SetFogEnd(float end)", asMETHODPR(CKRenderContext, SetFogEnd, (float), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "void SetFogDensity(float density)", asMETHODPR(CKRenderContext, SetFogDensity, (float), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "void SetFogColor(CKDWORD color)", asMETHODPR(CKRenderContext, SetFogColor, (CKDWORD), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKRenderContext", "VXFOG_MODE GetFogMode()", asMETHODPR(CKRenderContext, GetFogMode, (), VXFOG_MODE), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "CKDWORD GetFogColor()", asMETHODPR(CKRenderContext, GetFogColor, (), CKDWORD), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "float GetFogStart()", asMETHODPR(CKRenderContext, GetFogStart, (), float), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "float GetFogEnd()", asMETHODPR(CKRenderContext, GetFogEnd, (), float), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "float GetFogDensity()", asMETHODPR(CKRenderContext, GetFogDensity, (), float), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKRenderContext", "bool DrawPrimitive(VXPRIMITIVETYPE pType, NativePointer indices, int indexCount, VxDrawPrimitiveData &in data)", asFUNCTIONPR([](CKRenderContext *self, VXPRIMITIVETYPE pType, NativePointer indices, int indexCount, VxDrawPrimitiveData *data) -> bool { return self->DrawPrimitive(pType, reinterpret_cast<CKWORD *>(indices.Get()), indexCount, data); }, (CKRenderContext *, VXPRIMITIVETYPE, NativePointer, int, VxDrawPrimitiveData *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKRenderContext", "void SetWorldTransformationMatrix(const VxMatrix &in mat)", asMETHODPR(CKRenderContext, SetWorldTransformationMatrix, (const VxMatrix &), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "void SetProjectionTransformationMatrix(const VxMatrix &in mat)", asMETHODPR(CKRenderContext, SetProjectionTransformationMatrix, (const VxMatrix &), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "void SetViewTransformationMatrix(const VxMatrix &in mat)", asMETHODPR(CKRenderContext, SetViewTransformationMatrix, (const VxMatrix &), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKRenderContext", "const VxMatrix &GetWorldTransformationMatrix()", asMETHODPR(CKRenderContext, GetWorldTransformationMatrix, (), const VxMatrix &), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "const VxMatrix &GetProjectionTransformationMatrix()", asMETHODPR(CKRenderContext, GetProjectionTransformationMatrix, (), const VxMatrix &), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "const VxMatrix &GetViewTransformationMatrix()", asMETHODPR(CKRenderContext, GetViewTransformationMatrix, (), const VxMatrix &), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKRenderContext", "bool SetUserClipPlane(CKDWORD clipPlaneIndex, const VxPlane &in planeEquation)", asFUNCTIONPR([](CKRenderContext *self, CKDWORD clipPlaneIndex, const VxPlane &planeEquation) -> bool { return self->SetUserClipPlane(clipPlaneIndex, planeEquation); }, (CKRenderContext *, CKDWORD, const VxPlane &), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "bool GetUserClipPlane(CKDWORD clipPlaneIndex, VxPlane &out planeEquation)", asFUNCTIONPR([](CKRenderContext *self, CKDWORD clipPlaneIndex, VxPlane &planeEquation) -> bool { return self->GetUserClipPlane(clipPlaneIndex, planeEquation); }, (CKRenderContext *, CKDWORD, VxPlane &), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

#if CKVERSION == 0x13022002
    r = engine->RegisterObjectMethod("CKRenderContext", "CKRenderObject@ Pick(int x, int y, CKPICKRESULT &out res, bool ignoreUnpickable = false)", asFUNCTIONPR([](CKRenderContext *self, int x, int y, CKPICKRESULT *res, bool ignoreUnpickable) { return self->Pick(x, y, res, ignoreUnpickable); }, (CKRenderContext *, int, int, CKPICKRESULT *, bool), CKRenderObject *), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "CKRenderObject@ Pick(CKPOINT pt, CKPICKRESULT &out res, bool ignoreUnpickable = false)", asFUNCTIONPR([](CKRenderContext *self, CKPOINT pt, CKPICKRESULT *res, bool ignoreUnpickable) { return self->Pick(pt, res, ignoreUnpickable); }, (CKRenderContext *, CKPOINT, CKPICKRESULT *, bool), CKRenderObject *), asCALL_CDECL_OBJFIRST); assert(r >= 0);
#else
    r = engine->RegisterObjectMethod("CKRenderContext", "CKRenderObject@ Pick(int x, int y, VxIntersectionDesc &out res, bool ignoreUnpickable = false)", asFUNCTIONPR([](CKRenderContext *self, int x, int y, VxIntersectionDesc *res, bool ignoreUnpickable) { return self->Pick(x, y, res, ignoreUnpickable); }, (CKRenderContext *, int, int, VxIntersectionDesc *, bool), CKRenderObject *), asCALL_CDECL_OBJFIRST); assert(r >= 0);
#endif

    r = engine->RegisterObjectMethod("CKRenderContext", "CKERROR RectPick(const VxRect &in rect, XObjectPointerArray &out objects, bool intersect = true)", asFUNCTIONPR([](CKRenderContext *self, const VxRect &rect, XObjectPointerArray &objects, bool intersect) { return self->RectPick(rect, objects, intersect); }, (CKRenderContext *, const VxRect &, XObjectPointerArray &, bool), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKRenderContext", "void AttachViewpointToCamera(CKCamera@ cam)", asMETHODPR(CKRenderContext, AttachViewpointToCamera, (CKCamera*), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "void DetachViewpointFromCamera()", asMETHODPR(CKRenderContext, DetachViewpointFromCamera, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "CKCamera@ GetAttachedCamera()", asMETHODPR(CKRenderContext, GetAttachedCamera, (), CKCamera *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "CK3dEntity@ GetViewpoint()", asMETHODPR(CKRenderContext, GetViewpoint, (), CK3dEntity *), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKRenderContext", "CKMaterial@ GetBackgroundMaterial()", asMETHODPR(CKRenderContext, GetBackgroundMaterial, (), CKMaterial *), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKRenderContext", "void GetBoundingBox(VxBbox &out bbox)", asMETHODPR(CKRenderContext, GetBoundingBox, (VxBbox *), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKRenderContext", "void GetStats(VxStats &out stats)", asMETHODPR(CKRenderContext, GetStats, (VxStats*), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKRenderContext", "void SetCurrentMaterial(CKMaterial@ mat, bool lit = true)", asFUNCTIONPR([](CKRenderContext *self, CKMaterial *mat, bool lit) { self->SetCurrentMaterial(mat, lit); }, (CKRenderContext *, CKMaterial *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKRenderContext", "void Activate(bool active = true)", asFUNCTIONPR([](CKRenderContext *self, bool active) { self->Activate(active); }, (CKRenderContext *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKRenderContext", "int DumpToMemory(const VxRect &in rect, VXBUFFER_TYPE bufferType, VxImageDescEx &out desc)", asMETHODPR(CKRenderContext, DumpToMemory, (const VxRect*, VXBUFFER_TYPE, VxImageDescEx&), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "int CopyToVideo(const VxRect &in rect, VXBUFFER_TYPE bufferType, VxImageDescEx &out desc)", asMETHODPR(CKRenderContext, CopyToVideo, (const VxRect*, VXBUFFER_TYPE, VxImageDescEx&), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "CKERROR DumpToFile(const string &in filename, const VxRect &in rect, VXBUFFER_TYPE bufferType)", asFUNCTIONPR([](CKRenderContext *self, const std::string &filename, const VxRect *rect, VXBUFFER_TYPE bufferType) { return self->DumpToFile(const_cast<CKSTRING>(filename.c_str()), rect, bufferType); }, (CKRenderContext *, const std::string &, const VxRect *, VXBUFFER_TYPE), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKRenderContext", "VxDirectXData &GetDirectXInfo()", asMETHODPR(CKRenderContext, GetDirectXInfo, (), VxDirectXData*), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKRenderContext", "void WarnEnterThread()", asMETHODPR(CKRenderContext, WarnEnterThread, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "void WarnExitThread()", asMETHODPR(CKRenderContext, WarnExitThread, (), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKRenderContext", "CK2dEntity@ Pick2D(const Vx2DVector &in v)", asMETHODPR(CKRenderContext, Pick2D, (const Vx2DVector&), CK2dEntity*), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKRenderContext", "bool SetRenderTarget(CKTexture@ texture = null, int cubeMapFace = 0)", asFUNCTIONPR([](CKRenderContext *self, CKTexture *texture, int cubeMapFace) -> bool { return self->SetRenderTarget(texture, cubeMapFace); }, (CKRenderContext *, CKTexture *, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKRenderContext", "void AddRemoveSequence(bool start)", asFUNCTIONPR([](CKRenderContext *self, bool start) { self->AddRemoveSequence(start); }, (CKRenderContext *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "void SetTransparentMode(bool trans)", asFUNCTIONPR([](CKRenderContext *self, bool trans) { self->SetTransparentMode(trans); }, (CKRenderContext *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "void AddDirtyRect(const CKRECT &in rect)", asMETHODPR(CKRenderContext, AddDirtyRect, (CKRECT *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "void RestoreScreenBackup()", asMETHODPR(CKRenderContext, RestoreScreenBackup, (), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKRenderContext", "CKDWORD GetStencilFreeMask()", asMETHODPR(CKRenderContext, GetStencilFreeMask, (), CKDWORD), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "void UsedStencilBits(CKDWORD stencilBits)", asMETHODPR(CKRenderContext, UsedStencilBits, (CKDWORD), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "int GetFirstFreeStencilBits()", asMETHODPR(CKRenderContext, GetFirstFreeStencilBits, (), int), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKRenderContext", "VxDrawPrimitiveData &LockCurrentVB(CKDWORD vertexCount)", asMETHODPR(CKRenderContext, LockCurrentVB, (CKDWORD), VxDrawPrimitiveData *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "bool ReleaseCurrentVB()", asFUNCTIONPR([](CKRenderContext *self) -> bool { return self->ReleaseCurrentVB(); }, (CKRenderContext *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKRenderContext", "void SetTextureMatrix(const VxMatrix &in mat, int stage = 0)", asMETHODPR(CKRenderContext, SetTextureMatrix, (const VxMatrix &, int), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKRenderContext", "void SetStereoParameters(float eyeSeparation, float focalLength)", asMETHODPR(CKRenderContext, SetStereoParameters, (float, float), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKRenderContext", "void GetStereoParameters(float &out eyeSeparation, float &out focalLength)", asMETHODPR(CKRenderContext, GetStereoParameters, (float&, float&), void), asCALL_THISCALL); assert(r >= 0);
}

void RegisterCKSynchroObject(asIScriptEngine *engine) {
    assert(engine != nullptr);

    int r = 0;

    RegisterCKObjectMembers<CKSynchroObject>(engine, "CKSynchroObject");

    r = engine->RegisterObjectMethod("CKSynchroObject", "void Reset()", asMETHODPR(CKSynchroObject, Reset, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSynchroObject", "void SetRendezVousNumberOfWaiters(int waiters)", asMETHODPR(CKSynchroObject, SetRendezVousNumberOfWaiters, (int), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSynchroObject", "int GetRendezVousNumberOfWaiters()", asMETHODPR(CKSynchroObject, GetRendezVousNumberOfWaiters, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSynchroObject", "bool CanIPassRendezVous(CKBeObject@ asker)", asFUNCTIONPR([](CKSynchroObject *self, CKBeObject *asker) -> bool { return self->CanIPassRendezVous(asker); }, (CKSynchroObject *, CKBeObject *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSynchroObject", "int GetRendezVousNumberOfArrivedObjects()", asMETHODPR(CKSynchroObject, GetRendezVousNumberOfArrivedObjects, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSynchroObject", "CKBeObject@ GetRendezVousArrivedObject(int pos)", asMETHODPR(CKSynchroObject, GetRendezVousArrivedObject, (int), CKBeObject *), asCALL_THISCALL); assert(r >= 0);
}

void RegisterCKStateObject(asIScriptEngine *engine) {
    assert(engine != nullptr);

    int r = 0;

    RegisterCKObjectMembers<CKStateObject>(engine, "CKStateObject");

    r = engine->RegisterObjectMethod("CKStateObject", "bool IsStateActive()", asFUNCTIONPR([](CKStateObject *self) -> bool { return self->IsStateActive(); }, (CKStateObject *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateObject", "void EnterState()", asMETHODPR(CKStateObject, EnterState, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKStateObject", "void LeaveState()", asMETHODPR(CKStateObject, LeaveState, (), void), asCALL_THISCALL); assert(r >= 0);
}

void RegisterCKCriticalSectionObject(asIScriptEngine *engine) {
    assert(engine != nullptr);

    int r = 0;

    RegisterCKObjectMembers<CKCriticalSectionObject>(engine, "CKCriticalSectionObject");

    r = engine->RegisterObjectMethod("CKCriticalSectionObject", "void Reset()", asMETHODPR(CKCriticalSectionObject, Reset, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCriticalSectionObject", "bool EnterCriticalSection(CKBeObject@ asker)", asFUNCTIONPR([](CKCriticalSectionObject *self, CKBeObject *asker) -> bool { return self->EnterCriticalSection(asker); }, (CKCriticalSectionObject *, CKBeObject *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCriticalSectionObject", "bool LeaveCriticalSection(CKBeObject@ asker)", asFUNCTIONPR([](CKCriticalSectionObject *self, CKBeObject *asker) -> bool { return self->LeaveCriticalSection(asker); }, (CKCriticalSectionObject *, CKBeObject *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

}

void RegisterCKKinematicChain(asIScriptEngine *engine) {
    assert(engine != nullptr);

    int r = 0;

    RegisterCKObjectMembers<CKKinematicChain>(engine, "CKKinematicChain");

    r = engine->RegisterObjectMethod("CKKinematicChain", "float GetChainLength(CKBodyPart@ end = null)", asMETHODPR(CKKinematicChain, GetChainLength, (CKBodyPart*), float), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKKinematicChain", "int GetChainBodyCount(CKBodyPart@ end = null)", asMETHODPR(CKKinematicChain, GetChainBodyCount, (CKBodyPart*), int), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKKinematicChain", "CKBodyPart@ GetStartEffector()", asMETHODPR(CKKinematicChain, GetStartEffector, (), CKBodyPart*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKKinematicChain", "CKERROR SetStartEffector(CKBodyPart@ start)", asMETHODPR(CKKinematicChain, SetStartEffector, (CKBodyPart*), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKKinematicChain", "CKBodyPart@ GetEffector(int pos)", asMETHODPR(CKKinematicChain, GetEffector, (int), CKBodyPart*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKKinematicChain", "CKBodyPart@ GetEndEffector()", asMETHODPR(CKKinematicChain, GetEndEffector, (), CKBodyPart*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKKinematicChain", "CKERROR SetEndEffector(CKBodyPart@ end)", asMETHODPR(CKKinematicChain, SetEndEffector, (CKBodyPart*), CKERROR), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKKinematicChain", "CKERROR IKSetEffectorPos(const VxVector &in pos, CK3dEntity@ ref = null, CKBodyPart@ body = null)", asMETHODPR(CKKinematicChain, IKSetEffectorPos, (VxVector *, CK3dEntity *, CKBodyPart*), CKERROR), asCALL_THISCALL); assert(r >= 0);
}

void RegisterCKLayer(asIScriptEngine *engine) {
    assert(engine != nullptr);

    int r = 0;

    RegisterCKObjectMembers<CKLayer>(engine, "CKLayer");

    r = engine->RegisterObjectMethod("CKLayer", "void SetType(int type)", asMETHODPR(CKLayer, SetType, (int), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKLayer", "int GetType()", asMETHODPR(CKLayer, GetType, (), int), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKLayer", "void SetFormat(int format)", asMETHODPR(CKLayer, SetFormat, (int), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKLayer", "int GetFormat()", asMETHODPR(CKLayer, GetFormat, (), int), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKLayer", "void SetValue(int x, int y, NativePointer val)", asFUNCTIONPR([](CKLayer *layer, int x, int y, NativePointer val) { layer->SetValue(x, y, val.Get()); }, (CKLayer *, int, int, NativePointer), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKLayer", "void GetValue(int x, int y, NativePointer val)", asFUNCTIONPR([](CKLayer *layer, int x, int y, NativePointer val) { layer->GetValue(x, y, val.Get()); }, (CKLayer *, int, int, NativePointer), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKLayer", "bool SetValue2(int x, int y, NativePointer val)", asFUNCTIONPR([](CKLayer *layer, int x, int y, NativePointer val) -> bool { return layer->SetValue2(x, y, val.Get()); }, (CKLayer *, int, int, NativePointer), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKLayer", "bool GetValue2(int x, int y, NativePointer val)", asFUNCTIONPR([](CKLayer *layer, int x, int y, NativePointer val) -> bool { return layer->GetValue2(x, y, val.Get()); }, (CKLayer *, int, int, NativePointer), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKLayer", "CKSquare &GetSquareArray()", asMETHODPR(CKLayer, GetSquareArray, (), CKSquare*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKLayer", "void SetSquareArray(CKSquare &sqArray)", asMETHODPR(CKLayer, SetSquareArray, (CKSquare*), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKLayer", "void SetVisible(bool vis = true)", asFUNCTIONPR([](CKLayer *self, bool vis) { self->SetVisible(vis); }, (CKLayer *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    // r = engine->RegisterObjectMethod("CKLayer", "bool IsVisible()", asFUNCTIONPR([](CKLayer *layer) -> bool { return layer->IsVisible(); }, (CKLayer *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKLayer", "void InitOwner(CK_ID owner)", asMETHODPR(CKLayer, InitOwner, (CK_ID), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKLayer", "void SetOwner(CK_ID owner)", asMETHODPR(CKLayer, SetOwner, (CK_ID), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKLayer", "CK_ID GetOwner()", asMETHODPR(CKLayer, GetOwner, (), CK_ID), asCALL_THISCALL); assert(r >= 0);
}

template <typename T>
static void RegisterCKSceneObjectMembers(asIScriptEngine *engine, const char *name) {
    int r = 0;

    RegisterCKObjectMembers<T>(engine, name);

    r = engine->RegisterObjectMethod(name, "bool IsActiveInScene(CKScene@ scene)", asFUNCTIONPR([](T *self, CKScene *scene) -> bool { return self->IsActiveInScene(scene); }, (T *, CKScene *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool IsActiveInCurrentScene()", asFUNCTIONPR([](T *self) -> bool { return self->IsActiveInCurrentScene(); }, (T *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool IsInScene(CKScene@ scene)", asFUNCTIONPR([](T *self, CKScene *scene) -> bool { return self->IsInScene(scene); }, (T *, CKScene *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "int GetSceneInCount()", asMETHODPR(T, GetSceneInCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKScene@ GetSceneIn(int index)", asMETHODPR(T, GetSceneIn, (int), CKScene *), asCALL_THISCALL); assert(r >= 0);

    if (strcmp(name, "CKSceneObject") != 0) {
        RegisterCKObjectCast<T, CKSceneObject>(engine, name, "CKSceneObject");
    }
}

void RegisterCKSceneObject(asIScriptEngine *engine) {
    assert(engine != nullptr);

    RegisterCKSceneObjectMembers<CKSceneObject>(engine, "CKSceneObject");
}

template <int IO>
static void CKBehaviorGetParameterValueGeneric(asIScriptGeneric *gen) {
    asIScriptEngine *engine = gen->GetEngine();
    asIScriptContext *ctx = asGetActiveContext();

    auto *self = static_cast<CKBehavior *>(gen->GetObject());
    int pos = gen->GetArgDWord(0);
    const int typeId = gen->GetArgTypeId(1);
    void *buf = *static_cast<void **>(gen->GetAddressOfArg(1));

    CKERROR err = CK_OK;

    if (typeId & asTYPEID_SCRIPTOBJECT) {
        ctx->SetException("Cannot read script objects from buffer");
        gen->SetReturnDWord(CKERR_INVALIDPARAMETER);
        return;
    }

    if (typeId & asTYPEID_APPOBJECT) {
        asITypeInfo *type = engine->GetTypeInfoById(typeId);
        if (!type) {
            gen->SetReturnDWord(CKERR_INVALIDPARAMETER);
            return;
        }

        if (typeId & asTYPEID_OBJHANDLE) {
            ctx->SetException("Cannot read object handle from buffer");
            gen->SetReturnDWord(CKERR_INVALIDPARAMETER);
            return;
        }

        if (strcmp(type->GetName(), "string") == 0) {
            std::string &str = *static_cast<std::string *>(buf);
            CKSTRING value = nullptr;
            switch (IO) {
                case 0:
                    value = (CKSTRING) self->GetInputParameterReadDataPtr(pos);
                    break;
                case 1:
                    value = (CKSTRING) self->GetOutputParameterWriteDataPtr(pos);
                    break;
                case 2:
                    value = (CKSTRING) self->GetLocalParameterReadDataPtr(pos);
                    break;
                default:
                    ctx->SetException("Invalid parameter type");
                    gen->SetReturnDWord(CKERR_INVALIDPARAMETER);
                    return;
            }
            if (value)
                str = value;
            else
                err = CKERR_INVALIDPARAMETER;
        } else {
            int size = engine->GetSizeOfPrimitiveType(typeId);
            if (size == 0) {
                if (type->GetFlags() & asOBJ_POD) {
                    switch (IO) {
                        case 0:
                            err = self->GetInputParameterValue(pos, buf);
                        break;
                        case 1:
                            err = self->GetOutputParameterValue(pos, buf);
                        break;
                        case 2:
                            err = self->GetLocalParameterValue(pos, buf);
                        break;
                        default:
                            ctx->SetException("Invalid parameter type");
                            gen->SetReturnDWord(CKERR_INVALIDPARAMETER);
                            return;
                    }
                } else {
                    ctx->SetException("Cannot read non-POD objects from buffer");
                    gen->SetReturnDWord(CKERR_INVALIDPARAMETER);
                    return;
                }
            }
        }
    } else {
        int size = engine->GetSizeOfPrimitiveType(typeId);
        if (size != 0) {
            switch (IO) {
                case 0:
                    err = self->GetInputParameterValue(pos, buf);
                break;
                case 1:
                    err = self->GetOutputParameterValue(pos, buf);
                break;
                case 2:
                    err = self->GetLocalParameterValue(pos, buf);
                break;
                default:
                    ctx->SetException("Invalid parameter type");
                gen->SetReturnDWord(CKERR_INVALIDPARAMETER);
                return;
            }
        }
    }

    gen->SetReturnDWord(err);
}

template <int IO>
static void CKBehaviorSetParameterValueGeneric(asIScriptGeneric *gen) {
    asIScriptEngine *engine = gen->GetEngine();
    asIScriptContext *ctx = asGetActiveContext();

    auto *self = static_cast<CKBehavior *>(gen->GetObject());
    int pos = gen->GetArgDWord(0);
    const int typeId = gen->GetArgTypeId(1);
    void *buf = *static_cast<void **>(gen->GetAddressOfArg(1));

    CKERROR err = CK_OK;

    if (typeId & asTYPEID_SCRIPTOBJECT) {
        ctx->SetException("Cannot write script objects to buffer");
        gen->SetReturnDWord(CKERR_INVALIDPARAMETER);
        return;
    }

    if (typeId & asTYPEID_APPOBJECT) {
        asITypeInfo *type = engine->GetTypeInfoById(typeId);
        if (!type) {
            gen->SetReturnDWord(CKERR_INVALIDPARAMETER);
            return;
        }

        if (typeId & asTYPEID_OBJHANDLE) {
            ctx->SetException("Cannot write object handle from buffer");
            gen->SetReturnDWord(CKERR_INVALIDPARAMETER);
            return;
        }
        
        if (strcmp(type->GetName(), "string") == 0) {
            std::string &str = *static_cast<std::string *>(buf);
            switch (IO) {
                case 1:
                    err = self->SetOutputParameterValue(pos, str.data());
                break;
                case 2:
                    err = self->SetLocalParameterValue(pos, str.data());
                break;
                default:
                    ctx->SetException("Invalid parameter type");
                    gen->SetReturnDWord(CKERR_INVALIDPARAMETER);
                    return;
            }
        } else {
            int size = engine->GetSizeOfPrimitiveType(typeId);
            if (size == 0) {
                if (type->GetFlags() & asOBJ_POD) {
                    switch (IO) {
                        case 1:
                            err = self->SetOutputParameterValue(pos, buf);
                        break;
                        case 2:
                            err = self->SetLocalParameterValue(pos, buf);
                        break;
                        default:
                            ctx->SetException("Invalid parameter type");
                        gen->SetReturnDWord(CKERR_INVALIDPARAMETER);
                        return;
                    }
                } else {
                    ctx->SetException("Cannot write non-POD objects to buffer");
                    gen->SetReturnDWord(CKERR_INVALIDPARAMETER);
                    return;
                }
            }
        }
    } else {
        int size = engine->GetSizeOfPrimitiveType(typeId);
        if (size != 0) {
            switch (IO) {
                case 1:
                    err = self->SetOutputParameterValue(pos, buf);
                break;
                case 2:
                    err = self->SetLocalParameterValue(pos, buf);
                break;
                default:
                    ctx->SetException("Invalid parameter type");
                gen->SetReturnDWord(CKERR_INVALIDPARAMETER);
                return;
            }
        }
    }

    gen->SetReturnDWord(err);
}

void RegisterCKBehavior(asIScriptEngine *engine) {
    assert(engine != nullptr);

    int r = 0;

    RegisterCKSceneObjectMembers<CKBehavior>(engine, "CKBehavior");

    // Behavior type and flag
    r = engine->RegisterObjectMethod("CKBehavior", "CK_BEHAVIOR_TYPE GetType()", asMETHODPR(CKBehavior, GetType, (), CK_BEHAVIOR_TYPE), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "void SetType(CK_BEHAVIOR_TYPE type)", asMETHODPR(CKBehavior, SetType, (CK_BEHAVIOR_TYPE), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "void SetFlags(CK_BEHAVIOR_FLAGS flags)", asMETHODPR(CKBehavior, SetFlags, (CK_BEHAVIOR_FLAGS), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CK_BEHAVIOR_FLAGS GetFlags()", asMETHODPR(CKBehavior, GetFlags, (), CK_BEHAVIOR_FLAGS), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CK_BEHAVIOR_FLAGS ModifyFlags(CKDWORD add, CKDWORD remove)", asMETHODPR(CKBehavior, ModifyFlags, (CKDWORD, CKDWORD), CK_BEHAVIOR_FLAGS), asCALL_THISCALL); assert(r >= 0);

    // BuildingBlock or Graph
    r = engine->RegisterObjectMethod("CKBehavior", "void UseGraph()", asMETHODPR(CKBehavior, UseGraph, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "void UseFunction()", asMETHODPR(CKBehavior, UseFunction, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "int IsUsingFunction()", asFUNCTIONPR([](CKBehavior *self) -> bool { return self->IsUsingFunction(); }, (CKBehavior *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // Targetable Behavior
    r = engine->RegisterObjectMethod("CKBehavior", "bool IsTargetable()", asFUNCTIONPR([](CKBehavior *self) -> bool { return self->IsTargetable(); }, (CKBehavior *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKBeObject@ GetTarget()", asMETHODPR(CKBehavior, GetTarget, (), CKBeObject*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKERROR UseTarget(bool use = true)", asFUNCTIONPR([](CKBehavior *self, bool use) { return self->UseTarget(use); }, (CKBehavior *, bool), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "bool IsUsingTarget()", asFUNCTIONPR([](CKBehavior *self) -> bool { return self->IsUsingTarget(); }, (CKBehavior *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKParameterIn@ GetTargetParameter()", asMETHODPR(CKBehavior, GetTargetParameter, (), CKParameterIn *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "void SetAsTargetable(bool target = true)", asFUNCTIONPR([](CKBehavior *self, bool target) { self->SetAsTargetable(target); }, (CKBehavior *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKParameterIn@ ReplaceTargetParameter(CKParameterIn@ targetParam)", asMETHODPR(CKBehavior, ReplaceTargetParameter, (CKParameterIn *), CKParameterIn *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKParameterIn@ RemoveTargetParameter()", asMETHODPR(CKBehavior, RemoveTargetParameter, (), CKParameterIn *), asCALL_THISCALL); assert(r >= 0);

    // Compatible Class ID
    r = engine->RegisterObjectMethod("CKBehavior", "CK_CLASSID GetCompatibleClassID()", asMETHODPR(CKBehavior, GetCompatibleClassID, (), CK_CLASSID), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "void SetCompatibleClassID(CK_CLASSID cid)", asMETHODPR(CKBehavior, SetCompatibleClassID, (CK_CLASSID), void), asCALL_THISCALL); assert(r >= 0);

    // Function
    r = engine->RegisterObjectMethod("CKBehavior", "void SetFunction(NativePointer fct)", asFUNCTIONPR([](CKBehavior *self, NativePointer fct) { self->SetFunction(reinterpret_cast<CKBEHAVIORFCT>(fct.Get())); }, (CKBehavior *, NativePointer), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "NativePointer GetFunction()", asFUNCTIONPR([](CKBehavior *self) { return NativePointer(self->GetFunction()); }, (CKBehavior *), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // Callbacks
    r = engine->RegisterObjectMethod("CKBehavior", "void SetCallbackFunction(NativePointer fct)", asFUNCTIONPR([](CKBehavior *self, NativePointer fct) { self->SetCallbackFunction(reinterpret_cast<CKBEHAVIORCALLBACKFCT>(fct.Get())); }, (CKBehavior *, NativePointer), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "int CallCallbackFunction(CKDWORD message)", asMETHODPR(CKBehavior, CallCallbackFunction, (CKDWORD), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "int CallSubBehaviorsCallbackFunction(CKDWORD message, CKGUID &in behguid = void)", asMETHODPR(CKBehavior, CallSubBehaviorsCallbackFunction, (CKDWORD, CKGUID*), int), asCALL_THISCALL); assert(r >= 0);

    // Execution
    r = engine->RegisterObjectMethod("CKBehavior", "bool IsActive()", asFUNCTIONPR([](CKBehavior *self) -> bool { return self->IsActive(); }, (CKBehavior *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "int Execute(float delta)", asMETHODPR(CKBehavior, Execute, (float), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "bool IsParentScriptActiveInScene(CKScene@ scn)", asFUNCTIONPR([](CKBehavior *self, CKScene *scn) -> bool { return self->IsParentScriptActiveInScene(scn); }, (CKBehavior *, CKScene *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "int GetShortestDelay(CKBehavior@ beh)", asMETHODPR(CKBehavior, GetShortestDelay, (CKBehavior *), int), asCALL_THISCALL); assert(r >= 0);

    // Owner and Parent
    r = engine->RegisterObjectMethod("CKBehavior", "CKBeObject@ GetOwner()", asMETHODPR(CKBehavior, GetOwner, (), CKBeObject*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKBehavior@ GetParent()", asMETHODPR(CKBehavior, GetParent, (), CKBehavior *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKBehavior@ GetOwnerScript()", asMETHODPR(CKBehavior, GetOwnerScript, (), CKBehavior *), asCALL_THISCALL); assert(r >= 0);

    // Prototypes
    r = engine->RegisterObjectMethod("CKBehavior", "CKERROR InitFromPrototype(CKBehaviorPrototype &in proto)", asMETHODPR(CKBehavior, InitFromPrototype, (CKBehaviorPrototype*), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKERROR InitFromGuid(CKGUID guid)", asMETHODPR(CKBehavior, InitFromGuid, (CKGUID), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKERROR InitFctPtrFromGuid(CKGUID guid)", asMETHODPR(CKBehavior, InitFctPtrFromGuid, (CKGUID), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKERROR InitFctPtrFromPrototype(CKBehaviorPrototype &in proto)", asMETHODPR(CKBehavior, InitFctPtrFromPrototype, (CKBehaviorPrototype*), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKGUID GetPrototypeGuid()", asMETHODPR(CKBehavior, GetPrototypeGuid, (), CKGUID), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKBehaviorPrototype &GetPrototype()", asMETHODPR(CKBehavior, GetPrototype, (), CKBehaviorPrototype *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "string GetPrototypeName()", asFUNCTIONPR([](CKBehavior *self) -> std::string { return ScriptStringify(self->GetPrototypeName()); }, (CKBehavior *), std::string), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKDWORD GetVersion()", asMETHODPR(CKBehavior, GetVersion, (), CKDWORD), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "void SetVersion(CKDWORD version)", asMETHODPR(CKBehavior, SetVersion, (CKDWORD), void), asCALL_THISCALL); assert(r >= 0);

    // Outputs
    r = engine->RegisterObjectMethod("CKBehavior", "void ActivateOutput(int pos, bool active = true)", asFUNCTIONPR([](CKBehavior *self, int pos, bool active) { self->ActivateOutput(pos, active); }, (CKBehavior *, int, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "bool IsOutputActive(int pos)", asFUNCTIONPR([](CKBehavior *self, int pos) -> bool { return self->IsOutputActive(pos); }, (CKBehavior *, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKBehaviorIO@ RemoveOutput(int pos)", asMETHODPR(CKBehavior, RemoveOutput, (int), CKBehaviorIO *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKERROR DeleteOutput(int pos)", asMETHODPR(CKBehavior, DeleteOutput, (int), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKBehaviorIO@ GetOutput(int pos)", asMETHODPR(CKBehavior, GetOutput, (int), CKBehaviorIO *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "int GetOutputCount()", asMETHODPR(CKBehavior, GetOutputCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "int GetOutputPosition(CKBehaviorIO@ pbio)", asMETHODPR(CKBehavior, GetOutputPosition, (CKBehaviorIO *), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "int AddOutput(const string &in name)", asFUNCTIONPR([](CKBehavior *self, const std::string &name) { return self->AddOutput(const_cast<CKSTRING>(name.c_str())); }, (CKBehavior *, const std::string &), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKBehaviorIO@ ReplaceOutput(int pos, CKBehaviorIO@ io)", asMETHODPR(CKBehavior, ReplaceOutput, (int, CKBehaviorIO *), CKBehaviorIO *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKBehaviorIO@ CreateOutput(const string &in name)", asFUNCTIONPR([](CKBehavior *self, const std::string &name) { return self->CreateOutput(const_cast<CKSTRING>(name.c_str())); }, (CKBehavior *, const std::string &), CKBehaviorIO *), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // Inputs
    r = engine->RegisterObjectMethod("CKBehavior", "void ActivateInput(int pos, bool active = true)", asFUNCTIONPR([](CKBehavior *self, int pos, bool active) { self->ActivateInput(pos, active); }, (CKBehavior *, int, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "bool IsInputActive(int pos)", asFUNCTIONPR([](CKBehavior *self, int pos) -> bool { return self->IsInputActive(pos); }, (CKBehavior *, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKBehaviorIO@ RemoveInput(int pos)", asMETHODPR(CKBehavior, RemoveInput, (int), CKBehaviorIO *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKERROR DeleteInput(int pos)", asMETHODPR(CKBehavior, DeleteInput, (int), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKBehaviorIO@ GetInput(int pos)", asMETHODPR(CKBehavior, GetInput, (int), CKBehaviorIO *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "int GetInputCount()", asMETHODPR(CKBehavior, GetInputCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "int GetInputPosition(CKBehaviorIO@ pbio)", asMETHODPR(CKBehavior, GetInputPosition, (CKBehaviorIO *), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "int AddInput(const string &in name)", asFUNCTIONPR([](CKBehavior *self, const std::string &name) { return self->AddInput(const_cast<CKSTRING>(name.c_str())); }, (CKBehavior *, const std::string &), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKBehaviorIO@ ReplaceInput(int pos, CKBehaviorIO@ io)", asMETHODPR(CKBehavior, ReplaceInput, (int, CKBehaviorIO *), CKBehaviorIO *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKBehaviorIO@ CreateInput(const string &in name)", asFUNCTIONPR([](CKBehavior *self, const std::string &name) { return self->CreateInput(const_cast<CKSTRING>(name.c_str())); }, (CKBehavior *, const std::string &), CKBehaviorIO *), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // Inputs Parameters
    r = engine->RegisterObjectMethod("CKBehavior", "CKERROR ExportInputParameter(CKParameterIn@ p)", asMETHODPR(CKBehavior, ExportInputParameter, (CKParameterIn *), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKParameterIn@ CreateInputParameter(const string &in name, CKParameterType type)", asFUNCTIONPR([](CKBehavior *self, const std::string &name, CKParameterType type) { return self->CreateInputParameter(const_cast<CKSTRING>(name.c_str()), type); }, (CKBehavior *, const std::string &, CKParameterType), CKParameterIn *), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKParameterIn@ CreateInputParameter(const string &in name, CKGUID guid)", asFUNCTIONPR([](CKBehavior *self, const std::string &name, CKGUID guid) { return self->CreateInputParameter(const_cast<CKSTRING>(name.c_str()), guid); }, (CKBehavior *, const std::string &, CKGUID), CKParameterIn *), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKParameterIn@ InsertInputParameter(int pos, const string &in name, CKParameterType type)", asFUNCTIONPR([](CKBehavior *self, int pos, const std::string &name, CKParameterType type) { return self->InsertInputParameter(pos, const_cast<CKSTRING>(name.c_str()), type); }, (CKBehavior *, int, const std::string &, CKParameterType), CKParameterIn *), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "void AddInputParameter(CKParameterIn@ input)", asMETHODPR(CKBehavior, AddInputParameter, (CKParameterIn *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "int GetInputParameterPosition(CKParameterIn@ param)", asMETHODPR(CKBehavior, GetInputParameterPosition, (CKParameterIn *), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKParameterIn@ GetInputParameter(int pos)", asMETHODPR(CKBehavior, GetInputParameter, (int), CKParameterIn *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKParameterIn@ RemoveInputParameter(int pos)", asMETHODPR(CKBehavior, RemoveInputParameter, (int), CKParameterIn *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKParameterIn@ ReplaceInputParameter(int pos, CKParameterIn@ param)", asMETHODPR(CKBehavior, ReplaceInputParameter, (int, CKParameterIn *), CKParameterIn *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "int GetInputParameterCount()", asMETHODPR(CKBehavior, GetInputParameterCount, (), int), asCALL_THISCALL); assert(r >= 0);
    // r = engine->RegisterObjectMethod("CKBehavior", "CKERROR GetInputParameterValue(int pos, NativePointer buf)", asMETHODPR(CKBehavior, GetInputParameterValue, (int, void *), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKERROR GetInputParameterValue(int pos, ?&out value)", asFUNCTION(CKBehaviorGetParameterValueGeneric<0>), asCALL_GENERIC); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "NativePointer GetInputParameterReadDataPtr(int pos)", asMETHODPR(CKBehavior, GetInputParameterReadDataPtr, (int), void *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKObject@ GetInputParameterObject(int pos)", asMETHODPR(CKBehavior, GetInputParameterObject, (int), CKObject*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "bool IsInputParameterEnabled(int pos)", asFUNCTIONPR([](CKBehavior *self, int pos) -> bool { return self->IsInputParameterEnabled(pos); }, (CKBehavior *, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "void EnableInputParameter(int pos, bool enable)", asFUNCTIONPR([](CKBehavior *self, int pos, bool enable) { self->EnableInputParameter(pos, enable); }, (CKBehavior *, int, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // Outputs Parameters
    r = engine->RegisterObjectMethod("CKBehavior", "CKERROR ExportOutputParameter(CKParameterOut@ p)", asMETHODPR(CKBehavior, ExportOutputParameter, (CKParameterOut *), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKParameterOut@ CreateOutputParameter(const string &in name, CKParameterType type)", asFUNCTIONPR([](CKBehavior *self, const std::string &name, CKParameterType type) { return self->CreateOutputParameter(const_cast<CKSTRING>(name.c_str()), type); }, (CKBehavior *, const std::string &, CKParameterType), CKParameterOut *), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKParameterOut@ CreateOutputParameter(const string &in name, CKGUID guid)", asFUNCTIONPR([](CKBehavior *self, const std::string &name, CKGUID guid) { return self->CreateOutputParameter(const_cast<CKSTRING>(name.c_str()), guid); }, (CKBehavior *, const std::string &, CKGUID), CKParameterOut *), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKParameterOut@ InsertOutputParameter(int pos, const string &in name, CKParameterType type)", asFUNCTIONPR([](CKBehavior *self, int pos, const std::string &name, CKParameterType type) { return self->InsertOutputParameter(pos, const_cast<CKSTRING>(name.c_str()), type); }, (CKBehavior *, int, const std::string &, CKParameterType), CKParameterOut *), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKParameterOut@ GetOutputParameter(int pos)", asMETHODPR(CKBehavior, GetOutputParameter, (int), CKParameterOut *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "int GetOutputParameterPosition(CKParameterOut@ param)", asMETHODPR(CKBehavior, GetOutputParameterPosition, (CKParameterOut *), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKParameterOut@ ReplaceOutputParameter(int pos, CKParameterOut@ param)", asMETHODPR(CKBehavior, ReplaceOutputParameter, (int, CKParameterOut *), CKParameterOut *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKParameterOut@ RemoveOutputParameter(int pos)", asMETHODPR(CKBehavior, RemoveOutputParameter, (int), CKParameterOut *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "void AddOutputParameter(CKParameterOut@ param)", asMETHODPR(CKBehavior, AddOutputParameter, (CKParameterOut *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "int GetOutputParameterCount()", asMETHODPR(CKBehavior, GetOutputParameterCount, (), int), asCALL_THISCALL); assert(r >= 0);
    // r = engine->RegisterObjectMethod("CKBehavior", "CKERROR GetOutputParameterValue(int pos, NativePointer buf)", asMETHODPR(CKBehavior, GetOutputParameterValue, (int, void *), CKERROR), asCALL_THISCALL); assert(r >= 0);
    // r = engine->RegisterObjectMethod("CKBehavior", "CKERROR SetOutputParameterValue(int pos, const NativePointer buf, int size = 0)", asMETHODPR(CKBehavior, SetOutputParameterValue, (int, const void *, int), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKERROR GetOutputParameterValue(int pos, ?&out value)", asFUNCTION(CKBehaviorGetParameterValueGeneric<1>), asCALL_GENERIC); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKERROR SetOutputParameterValue(?&in value)", asFUNCTION(CKBehaviorSetParameterValueGeneric<1>), asCALL_GENERIC); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "NativePointer GetOutputParameterWriteDataPtr(int pos)", asMETHODPR(CKBehavior, GetOutputParameterWriteDataPtr, (int), void *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKERROR SetOutputParameterObject(int pos, CKObject@ obj)", asMETHODPR(CKBehavior, SetOutputParameterObject, (int, CKObject*), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKObject@ GetOutputParameterObject(int pos)", asMETHODPR(CKBehavior, GetOutputParameterObject, (int), CKObject*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "bool IsOutputParameterEnabled(int pos)", asFUNCTIONPR([](CKBehavior *self, int pos) -> bool { return self->IsOutputParameterEnabled(pos); }, (CKBehavior *, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "void EnableOutputParameter(int pos, bool enable)", asFUNCTIONPR([](CKBehavior *self, int pos, bool enable) { self->EnableOutputParameter(pos, enable); }, (CKBehavior *, int, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // Local Parameters
    r = engine->RegisterObjectMethod("CKBehavior", "CKParameterLocal@ CreateLocalParameter(const string &in name, CKParameterType type)", asFUNCTIONPR([](CKBehavior *self, const std::string &name, CKParameterType type) { return self->CreateLocalParameter(const_cast<CKSTRING>(name.c_str()), type); }, (CKBehavior *, const std::string &, CKParameterType), CKParameterLocal *), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKParameterLocal@ CreateLocalParameter(const string &in name, CKGUID guid)", asFUNCTIONPR([](CKBehavior *self, const std::string &name, CKGUID guid) { return self->CreateLocalParameter(const_cast<CKSTRING>(name.c_str()), guid); }, (CKBehavior *, const std::string &, CKGUID), CKParameterLocal *), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKParameterLocal@ GetLocalParameter(int pos)", asMETHODPR(CKBehavior, GetLocalParameter, (int), CKParameterLocal *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKParameterLocal@ RemoveLocalParameter(int pos)", asMETHODPR(CKBehavior, RemoveLocalParameter, (int), CKParameterLocal *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "void AddLocalParameter(CKParameterLocal@ param)", asMETHODPR(CKBehavior, AddLocalParameter, (CKParameterLocal*), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "int GetLocalParameterPosition(CKParameterLocal@ param)", asMETHODPR(CKBehavior, GetLocalParameterPosition, (CKParameterLocal *), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "int GetLocalParameterCount()", asMETHODPR(CKBehavior, GetLocalParameterCount, (), int), asCALL_THISCALL); assert(r >= 0);
    // r = engine->RegisterObjectMethod("CKBehavior", "CKERROR GetLocalParameterValue(int pos, NativePointer buf)", asMETHODPR(CKBehavior, GetLocalParameterValue, (int, void *), CKERROR), asCALL_THISCALL); assert(r >= 0);
    // r = engine->RegisterObjectMethod("CKBehavior", "CKERROR SetLocalParameterValue(int pos, const NativePointer buf, int size = 0)", asMETHODPR(CKBehavior, SetLocalParameterValue, (int, const void *, int), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKERROR GetLocalParameterValue(int pos, ?&out value)", asFUNCTION(CKBehaviorGetParameterValueGeneric<2>), asCALL_GENERIC); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKERROR SetLocalParameterValue(?&in value)", asFUNCTION(CKBehaviorSetParameterValueGeneric<2>), asCALL_GENERIC); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "NativePointer GetLocalParameterWriteDataPtr(int pos)", asMETHODPR(CKBehavior, GetLocalParameterWriteDataPtr, (int), void *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "NativePointer GetLocalParameterReadDataPtr(int pos)", asMETHODPR(CKBehavior, GetLocalParameterReadDataPtr, (int), void *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKObject@ GetLocalParameterObject(int pos)", asMETHODPR(CKBehavior, GetLocalParameterObject, (int), CKObject *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKERROR SetLocalParameterObject(int pos, CKObject@ obj)", asMETHODPR(CKBehavior, SetLocalParameterObject, (int, CKObject *), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "bool IsLocalParameterSetting(int pos)", asFUNCTIONPR([](CKBehavior *self, int pos) -> bool { return self->IsLocalParameterSetting(pos); }, (CKBehavior *, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // Run Time Graph: Activation
    r = engine->RegisterObjectMethod("CKBehavior", "void Activate(bool active = true, bool breset = false)", asFUNCTIONPR([](CKBehavior *self, bool active, bool breset) { self->Activate(active, breset); }, (CKBehavior *, bool, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // Run Time Graph: Sub Behaviors
    r = engine->RegisterObjectMethod("CKBehavior", "CKERROR AddSubBehavior(CKBehavior@ cbk)", asMETHODPR(CKBehavior, AddSubBehavior, (CKBehavior *), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKBehavior@ RemoveSubBehavior(CKBehavior@ cbk)", asMETHODPR(CKBehavior, RemoveSubBehavior, (CKBehavior *), CKBehavior *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKBehavior@ RemoveSubBehavior(int pos)", asMETHODPR(CKBehavior, RemoveSubBehavior, (int), CKBehavior *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKBehavior@ GetSubBehavior(int pos)", asMETHODPR(CKBehavior, GetSubBehavior, (int), CKBehavior *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "int GetSubBehaviorCount()", asMETHODPR(CKBehavior, GetSubBehaviorCount, (), int), asCALL_THISCALL); assert(r >= 0);

    // Run Time Graph: Links between sub behaviors
    r = engine->RegisterObjectMethod("CKBehavior", "CKERROR AddSubBehaviorLink(CKBehaviorLink@ cbkl)", asMETHODPR(CKBehavior, AddSubBehaviorLink, (CKBehaviorLink*), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKBehaviorLink@ RemoveSubBehaviorLink(CKBehaviorLink@ cbkl)", asMETHODPR(CKBehavior, RemoveSubBehaviorLink, (CKBehaviorLink*), CKBehaviorLink*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKBehaviorLink@ RemoveSubBehaviorLink(int pos)", asMETHODPR(CKBehavior, RemoveSubBehaviorLink, (int), CKBehaviorLink*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKBehaviorLink@ GetSubBehaviorLink(int pos)", asMETHODPR(CKBehavior, GetSubBehaviorLink, (int), CKBehaviorLink*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "int GetSubBehaviorLinkCount()", asMETHODPR(CKBehavior, GetSubBehaviorLinkCount, (), int), asCALL_THISCALL); assert(r >= 0);

    // Run Time Graph: Parameter Operation
    r = engine->RegisterObjectMethod("CKBehavior", "CKERROR AddParameterOperation(CKParameterOperation@ op)", asMETHODPR(CKBehavior, AddParameterOperation, (CKParameterOperation*), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKParameterOperation@ GetParameterOperation(int pos)", asMETHODPR(CKBehavior, GetParameterOperation, (int), CKParameterOperation*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKParameterOperation@ RemoveParameterOperation(int pos)", asMETHODPR(CKBehavior, RemoveParameterOperation, (int), CKParameterOperation*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKParameterOperation@ RemoveParameterOperation(CKParameterOperation@ op)", asMETHODPR(CKBehavior, RemoveParameterOperation, (CKParameterOperation*), CKParameterOperation*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "int GetParameterOperationCount()", asMETHODPR(CKBehavior, GetParameterOperationCount, (), int), asCALL_THISCALL); assert(r >= 0);

    // Run Time Graph: Behavior Priority
    r = engine->RegisterObjectMethod("CKBehavior", "int GetPriority()", asMETHODPR(CKBehavior, GetPriority, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "void SetPriority(int priority)", asMETHODPR(CKBehavior, SetPriority, (int), void), asCALL_THISCALL); assert(r >= 0);

    // Profiling
    r = engine->RegisterObjectMethod("CKBehavior", "float GetLastExecutionTime()", asMETHODPR(CKBehavior, GetLastExecutionTime, (), float), asCALL_THISCALL); assert(r >= 0);

    // Internal Functions
    r = engine->RegisterObjectMethod("CKBehavior", "CKERROR SetOwner(CKBeObject@ beo, bool callback = true)", asFUNCTIONPR([](CKBehavior *self, CKBeObject *beo, bool callback) { return self->SetOwner(beo, callback); }, (CKBehavior *, CKBeObject *, bool), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "CKERROR SetSubBehaviorOwner(CKBeObject@ beo, bool callback = true)", asFUNCTIONPR([](CKBehavior *self, CKBeObject *beo, bool callback) { return self->SetSubBehaviorOwner(beo, callback); }, (CKBehavior *, CKBeObject *, bool), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKBehavior", "void NotifyEdition()", asMETHODPR(CKBehavior, NotifyEdition, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "void NotifySettingsEdition()", asMETHODPR(CKBehavior, NotifySettingsEdition, (), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKBehavior", "CKStateChunk@ GetInterfaceChunk()", asMETHODPR(CKBehavior, GetInterfaceChunk, (), CKStateChunk *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBehavior", "void SetInterfaceChunk(CKStateChunk@ state)", asMETHODPR(CKBehavior, SetInterfaceChunk, (CKStateChunk *), void), asCALL_THISCALL); assert(r >= 0);

    // Not implemented
    // r = engine->RegisterObjectMethod("CKBehavior", "void Reset()", asMETHODPR(CKBehavior, Reset, (), void), asCALL_THISCALL); assert(r >= 0);
    // r = engine->RegisterObjectMethod("CKBehavior", "void ErrorMessage(const string &in error, const string &in context, bool showOwner = true, bool showScript = true)", asFUNCTIONPR([](CKBehavior *self, const std::string &error, const std::string &context, bool showOwner, bool showScript) { self->ErrorMessage(const_cast<CKSTRING>(error.c_str()), const_cast<CKSTRING>(context.c_str()), showOwner, showScript); }, (CKBehavior *, const std::string &, const std::string &, bool, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    // r = engine->RegisterObjectMethod("CKBehavior", "void ErrorMessage(const string &in error, CKDWORD context, bool showOwner = true, bool showScript = true)", asFUNCTIONPR([](CKBehavior *self, const std::string &error, CKDWORD context, bool showOwner, bool showScript) { self->ErrorMessage(const_cast<CKSTRING>(error.c_str()), context, showOwner, showScript); }, (CKBehavior *, const std::string &, CKDWORD, bool, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    // r = engine->RegisterObjectMethod("CKBehavior", "void SetPrototypeGuid(CKGUID ckguid)", asMETHODPR(CKBehavior, SetPrototypeGuid, (CKGUID), void), asCALL_THISCALL); assert(r >= 0);
}

void RegisterCKObjectAnimation(asIScriptEngine *engine) {
    assert(engine != nullptr);

    int r = 0;

    RegisterCKSceneObjectMembers<CKObjectAnimation>(engine, "CKObjectAnimation");

    // Controller functions
    r = engine->RegisterObjectMethod("CKObjectAnimation", "CKAnimController@ CreateController(CKANIMATION_CONTROLLER controlType)", asMETHODPR(CKObjectAnimation, CreateController, (CKANIMATION_CONTROLLER), CKAnimController*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectAnimation", "bool DeleteController(CKANIMATION_CONTROLLER controlType)", asFUNCTIONPR([](CKObjectAnimation *self, CKANIMATION_CONTROLLER controlType) -> bool { return self->DeleteController(controlType); }, (CKObjectAnimation *, CKANIMATION_CONTROLLER), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectAnimation", "CKAnimController@ GetPositionController()", asMETHODPR(CKObjectAnimation, GetPositionController, (), CKAnimController*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectAnimation", "CKAnimController@ GetScaleController()", asMETHODPR(CKObjectAnimation, GetScaleController, (), CKAnimController*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectAnimation", "CKAnimController@ GetRotationController()", asMETHODPR(CKObjectAnimation, GetRotationController, (), CKAnimController*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectAnimation", "CKAnimController@ GetScaleAxisController()", asMETHODPR(CKObjectAnimation, GetScaleAxisController, (), CKAnimController*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectAnimation", "CKMorphController@ GetMorphController()", asMETHODPR(CKObjectAnimation, GetMorphController, (), CKMorphController*), asCALL_THISCALL); assert(r >= 0);

    // Evaluate functions
    r = engine->RegisterObjectMethod("CKObjectAnimation", "bool EvaluatePosition(float time, VxVector &out pos)", asFUNCTIONPR([](CKObjectAnimation *self, float time, VxVector &pos) -> bool { return self->EvaluatePosition(time, pos); }, (CKObjectAnimation *, float, VxVector &), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectAnimation", "bool EvaluateScale(float time, VxVector &out scl)", asFUNCTIONPR([](CKObjectAnimation *self, float time, VxVector &scl) -> bool { return self->EvaluateScale(time, scl); }, (CKObjectAnimation *, float, VxVector &), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectAnimation", "bool EvaluateRotation(float time, VxQuaternion &out rot)", asFUNCTIONPR([](CKObjectAnimation *self, float time, VxQuaternion &rot) -> bool { return self->EvaluateRotation(time, rot); }, (CKObjectAnimation *, float, VxQuaternion &), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectAnimation", "bool EvaluateScaleAxis(float time, VxQuaternion &out scaleAxis)", asFUNCTIONPR([](CKObjectAnimation *self, float time, VxQuaternion &scaleAxis) -> bool { return self->EvaluateScaleAxis(time, scaleAxis); }, (CKObjectAnimation *, float, VxQuaternion &), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectAnimation", "bool EvaluateMorphTarget(float time, int vertexCount, NativePointer vertices, CKDWORD vStride, NativePointer normals)", asFUNCTIONPR([](CKObjectAnimation *self, float time, int vertexCount, NativePointer vertices, CKDWORD vStride, NativePointer normals) -> bool { return self->EvaluateMorphTarget(time, vertexCount, reinterpret_cast<VxVector *>(vertices.Get()), vStride, reinterpret_cast<VxCompressedVector *>(normals.Get())); }, (CKObjectAnimation *, float, int, NativePointer, CKDWORD, NativePointer), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectAnimation", "bool EvaluateKeys(float step, VxQuaternion &out rot, VxVector &out pos, VxVector &out scale, VxQuaternion &out scaleRot = void)", asFUNCTIONPR([](CKObjectAnimation *self, float step, VxQuaternion *rot, VxVector *pos, VxVector *scale, VxQuaternion *scaleRot) -> bool { return self->EvaluateKeys(step, rot, pos, scale, scaleRot); }, (CKObjectAnimation *, float, VxQuaternion *, VxVector *, VxVector *, VxQuaternion *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // Info functions
    r = engine->RegisterObjectMethod("CKObjectAnimation", "bool HasMorphNormalInfo()", asFUNCTIONPR([](CKObjectAnimation *self) -> bool { return self->HasMorphNormalInfo(); }, (CKObjectAnimation *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectAnimation", "bool HasMorphInfo()", asFUNCTIONPR([](CKObjectAnimation *self) -> bool { return self->HasMorphInfo(); }, (CKObjectAnimation *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectAnimation", "bool HasScaleInfo()", asFUNCTIONPR([](CKObjectAnimation *self) -> bool { return self->HasScaleInfo(); }, (CKObjectAnimation *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectAnimation", "bool HasPositionInfo()", asFUNCTIONPR([](CKObjectAnimation *self) -> bool { return self->HasPositionInfo(); }, (CKObjectAnimation *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectAnimation", "bool HasRotationInfo()", asFUNCTIONPR([](CKObjectAnimation *self) -> bool { return self->HasRotationInfo(); }, (CKObjectAnimation *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectAnimation", "bool HasScaleAxisInfo()", asFUNCTIONPR([](CKObjectAnimation *self) -> bool { return self->HasScaleAxisInfo(); }, (CKObjectAnimation *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // Key functions
    r = engine->RegisterObjectMethod("CKObjectAnimation", "void AddPositionKey(float timeStep, VxVector &in pos)", asMETHODPR(CKObjectAnimation, AddPositionKey, (float, VxVector *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectAnimation", "void AddRotationKey(float timeStep, VxQuaternion &in rot)", asMETHODPR(CKObjectAnimation, AddRotationKey, (float, VxQuaternion*), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectAnimation", "void AddScaleKey(float timeStep, VxVector &in scl)", asMETHODPR(CKObjectAnimation, AddScaleKey, (float, VxVector *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectAnimation", "void AddScaleAxisKey(float timeStep, VxQuaternion &in sclAxis)", asMETHODPR(CKObjectAnimation, AddScaleAxisKey, (float, VxQuaternion*), void), asCALL_THISCALL); assert(r >= 0);

    // Comparison and sharing functions
    r = engine->RegisterObjectMethod("CKObjectAnimation", "bool Compare(CKObjectAnimation@ anim, float threshold = 0.0)", asFUNCTIONPR([](CKObjectAnimation *self, CKObjectAnimation *anim, float threshold) -> bool { return self->Compare(anim, threshold); }, (CKObjectAnimation *, CKObjectAnimation *, float), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectAnimation", "bool ShareDataFrom(CKObjectAnimation@ anim)", asFUNCTIONPR([](CKObjectAnimation *self, CKObjectAnimation *anim) -> bool { return self->ShareDataFrom(anim); }, (CKObjectAnimation *, CKObjectAnimation *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectAnimation", "CKObjectAnimation@ Shared()", asMETHODPR(CKObjectAnimation, Shared, (), CKObjectAnimation *), asCALL_THISCALL); assert(r >= 0);

    // Flags
    r = engine->RegisterObjectMethod("CKObjectAnimation", "void SetFlags(CKDWORD flags)", asMETHODPR(CKObjectAnimation, SetFlags, (CKDWORD), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectAnimation", "CKDWORD GetFlags()", asMETHODPR(CKObjectAnimation, GetFlags, (), CKDWORD), asCALL_THISCALL); assert(r >= 0);

    // Clear functions
    r = engine->RegisterObjectMethod("CKObjectAnimation", "void Clear()", asMETHODPR(CKObjectAnimation, Clear, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectAnimation", "void ClearAll()", asMETHODPR(CKObjectAnimation, ClearAll, (), void), asCALL_THISCALL); assert(r >= 0);

    // Merged animations
    r = engine->RegisterObjectMethod("CKObjectAnimation", "float GetMergeFactor()", asMETHODPR(CKObjectAnimation, GetMergeFactor, (), float), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectAnimation", "void SetMergeFactor(float factor)", asMETHODPR(CKObjectAnimation, SetMergeFactor, (float), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectAnimation", "bool IsMerged()", asFUNCTIONPR([](CKObjectAnimation *self) -> bool { return self->IsMerged(); }, (CKObjectAnimation *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectAnimation", "CKObjectAnimation@ CreateMergedAnimation(CKObjectAnimation@ subAnim2, bool dynamic = false)", asFUNCTIONPR([](CKObjectAnimation *self, CKObjectAnimation *subAnim2, bool dynamic) { return self->CreateMergedAnimation(subAnim2, dynamic); }, (CKObjectAnimation *, CKObjectAnimation *, bool), CKObjectAnimation *), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // Length
    r = engine->RegisterObjectMethod("CKObjectAnimation", "void SetLength(float nbFrame)", asMETHODPR(CKObjectAnimation, SetLength, (float), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectAnimation", "float GetLength()", asMETHODPR(CKObjectAnimation, GetLength, (), float), asCALL_THISCALL); assert(r >= 0);

    // Velocity
    r = engine->RegisterObjectMethod("CKObjectAnimation", "void GetVelocity(float step, VxVector &out vel)", asMETHODPR(CKObjectAnimation, GetVelocity, (float, VxVector *), void), asCALL_THISCALL); assert(r >= 0);

    // Step and frame
    r = engine->RegisterObjectMethod("CKObjectAnimation", "CKERROR SetStep(float step, CKKeyedAnimation@ anim = null)", asMETHODPR(CKObjectAnimation, SetStep, (float, CKKeyedAnimation*), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectAnimation", "CKERROR SetFrame(float step, CKKeyedAnimation@ anim = null)", asMETHODPR(CKObjectAnimation, SetFrame, (float, CKKeyedAnimation*), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectAnimation", "float GetCurrentStep()", asMETHODPR(CKObjectAnimation, GetCurrentStep, (), float), asCALL_THISCALL); assert(r >= 0);

    // 3D Entity
    r = engine->RegisterObjectMethod("CKObjectAnimation", "void Set3dEntity(CK3dEntity@ ent)", asMETHODPR(CKObjectAnimation, Set3dEntity, (CK3dEntity *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKObjectAnimation", "CK3dEntity@ Get3dEntity()", asMETHODPR(CKObjectAnimation, Get3dEntity, (), CK3dEntity *), asCALL_THISCALL); assert(r >= 0);

    // Morph Vertex Count
    r = engine->RegisterObjectMethod("CKObjectAnimation", "int GetMorphVertexCount()", asMETHODPR(CKObjectAnimation, GetMorphVertexCount, (), int), asCALL_THISCALL); assert(r >= 0);

    // Transitions
    r = engine->RegisterObjectMethod("CKObjectAnimation", "void CreateTransition(float length, CKObjectAnimation@ animIn, float stepFrom, CKObjectAnimation@ animOut, float stepTo, bool veloc, bool dontTurn, CKAnimKey &in startingSet = void)", asFUNCTIONPR([](CKObjectAnimation *self, float length, CKObjectAnimation *animIn, float stepFrom, CKObjectAnimation *animOut, float stepTo, bool veloc, bool dontTurn, CKAnimKey *startingSet) { self->CreateTransition(length, animIn, stepFrom, animOut, stepTo, veloc, dontTurn, startingSet); }, (CKObjectAnimation *, float, CKObjectAnimation *, float, CKObjectAnimation *, float, bool, bool, CKAnimKey *), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // Clone
    r = engine->RegisterObjectMethod("CKObjectAnimation", "void Clone(CKObjectAnimation@ anim)", asMETHODPR(CKObjectAnimation, Clone, (CKObjectAnimation *), void), asCALL_THISCALL); assert(r >= 0);
}

template <typename T>
static void RegisterCKAnimationMembers(asIScriptEngine *engine, const char *name) {
    int r = 0;

    RegisterCKSceneObjectMembers<CKAnimation>(engine, name);

    // Stepping along
    r = engine->RegisterObjectMethod(name, "float GetLength()", asMETHODPR(T, GetLength, (), float), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "float GetFrame()", asMETHODPR(T, GetFrame, (), float), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "float GetNextFrame(float delta)", asMETHODPR(T, GetNextFrame, (float), float), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "float GetStep()", asMETHODPR(T, GetStep, (), float), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetFrame(float frame)", asMETHODPR(T, SetFrame, (float), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetStep(float step)", asMETHODPR(T, SetStep, (float), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetLength(float nbFrame)", asMETHODPR(T, SetLength, (float), void), asCALL_THISCALL); assert(r >= 0);

    // Character functions
    r = engine->RegisterObjectMethod(name, "CKCharacter@ GetCharacter()", asMETHODPR(T, GetCharacter, (), CKCharacter*), asCALL_THISCALL); assert(r >= 0);

    // Frame rate link
    r = engine->RegisterObjectMethod(name, "void LinkToFrameRate(bool link, float fps = 30.0)", asFUNCTIONPR([](T *self, bool link, float fps) { self->LinkToFrameRate(link, fps); }, (T *, bool, float), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "float GetLinkedFrameRate()", asMETHODPR(T, GetLinkedFrameRate, (), float), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool IsLinkedToFrameRate()", asFUNCTIONPR([](T *self) -> bool { return self->IsLinkedToFrameRate(); }, (T *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // Transition mode
    r = engine->RegisterObjectMethod(name, "void SetTransitionMode(CK_ANIMATION_TRANSITION_MODE mode)", asMETHODPR(T, SetTransitionMode, (CK_ANIMATION_TRANSITION_MODE), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CK_ANIMATION_TRANSITION_MODE GetTransitionMode()", asMETHODPR(T, GetTransitionMode, (), CK_ANIMATION_TRANSITION_MODE), asCALL_THISCALL); assert(r >= 0);

    // Secondary animation mode
    r = engine->RegisterObjectMethod(name, "void SetSecondaryAnimationMode(CK_SECONDARYANIMATION_FLAGS mode)", asMETHODPR(T, SetSecondaryAnimationMode, (CK_SECONDARYANIMATION_FLAGS), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CK_SECONDARYANIMATION_FLAGS GetSecondaryAnimationMode()", asMETHODPR(T, GetSecondaryAnimationMode, (), CK_SECONDARYANIMATION_FLAGS), asCALL_THISCALL); assert(r >= 0);

    // Priority and interruption
    r = engine->RegisterObjectMethod(name, "void SetCanBeInterrupt(bool can = true)", asFUNCTIONPR([](T *self, bool can) { self->SetCanBeInterrupt(can); }, (T *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool CanBeInterrupt()", asFUNCTIONPR([](T *self) -> bool { return self->CanBeInterrupt(); }, (T *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // Orientation
    r = engine->RegisterObjectMethod(name, "void SetCharacterOrientation(bool orient = true)", asFUNCTIONPR([](T *self, bool orient) { self->SetCharacterOrientation(orient); }, (T *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool DoesCharacterTakeOrientation()", asFUNCTIONPR([](T *self) -> bool { return self->DoesCharacterTakeOrientation(); }, (T *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // Flags
    r = engine->RegisterObjectMethod(name, "void SetFlags(CKDWORD flags)", asMETHODPR(T, SetFlags, (CKDWORD), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKDWORD GetFlags()", asMETHODPR(T, GetFlags, (), CKDWORD), asCALL_THISCALL); assert(r >= 0);

    // Root entity
    r = engine->RegisterObjectMethod(name, "CK3dEntity@ GetRootEntity()", asMETHODPR(T, GetRootEntity, (), CK3dEntity *), asCALL_THISCALL); assert(r >= 0);

    // Merged animations
    r = engine->RegisterObjectMethod(name, "float GetMergeFactor()", asMETHODPR(T, GetMergeFactor, (), float), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetMergeFactor(float frame)", asMETHODPR(T, SetMergeFactor, (float), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool IsMerged()", asFUNCTIONPR([](T *self) -> bool { return self->IsMerged(); }, (T *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKAnimation@ CreateMergedAnimation(CKAnimation@ anim2, bool dynamic = false)", asFUNCTIONPR([](T *self, CKAnimation *anim2, bool dynamic) { return self->CreateMergedAnimation(anim2, dynamic); }, (T *, CKAnimation *, bool), CKAnimation *), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // Set current step
    r = engine->RegisterObjectMethod(name, "void SetCurrentStep(float step)", asMETHODPR(T, SetCurrentStep, (float), void), asCALL_THISCALL); assert(r >= 0);

    // Transition animation
    r = engine->RegisterObjectMethod(name, "float CreateTransition(CKAnimation@ input, CKAnimation@ output, CKDWORD outTransitionMode, float length = 6.0, float frameTo = 0)", asMETHODPR(T, CreateTransition, (CKAnimation*, CKAnimation*, CKDWORD, float, float), float), asCALL_THISCALL); assert(r >= 0);

    if (strcmp(name, "CKAnimation") != 0) {
        RegisterCKObjectCast<T, CKAnimation>(engine, name, "CKAnimation");
    }
}

void RegisterCKAnimation(asIScriptEngine *engine) {
    assert(engine != nullptr);

    RegisterCKAnimationMembers<CKAnimation>(engine, "CKAnimation");
}

void RegisterCKKeyedAnimation(asIScriptEngine *engine) {
    assert(engine != nullptr);

    int r = 0;

    RegisterCKAnimationMembers<CKKeyedAnimation>(engine, "CKKeyedAnimation");

    r = engine->RegisterObjectMethod("CKKeyedAnimation", "CKERROR AddAnimation(CKObjectAnimation@ anim)", asMETHODPR(CKKeyedAnimation, AddAnimation, (CKObjectAnimation *), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKKeyedAnimation", "CKERROR RemoveAnimation(CKObjectAnimation@ anim)", asMETHODPR(CKKeyedAnimation, RemoveAnimation, (CKObjectAnimation *), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKKeyedAnimation", "int GetAnimationCount()", asMETHODPR(CKKeyedAnimation, GetAnimationCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKKeyedAnimation", "CKObjectAnimation@ GetAnimation(CK3dEntity@ ent)", asMETHODPR(CKKeyedAnimation, GetAnimation, (CK3dEntity *), CKObjectAnimation *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKKeyedAnimation", "CKObjectAnimation@ GetAnimation(int index)", asMETHODPR(CKKeyedAnimation, GetAnimation, (int), CKObjectAnimation *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKKeyedAnimation", "void Clear()", asMETHODPR(CKKeyedAnimation, Clear, (), void), asCALL_THISCALL); assert(r >= 0);
}

template <typename T>
static void RegisterCKBeObjectMembers(asIScriptEngine *engine, const char *name) {
    int r = 0;

    RegisterCKSceneObjectMembers<T>(engine, name);

    r = engine->RegisterObjectMethod(name, "void ExecuteBehaviors(float delta)", asMETHODPR(T, ExecuteBehaviors, (float), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool IsInGroup(CKGroup@ group)", asFUNCTIONPR([](T *self, CKGroup *group) -> bool { return self->IsInGroup(group); }, (T *, CKGroup *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool HasAttribute(CKAttributeType attribType)", asFUNCTIONPR([](T *self, CKAttributeType attribType) -> bool { return self->HasAttribute(attribType); }, (T *, CKAttributeType), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool SetAttribute(CKAttributeType attribType, CK_ID parameter = 0)", asFUNCTIONPR([](T *self, CKAttributeType attribType, CK_ID parameter) -> bool { return self->SetAttribute(attribType, parameter); }, (T *, CKAttributeType, CK_ID), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool RemoveAttribute(CKAttributeType attribType)", asFUNCTIONPR([](T *self, CKAttributeType attribType) -> bool { return self->RemoveAttribute(attribType); }, (T *, CKAttributeType), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKParameterOut@ GetAttributeParameter(CKAttributeType attribType)", asMETHODPR(T, GetAttributeParameter, (CKAttributeType), CKParameterOut *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "int GetAttributeCount()", asMETHODPR(T, GetAttributeCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "int GetAttributeType(int index)", asMETHODPR(T, GetAttributeType, (int), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKParameterOut@ GetAttributeParameterByIndex(int index)", asMETHODPR(T, GetAttributeParameterByIndex, (int), CKParameterOut *), asCALL_THISCALL); assert(r >= 0);
    // r = engine->RegisterObjectMethod(name, "void GetAttributeList(CKAttributeVal &out)", asMETHODPR(T, GetAttributeList, (CKAttributeVal*), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void RemoveAllAttributes()", asMETHODPR(T, RemoveAllAttributes, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKERROR AddScript(CKBehavior@ ckb)", asMETHODPR(T, AddScript, (CKBehavior *), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKBehavior@ RemoveScript(CK_ID id)", asMETHODPR(T, RemoveScript, (CK_ID), CKBehavior *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKBehavior@ RemoveScript(int pos)", asMETHODPR(T, RemoveScript, (int), CKBehavior *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKERROR RemoveAllScripts()", asMETHODPR(T, RemoveAllScripts, (), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKBehavior@ GetScript(int pos)", asMETHODPR(T, GetScript, (int), CKBehavior *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "int GetScriptCount()", asMETHODPR(T, GetScriptCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "int GetPriority()", asMETHODPR(T, GetPriority, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetPriority(int priority)", asMETHODPR(T, SetPriority, (int), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "int GetLastFrameMessageCount()", asMETHODPR(T, GetLastFrameMessageCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKMessage@ GetLastFrameMessage(int pos)", asMETHODPR(T, GetLastFrameMessage, (int), CKMessage*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetAsWaitingForMessages(bool wait = true)", asFUNCTIONPR([](T *self, bool wait) { self->SetAsWaitingForMessages(wait); }, (T *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool IsWaitingForMessages()", asFUNCTIONPR([](T *self) -> bool { return self->IsWaitingForMessages(); }, (T *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "int CallBehaviorCallbackFunction(CKDWORD message, CKGUID &in behGuid = void)", asMETHODPR(T, CallBehaviorCallbackFunction, (CKDWORD, CKGUID*), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "float GetLastExecutionTime()", asMETHODPR(T, GetLastExecutionTime, (), float), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void ApplyPatchForOlderVersion(int nbObject, CKFileObject &in fileObjects)", asMETHODPR(T, ApplyPatchForOlderVersion, (int, CKFileObject*), void), asCALL_THISCALL); assert(r >= 0);

    if (strcmp(name, "CKBeObject") != 0) {
        RegisterCKObjectCast<T, CKBeObject>(engine, name, "CKBeObject");
    }
}

template <>
static void RegisterCKBeObjectMembers<CKWaveSound>(asIScriptEngine *engine, const char *name) {
    int r = 0;

    RegisterCKSceneObjectMembers<CKWaveSound>(engine, name);

    r = engine->RegisterObjectMethod(name, "void ExecuteBehaviors(float delta)", asMETHODPR(CKWaveSound, ExecuteBehaviors, (float), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool IsInGroup(CKGroup@ group)", asFUNCTIONPR([](CKWaveSound *self, CKGroup *group) -> bool { return self->IsInGroup(group); }, (CKWaveSound *, CKGroup *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool HasAttribute(CKAttributeType attribType)", asFUNCTIONPR([](CKWaveSound *self, CKAttributeType attribType) -> bool { return self->HasAttribute(attribType); }, (CKWaveSound *, CKAttributeType), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool SetAttribute(CKAttributeType attribType, CK_ID parameter = 0)", asFUNCTIONPR([](CKWaveSound *self, CKAttributeType attribType, CK_ID parameter) -> bool { return self->SetAttribute(attribType, parameter); }, (CKWaveSound *, CKAttributeType, CK_ID), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool RemoveAttribute(CKAttributeType attribType)", asFUNCTIONPR([](CKWaveSound *self, CKAttributeType attribType) -> bool { return self->RemoveAttribute(attribType); }, (CKWaveSound *, CKAttributeType), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKParameterOut@ GetAttributeParameter(CKAttributeType attribType)", asMETHODPR(CKWaveSound, GetAttributeParameter, (CKAttributeType), CKParameterOut *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "int GetAttributeCount()", asMETHODPR(CKWaveSound, GetAttributeCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "int GetAttributeType(int index)", asMETHODPR(CKWaveSound, GetAttributeType, (int), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKParameterOut@ GetAttributeParameterByIndex(int index)", asMETHODPR(CKWaveSound, GetAttributeParameterByIndex, (int), CKParameterOut *), asCALL_THISCALL); assert(r >= 0);
    // r = engine->RegisterObjectMethod(name, "void GetAttributeList(CKAttributeVal &out)", asMETHODPR(CKWaveSound, GetAttributeList, (CKAttributeVal*), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void RemoveAllAttributes()", asMETHODPR(CKWaveSound, RemoveAllAttributes, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKERROR AddScript(CKBehavior@ ckb)", asMETHODPR(CKWaveSound, AddScript, (CKBehavior *), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKBehavior@ RemoveScript(CK_ID id)", asMETHODPR(CKWaveSound, RemoveScript, (CK_ID), CKBehavior *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKBehavior@ RemoveScript(int pos)", asMETHODPR(CKWaveSound, RemoveScript, (int), CKBehavior *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKERROR RemoveAllScripts()", asMETHODPR(CKWaveSound, RemoveAllScripts, (), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKBehavior@ GetScript(int pos)", asMETHODPR(CKWaveSound, GetScript, (int), CKBehavior *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "int GetScriptCount()", asMETHODPR(CKWaveSound, GetScriptCount, (), int), asCALL_THISCALL); assert(r >= 0);
    // r = engine->RegisterObjectMethod(name, "int GetPriority()", asMETHODPR(CKWaveSound, GetPriority, (), int), asCALL_THISCALL); assert(r >= 0);
    // r = engine->RegisterObjectMethod(name, "void SetPriority(int priority)", asMETHODPR(CKWaveSound, SetPriority, (int), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "int GetLastFrameMessageCount()", asMETHODPR(CKWaveSound, GetLastFrameMessageCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKMessage@ GetLastFrameMessage(int pos)", asMETHODPR(CKWaveSound, GetLastFrameMessage, (int), CKMessage*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetAsWaitingForMessages(bool wait = true)", asFUNCTIONPR([](CKWaveSound *self, bool wait) { self->SetAsWaitingForMessages(wait); }, (CKWaveSound *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool IsWaitingForMessages()", asFUNCTIONPR([](CKWaveSound *self) -> bool { return self->IsWaitingForMessages(); }, (CKWaveSound *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "int CallBehaviorCallbackFunction(CKDWORD message, CKGUID &in behguid = void)", asMETHODPR(CKWaveSound, CallBehaviorCallbackFunction, (CKDWORD, CKGUID*), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "float GetLastExecutionTime()", asMETHODPR(CKWaveSound, GetLastExecutionTime, (), float), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void ApplyPatchForOlderVersion(int nbObject, CKFileObject &in fileObjects)", asMETHODPR(CKWaveSound, ApplyPatchForOlderVersion, (int, CKFileObject*), void), asCALL_THISCALL); assert(r >= 0);

    RegisterCKObjectCast<CKWaveSound, CKBeObject>(engine, name, "CKBeObject");
}

void RegisterCKBeObject(asIScriptEngine *engine) {
    assert(engine != nullptr);

    RegisterCKBeObjectMembers<CKBeObject>(engine, "CKBeObject");
}

void RegisterCKScene(asIScriptEngine *engine) {
    assert(engine != nullptr);

    int r = 0;

    RegisterCKBeObjectMembers<CKScene>(engine, "CKScene");

    // Objects functions
    r = engine->RegisterObjectMethod("CKScene", "void AddObjectToScene(CKSceneObject@ obj, bool dependencies = true)", asFUNCTIONPR([](CKScene *scene, CKSceneObject *obj, bool dependencies) { scene->AddObjectToScene(obj, dependencies); }, (CKScene *, CKSceneObject *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKScene", "void RemoveObjectFromScene(CKSceneObject@ obj, bool dependencies = true)", asFUNCTIONPR([](CKScene *scene, CKSceneObject *obj, bool dependencies) { scene->RemoveObjectFromScene(obj, dependencies); }, (CKScene *, CKSceneObject *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKScene", "bool IsObjectHere(CKObject@ obj)", asFUNCTIONPR([](CKScene *scene, CKObject *obj) -> bool { return scene->IsObjectHere(obj); }, (CKScene *, CKObject *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKScene", "void BeginAddSequence(bool begin)", asFUNCTIONPR([](CKScene *scene, bool begin) { scene->BeginAddSequence(begin); }, (CKScene *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKScene", "void BeginRemoveSequence(bool begin)", asFUNCTIONPR([](CKScene *scene, bool begin) { scene->BeginRemoveSequence(begin); }, (CKScene *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // Object List
    r = engine->RegisterObjectMethod("CKScene", "int GetObjectCount()", asMETHODPR(CKScene, GetObjectCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKScene", "const XObjectPointerArray &ComputeObjectList(CK_CLASSID cid, bool derived = true)", asFUNCTIONPR([](CKScene *scene, CK_CLASSID cid, bool derived) -> const XObjectPointerArray& { return scene->ComputeObjectList(cid, derived); }, (CKScene *, CK_CLASSID, bool), const XObjectPointerArray&), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // Object Settings by index in list
    r = engine->RegisterObjectMethod("CKScene", "CKSODHashIt GetObjectIterator()", asMETHODPR(CKScene, GetObjectIterator, (), CKSceneObjectIterator), asCALL_THISCALL); assert(r >= 0);

    // BeObject and Script Activation/deactivation
    r = engine->RegisterObjectMethod("CKScene", "void Activate(CKSceneObject@ obj, bool reset)", asFUNCTIONPR([](CKScene *scene, CKSceneObject *obj, bool reset) { scene->Activate(obj, reset); }, (CKScene *, CKSceneObject *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKScene", "void DeActivate(CKSceneObject@ obj)", asMETHODPR(CKScene, DeActivate, (CKSceneObject*), void), asCALL_THISCALL); assert(r >= 0);

    // Object Settings by object
    r = engine->RegisterObjectMethod("CKScene", "void SetObjectFlags(CKSceneObject@ obj, CK_SCENEOBJECT_FLAGS flags)", asMETHODPR(CKScene, SetObjectFlags, (CKSceneObject *, CK_SCENEOBJECT_FLAGS), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKScene", "CK_SCENEOBJECT_FLAGS GetObjectFlags(CKSceneObject@ obj)", asMETHODPR(CKScene, GetObjectFlags, (CKSceneObject *), CK_SCENEOBJECT_FLAGS), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKScene", "CK_SCENEOBJECT_FLAGS ModifyObjectFlags(CKSceneObject@ obj, CKDWORD add, CKDWORD remove)", asMETHODPR(CKScene, ModifyObjectFlags, (CKSceneObject *, CKDWORD, CKDWORD), CK_SCENEOBJECT_FLAGS), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKScene", "bool SetObjectInitialValue(CKSceneObject@ obj, CKStateChunk@ chunk)", asFUNCTIONPR([](CKScene *scene, CKSceneObject *obj, CKStateChunk *chunk) -> bool { return scene->SetObjectInitialValue(obj, chunk); }, (CKScene *, CKSceneObject *, CKStateChunk *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKScene", "CKStateChunk@ GetObjectInitialValue(CKSceneObject@ obj)", asMETHODPR(CKScene, GetObjectInitialValue, (CKSceneObject *), CKStateChunk *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKScene", "bool IsObjectActive(CKSceneObject@ obj)", asFUNCTIONPR([](CKScene *scene, CKSceneObject *obj) -> bool { return scene->IsObjectActive(obj); }, (CKScene *, CKSceneObject *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // Render Settings
    r = engine->RegisterObjectMethod("CKScene", "void ApplyEnvironmentSettings(XObjectPointerArray &in renderList = void)", asMETHODPR(CKScene, ApplyEnvironmentSettings, (XObjectPointerArray*), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKScene", "void UseEnvironmentSettings(bool use = true)", asFUNCTIONPR([](CKScene *scene, bool use) { scene->UseEnvironmentSettings(use); }, (CKScene *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKScene", "bool EnvironmentSettings()", asFUNCTIONPR([](CKScene *scene) -> bool { return scene->EnvironmentSettings(); }, (CKScene *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // Ambient Light
    r = engine->RegisterObjectMethod("CKScene", "void SetAmbientLight(CKDWORD color)", asMETHODPR(CKScene, SetAmbientLight, (CKDWORD), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKScene", "CKDWORD GetAmbientLight()", asMETHODPR(CKScene, GetAmbientLight, (), CKDWORD), asCALL_THISCALL); assert(r >= 0);

    // Fog Access
    r = engine->RegisterObjectMethod("CKScene", "void SetFogMode(VXFOG_MODE mode)", asMETHODPR(CKScene, SetFogMode, (VXFOG_MODE), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKScene", "void SetFogStart(float start)", asMETHODPR(CKScene, SetFogStart, (float), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKScene", "void SetFogEnd(float end)", asMETHODPR(CKScene, SetFogEnd, (float), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKScene", "void SetFogDensity(float density)", asMETHODPR(CKScene, SetFogDensity, (float), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKScene", "void SetFogColor(CKDWORD color)", asMETHODPR(CKScene, SetFogColor, (CKDWORD), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKScene", "VXFOG_MODE GetFogMode()", asMETHODPR(CKScene, GetFogMode, (), VXFOG_MODE), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKScene", "float GetFogStart()", asMETHODPR(CKScene, GetFogStart, (), float), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKScene", "float GetFogEnd()", asMETHODPR(CKScene, GetFogEnd, (), float), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKScene", "float GetFogDensity()", asMETHODPR(CKScene, GetFogDensity, (), float), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKScene", "CKDWORD GetFogColor()", asMETHODPR(CKScene, GetFogColor, (), CKDWORD), asCALL_THISCALL); assert(r >= 0);

    // Background
    r = engine->RegisterObjectMethod("CKScene", "void SetBackgroundColor(CKDWORD color)", asMETHODPR(CKScene, SetBackgroundColor, (CKDWORD), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKScene", "CKDWORD GetBackgroundColor()", asMETHODPR(CKScene, GetBackgroundColor, (), CKDWORD), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKScene", "void SetBackgroundTexture(CKTexture@ texture)", asMETHODPR(CKScene, SetBackgroundTexture, (CKTexture *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKScene", "CKTexture@ GetBackgroundTexture()", asMETHODPR(CKScene, GetBackgroundTexture, (), CKTexture *), asCALL_THISCALL); assert(r >= 0);

    // Active camera
    r = engine->RegisterObjectMethod("CKScene", "void SetStartingCamera(CKCamera@ camera)", asMETHODPR(CKScene, SetStartingCamera, (CKCamera *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKScene", "CKCamera@ GetStartingCamera()", asMETHODPR(CKScene, GetStartingCamera, (), CKCamera *), asCALL_THISCALL); assert(r >= 0);

    // Level functions
    r = engine->RegisterObjectMethod("CKScene", "CKLevel@ GetLevel()", asMETHODPR(CKScene, GetLevel, (), CKLevel*), asCALL_THISCALL); assert(r >= 0);

    // Merge functions
    r = engine->RegisterObjectMethod("CKScene", "CKERROR Merge(CKScene@ mergedScene, CKLevel@ fromLevel = null)", asMETHODPR(CKScene, Merge, (CKScene *, CKLevel*), CKERROR), asCALL_THISCALL); assert(r >= 0);
}

void RegisterCKLevel(asIScriptEngine *engine) {
    assert(engine != nullptr);

    int r = 0;

    RegisterCKBeObjectMembers<CKLevel>(engine, "CKLevel");

    // Object Management
    r = engine->RegisterObjectMethod("CKLevel", "CKERROR AddObject(CKObject@ obj)", asMETHODPR(CKLevel, AddObject, (CKObject*), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKLevel", "CKERROR RemoveObject(CKObject@ obj)", asMETHODPR(CKLevel, RemoveObject, (CKObject*), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKLevel", "CKERROR RemoveObject(CK_ID id)", asMETHODPR(CKLevel, RemoveObject, (CK_ID), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKLevel", "void BeginAddSequence(bool begin)", asFUNCTIONPR([](CKLevel *level, bool begin) { level->BeginAddSequence(begin); }, (CKLevel *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKLevel", "void BeginRemoveSequence(bool begin)", asFUNCTIONPR([](CKLevel *level, bool begin) { level->BeginRemoveSequence(begin); }, (CKLevel *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // Object List
    r = engine->RegisterObjectMethod("CKLevel", "const XObjectPointerArray &ComputeObjectList(CK_CLASSID cid, bool derived = true)", asFUNCTIONPR([](CKLevel *level, CK_CLASSID cid, bool derived) -> const XObjectPointerArray & { return level->ComputeObjectList(cid, derived); }, (CKLevel *, CK_CLASSID, bool), const XObjectPointerArray &), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // Place Management
    r = engine->RegisterObjectMethod("CKLevel", "CKERROR AddPlace(CKPlace@ place)", asMETHODPR(CKLevel, AddPlace, (CKPlace *), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKLevel", "CKERROR RemovePlace(CKPlace@ place)", asMETHODPR(CKLevel, RemovePlace, (CKPlace *), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKLevel", "CKPlace@ RemovePlace(int pos)", asMETHODPR(CKLevel, RemovePlace, (int), CKPlace *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKLevel", "CKPlace@ GetPlace(int pos)", asMETHODPR(CKLevel, GetPlace, (int), CKPlace *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKLevel", "int GetPlaceCount()", asMETHODPR(CKLevel, GetPlaceCount, (), int), asCALL_THISCALL); assert(r >= 0);

    // Scene Management
    r = engine->RegisterObjectMethod("CKLevel", "CKERROR AddScene(CKScene@ scene)", asMETHODPR(CKLevel, AddScene, (CKScene *), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKLevel", "CKERROR RemoveScene(CKScene@ scene)", asMETHODPR(CKLevel, RemoveScene, (CKScene *), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKLevel", "CKScene@ RemoveScene(int pos)", asMETHODPR(CKLevel, RemoveScene, (int), CKScene *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKLevel", "CKScene@ GetScene(int pos)", asMETHODPR(CKLevel, GetScene, (int), CKScene *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKLevel", "int GetSceneCount()", asMETHODPR(CKLevel, GetSceneCount, (), int), asCALL_THISCALL); assert(r >= 0);

    // Active Scene
    r = engine->RegisterObjectMethod("CKLevel", "CKERROR SetNextActiveScene(CKScene@ scene, CK_SCENEOBJECTACTIVITY_FLAGS active = CK_SCENEOBJECTACTIVITY_SCENEDEFAULT, CK_SCENEOBJECTRESET_FLAGS reset = CK_SCENEOBJECTRESET_RESET)", asMETHODPR(CKLevel, SetNextActiveScene, (CKScene *, CK_SCENEOBJECTACTIVITY_FLAGS, CK_SCENEOBJECTRESET_FLAGS), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKLevel", "CKERROR LaunchScene(CKScene@ scene, CK_SCENEOBJECTACTIVITY_FLAGS active = CK_SCENEOBJECTACTIVITY_SCENEDEFAULT, CK_SCENEOBJECTRESET_FLAGS reset = CK_SCENEOBJECTRESET_RESET)", asMETHODPR(CKLevel, LaunchScene, (CKScene *, CK_SCENEOBJECTACTIVITY_FLAGS, CK_SCENEOBJECTRESET_FLAGS), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKLevel", "CKScene@ GetCurrentScene()", asMETHODPR(CKLevel, GetCurrentScene, (), CKScene *), asCALL_THISCALL); assert(r >= 0);

    // Render Context functions
    r = engine->RegisterObjectMethod("CKLevel", "void AddRenderContext(CKRenderContext@ dev, bool main = false)", asFUNCTIONPR([](CKLevel *level, CKRenderContext *dev, bool main) { level->AddRenderContext(dev, main); }, (CKLevel *, CKRenderContext *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKLevel", "void RemoveRenderContext(CKRenderContext@ dev)", asMETHODPR(CKLevel, RemoveRenderContext, (CKRenderContext*), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKLevel", "int GetRenderContextCount()", asMETHODPR(CKLevel, GetRenderContextCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKLevel", "CKRenderContext@ GetRenderContext(int count)", asMETHODPR(CKLevel, GetRenderContext, (int), CKRenderContext*), asCALL_THISCALL); assert(r >= 0);

    // Main Scene for this Level
    r = engine->RegisterObjectMethod("CKLevel", "CKScene@ GetLevelScene()", asMETHODPR(CKLevel, GetLevelScene, (), CKScene *), asCALL_THISCALL); assert(r >= 0);

    // Merge
    r = engine->RegisterObjectMethod("CKLevel", "CKERROR Merge(CKLevel@ mergedLevel, bool asScene)", asFUNCTIONPR([](CKLevel *level, CKLevel *mergedLevel, bool asScene) { return level->Merge(mergedLevel, asScene); }, (CKLevel *, CKLevel *, bool), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
}

void RegisterCKGroup(asIScriptEngine *engine) {
    assert(engine != nullptr);

    int r = 0;

    RegisterCKBeObjectMembers<CKGroup>(engine, "CKGroup");

    // Insertion/Removal
    r = engine->RegisterObjectMethod("CKGroup", "CKERROR AddObject(CKBeObject@ obj)", asMETHODPR(CKGroup, AddObject, (CKBeObject*), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKGroup", "CKERROR AddObjectFront(CKBeObject@ obj)", asMETHODPR(CKGroup, AddObjectFront, (CKBeObject*), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKGroup", "CKERROR InsertObjectAt(CKBeObject@ obj, int pos)", asMETHODPR(CKGroup, InsertObjectAt, (CKBeObject*, int), CKERROR), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKGroup", "CKBeObject@ RemoveObject(int pos)", asMETHODPR(CKGroup, RemoveObject, (int), CKBeObject*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKGroup", "void RemoveObject(CKBeObject@ obj)", asMETHODPR(CKGroup, RemoveObject, (CKBeObject*), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKGroup", "void Clear()", asMETHODPR(CKGroup, Clear, (), void), asCALL_THISCALL); assert(r >= 0);

    // Order
    r = engine->RegisterObjectMethod("CKGroup", "void MoveObjectUp(CKBeObject@ obj)", asMETHODPR(CKGroup, MoveObjectUp, (CKBeObject*), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKGroup", "void MoveObjectDown(CKBeObject@ obj)", asMETHODPR(CKGroup, MoveObjectDown, (CKBeObject*), void), asCALL_THISCALL); assert(r >= 0);

    // Object Access
    r = engine->RegisterObjectMethod("CKGroup", "CKBeObject@ GetObject(int)", asMETHODPR(CKGroup, GetObjectA, (int), CKBeObject*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKGroup", "int GetObjectCount()", asMETHODPR(CKGroup, GetObjectCount, (), int), asCALL_THISCALL); assert(r >= 0);

    // Common Class ID
    r = engine->RegisterObjectMethod("CKGroup", "CK_CLASSID GetCommonClassID()", asMETHODPR(CKGroup, GetCommonClassID, (), CK_CLASSID), asCALL_THISCALL); assert(r >= 0);
}

void RegisterCKMaterial(asIScriptEngine *engine) {
    assert(engine != nullptr);

    int r = 0;

    RegisterCKBeObjectMembers<CKMaterial>(engine, "CKMaterial");

    r = engine->RegisterObjectMethod("CKMaterial", "float GetPower() const", asMETHODPR(CKMaterial, GetPower, (), float), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMaterial", "void SetPower(float value)", asMETHODPR(CKMaterial, SetPower, (float), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKMaterial", "const VxColor &GetAmbient() const", asMETHODPR(CKMaterial, GetAmbient, (), const VxColor&), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMaterial", "void SetAmbient(const VxColor &in color)", asMETHODPR(CKMaterial, SetAmbient, (const VxColor&), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKMaterial", "const VxColor &GetDiffuse() const", asMETHODPR(CKMaterial, GetDiffuse, (), const VxColor&), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMaterial", "void SetDiffuse(const VxColor &in color)", asMETHODPR(CKMaterial, SetDiffuse, (const VxColor&), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKMaterial", "const VxColor &GetSpecular() const", asMETHODPR(CKMaterial, GetSpecular, (), const VxColor&), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMaterial", "void SetSpecular(const VxColor &in color)", asMETHODPR(CKMaterial, SetSpecular, (const VxColor&), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKMaterial", "const VxColor &GetEmissive() const", asMETHODPR(CKMaterial, GetEmissive, (), const VxColor&), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMaterial", "void SetEmissive(const VxColor &in color)", asMETHODPR(CKMaterial, SetEmissive, (const VxColor&), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKMaterial", "CKTexture@ GetTexture(int texIndex = 0)", asMETHODPR(CKMaterial, GetTexture, (int), CKTexture *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMaterial", "void SetTexture(int texIndex, CKTexture@ tex)", asMETHODPR(CKMaterial, SetTexture, (int, CKTexture *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMaterial", "void SetTexture(CKTexture@ tex)", asMETHODPR(CKMaterial, SetTexture0, (CKTexture *), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKMaterial", "VXTEXTURE_BLENDMODE GetTextureBlendMode() const", asMETHODPR(CKMaterial, GetTextureBlendMode, (), VXTEXTURE_BLENDMODE), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMaterial", "void SetTextureBlendMode(VXTEXTURE_BLENDMODE blendMode)", asMETHODPR(CKMaterial, SetTextureBlendMode, (VXTEXTURE_BLENDMODE), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKMaterial", "VXTEXTURE_FILTERMODE GetTextureMinMode() const", asMETHODPR(CKMaterial, GetTextureMinMode, (), VXTEXTURE_FILTERMODE), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMaterial", "void SetTextureMinMode(VXTEXTURE_FILTERMODE filterMode)", asMETHODPR(CKMaterial, SetTextureMinMode, (VXTEXTURE_FILTERMODE), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKMaterial", "VXTEXTURE_FILTERMODE GetTextureMagMode() const", asMETHODPR(CKMaterial, GetTextureMagMode, (), VXTEXTURE_FILTERMODE), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMaterial", "void SetTextureMagMode(VXTEXTURE_FILTERMODE filterMode)", asMETHODPR(CKMaterial, SetTextureMagMode, (VXTEXTURE_FILTERMODE), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKMaterial", "VXTEXTURE_ADDRESSMODE GetTextureAddressMode() const", asMETHODPR(CKMaterial, GetTextureAddressMode, (), VXTEXTURE_ADDRESSMODE), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMaterial", "void SetTextureAddressMode(VXTEXTURE_ADDRESSMODE mode)", asMETHODPR(CKMaterial, SetTextureAddressMode, (VXTEXTURE_ADDRESSMODE), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKMaterial", "CKDWORD GetTextureBorderColor() const", asMETHODPR(CKMaterial, GetTextureBorderColor, (), CKDWORD), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMaterial", "void SetTextureBorderColor(CKDWORD color)", asMETHODPR(CKMaterial, SetTextureBorderColor, (CKDWORD), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKMaterial", "VXBLEND_MODE GetSourceBlend() const", asMETHODPR(CKMaterial, GetSourceBlend, (), VXBLEND_MODE), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMaterial", "void SetSourceBlend(VXBLEND_MODE blendMode)", asMETHODPR(CKMaterial, SetSourceBlend, (VXBLEND_MODE), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKMaterial", "VXBLEND_MODE GetDestBlend() const", asMETHODPR(CKMaterial, GetDestBlend, (), VXBLEND_MODE), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMaterial", "void SetDestBlend(VXBLEND_MODE blendMode)", asMETHODPR(CKMaterial, SetDestBlend, (VXBLEND_MODE), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKMaterial", "bool IsTwoSided() const", asFUNCTIONPR([](CKMaterial *mat) -> bool { return mat->IsTwoSided(); }, (CKMaterial *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMaterial", "void SetTwoSided(bool twoSided)", asFUNCTIONPR([](CKMaterial *mat, bool twoSided) { mat->SetTwoSided(twoSided); }, (CKMaterial *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKMaterial", "bool ZWriteEnabled() const", asFUNCTIONPR([](CKMaterial *mat) -> bool { return mat->ZWriteEnabled(); }, (CKMaterial *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMaterial", "void EnableZWrite(bool zWrite = true)", asFUNCTIONPR([](CKMaterial *mat, bool zWrite) { mat->EnableZWrite(zWrite); }, (CKMaterial *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKMaterial", "bool AlphaBlendEnabled() const", asFUNCTIONPR([](CKMaterial *mat) -> bool { return mat->AlphaBlendEnabled(); }, (CKMaterial *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMaterial", "void EnableAlphaBlend(bool blend = true)", asFUNCTIONPR([](CKMaterial *mat, bool blend) { mat->EnableAlphaBlend(blend); }, (CKMaterial *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKMaterial", "VXCMPFUNC GetZFunc() const", asMETHODPR(CKMaterial, GetZFunc, (), VXCMPFUNC), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMaterial", "void SetZFunc(VXCMPFUNC zFunc = VXCMP_LESSEQUAL)", asMETHODPR(CKMaterial, SetZFunc, (VXCMPFUNC), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKMaterial", "bool PerspectiveCorrectionEnabled() const", asFUNCTIONPR([](CKMaterial *mat) -> bool { return mat->PerspectiveCorrectionEnabled(); }, (CKMaterial *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMaterial", "void EnablePerspectiveCorrection(bool perspective = true)", asFUNCTIONPR([](CKMaterial *mat, bool perspective) { mat->EnablePerspectiveCorrection(perspective); }, (CKMaterial *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKMaterial", "void SetFillMode(VXFILL_MODE fillMode)", asMETHODPR(CKMaterial, SetFillMode, (VXFILL_MODE), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMaterial", "VXFILL_MODE GetFillMode() const", asMETHODPR(CKMaterial, GetFillMode, (), VXFILL_MODE), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKMaterial", "void SetShadeMode(VXSHADE_MODE shadeMode)", asMETHODPR(CKMaterial, SetShadeMode, (VXSHADE_MODE), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMaterial", "VXSHADE_MODE GetShadeMode() const", asMETHODPR(CKMaterial, GetShadeMode, (), VXSHADE_MODE), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKMaterial", "bool SetAsCurrent(CKRenderContext@ dev, bool lit = true, int textureStage = 0)", asFUNCTIONPR([](CKMaterial *mat, CKRenderContext *dev, bool lit, int textureStage) -> bool { return mat->SetAsCurrent(dev, lit, textureStage); }, (CKMaterial *, CKRenderContext *, bool, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKMaterial", "bool IsAlphaTransparent() const", asFUNCTIONPR([](CKMaterial *mat) -> bool { return mat->IsAlphaTransparent(); }, (CKMaterial *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKMaterial", "bool AlphaTestEnabled() const", asFUNCTIONPR([](CKMaterial *mat) -> bool { return mat->AlphaTestEnabled(); }, (CKMaterial *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMaterial", "void EnableAlphaTest(bool enable = true)", asFUNCTIONPR([](CKMaterial *mat, bool enable) { mat->EnableAlphaTest(enable); }, (CKMaterial *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKMaterial", "VXCMPFUNC GetAlphaFunc() const", asMETHODPR(CKMaterial, GetAlphaFunc, (), VXCMPFUNC), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMaterial", "void SetAlphaFunc(VXCMPFUNC alphaFunc = VXCMP_ALWAYS)", asMETHODPR(CKMaterial, SetAlphaFunc, (VXCMPFUNC), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKMaterial", "CKBYTE GetAlphaRef() const", asMETHODPR(CKMaterial, GetAlphaRef, (), CKBYTE), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMaterial", "void SetAlphaRef(CKBYTE alphaRef = 0)", asMETHODPR(CKMaterial, SetAlphaRef, (CKBYTE), void), asCALL_THISCALL); assert(r >= 0);

#if CKVERSION != 0x26052005
    r = engine->RegisterObjectMethod("CKMaterial", "void SetCallback(NativePointer fct, NativePointer argument)", asFUNCTIONPR([](CKMaterial *self, NativePointer fct, NativePointer argument) { self->SetCallback(reinterpret_cast<CK_MATERIALCALLBACK>(fct.Get()), argument.Get()); }, (CKMaterial *, NativePointer, NativePointer), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMaterial", "NativePointer GetCallback(NativePointer &out argument = void) const", asFUNCTIONPR([](CKMaterial *self, NativePointer *argument) { return NativePointer(self->GetCallback(reinterpret_cast<void**>(argument))); }, (CKMaterial *, NativePointer *), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
#endif

    r = engine->RegisterObjectMethod("CKMaterial", "void SetEffect(VX_EFFECT effect)", asMETHODPR(CKMaterial, SetEffect, (VX_EFFECT), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMaterial", "VX_EFFECT GetEffect() const", asMETHODPR(CKMaterial, GetEffect, (), VX_EFFECT), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKMaterial", "CKParameter@ GetEffectParameter() const", asMETHODPR(CKMaterial, GetEffectParameter, (), CKParameter*), asCALL_THISCALL); assert(r >= 0);
}

template<typename T>
void RegisterCKBitmapDataMembers(asIScriptEngine *engine, const char *name) {
    int r = 0;

    r = engine->RegisterObjectProperty(name, "CKMovieInfo@ m_MovieInfo", asOFFSET(T, m_MovieInfo)); assert(r >= 0);
    // r = engine->RegisterObjectProperty(name, "XArray<CKBitmapSlot *> m_Slots", asOFFSET(T, m_Slots)); assert(r >= 0);
    r = engine->RegisterObjectProperty(name, "int m_Width", asOFFSET(T, m_Width)); assert(r >= 0);
    r = engine->RegisterObjectProperty(name, "int m_Height", asOFFSET(T, m_Height)); assert(r >= 0);
    r = engine->RegisterObjectProperty(name, "int m_CurrentSlot", asOFFSET(T, m_CurrentSlot)); assert(r >= 0);
    r = engine->RegisterObjectProperty(name, "int m_PickThreshold", asOFFSET(T, m_PickThreshold)); assert(r >= 0);
    r = engine->RegisterObjectProperty(name, "CKDWORD m_BitmapFlags", asOFFSET(T, m_BitmapFlags)); assert(r >= 0);
    r = engine->RegisterObjectProperty(name, "CKDWORD m_TransColor", asOFFSET(T, m_TransColor)); assert(r >= 0);

#if CKVERSION == 0x13022002
    r = engine->RegisterObjectMethod(name, "bool CreateImage(int width, int height, int bpp = 32, int slot = 0)", asFUNCTIONPR([](T *self, int width, int height, int bpp, int slot) -> bool { return self->CreateImage(width, height, bpp, slot); }, (T *, int, int, int, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
#else
    r = engine->RegisterObjectMethod(name, "bool CreateImage(int width, int height, int bpp = 32, int slot = 0, NativePointer imagePointer = 0)", asFUNCTIONPR([](T *self, int width, int height, int bpp, int slot, NativePointer imagePointer) -> bool { return self->CreateImage(width, height, bpp, slot, imagePointer.Get()); }, (T *, int, int, int, int, NativePointer), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
#endif
    r = engine->RegisterObjectMethod(name, "bool SaveImage(const string &in name, int slot = 0, bool useFormat = false)", asFUNCTIONPR([](T *self, const std::string &name, int slot, bool useFormat) -> bool { return self->SaveImage(const_cast<CKSTRING>(name.c_str()), slot, useFormat); }, (T *, const std::string &, int, bool), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool SaveImageAlpha(const string &in name, int slot = 0)", asFUNCTIONPR([](T *self, const std::string &name, int slot) -> bool { return self->SaveImageAlpha(const_cast<CKSTRING>(name.c_str()), slot); }, (T *, const std::string &, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "string GetMovieFileName()", asFUNCTIONPR([](T *self) -> std::string { return ScriptStringify(self->GetMovieFileName()); }, (T *), std::string), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKMovieReader@ GetMovieReader()", asMETHODPR(T, GetMovieReader, (), CKMovieReader *), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "NativePointer LockSurfacePtr(int slot = -1)", asFUNCTIONPR([](T *self, int slot) { return NativePointer(self->LockSurfacePtr(slot)); }, (T *, int), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool ReleaseSurfacePtr(int slot = -1)", asFUNCTIONPR([](T *self, int slot) -> bool { return self->ReleaseSurfacePtr(slot); }, (T *, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "string GetSlotFileName(int slot)", asFUNCTIONPR([](T *self, int slot) -> std::string { return ScriptStringify(self->GetSlotFileName(slot)); }, (T *, int), std::string), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool SetSlotFileName(int slot, const string &in filename)", asFUNCTIONPR([](T *self, int slot, const std::string &filename) -> bool { return self->SetSlotFileName(slot, const_cast<CKSTRING>(filename.c_str())); }, (T *, int, const std::string &), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "int GetWidth()", asMETHODPR(T, GetWidth, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "int GetHeight()", asMETHODPR(T, GetHeight, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool GetImageDesc(VxImageDescEx &out desc)", asFUNCTIONPR([](T *self, VxImageDescEx &desc) -> bool { return self->GetImageDesc(desc); }, (T *, VxImageDescEx &), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "int GetSlotCount()", asMETHODPR(T, GetSlotCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool SetSlotCount(int count)", asFUNCTIONPR([](T *self, int count) -> bool { return self->SetSlotCount(count); }, (T *, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool SetCurrentSlot(int slot)", asFUNCTIONPR([](T *self, int slot) -> bool { return self->SetCurrentSlot(slot); }, (T *, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "int GetCurrentSlot()", asMETHODPR(T, GetCurrentSlot, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool ReleaseSlot(int slot)", asFUNCTIONPR([](T *self, int slot) -> bool { return self->ReleaseSlot(slot); }, (T *, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool ReleaseAllSlots()", asFUNCTIONPR([](T *self) -> bool { return self->ReleaseAllSlots(); }, (T *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "bool SetPixel(int x, int y, CKDWORD color, int slot = -1)", asFUNCTIONPR([](T *self, int x, int y, CKDWORD color, int slot) -> bool { return self->SetPixel(x, y, color, slot); }, (T *, int, int, CKDWORD, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKDWORD GetPixel(int x, int y, int slot = -1)", asMETHODPR(T, GetPixel, (int, int, int), CKDWORD), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "CKDWORD GetTransparentColor()", asMETHODPR(T, GetTransparentColor, (), CKDWORD), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetTransparentColor(CKDWORD color)", asMETHODPR(T, SetTransparentColor, (CKDWORD), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetTransparent(bool transparency)", asFUNCTIONPR([](T *self, bool transparency) { self->SetTransparent(transparency); }, (T *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool IsTransparent()", asFUNCTIONPR([](T *self) -> bool { return self->IsTransparent(); }, (T *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "CK_TEXTURE_SAVEOPTIONS GetSaveOptions()", asMETHODPR(T, GetSaveOptions, (), CK_BITMAP_SAVEOPTIONS), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetSaveOptions(CK_TEXTURE_SAVEOPTIONS options)", asMETHODPR(T, SetSaveOptions, (CK_BITMAP_SAVEOPTIONS), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKBitmapProperties@ GetSaveFormat()", asMETHODPR(T, GetSaveFormat, (), CKBitmapProperties *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetSaveFormat(CKBitmapProperties@ format)", asMETHODPR(T, SetSaveFormat, (CKBitmapProperties *), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void SetPickThreshold(int pt)", asMETHODPR(T, SetPickThreshold, (int), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "int GetPickThreshold()", asMETHODPR(T, GetPickThreshold, (), int), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void SetCubeMap(bool cubeMap)", asFUNCTIONPR([](T *self, bool cubeMap) { self->SetCubeMap(cubeMap); }, (T *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool IsCubeMap()", asFUNCTIONPR([](T *self) -> bool { return self->IsCubeMap(); }, (T *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "bool ResizeImages(int width, int height)", asFUNCTIONPR([](T *self, int width, int height) -> bool { return self->ResizeImages(width, height); }, (T *, int, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetDynamicHint(bool dynamic)", asFUNCTIONPR([](T *self, bool dynamic) { self->SetDynamicHint(dynamic); }, (T *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool GetDynamicHint()", asFUNCTIONPR([](T *self) -> bool { return self->GetDynamicHint(); }, (T *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "bool ToRestore()", asFUNCTIONPR([](T *self) -> bool { return self->ToRestore(); }, (T *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
#if CKVERSION == 0x13022002
    r = engine->RegisterObjectMethod(name, "bool LoadSlotImage(XString name, int slot = 0)", asFUNCTIONPR([](T *self, XString name, int slot) -> bool { return self->LoadSlotImage(name, slot); }, (T *, XString, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool LoadMovieFile(XString name)", asFUNCTIONPR([](T *self, XString name) -> bool { return self->LoadMovieFile(name); }, (T *, XString), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKMovieInfo@ CreateMovieInfo(XString s, CKMovieProperties@ &out mp)", asMETHODPR(T, CreateMovieInfo, (XString, CKMovieProperties **), CKMovieInfo *), asCALL_THISCALL); assert(r >= 0);
#else
    r = engine->RegisterObjectMethod(name, "bool LoadSlotImage(const XString &name, int slot = 0)", asFUNCTIONPR([](T *self, const XString &name, int slot) -> bool { return self->LoadSlotImage(name, slot); }, (T *, const XString &, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool LoadMovieFile(const XString &name)", asFUNCTIONPR([](T *self, const XString &name) -> bool { return self->LoadMovieFile(name); }, (T *, const XString &), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKMovieInfo@ CreateMovieInfo(const XString &s, CKMovieProperties@ &out mp)", asMETHODPR(T, CreateMovieInfo, (const XString &, CKMovieProperties **), CKMovieInfo *), asCALL_THISCALL); assert(r >= 0);
#endif
    r = engine->RegisterObjectMethod(name, "void SetMovieInfo(CKMovieInfo@ mi)", asMETHODPR(T, SetMovieInfo, (CKMovieInfo *), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void SetAlphaForTransparentColor(const VxImageDescEx &in desc)", asMETHODPR(T, SetAlphaForTransparentColor, (const VxImageDescEx &), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetBorderColorForClamp(const VxImageDescEx &in desc)", asMETHODPR(T, SetBorderColorForClamp, (const VxImageDescEx &), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool SetSlotImage(int slot, NativePointer buffer, const VxImageDescEx &in desc)", asFUNCTIONPR([](T *self, int slot, NativePointer buffer, VxImageDescEx &desc) -> bool { return self->SetSlotImage(slot, buffer.Get(), desc); }, (T *, int, NativePointer, VxImageDescEx &), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // r = engine->RegisterObjectMethod(name, "bool DumpToChunk(CKStateChunk@ chunk, CKContext@ context, CKFile@ file, NativeBuffer Identifiers)", asFUNCTIONPR([](T *self, CKStateChunk *chunk, CKContext *context, CKFile *file, NativeBuffer identifiers) -> bool { return self->DumpToChunk(chunk, context, file, identifiers); }, (T *, CKStateChunk *, CKContext *, CKFile *, NativeBuffer), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    // r = engine->RegisterObjectMethod(name, "bool ReadFromChunk(CKStateChunk@ chunk, CKContext@ context, CKFile@ file, NativeBuffer Identifiers)", asFUNCTIONPR([](T *self, CKStateChunk *chunk, CKContext *context, CKFile *file, NativeBuffer identifiers) -> bool { return self->ReadFromChunk(chunk, context, file, identifiers); }, (T *, CKStateChunk *, CKContext *, CKFile *, NativeBuffer), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
}

void RegisterCKTexture(asIScriptEngine *engine) {
    assert(engine != nullptr);

    int r = 0;

    RegisterCKBeObjectMembers<CKTexture>(engine, "CKTexture");
    RegisterCKBitmapDataMembers<CKTexture>(engine, "CKTexture");

    r = engine->RegisterObjectMethod("CKTexture", "bool LoadImage(const string &in name, int slot = 0)", asFUNCTIONPR([](CKTexture *texture, const std::string &name, int slot) -> bool { return texture->LoadImage(const_cast<CKSTRING>(name.c_str()), slot); }, (CKTexture *, const std::string &, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKTexture", "bool LoadMovie(const string &in name)", asFUNCTIONPR([](CKTexture *texture, const std::string &name) -> bool { return texture->LoadMovie(const_cast<CKSTRING>(name.c_str())); }, (CKTexture *, const std::string &), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKTexture", "bool SetAsCurrent(CKRenderContext@ dev, bool clamping = false, int textureStage = 0)", asFUNCTIONPR([](CKTexture *self, CKRenderContext *dev, bool clamping, int textureStage) -> bool { return self->SetAsCurrent(dev, clamping, textureStage); }, (CKTexture *, CKRenderContext *, bool, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKTexture", "bool Restore(bool clamp = false)", asFUNCTIONPR([](CKTexture *self, bool clamp) -> bool { return self->Restore(clamp); }, (CKTexture *, bool), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKTexture", "bool SystemToVideoMemory(CKRenderContext@ dev, bool clamping = false)", asFUNCTIONPR([](CKTexture *self, CKRenderContext *dev, bool clamping) -> bool { return self->SystemToVideoMemory(dev, clamping); }, (CKTexture *, CKRenderContext *, bool), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKTexture", "bool FreeVideoMemory()", asFUNCTIONPR([](CKTexture *self) -> bool { return self->FreeVideoMemory(); }, (CKTexture *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKTexture", "bool IsInVideoMemory() const", asFUNCTIONPR([](CKTexture *self) -> bool { return self->IsInVideoMemory(); }, (CKTexture *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKTexture", "bool CopyContext(CKRenderContext@ dev, const VxRect &in src, const VxRect &in dest, int cubeMapFace = 0)", asFUNCTIONPR([](CKTexture *self, CKRenderContext *dev, VxRect &src, VxRect &dest, int cubeMapFace) -> bool { return self->CopyContext(dev, &src, &dest, cubeMapFace); }, (CKTexture *, CKRenderContext *, VxRect &, VxRect &, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKTexture", "bool UseMipmap(bool useMipmap)", asFUNCTIONPR([](CKTexture *self, bool useMipmap) -> bool { return self->UseMipmap(useMipmap); }, (CKTexture *, bool), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKTexture", "int GetMipmapCount() const", asMETHODPR(CKTexture, GetMipmapCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKTexture", "bool GetVideoTextureDesc(VxImageDescEx &out desc)", asFUNCTIONPR([](CKTexture *self, VxImageDescEx &desc) -> bool { return self->GetVideoTextureDesc(desc); }, (CKTexture *, VxImageDescEx &), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKTexture", "VX_PIXELFORMAT GetVideoPixelFormat() const", asMETHODPR(CKTexture, GetVideoPixelFormat, (), VX_PIXELFORMAT), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKTexture", "bool GetSystemTextureDesc(VxImageDescEx &out desc)", asFUNCTIONPR([](CKTexture *self, VxImageDescEx &desc) -> bool { return self->GetSystemTextureDesc(desc); }, (CKTexture *, VxImageDescEx &), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKTexture", "void SetDesiredVideoFormat(VX_PIXELFORMAT format)", asMETHODPR(CKTexture, SetDesiredVideoFormat, (VX_PIXELFORMAT), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKTexture", "VX_PIXELFORMAT GetDesiredVideoFormat() const", asMETHODPR(CKTexture, GetDesiredVideoFormat, (), VX_PIXELFORMAT), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKTexture", "bool SetUserMipMapMode(bool userMipmap)", asFUNCTIONPR([](CKTexture *self, bool userMipmap) -> bool { return self->SetUserMipMapMode(userMipmap); }, (CKTexture *, bool), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKTexture", "bool GetUserMipMapLevel(int level, VxImageDescEx &out resultImage)", asFUNCTIONPR([](CKTexture *self, int level, VxImageDescEx &resultImage) -> bool { return self->GetUserMipMapLevel(level, resultImage); }, (CKTexture *, int, VxImageDescEx &), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKTexture", "int GetRstTextureIndex() const", asMETHODPR(CKTexture, GetRstTextureIndex, (), int), asCALL_THISCALL); assert(r >= 0);
}

static void CKMesh_MeshRenderCallback(CKRenderContext *dev, CK3dEntity *mov, CKMesh *object, void *data) {
    auto *func = static_cast<asIScriptFunction *>(data);
    if (!func)
        return;

    asIScriptEngine *engine = func->GetEngine();
    asIScriptContext *ctx = engine->RequestContext();

    int r = 0;
    if (func->GetFuncType() == asFUNC_DELEGATE) {
        asIScriptFunction *callback = func->GetDelegateFunction();
        void *callbackObject = func->GetDelegateObject();
        r = ctx->Prepare(callback);
        ctx->SetObject(callbackObject);
    } else {
        r = ctx->Prepare(func);
    }

    if (r < 0) {
        engine->ReturnContext(ctx);
        return;
    }

    ctx->SetArgObject(0, dev);
    ctx->SetArgObject(1, mov);
    ctx->SetArgObject(2, object);

    ctx->Execute();

    engine->ReturnContext(ctx);

    if (IsMarkedAsTemporary(func)) {
        func->Release();
        ClearTemporaryMark(func);
        MarkAsReleasedOnce(func);
    }
}

static void CKMesh_SubMeshRenderCallback(CKRenderContext *dev, CK3dEntity *mov, CKMesh *object, CKMaterial *mat, void *data) {
    auto *func = static_cast<asIScriptFunction *>(data);
    if (!func)
        return;

    asIScriptEngine *engine = func->GetEngine();
    asIScriptContext *ctx = engine->RequestContext();

    int r = 0;
    if (func->GetFuncType() == asFUNC_DELEGATE) {
        asIScriptFunction *callback = func->GetDelegateFunction();
        void *callbackObject = func->GetDelegateObject();
        r = ctx->Prepare(callback);
        ctx->SetObject(callbackObject);
    } else {
        r = ctx->Prepare(func);
    }

    if (r < 0) {
        engine->ReturnContext(ctx);
        return;
    }

    ctx->SetArgObject(0, dev);
    ctx->SetArgObject(1, mov);
    ctx->SetArgObject(2, object);
    ctx->SetArgObject(3, mat);

    ctx->Execute();

    engine->ReturnContext(ctx);

    if (IsMarkedAsTemporary(func)) {
        func->Release();
        ClearTemporaryMark(func);
        MarkAsReleasedOnce(func);
    }
}

template <typename T>
static void RegisterCKMeshMembers(asIScriptEngine *engine, const char *name) {
    assert(engine != nullptr);

    int r = 0;

    RegisterCKBeObjectMembers<T>(engine, name);

    r = engine->RegisterObjectMethod(name, "bool IsTransparent() const", asFUNCTIONPR([](T *self) -> bool { return self->IsTransparent(); }, (T *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetTransparent(bool transparency)", asFUNCTIONPR([](T *self, bool transparency) { self->SetTransparent(transparency); }, (T *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void SetWrapMode(VXTEXTURE_WRAPMODE mode)", asMETHODPR(T, SetWrapMode, (VXTEXTURE_WRAPMODE), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "VXTEXTURE_WRAPMODE GetWrapMode() const", asMETHODPR(T, GetWrapMode, (), VXTEXTURE_WRAPMODE), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void SetLitMode(VXMESH_LITMODE mode)", asMETHODPR(T, SetLitMode, (VXMESH_LITMODE), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "VXMESH_LITMODE GetLitMode() const", asMETHODPR(T, GetLitMode, (), VXMESH_LITMODE), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "CKDWORD GetFlags() const", asMETHODPR(T, GetFlags, (), CKDWORD), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetFlags(CKDWORD flags)", asMETHODPR(T, SetFlags, (CKDWORD), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "NativePointer GetModifierVertices(NativePointer stride)", asFUNCTIONPR([](T *self, NativePointer stride) { return NativePointer(self->GetModifierVertices(reinterpret_cast<CKDWORD *>(stride.Get()))); }, (T *, NativePointer), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "int GetModifierVertexCount() const", asMETHODPR(T, GetModifierVertexCount, (), int), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void ModifierVertexMove(bool rebuildNormals, bool rebuildFaceNormals)", asFUNCTIONPR([](T *self, bool rebuildNormals, bool rebuildFaceNormals) { self->ModifierVertexMove(rebuildNormals, rebuildFaceNormals); }, (T *, bool, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "NativePointer GetModifierUVs(NativePointer stride, int channel = -1)", asFUNCTIONPR([](T *self, NativePointer stride, int channel) { return NativePointer(self->GetModifierUVs(reinterpret_cast<CKDWORD *>(stride.Get()), channel)); }, (T *, NativePointer, int), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "int GetModifierUVCount(int channel = -1) const", asMETHODPR(T, GetModifierUVCount, (int), int), asCALL_THISCALL); assert(r >= 0);

#if CKVERSION != 0x26052005
    r = engine->RegisterObjectMethod(name, "void ModifierUVMove()", asMETHODPR(T, ModifierUVMove, (), void), asCALL_THISCALL); assert(r >= 0);
#else
    r = engine->RegisterObjectMethod(name, "void ModifierUVMove(int channel = -1)", asMETHODPR(T, ModifierUVMove, (int), void), asCALL_THISCALL); assert(r >= 0);
#endif

    r = engine->RegisterObjectMethod(name, "int GetVertexCount() const", asMETHODPR(T, GetVertexCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool SetVertexCount(int count)", asFUNCTIONPR([](T *self, int count) -> bool { return self->SetVertexCount(count); }, (T *, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetVertexColor(int index, CKDWORD color)", asMETHODPR(T, SetVertexColor, (int, CKDWORD), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetVertexSpecularColor(int index, CKDWORD color)", asMETHODPR(T, SetVertexSpecularColor, (int, CKDWORD), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetVertexNormal(int index, const VxVector &in vector)", asMETHODPR(T, SetVertexNormal, (int, VxVector *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetVertexPosition(int index, const VxVector &in vector)", asMETHODPR(T, SetVertexPosition, (int, VxVector *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetVertexTextureCoordinates(int index, float u, float v, int channel = -1)", asMETHODPR(T, SetVertexTextureCoordinates, (int, float, float, int), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "NativePointer GetColorsPtr(NativePointer stride)", asFUNCTIONPR([](T *self, NativePointer stride) { return NativePointer(self->GetColorsPtr(reinterpret_cast<CKDWORD *>(stride.Get()))); }, (T *, NativePointer), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "NativePointer GetSpecularColorsPtr(NativePointer stride)", asFUNCTIONPR([](T *self, NativePointer stride) { return NativePointer(self->GetSpecularColorsPtr(reinterpret_cast<CKDWORD *>(stride.Get()))); }, (T *, NativePointer), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "NativePointer GetNormalsPtr(NativePointer stride)", asFUNCTIONPR([](T *self, NativePointer stride) { return NativePointer(self->GetNormalsPtr(reinterpret_cast<CKDWORD *>(stride.Get()))); }, (T *, NativePointer), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "NativePointer GetPositionsPtr(NativePointer stride)", asFUNCTIONPR([](T *self, NativePointer stride) { return NativePointer(self->GetPositionsPtr(reinterpret_cast<CKDWORD *>(stride.Get()))); }, (T *, NativePointer), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "NativePointer GetTextureCoordinatesPtr(NativePointer stride, int channel = -1)", asFUNCTIONPR([](T *self, NativePointer stride, int channel) { return NativePointer(self->GetTextureCoordinatesPtr(reinterpret_cast<CKDWORD *>(stride.Get()), channel)); }, (T *, NativePointer, int), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKDWORD GetVertexColor(int index) const", asMETHODPR(T, GetVertexColor, (int), CKDWORD), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKDWORD GetVertexSpecularColor(int index) const", asMETHODPR(T, GetVertexSpecularColor, (int), CKDWORD), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void GetVertexNormal(int index, VxVector &out vector)", asMETHODPR(T, GetVertexNormal, (int, VxVector *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void GetVertexPosition(int index, VxVector &out vector)", asMETHODPR(T, GetVertexPosition, (int, VxVector *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void GetVertexTextureCoordinates(int index, float &out u, float &out v, int channel = -1)", asMETHODPR(T, GetVertexTextureCoordinates, (int, float *, float *, int), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void TranslateVertices(NativePointer vector)", asFUNCTIONPR([](T *self, NativePointer vector) { self->TranslateVertices(reinterpret_cast<VxVector *>(vector.Get())); }, (T *, NativePointer), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void ScaleVertices(const VxVector &in vector, const VxVector &in pivot = void)", asMETHODPR(T, ScaleVertices, (VxVector *, VxVector *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void ScaleVertices(float x, float y, float z, const VxVector &in pivot = void)", asMETHODPR(T, ScaleVertices3f, (float, float, float, VxVector *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void RotateVertices(const VxVector &in vector, float angle)", asMETHODPR(T, RotateVertices, (VxVector *, float), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void VertexMove()", asMETHODPR(T, VertexMove, (), void), asCALL_THISCALL); assert(r >= 0);
#if CKVERSION != 0x26052005
    r = engine->RegisterObjectMethod(name, "void UVChanged()", asMETHODPR(T, UVChanged, (), void), asCALL_THISCALL); assert(r >= 0);
#else
    r = engine->RegisterObjectMethod(name, "void UVChanged(int channel = -1)", asMETHODPR(T, UVChanged, (int), void), asCALL_THISCALL); assert(r >= 0);
#endif
    r = engine->RegisterObjectMethod(name, "void NormalChanged()", asMETHODPR(T, NormalChanged, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void ColorChanged()", asMETHODPR(T, ColorChanged, (), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "int GetFaceCount() const", asMETHODPR(T, GetFaceCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool SetFaceCount(int count)", asFUNCTIONPR([](T *self, int count) -> bool { return self->SetFaceCount(count); }, (T *, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "NativePointer GetFacesIndices()", asFUNCTIONPR([](T *self) { return NativePointer(self->GetFacesIndices()); }, (T *), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void GetFaceVertexIndex(int index, int &out vertex1, int &out vertex2, int &out vertex3)", asMETHODPR(T, GetFaceVertexIndex, (int, int&, int&, int&), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKMaterial@ GetFaceMaterial(int index)", asMETHODPR(T, GetFaceMaterial, (int), CKMaterial *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "const VxVector &GetFaceNormal(int index)", asMETHODPR(T, GetFaceNormal, (int), const VxVector &), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKWORD GetFaceChannelMask(int faceIndex)", asMETHODPR(T, GetFaceChannelMask, (int), CKWORD), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "VxVector &GetFaceVertex(int FaceIndex, int vertexIndex)", asMETHODPR(T, GetFaceVertex, (int, int), VxVector &), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "NativePointer GetFaceNormalsPtr(NativePointer stride)", asFUNCTIONPR([](T *self, NativePointer stride) { return NativePointer(self->GetFaceNormalsPtr(reinterpret_cast<CKDWORD *>(stride.Get()))); }, (T *, NativePointer), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetFaceVertexIndex(int faceIndex, int vertex1, int vertex2, int vertex3)", asMETHODPR(T, SetFaceVertexIndex, (int, int, int, int), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetFaceMaterial(NativePointer faceIndices, int faceCount, CKMaterial@ mat)", asFUNCTIONPR([](T *self, NativePointer faceIndices, int faceCount, CKMaterial *mat) { self->SetFaceMaterialEx(reinterpret_cast<int *>(faceIndices.Get()), faceCount, mat); }, (T *, NativePointer, int, CKMaterial *), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetFaceMaterial(int faceIndex, CKMaterial@ mat)", asMETHODPR(T, SetFaceMaterial, (int, CKMaterial *), void), asCALL_THISCALL); assert(r >= 0);
#if CKVERSION != 0x26052005
    r = engine->RegisterObjectMethod(name, "void SetFaceChannelMask(int faceIndex, CKWORD channelMask)", asMETHODPR(T, SetFaceChannelMask, (int, CKWORD), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void ReplaceMaterial(CKMaterial@ oldMat, CKMaterial@ newMat)", asMETHODPR(T, ReplaceMaterial, (CKMaterial *, CKMaterial *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void ChangeFaceChannelMask(int faceIndex, CKWORD addChannelMask, CKWORD removeChannelMask)", asMETHODPR(T, ChangeFaceChannelMask, (int, CKWORD, CKWORD), void), asCALL_THISCALL); assert(r >= 0);
#else
    r = engine->RegisterObjectMethod(name, "void ReplaceMaterial(CKMaterial@ oldMat, CKMaterial@ newMat, bool merge = false)", asFUNCTIONPR([](T* self, CKMaterial *oldMat, CKMaterial *newMat, bool merge) { self->ReplaceMaterial(oldMat, newMat, merge); }, (T *, CKMaterial *, CKMaterial *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
#endif
    r = engine->RegisterObjectMethod(name, "void ApplyGlobalMaterial(CKMaterial@ mat)", asMETHODPR(T, ApplyGlobalMaterial, (CKMaterial *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void DissociateAllFaces()", asMETHODPR(T, DissociateAllFaces, (), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "bool SetLineCount(int count)", asFUNCTIONPR([](T *self, int count) -> bool { return self->SetLineCount(count); }, (T *, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "int GetLineCount()", asMETHODPR(T, GetLineCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "NativePointer GetLineIndices()", asFUNCTIONPR([](T *self) { return NativePointer(self->GetLineIndices()); }, (T *), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetLine(int lineIndex, int vIndex1, int vIndex2)", asMETHODPR(T, SetLine, (int, int, int), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void GetLine(int lineIndex, int &out vIndex1, int &out vIndex2)", asMETHODPR(T, GetLine, (int, int*, int*), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void CreateLineStrip(int startingLine, int count, int startingVertexIndex)", asMETHODPR(T, CreateLineStrip, (int, int, int), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void Clean(bool keepVertices = false)", asFUNCTIONPR([](T *self, bool keepVertices) { self->Clean(keepVertices); }, (T *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void InverseWinding()", asMETHODPR(T, InverseWinding, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void Consolidate()", asMETHODPR(T, Consolidate, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void UnOptimize()", asMETHODPR(T, UnOptimize, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "float GetRadius() const", asMETHODPR(T, GetRadius, (), float), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "const VxBbox &GetLocalBox() const", asMETHODPR(T, GetLocalBox, (), const VxBbox&), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void GetBaryCenter(VxVector &out vector) const", asMETHODPR(T, GetBaryCenter, (VxVector *), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "int GetChannelCount() const", asMETHODPR(T, GetChannelCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "int AddChannel(CKMaterial@ mat, bool copySrcUv = true)", asFUNCTIONPR([](T *self, CKMaterial *mat, bool copySrcUv) { return self->AddChannel(mat, copySrcUv); }, (T *, CKMaterial *, bool), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void RemoveChannel(CKMaterial@ mat)", asMETHODPR(T, RemoveChannelByMaterial, (CKMaterial *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void RemoveChannel(int index)", asMETHODPR(T, RemoveChannel, (int), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "int GetChannelByMaterial(CKMaterial@ mat) const", asMETHODPR(T, GetChannelByMaterial, (CKMaterial *), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void ActivateChannel(int index, bool active = true)", asFUNCTIONPR([](T *self, int index, bool active) { self->ActivateChannel(index, active); }, (T *, int, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool IsChannelActive(int index) const", asFUNCTIONPR([](T *self, int index) -> bool { return self->IsChannelActive(index); }, (T *, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void ActivateAllChannels(bool active = true)", asFUNCTIONPR([](T *self, bool active) { self->ActivateAllChannels(active); }, (T *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void LitChannel(int index, bool lit = true)", asFUNCTIONPR([](T *self, int index, bool lit) { self->LitChannel(index, lit); }, (T *, int, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool IsChannelLit(int index) const", asFUNCTIONPR([](T *self, int index) -> bool { return self->IsChannelLit(index); }, (T *, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKDWORD GetChannelFlags(int index) const", asMETHODPR(T, GetChannelFlags, (int), CKDWORD), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetChannelFlags(int index, CKDWORD flags)", asMETHODPR(T, SetChannelFlags, (int, CKDWORD), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKMaterial@ GetChannelMaterial(int index) const", asMETHODPR(T, GetChannelMaterial, (int), CKMaterial *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetChannelMaterial(int index, CKMaterial@ mat)", asMETHODPR(T, SetChannelMaterial, (int, CKMaterial *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "VXBLEND_MODE GetChannelSourceBlend(int index) const", asMETHODPR(T, GetChannelSourceBlend, (int), VXBLEND_MODE), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "VXBLEND_MODE GetChannelDestBlend(int index) const", asMETHODPR(T, GetChannelDestBlend, (int), VXBLEND_MODE), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetChannelSourceBlend(int index, VXBLEND_MODE blendMode)", asMETHODPR(T, SetChannelSourceBlend, (int, VXBLEND_MODE), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetChannelDestBlend(int index, VXBLEND_MODE blendMode)", asMETHODPR(T, SetChannelDestBlend, (int, VXBLEND_MODE), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void BuildNormals()", asMETHODPR(T, BuildNormals, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void BuildFaceNormals()", asMETHODPR(T, BuildFaceNormals, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKERROR Render(CKRenderContext@ dev, CK3dEntity@ mov)", asMETHODPR(T, Render, (CKRenderContext*, CK3dEntity *), CKERROR), asCALL_THISCALL); assert(r >= 0);

    // r = engine->RegisterObjectMethod(name, "bool AddPreRenderCallBack(uintptr_t func, uintptr_t argument, bool temporary = false)", asMETHODPR(T, AddPreRenderCallBack, (CK_MESHRENDERCALLBACK, void *, CKBOOL), CKBOOL), asCALL_THISCALL); assert(r >= 0);
    // r = engine->RegisterObjectMethod(name, "bool RemovePreRenderCallBack(uintptr_t func, uintptr_t argument)", asMETHODPR(T, RemovePreRenderCallBack, (CK_MESHRENDERCALLBACK, void *), CKBOOL), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void AddPreRenderCallBack(CK_MESHRENDERCALLBACK@ callback, bool temporary = false)", asFUNCTIONPR([](T *self, asIScriptFunction *scriptFunc, bool temporary) -> bool {
        scriptFunc->AddRef(); if (temporary) MarkAsTemporary(scriptFunc); return self->AddPreRenderCallBack(CKMesh_MeshRenderCallback, scriptFunc, temporary);
    }, (T *, asIScriptFunction *, bool), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void RemovePreRenderCallBack(CK_MESHRENDERCALLBACK@ callback)", asFUNCTIONPR([](T *self, asIScriptFunction *scriptFunc) -> bool {
        bool result = self->RemovePreRenderCallBack(CKMesh_MeshRenderCallback, scriptFunc); if (IsMarkedAsReleasedOnce(scriptFunc)) ClearReleasedOnceMark(scriptFunc); else scriptFunc->Release(); return result;
    }, (T *, asIScriptFunction *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // r = engine->RegisterObjectMethod(name, "bool AddPostRenderCallBack(uintptr_t func, uintptr_t argument, bool temporary = false)", asMETHODPR(T, AddPostRenderCallBack, (CK_MESHRENDERCALLBACK, void *, CKBOOL), CKBOOL), asCALL_THISCALL); assert(r >= 0);
    // r = engine->RegisterObjectMethod(name, "bool RemovePostRenderCallBack(uintptr_t func, uintptr_t argument)", asMETHODPR(T, RemovePostRenderCallBack, (CK_MESHRENDERCALLBACK, void *), CKBOOL), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void AddPostRenderCallBack(CK_MESHRENDERCALLBACK@ callback, bool temporary = false)", asFUNCTIONPR([](T *self, asIScriptFunction *scriptFunc, bool temporary) -> bool {
        scriptFunc->AddRef(); if (temporary) MarkAsTemporary(scriptFunc); return self->AddPostRenderCallBack(CKMesh_MeshRenderCallback, scriptFunc, temporary);
    }, (T *, asIScriptFunction *, bool), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void RemovePostRenderCallBack(CK_MESHRENDERCALLBACK@ callback)", asFUNCTIONPR([](T *self, asIScriptFunction *scriptFunc) -> bool {
        bool result = self->RemovePostRenderCallBack(CKMesh_MeshRenderCallback, scriptFunc); if (IsMarkedAsReleasedOnce(scriptFunc)) ClearReleasedOnceMark(scriptFunc); else scriptFunc->Release(); return result;
    }, (T *, asIScriptFunction *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // r = engine->RegisterObjectMethod(name, "void SetRenderCallBack(uintptr_t func, uintptr_t argument)", asMETHODPR(T, SetRenderCallBack, (CK_MESHRENDERCALLBACK, void *), void), asCALL_THISCALL); assert(r >= 0);
    // r = engine->RegisterObjectMethod(name, "void SetDefaultRenderCallBack()", asMETHODPR(T, SetDefaultRenderCallBack, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetRenderCallBack(CK_MESHRENDERCALLBACK@ callback)", asFUNCTIONPR([](T *self, asIScriptFunction *scriptFunc) {
        scriptFunc->AddRef();
        // ScriptManager::SetCKObjectData removed - not available in this build
        self->SetRenderCallBack(CKMesh_MeshRenderCallback, scriptFunc);
    }, (T *, asIScriptFunction *), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetDefaultRenderCallBack()", asFUNCTIONPR([](T *self) {
        self->SetDefaultRenderCallBack();
        // ScriptManager cleanup removed - not available in this build
    }, (T *), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void RemoveAllCallbacks()", asMETHODPR(T, RemoveAllCallbacks, (), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "int GetMaterialCount() const", asMETHODPR(T, GetMaterialCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKMaterial@ GetMaterial(int index) const", asMETHODPR(T, GetMaterial, (int), CKMaterial *), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "int GetVertexWeightsCount() const", asMETHODPR(T, GetVertexWeightsCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetVertexWeightsCount(int count)", asMETHODPR(T, SetVertexWeightsCount, (int), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "NativePointer GetVertexWeightsPtr()", asFUNCTIONPR([](T *self) { return NativePointer(self->GetVertexWeightsPtr()); }, (T *), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "float GetVertexWeight(int index) const", asMETHODPR(T, GetVertexWeight, (int), float), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetVertexWeight(int index, float w)", asMETHODPR(T, SetVertexWeight, (int, float), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void LoadVertices(CKStateChunk@ chunk)", asMETHODPR(T, LoadVertices, (CKStateChunk *), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void SetVerticesRendered(int count)", asMETHODPR(T, SetVerticesRendered, (int), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "int GetVerticesRendered() const", asMETHODPR(T, GetVerticesRendered, (), int), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "CKERROR CreatePM()", asMETHODPR(T, CreatePM, (), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void DestroyPM()", asMETHODPR(T, DestroyPM, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool IsPM() const", asFUNCTIONPR([](T *self) -> bool { return self->IsPM(); }, (T *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void EnablePMGeoMorph(bool enable)", asFUNCTIONPR([](T *self, bool enable) { self->EnablePMGeoMorph(enable); }, (T *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool IsPMGeoMorphEnabled() const", asFUNCTIONPR([](T *self) -> bool { return self->IsPMGeoMorphEnabled(); }, (T *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetPMGeoMorphStep(int gs)", asMETHODPR(T, SetPMGeoMorphStep, (int), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "int GetPMGeoMorphStep() const", asMETHODPR(T, GetPMGeoMorphStep, (), int), asCALL_THISCALL); assert(r >= 0);

    // r = engine->RegisterObjectMethod(name, "bool AddSubMeshPreRenderCallBack(uintptr_t func, uintptr_t argument, bool temporary = false)", asMETHODPR(T, AddSubMeshPreRenderCallBack, (CK_SUBMESHRENDERCALLBACK, void *, CKBOOL), CKBOOL), asCALL_THISCALL); assert(r >= 0);
    // r = engine->RegisterObjectMethod(name, "bool RemoveSubMeshPreRenderCallBack(uintptr_t func, uintptr_t argument)", asMETHODPR(T, RemoveSubMeshPreRenderCallBack, (CK_SUBMESHRENDERCALLBACK, void *), CKBOOL), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void AddSubMeshPreRenderCallBack(CK_SUBMESHRENDERCALLBACK@ callback, bool temporary = false)", asFUNCTIONPR([](T *self, asIScriptFunction *scriptFunc, bool temporary) -> bool {
        scriptFunc->AddRef(); if (temporary) MarkAsTemporary(scriptFunc); return self->AddSubMeshPreRenderCallBack(CKMesh_SubMeshRenderCallback, scriptFunc, temporary);
    }, (T *, asIScriptFunction *, bool), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void RemoveSubMeshPreRenderCallBack(CK_SUBMESHRENDERCALLBACK@ callback)", asFUNCTIONPR([](T *self, asIScriptFunction *scriptFunc) -> bool {
        bool result = self->RemoveSubMeshPreRenderCallBack(CKMesh_SubMeshRenderCallback, scriptFunc); if (IsMarkedAsReleasedOnce(scriptFunc)) ClearReleasedOnceMark(scriptFunc); else scriptFunc->Release(); return result;
    }, (T *, asIScriptFunction *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // r = engine->RegisterObjectMethod(name, "bool AddSubMeshPostRenderCallBack(uintptr_t func, uintptr_t argument, bool temporary = false)", asMETHODPR(T, AddSubMeshPostRenderCallBack, (CK_SUBMESHRENDERCALLBACK, void *, CKBOOL), CKBOOL), asCALL_THISCALL); assert(r >= 0);
    // r = engine->RegisterObjectMethod(name, "bool RemoveSubMeshPostRenderCallBack(uintptr_t func, uintptr_t argument)", asMETHODPR(T, RemoveSubMeshPostRenderCallBack, (CK_SUBMESHRENDERCALLBACK, void *), CKBOOL), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void AddSubMeshPostRenderCallBack(CK_SUBMESHRENDERCALLBACK@ callback, bool temporary = false)", asFUNCTIONPR([](T *self, asIScriptFunction *scriptFunc, bool temporary) -> bool {
        scriptFunc->AddRef(); if (temporary) MarkAsTemporary(scriptFunc); return self->AddSubMeshPostRenderCallBack(CKMesh_SubMeshRenderCallback, scriptFunc, temporary);
    }, (T *, asIScriptFunction *, bool), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void RemoveSubMeshPostRenderCallBack(CK_SUBMESHRENDERCALLBACK@ callback)", asFUNCTIONPR([](T *self, asIScriptFunction *scriptFunc) -> bool {
        bool result = self->RemoveSubMeshPostRenderCallBack(CKMesh_SubMeshRenderCallback, scriptFunc); if (IsMarkedAsReleasedOnce(scriptFunc)) ClearReleasedOnceMark(scriptFunc); else scriptFunc->Release(); return result;
    }, (T *, asIScriptFunction *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    if (strcmp(name, "CKMesh") != 0) {
        RegisterCKObjectCast<T, CKMesh>(engine, name, "CKMesh");
    }
}

void RegisterCKMesh(asIScriptEngine *engine) {
    assert(engine != nullptr);

    RegisterCKMeshMembers<CKMesh>(engine, "CKMesh");
}

void RegisterCKPatchMesh(asIScriptEngine *engine) {
    assert(engine != nullptr);

    int r = 0;

    RegisterCKMeshMembers<CKPatchMesh>(engine, "CKPatchMesh");

    r = engine->RegisterObjectMethod("CKPatchMesh", "CKERROR FromMesh(CKMesh@ m)", asMETHODPR(CKPatchMesh, FromMesh, (CKMesh *), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPatchMesh", "CKERROR ToMesh(CKMesh@ m, int stepCount)", asMETHODPR(CKPatchMesh, ToMesh, (CKMesh *, int), CKERROR), asCALL_THISCALL); assert(r >= 0);

    // Tessellation
    r = engine->RegisterObjectMethod("CKPatchMesh", "void SetIterationCount(int count)", asMETHODPR(CKPatchMesh, SetIterationCount, (int), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPatchMesh", "int GetIterationCount()", asMETHODPR(CKPatchMesh, GetIterationCount, (), int), asCALL_THISCALL); assert(r >= 0);

    // Mesh
    r = engine->RegisterObjectMethod("CKPatchMesh", "void BuildRenderMesh()", asMETHODPR(CKPatchMesh, BuildRenderMesh, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPatchMesh", "void CleanRenderMesh()", asMETHODPR(CKPatchMesh, CleanRenderMesh, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPatchMesh", "void Clear()", asMETHODPR(CKPatchMesh, Clear, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPatchMesh", "void ComputePatchAux(int index)", asMETHODPR(CKPatchMesh, ComputePatchAux, (int), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPatchMesh", "void ComputePatchInteriors(int index)", asMETHODPR(CKPatchMesh, ComputePatchInteriors, (int), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPatchMesh", "CKDWORD GetPatchFlags()", asMETHODPR(CKPatchMesh, GetPatchFlags, (), CKDWORD), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPatchMesh", "void SetPatchFlags(CKDWORD flags)", asMETHODPR(CKPatchMesh, SetPatchFlags, (CKDWORD), void), asCALL_THISCALL); assert(r >= 0);

    // Control Points
    r = engine->RegisterObjectMethod("CKPatchMesh", "void SetVertVecCount(int vertCount, int vecCount)", asMETHODPR(CKPatchMesh, SetVertVecCount, (int, int), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPatchMesh", "int GetVertCount()", asMETHODPR(CKPatchMesh, GetVertCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPatchMesh", "void SetVert(int index, VxVector &in cp)", asMETHODPR(CKPatchMesh, SetVert, (int, VxVector *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPatchMesh", "void GetVert(int index, VxVector &out cp)", asMETHODPR(CKPatchMesh, GetVert, (int, VxVector *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPatchMesh", "NativePointer GetVerts()", asFUNCTIONPR([](CKPatchMesh *self) { return NativePointer(self->GetVerts()); }, (CKPatchMesh *), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPatchMesh", "int GetVecCount()", asMETHODPR(CKPatchMesh, GetVecCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPatchMesh", "void SetVec(int index, VxVector &in cp)", asMETHODPR(CKPatchMesh, SetVec, (int, VxVector *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPatchMesh", "void GetVec(int index, VxVector &out cp)", asMETHODPR(CKPatchMesh, GetVec, (int, VxVector *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPatchMesh", "NativePointer GetVecs()", asFUNCTIONPR([](CKPatchMesh *self) { return NativePointer(self->GetVecs()); }, (CKPatchMesh *), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // Edges
    r = engine->RegisterObjectMethod("CKPatchMesh", "void SetEdgeCount(int count)", asMETHODPR(CKPatchMesh, SetEdgeCount, (int), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPatchMesh", "int GetEdgeCount()", asMETHODPR(CKPatchMesh, GetEdgeCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPatchMesh", "void SetEdge(int index, CKPatchEdge &in edge)", asMETHODPR(CKPatchMesh, SetEdge, (int, CKPatchEdge *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPatchMesh", "void GetEdge(int index, CKPatchEdge &out edge)", asMETHODPR(CKPatchMesh, GetEdge, (int, CKPatchEdge *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPatchMesh", "NativePointer GetEdges()", asFUNCTIONPR([](CKPatchMesh *self) { return NativePointer(self->GetEdges()); }, (CKPatchMesh *), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // Patches
    r = engine->RegisterObjectMethod("CKPatchMesh", "void SetPatchCount(int count)", asMETHODPR(CKPatchMesh, SetPatchCount, (int), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPatchMesh", "int GetPatchCount()", asMETHODPR(CKPatchMesh, GetPatchCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPatchMesh", "void SetPatch(int index, CKPatch &in p)", asMETHODPR(CKPatchMesh, SetPatch, (int, CKPatch *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPatchMesh", "void GetPatch(int index, CKPatch &out p)", asMETHODPR(CKPatchMesh, GetPatch, (int, CKPatch *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPatchMesh", "CKDWORD GetPatchSM(int index)", asMETHODPR(CKPatchMesh, GetPatchSM, (int), CKDWORD), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPatchMesh", "void SetPatchSM(int index, CKDWORD smoothing)", asMETHODPR(CKPatchMesh, SetPatchSM, (int, CKDWORD), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPatchMesh", "CKMaterial@ GetPatchMaterial(int index)", asMETHODPR(CKPatchMesh, GetPatchMaterial, (int), CKMaterial *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPatchMesh", "void SetPatchMaterial(int index, CKMaterial@ mat)", asMETHODPR(CKPatchMesh, SetPatchMaterial, (int, CKMaterial *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPatchMesh", "NativePointer GetPatches()", asFUNCTIONPR([](CKPatchMesh *self) { return NativePointer(self->GetPatches()); }, (CKPatchMesh *), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // Texture Patches
    r = engine->RegisterObjectMethod("CKPatchMesh", "void SetTVPatchCount(int count, int channel = -1)", asMETHODPR(CKPatchMesh, SetTVPatchCount, (int, int), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPatchMesh", "int GetTVPatchCount(int channel = -1)", asMETHODPR(CKPatchMesh, GetTVPatchCount, (int), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPatchMesh", "void SetTVPatch(int index, CKTVPatch &in tvPatch, int channel = -1)", asMETHODPR(CKPatchMesh, SetTVPatch, (int, CKTVPatch *, int), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPatchMesh", "void GetTVPatch(int index, CKTVPatch &out tvPatch, int channel = -1)", asMETHODPR(CKPatchMesh, GetTVPatch, (int, CKTVPatch *, int), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPatchMesh", "NativePointer GetTVPatches(int channel = -1)", asFUNCTIONPR([](CKPatchMesh *self, int channel) { return NativePointer(self->GetTVPatches(channel)); }, (CKPatchMesh *, int), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // Texture Vertices
    r = engine->RegisterObjectMethod("CKPatchMesh", "void SetTVCount(int count, int channel = -1)", asMETHODPR(CKPatchMesh, SetTVCount, (int, int), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPatchMesh", "int GetTVCount(int channel = -1)", asMETHODPR(CKPatchMesh, GetTVCount, (int), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPatchMesh", "void SetTV(int index, float u, float v, int channel = -1)", asMETHODPR(CKPatchMesh, SetTV, (int, float, float, int), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPatchMesh", "void GetTV(int index, float &out u, float &out v, int channel = -1)", asMETHODPR(CKPatchMesh, GetTV, (int, float*, float*, int), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPatchMesh", "NativePointer GetTVs(int channel = -1)", asFUNCTIONPR([](CKPatchMesh *self, int channel) { return NativePointer(self->GetTVs(channel)); }, (CKPatchMesh *, int), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
}

static void CKDataArrayGetElementValueGeneric(asIScriptGeneric *gen) {
    asIScriptEngine *engine = gen->GetEngine();
    asIScriptContext *ctx = asGetActiveContext();

    auto *self = static_cast<CKDataArray *>(gen->GetObject());
    int i = gen->GetArgDWord(0);
    int c = gen->GetArgDWord(1);
    const int typeId = gen->GetArgTypeId(2);
    void *buf = *static_cast<void **>(gen->GetAddressOfArg(2));

    CKERROR err = CK_OK;

    if (typeId & asTYPEID_SCRIPTOBJECT) {
        ctx->SetException("Cannot read script objects from buffer");
        gen->SetReturnDWord(CKERR_INVALIDPARAMETER);
        return;
    }

    if (typeId & asTYPEID_APPOBJECT) {
        asITypeInfo *type = engine->GetTypeInfoById(typeId);
        if (!type) {
            gen->SetReturnDWord(CKERR_INVALIDPARAMETER);
            return;
        }

        if (typeId & asTYPEID_OBJHANDLE) {
            ctx->SetException("Cannot read object handle from buffer");
            gen->SetReturnDWord(CKERR_INVALIDPARAMETER);
            return;
        }

        if (strcmp(type->GetName(), "string") == 0) {
            std::string &str = *static_cast<std::string *>(buf);
            CKSTRING value = (CKSTRING) self->GetElement(i, c);
            if (value)
                str = value;
            else
                err = CKERR_INVALIDPARAMETER;
        } else {
            int size = engine->GetSizeOfPrimitiveType(typeId);
            if (size == 0) {
                if (type->GetFlags() & asOBJ_POD) {
                    err = self->GetElementValue(i, c, buf);
                } else {
                    ctx->SetException("Cannot read non-POD objects from buffer");
                    gen->SetReturnDWord(CKERR_INVALIDPARAMETER);
                    return;
                }
            }
        }
    } else {
        int size = engine->GetSizeOfPrimitiveType(typeId);
        if (size != 0) {
            err = self->GetElementValue(i, c, buf);
        }
    }

    gen->SetReturnDWord(err);
}

static void CKDataArraySetElementValueGeneric(asIScriptGeneric *gen) {
    asIScriptEngine *engine = gen->GetEngine();
    asIScriptContext *ctx = asGetActiveContext();

    auto *self = static_cast<CKDataArray *>(gen->GetObject());
    int i = gen->GetArgDWord(0);
    int c = gen->GetArgDWord(1);
    const int typeId = gen->GetArgTypeId(2);
    void *buf = *static_cast<void **>(gen->GetAddressOfArg(2));

    CKERROR err = CK_OK;

    if (typeId & asTYPEID_SCRIPTOBJECT) {
        ctx->SetException("Cannot write script objects to buffer");
        gen->SetReturnDWord(CKERR_INVALIDPARAMETER);
        return;
    }

    if (typeId & asTYPEID_APPOBJECT) {
        asITypeInfo *type = engine->GetTypeInfoById(typeId);
        if (!type) {
            gen->SetReturnDWord(CKERR_INVALIDPARAMETER);
            return;
        }

        if (typeId & asTYPEID_OBJHANDLE) {
            ctx->SetException("Cannot write object handle to buffer");
            gen->SetReturnDWord(CKERR_INVALIDPARAMETER);
            return;
        }

        if (strcmp(type->GetName(), "string") == 0) {
            std::string &str = *static_cast<std::string *>(buf);
            err = self->SetElementStringValue(i, c, str.data());
        } else {
            int size = engine->GetSizeOfPrimitiveType(typeId);
            if (size == 0) {
                if (type->GetFlags() & asOBJ_POD) {
                    err = self->SetElementValue(i, c, buf);
                } else {
                    ctx->SetException("Cannot write non-POD objects to buffer");
                    gen->SetReturnDWord(CKERR_INVALIDPARAMETER);
                    return;
                }
            }
        }
    } else {
        int size = engine->GetSizeOfPrimitiveType(typeId);
        if (size != 0) {
            err = self->SetElementValue(i, c, buf);
        }
    }

    gen->SetReturnDWord(err);
}

void RegisterCKDataArray(asIScriptEngine *engine) {
    assert(engine != nullptr);

    int r = 0;

    RegisterCKBeObjectMembers<CKDataArray>(engine, "CKDataArray");

    // Column/Format Functions
    r = engine->RegisterObjectMethod("CKDataArray", "void InsertColumn(int cDest, CK_ARRAYTYPE type, const string &in name, CKGUID paramGuid = CKGUID(0, 0))", asFUNCTIONPR([](CKDataArray *array, int cDest, CK_ARRAYTYPE type, const std::string &name, CKGUID paramGuid) { array->InsertColumn(cDest, type, const_cast<char *>(name.c_str()), paramGuid); }, (CKDataArray *, int, CK_ARRAYTYPE, const std::string &, CKGUID), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDataArray", "void MoveColumn(int cSrc, int cDest)", asMETHODPR(CKDataArray, MoveColumn, (int, int), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDataArray", "void RemoveColumn(int c)", asMETHODPR(CKDataArray, RemoveColumn, (int), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDataArray", "void SetColumnName(int c, const string &in name)", asFUNCTIONPR([](CKDataArray *array, int c, const std::string &name) { array->SetColumnName(c, const_cast<char *>(name.c_str())); }, (CKDataArray *, int, const std::string &), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDataArray", "string GetColumnName(int c)", asFUNCTIONPR([](CKDataArray *self, int c) -> std::string { return ScriptStringify(self->GetColumnName(c)); }, (CKDataArray *, int), std::string), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDataArray", "void SetColumnType(int c, CK_ARRAYTYPE type, CKGUID paramGuid = CKGUID(0, 0))", asMETHODPR(CKDataArray, SetColumnType, (int, CK_ARRAYTYPE, CKGUID), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDataArray", "CK_ARRAYTYPE GetColumnType(int c)", asMETHODPR(CKDataArray, GetColumnType, (int), CK_ARRAYTYPE), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDataArray", "CKGUID GetColumnParameterGuid(int c)", asMETHODPR(CKDataArray, GetColumnParameterGuid, (int), CKGUID), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDataArray", "int GetKeyColumn()", asMETHODPR(CKDataArray, GetKeyColumn, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDataArray", "void SetKeyColumn(int c)", asMETHODPR(CKDataArray, SetKeyColumn, (int), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDataArray", "int GetColumnCount()", asMETHODPR(CKDataArray, GetColumnCount, (), int), asCALL_THISCALL); assert(r >= 0);

    // Elements Functions
    r = engine->RegisterObjectMethod("CKDataArray", "NativePointer GetElement(int i, int c)", asFUNCTIONPR([](CKDataArray *self, int i, int c) -> NativePointer { return NativePointer(self->GetElement(i, c)); }, (CKDataArray *, int, int), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    // r = engine->RegisterObjectMethod("CKDataArray", "bool GetElementValue(int i, int c, NativePointer value)", asFUNCTIONPR([](CKDataArray *self, int i, int c, NativePointer value) -> bool { return self->GetElementValue(i, c, value.Get()); }, (CKDataArray *, int, int, NativePointer), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDataArray", "bool GetElementValue(int i, int c, ?&out value)", asFUNCTION(CKDataArrayGetElementValueGeneric), asCALL_GENERIC); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDataArray", "CKObject@ GetElementObject(int i, int c)", asMETHODPR(CKDataArray, GetElementObject, (int, int), CKObject*), asCALL_THISCALL); assert(r >= 0);
    // r = engine->RegisterObjectMethod("CKDataArray", "bool SetElementValue(int i, int c, NativePointer value, int size = 0)", asFUNCTIONPR([](CKDataArray *self, int i, int c, NativePointer value, int size) -> bool { return self->SetElementValue(i, c, value.Get(), size); }, (CKDataArray *, int, int, NativePointer, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDataArray", "bool SetElementValue(int i, int c, ?&in value)", asFUNCTION(CKDataArraySetElementValueGeneric), asCALL_GENERIC); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDataArray", "bool SetElementValueFromParameter(int i, int c, CKParameter@ pout)", asFUNCTIONPR([](CKDataArray *self, int i, int c, CKParameter *pout) -> bool { return self->SetElementValueFromParameter(i, c, pout); }, (CKDataArray *, int, int, CKParameter *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDataArray", "bool SetElementObject(int i, int c, CKObject@ obj)", asFUNCTIONPR([](CKDataArray *self, int i, int c, CKObject *obj) -> bool { return self->SetElementObject(i, c, obj); }, (CKDataArray *, int, int, CKObject *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // Parameters Shortcuts
    r = engine->RegisterObjectMethod("CKDataArray", "bool PasteShortcut(int i, int c, CKParameter@ pout)", asFUNCTIONPR([](CKDataArray *self, int i, int c, CKParameter *pout) -> bool { return self->PasteShortcut(i, c, pout); }, (CKDataArray *, int, int, CKParameter *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDataArray", "CKParameterOut@ RemoveShortcut(int i, int c)", asMETHODPR(CKDataArray, RemoveShortcut, (int, int), CKParameterOut *), asCALL_THISCALL); assert(r >= 0);

    // String Value
    r = engine->RegisterObjectMethod("CKDataArray", "bool SetElementStringValue(int i, int c, const string &in value)", asFUNCTIONPR([](CKDataArray *array, int i, int c, const std::string &value) -> bool { return array->SetElementStringValue(i, c, const_cast<char *>(value.c_str())); }, (CKDataArray *, int, int, const std::string &), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDataArray", "int GetStringValue(CKDWORD key, int c, string &out value)", asFUNCTIONPR([](CKDataArray *array, CKDWORD key, int c, std::string &value) -> int { char buffer[256]; int result = array->GetStringValue(key, c, buffer); value = buffer; return result; }, (CKDataArray *, CKDWORD, int, std::string &), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDataArray", "int GetElementStringValue(int i, int c, string &out value)", asFUNCTIONPR([](CKDataArray *array, int i, int c, std::string &value) -> int { char buffer[256]; int result = array->GetElementStringValue(i, c, buffer); value = buffer; return result; }, (CKDataArray *, int, int, std::string &), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // Load / Write
    r = engine->RegisterObjectMethod("CKDataArray", "bool LoadElements(const string &in str, bool append, int column)", asFUNCTIONPR([](CKDataArray *array, const std::string &str, bool append, int column) -> bool { return array->LoadElements(const_cast<char *>(str.c_str()), append, column); }, (CKDataArray *, const std::string &, bool, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDataArray", "bool WriteElements(const string &in str, int column, int number, bool append = false)", asFUNCTIONPR([](CKDataArray *array, const std::string &str, int column, int number, bool append) -> bool { return array->WriteElements(const_cast<char *>(str.c_str()), column, number, append); }, (CKDataArray *, const std::string &, int, int, bool), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // Rows Functions
    r = engine->RegisterObjectMethod("CKDataArray", "int GetRowCount()", asMETHODPR(CKDataArray, GetRowCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDataArray", "CKDataRow &GetRow(int n)", asMETHODPR(CKDataArray, GetRow, (int), CKDataRow *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDataArray", "void AddRow()", asMETHODPR(CKDataArray, AddRow, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDataArray", "CKDataRow &InsertRow(int n = -1)", asMETHODPR(CKDataArray, InsertRow, (int), CKDataRow *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDataArray", "bool TestRow(int row, int c, CK_COMPOPERATOR op, CKDWORD key, int size = 0)", asFUNCTIONPR([](CKDataArray *self, int row, int c, CK_COMPOPERATOR op, CKDWORD key, int size) -> bool { return self->TestRow(row, c, op, key, size); }, (CKDataArray *, int, int, CK_COMPOPERATOR, CKDWORD, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDataArray", "int FindRowIndex(int c, CK_COMPOPERATOR op, CKDWORD key, int size = 0, int startingIndex = 0)", asMETHODPR(CKDataArray, FindRowIndex, (int, CK_COMPOPERATOR, CKDWORD, int, int), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDataArray", "CKDataRow &FindRow(int c, CK_COMPOPERATOR op, CKDWORD key, int size = 0, int startIndex = 0)", asMETHODPR(CKDataArray, FindRow, (int, CK_COMPOPERATOR, CKDWORD, int, int), CKDataRow *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDataArray", "void RemoveRow(int row)", asMETHODPR(CKDataArray, RemoveRow, (int), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDataArray", "void MoveRow(int rSrc, int rDest)", asMETHODPR(CKDataArray, MoveRow, (int, int), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDataArray", "void SwapRows(int i1, int i2)", asMETHODPR(CKDataArray, SwapRows, (int, int), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDataArray", "void Clear(bool params = true)", asFUNCTIONPR([](CKDataArray *self, bool params) { self->Clear(params); }, (CKDataArray *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // Algorithm
    r = engine->RegisterObjectMethod("CKDataArray", "bool GetHighest(int c, int &out row)", asFUNCTIONPR([](CKDataArray *self, int c, int &row) -> bool { return self->GetHighest(c, row); }, (CKDataArray *, int, int &), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDataArray", "bool GetLowest(int c, int &out row)", asFUNCTIONPR([](CKDataArray *self, int c, int &row) -> bool { return self->GetLowest(c, row); }, (CKDataArray *, int, int &), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDataArray", "bool GetNearest(int c, NativePointer value, int &out row)", asFUNCTIONPR([](CKDataArray *self, int c, NativePointer value, int &row) -> bool { return self->GetNearest(c, value.Get(), row); }, (CKDataArray *, int, NativePointer, int &), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDataArray", "void ColumnTransform(int c, CK_BINARYOPERATOR op, CKDWORD value)", asMETHODPR(CKDataArray, ColumnTransform, (int, CK_BINARYOPERATOR, CKDWORD), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDataArray", "void ColumnsOperate(int c, CK_BINARYOPERATOR op, int c2, int cr)", asMETHODPR(CKDataArray, ColumnsOperate, (int, CK_BINARYOPERATOR, int, int), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDataArray", "void Sort(int c, bool ascending)", asFUNCTIONPR([](CKDataArray *self, int c, bool ascending) { self->Sort(c, ascending); }, (CKDataArray *, int, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDataArray", "void Unique(int c)", asMETHODPR(CKDataArray, Unique, (int), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDataArray", "void RandomShuffle()", asMETHODPR(CKDataArray, RandomShuffle, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDataArray", "void Reverse()", asMETHODPR(CKDataArray, Reverse, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDataArray", "CKDWORD Sum(int c)", asMETHODPR(CKDataArray, Sum, (int), CKDWORD), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDataArray", "CKDWORD Product(int c)", asMETHODPR(CKDataArray, Product, (int), CKDWORD), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDataArray", "int GetCount(int c, CK_COMPOPERATOR op, CKDWORD key, int size = 0)", asMETHODPR(CKDataArray, GetCount, (int, CK_COMPOPERATOR, CKDWORD, int), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKDataArray", "void CreateGroup(int mc, CK_COMPOPERATOR op, CKDWORD key, int size, CKGroup@ group, int ec = 0)", asMETHODPR(CKDataArray, CreateGroup, (int, CK_COMPOPERATOR, CKDWORD, int, CKGroup*, int), void), asCALL_THISCALL); assert(r >= 0);
}

template <typename T>
static void RegisterCKSoundMembers(asIScriptEngine *engine, const char *name) {
    assert(engine != nullptr);

    int r = 0;

    RegisterCKBeObjectMembers<T>(engine, name);

    r = engine->RegisterObjectMethod(name, "CK_SOUND_SAVEOPTIONS GetSaveOptions()", asMETHOD(T, GetSaveOptions), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetSaveOptions(CK_SOUND_SAVEOPTIONS options)", asMETHOD(T, SetSaveOptions), asCALL_THISCALL); assert(r >= 0);

    if (strcmp(name, "CKSound") != 0) {
        RegisterCKObjectCast<T, CKSound>(engine, name, "CKSound");
    }
}

template <>
static void RegisterCKSoundMembers<CKWaveSound>(asIScriptEngine *engine, const char *name) {
    assert(engine != nullptr);

    int r = 0;

    RegisterCKBeObjectMembers<CKWaveSound>(engine, name);

    r = engine->RegisterObjectMethod(name, "CK_SOUND_SAVEOPTIONS GetSaveOptions()", asMETHOD(CKWaveSound, GetSaveOptions), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetSaveOptions(CK_SOUND_SAVEOPTIONS options)", asMETHOD(CKWaveSound, SetSaveOptions), asCALL_THISCALL); assert(r >= 0);

    if (strcmp(name, "CKSound") != 0) {
        RegisterCKObjectCast<CKWaveSound, CKSound>(engine, name, "CKSound");
    }
}

void RegisterCKSound(asIScriptEngine *engine) {
    assert(engine != nullptr);

    RegisterCKSoundMembers<CKSound>(engine, "CKSound");
}

void RegisterCKWaveSound(asIScriptEngine *engine) {
    assert(engine != nullptr);

    int r = 0;

    RegisterCKSoundMembers<CKWaveSound>(engine, "CKWaveSound");

    r = engine->RegisterObjectMethod("CKWaveSound", "CKSOUNDHANDLE PlayMinion(bool background = true, CK3dEntity@ ent = null, VxVector &in position = void, VxVector &in direction = void, float minDelay = 0.0)", asFUNCTIONPR([](CKWaveSound *self, bool background, CK3dEntity *ent, VxVector *position, VxVector *direction, float minDelay) { return self->PlayMinion(background, ent, position, direction, minDelay); }, (CKWaveSound *, bool, CK3dEntity *, VxVector *, VxVector *, float), CKSOUNDHANDLE), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKWaveSound", "CKERROR SetSoundFileName(const string &in filename)", asFUNCTIONPR([](CKWaveSound *self, const std::string &filename) { return self->SetSoundFileName(const_cast<char *>(filename.c_str())); }, (CKWaveSound *, const std::string &), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKWaveSound", "string GetSoundFileName()", asFUNCTIONPR([](CKWaveSound *self) -> std::string { return ScriptStringify(self->GetSoundFileName()); }, (CKWaveSound *), std::string), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKWaveSound", "int GetSoundLength()", asMETHODPR(CKWaveSound, GetSoundLength, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKWaveSound", "CKERROR GetSoundFormat(CKWaveFormat &out format)", asMETHODPR(CKWaveSound, GetSoundFormat, (CKWaveFormat &), CKERROR), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKWaveSound", "CK_WAVESOUND_TYPE GetType()", asMETHODPR(CKWaveSound, GetType, (), CK_WAVESOUND_TYPE), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKWaveSound", "void SetType(CK_WAVESOUND_TYPE type)", asMETHODPR(CKWaveSound, SetType, (CK_WAVESOUND_TYPE), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKWaveSound", "CKDWORD GetState()", asMETHODPR(CKWaveSound, GetState, (), CKDWORD), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKWaveSound", "void SetState(CKDWORD state)", asMETHODPR(CKWaveSound, SetState, (CKDWORD), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKWaveSound", "void SetPriority(float priority)", asMETHODPR(CKWaveSound, SetPriority, (float), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKWaveSound", "float GetPriority()", asMETHODPR(CKWaveSound, GetPriority, (), float), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKWaveSound", "void SetLoopMode(bool enabled)", asFUNCTIONPR([](CKWaveSound *self, bool enabled) { self->SetLoopMode(enabled); }, (CKWaveSound *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKWaveSound", "bool GetLoopMode()", asFUNCTIONPR([](CKWaveSound *self) -> bool { return self->GetLoopMode(); }, (CKWaveSound *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKWaveSound", "CKERROR SetFileStreaming(bool enabled, bool recreateSound = false)", asFUNCTIONPR([](CKWaveSound *self, bool enabled, bool recreateSound) { return self->SetFileStreaming(enabled, recreateSound); }, (CKWaveSound *, bool, bool), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKWaveSound", "bool GetFileStreaming()", asFUNCTIONPR([](CKWaveSound *self) -> bool { return self->GetFileStreaming(); }, (CKWaveSound *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKWaveSound", "void Play(float fadeIn = 0, float finalGain = 1.0)", asMETHODPR(CKWaveSound, Play, (float, float), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKWaveSound", "void Resume()", asMETHODPR(CKWaveSound, Resume, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKWaveSound", "void Rewind()", asMETHODPR(CKWaveSound, Rewind, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKWaveSound", "void Stop(float fadeOut = 0)", asMETHODPR(CKWaveSound, Stop, (float), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKWaveSound", "void Pause()", asMETHODPR(CKWaveSound, Pause, (), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKWaveSound", "bool IsPlaying()", asFUNCTIONPR([](CKWaveSound *self) -> bool { return self->IsPlaying(); }, (CKWaveSound *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKWaveSound", "bool IsPaused()", asFUNCTIONPR([](CKWaveSound *self) -> bool { return self->IsPaused(); }, (CKWaveSound *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKWaveSound", "void SetGain(float gain)", asMETHODPR(CKWaveSound, SetGain, (float), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKWaveSound", "float GetGain()", asMETHODPR(CKWaveSound, GetGain, (), float), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKWaveSound", "void SetPitch(float rate)", asMETHODPR(CKWaveSound, SetPitch, (float), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKWaveSound", "float GetPitch()", asMETHODPR(CKWaveSound, GetPitch, (), float), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKWaveSound", "void SetPan(float pan)", asMETHODPR(CKWaveSound, SetPan, (float), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKWaveSound", "float GetPan()", asMETHODPR(CKWaveSound, GetPan, (), float), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKWaveSound", "CKSOUNDHANDLE GetSource()", asMETHODPR(CKWaveSound, GetSource, (), CKSOUNDHANDLE), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKWaveSound", "void PositionSound(CK3dEntity@ obj, VxVector &in position = void, VxVector &in direction = void, bool commit = false)", asFUNCTIONPR([](CKWaveSound *self, CK3dEntity *obj, VxVector *position, VxVector *direction, bool commit) { self->PositionSound(obj, position, direction, commit); }, (CKWaveSound *, CK3dEntity *, VxVector *, VxVector *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKWaveSound", "CK3dEntity@ GetAttachedEntity()", asMETHODPR(CKWaveSound, GetAttachedEntity, (), CK3dEntity *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKWaveSound", "void GetPosition(VxVector &out pos)", asMETHODPR(CKWaveSound, GetPosition, (VxVector &), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKWaveSound", "void GetDirection(VxVector &out dir)", asMETHODPR(CKWaveSound, GetDirection, (VxVector &), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKWaveSound", "void GetSound3DInformation(VxVector &out pos, VxVector &out dir, float &out distanceFromListener)", asMETHODPR(CKWaveSound, GetSound3DInformation, (VxVector &, VxVector &, float &), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKWaveSound", "void SetCone(float inAngle, float outAngle, float outsideGain)", asMETHODPR(CKWaveSound, SetCone, (float, float, float), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKWaveSound", "void GetCone(float &out inAngle, float &out outAngle, float &out outsideGain)", asMETHODPR(CKWaveSound, GetCone, (float *, float *, float *), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKWaveSound", "void SetMinMaxDistance(float minDistance, float maxDistance, CKDWORD maxDistanceBehavior = 1)", asMETHODPR(CKWaveSound, SetMinMaxDistance, (float, float, CKDWORD), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKWaveSound", "void GetMinMaxDistance(float &out minDistance, float &out maxDistance, CKDWORD &out maxDistanceBehavior)", asMETHODPR(CKWaveSound, GetMinMaxDistance, (float *, float *, CKDWORD *), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKWaveSound", "void SetVelocity(VxVector &in pos)", asMETHODPR(CKWaveSound, SetVelocity, (VxVector &), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKWaveSound", "void GetVelocity(VxVector &out pos)", asMETHODPR(CKWaveSound, GetVelocity, (VxVector &), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKWaveSound", "void SetOrientation(VxVector &in dir, VxVector &in up)", asMETHODPR(CKWaveSound, SetOrientation, (VxVector &, VxVector &), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKWaveSound", "void GetOrientation(VxVector &out dir, VxVector &out up)", asMETHODPR(CKWaveSound, GetOrientation, (VxVector &, VxVector &), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKWaveSound", "CKERROR WriteData(NativePointer buffer, int size)", asFUNCTIONPR([](CKWaveSound *self, NativePointer buffer, int size) { return self->WriteData(reinterpret_cast<CKBYTE *>(buffer.Get()), size); }, (CKWaveSound *, NativePointer, int), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKWaveSound", "CKERROR Lock(CKDWORD writeCursor, CKDWORD numBytes, NativePointer &out ptr1, CKDWORD &out bytes1, NativePointer &out ptr2, CKDWORD &out bytes2, CK_WAVESOUND_LOCKMODE flags)", asFUNCTIONPR([](CKWaveSound *self, CKDWORD writeCursor, CKDWORD numBytes, NativePointer *ptr1, CKDWORD *bytes1, NativePointer *ptr2, CKDWORD* bytes2, CK_WAVESOUND_LOCKMODE flags) { return self->Lock(writeCursor, numBytes, reinterpret_cast<void **>(ptr1), bytes1, reinterpret_cast<void **>(ptr2), bytes2, flags); }, (CKWaveSound *, CKDWORD, CKDWORD, NativePointer *, CKDWORD *, NativePointer *, CKDWORD *, CK_WAVESOUND_LOCKMODE), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKWaveSound", "CKERROR Unlock(NativePointer ptr1, CKDWORD bytes1, NativePointer ptr2, CKDWORD bytes2)", asFUNCTIONPR([](CKWaveSound *self, NativePointer ptr1, CKDWORD bytes1, NativePointer ptr2, CKDWORD bytes2) { return self->Unlock(ptr1.Get(), bytes1, ptr2.Get(), bytes2); }, (CKWaveSound *, NativePointer, CKDWORD, NativePointer, CKDWORD), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKWaveSound", "CKDWORD GetPlayPosition()", asMETHODPR(CKWaveSound, GetPlayPosition, (), CKDWORD), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKWaveSound", "int GetPlayedMs()", asMETHODPR(CKWaveSound, GetPlayedMs, (), int), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKWaveSound", "CKERROR Create(bool fileStreaming, const string &in filename)", asFUNCTIONPR([](CKWaveSound *self, bool fileStreaming, const std::string &filename) { return self->Create(fileStreaming, const_cast<char *>(filename.c_str())); }, (CKWaveSound *, bool, const std::string &), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKWaveSound", "CKERROR Create(CK_WAVESOUND_TYPE type, const CKWaveFormat &in format, int size)", asMETHODPR(CKWaveSound, Create, (CK_WAVESOUND_TYPE, CKWaveFormat *, int), CKERROR), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKWaveSound", "CKERROR SetReader(CKSoundReader@ reader)", asMETHODPR(CKWaveSound, SetReader, (CKSoundReader *), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKWaveSound", "CKSoundReader@ GetReader()", asMETHODPR(CKWaveSound, GetReader, (), CKSoundReader *), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKWaveSound", "void SetDataToRead(int size)", asMETHODPR(CKWaveSound, SetDataToRead, (int), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKWaveSound", "CKERROR Recreate(bool safe = false)", asFUNCTIONPR([](CKWaveSound *self, bool safe) { return self->Recreate(safe); }, (CKWaveSound *, bool), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKWaveSound", "void Release()", asMETHODPR(CKWaveSound, Release, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKWaveSound", "CKERROR TryRecreate()", asMETHODPR(CKWaveSound, TryRecreate, (), CKERROR), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKWaveSound", "void UpdatePosition(float delta)", asMETHODPR(CKWaveSound, UpdatePosition, (float), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKWaveSound", "void UpdateFade()", asMETHODPR(CKWaveSound, UpdateFade, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKWaveSound", "CKERROR WriteDataFromReader()", asMETHODPR(CKWaveSound, WriteDataFromReader, (), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKWaveSound", "void FillWithBlanks(bool incBf = false)", asFUNCTIONPR([](CKWaveSound *self, bool incBf) { self->FillWithBlanks(incBf); }, (CKWaveSound *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKWaveSound", "void InternalStop()", asMETHODPR(CKWaveSound, InternalStop, (), void), asCALL_THISCALL); assert(r >= 0);

    // Not existing in Virtools 2.1
    // r = engine->RegisterObjectMethod("CKWaveSound", "int GetDistanceFromCursor()", asMETHODPR(CKWaveSound, GetDistanceFromCursor, (), int), asCALL_THISCALL); assert(r >= 0);
    // r = engine->RegisterObjectMethod("CKWaveSound", "void InternalSetGain(float)", asMETHODPR(CKWaveSound, InternalSetGain, (float), void), asCALL_THISCALL); assert(r >= 0);
    // r = engine->RegisterObjectMethod("CKWaveSound", "void SaveSettings()", asMETHODPR(CKWaveSound, SaveSettings, (), void), asCALL_THISCALL); assert(r >= 0);
    // r = engine->RegisterObjectMethod("CKWaveSound", "void RestoreSettings()", asMETHODPR(CKWaveSound, RestoreSettings, (), void), asCALL_THISCALL); assert(r >= 0);
    // r = engine->RegisterObjectMethod("CKWaveSound", "uintptr_t ReallocSource(uintptr_t, int, int)", asMETHODPR(CKWaveSound, ReallocSource, (void *, int, int), void *), asCALL_THISCALL); assert(r >= 0);
}

void RegisterCKMidiSound(asIScriptEngine *engine) {
    assert(engine != nullptr);

    int r = 0;

    RegisterCKSoundMembers<CKMidiSound>(engine, "CKMidiSound");

    r = engine->RegisterObjectMethod("CKMidiSound", "CKERROR SetSoundFileName(const string &in filename)", asFUNCTIONPR([](CKMidiSound *self, const std::string &filename) { return self->SetSoundFileName(const_cast<char *>(filename.c_str())); }, (CKMidiSound *, const std::string &), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMidiSound", "string GetSoundFileName()", asFUNCTIONPR([](CKMidiSound *self) -> std::string { return ScriptStringify(self->GetSoundFileName()); }, (CKMidiSound *), std::string), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKMidiSound", "CKDWORD GetCurrentPos()", asMETHODPR(CKMidiSound, GetCurrentPos, (), CKDWORD), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKMidiSound", "CKERROR Play()", asMETHODPR(CKMidiSound, Play, (), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMidiSound", "CKERROR Stop()", asMETHODPR(CKMidiSound, Stop, (), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMidiSound", "CKERROR Pause(bool pause = true)", asFUNCTIONPR([](CKMidiSound *obj, bool pause) { return obj->Pause(pause); }, (CKMidiSound *, bool), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKMidiSound", "bool IsPlaying()", asFUNCTIONPR([](CKMidiSound *self) -> bool { return self->IsPlaying(); }, (CKMidiSound *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKMidiSound", "bool IsPaused()", asFUNCTIONPR([](CKMidiSound *self) -> bool { return self->IsPaused(); }, (CKMidiSound *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // Not existing in Virtools 2.1
    // r = engine->RegisterObjectMethod("CKMidiSound", "CKERROR OpenFile()", asMETHODPR(CKMidiSound, OpenFile, (), CKERROR), asCALL_THISCALL); assert(r >= 0);
    // r = engine->RegisterObjectMethod("CKMidiSound", "CKERROR CloseFile()", asMETHODPR(CKMidiSound, CloseFile, (), CKERROR), asCALL_THISCALL); assert(r >= 0);
    // r = engine->RegisterObjectMethod("CKMidiSound", "CKERROR Prepare()", asMETHODPR(CKMidiSound, Prepare, (), CKERROR), asCALL_THISCALL); assert(r >= 0);
    // r = engine->RegisterObjectMethod("CKMidiSound", "CKERROR Start()", asMETHODPR(CKMidiSound, Start, (), CKERROR), asCALL_THISCALL); assert(r >= 0);
}

static CKBOOL CKRenderObject_Callback(CKRenderContext *dev, CKRenderObject *ent, void *data) {
    auto *func = static_cast<asIScriptFunction *>(data);
    if (!func)
        return FALSE;

    asIScriptEngine *engine = func->GetEngine();
    asIScriptContext *ctx = engine->RequestContext();

    int r = 0;
    if (func->GetFuncType() == asFUNC_DELEGATE) {
        asIScriptFunction *callback = func->GetDelegateFunction();
        void *callbackObject = func->GetDelegateObject();
        r = ctx->Prepare(callback);
        ctx->SetObject(callbackObject);
    } else {
        r = ctx->Prepare(func);
    }

    if (r < 0) {
        engine->ReturnContext(ctx);
        return FALSE;
    }

    ctx->SetArgObject(0, dev);
    ctx->SetArgObject(1, ent);

    ctx->Execute();

    engine->ReturnContext(ctx);

    if (IsMarkedAsTemporary(func)) {
        func->Release();
        ClearTemporaryMark(func);
        MarkAsReleasedOnce(func);
    }

    return r >= 0;
}

template <typename T>
static void RegisterCKRenderObjectMembers(asIScriptEngine *engine, const char *name) {
    assert(engine != nullptr);

    int r = 0;

    RegisterCKBeObjectMembers<T>(engine, name);

    r = engine->RegisterObjectMethod(name, "bool IsInRenderContext(CKRenderContext@ context)", asFUNCTIONPR([](T *self, CKRenderContext *context) -> bool { return self->IsInRenderContext(context); }, (T *, CKRenderContext *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool IsRootObject()", asFUNCTIONPR([](T *self) -> bool { return self->IsRootObject(); }, (T *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool IsToBeRendered()", asFUNCTIONPR([](T *self) -> bool { return self->IsToBeRendered(); }, (T *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void SetZOrder(int z)", asMETHODPR(T, SetZOrder, (int), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "int GetZOrder()", asMETHODPR(T, GetZOrder, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool IsToBeRenderedLast()", asFUNCTIONPR([](T *self) -> bool { return self->IsToBeRenderedLast(); }, (T *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // r = engine->RegisterObjectMethod(name, "bool AddPreRenderCallBack(uintptr_t func, uintptr_t argument, bool temp = false)", asMETHODPR(T, AddPreRenderCallBack, (CK_RENDEROBJECT_CALLBACK, void *, CKBOOL), CKBOOL), asCALL_THISCALL); assert(r >= 0);
    // r = engine->RegisterObjectMethod(name, "bool RemovePreRenderCallBack(uintptr_t func, uintptr_t argument)", asMETHODPR(T, RemovePreRenderCallBack, (CK_RENDEROBJECT_CALLBACK, void *), CKBOOL), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void AddPreRenderCallBack(CK_RENDEROBJECT_CALLBACK@ callback, bool temporary = false)", asFUNCTIONPR([](T *self, asIScriptFunction *scriptFunc, bool temporary) -> bool {
        scriptFunc->AddRef(); if (temporary) MarkAsTemporary(scriptFunc); return self->AddPreRenderCallBack(CKRenderObject_Callback, scriptFunc, temporary);
    }, (T *, asIScriptFunction *, bool), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void RemovePreRenderCallBack(CK_RENDEROBJECT_CALLBACK@ callback)", asFUNCTIONPR([](T *self, asIScriptFunction *scriptFunc) -> bool {
        bool result = self->RemovePreRenderCallBack(CKRenderObject_Callback, scriptFunc); if (IsMarkedAsReleasedOnce(scriptFunc)) ClearReleasedOnceMark(scriptFunc); else scriptFunc->Release(); return result;
    }, (T *, asIScriptFunction *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // r = engine->RegisterObjectMethod(name, "bool SetRenderCallBack(uintptr_t func, uintptr_t argument)", asMETHODPR(T, SetRenderCallBack, (CK_RENDEROBJECT_CALLBACK, void *), CKBOOL), asCALL_THISCALL); assert(r >= 0);
    // r = engine->RegisterObjectMethod(name, "bool RemoveRenderCallBack()", asMETHODPR(T, RemoveRenderCallBack, (), CKBOOL), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetRenderCallBack(CK_RENDEROBJECT_CALLBACK@ callback)", asFUNCTIONPR([](T *self, asIScriptFunction *scriptFunc) -> bool {
        scriptFunc->AddRef();
        // ScriptManager::SetCKObjectData removed - not available in this build
        return self->SetRenderCallBack(CKRenderObject_Callback, scriptFunc);
    }, (T *, asIScriptFunction *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void RemoveRenderCallBack()", asFUNCTIONPR([](T *self) -> bool {
        bool result = self->RemoveRenderCallBack();
        // ScriptManager cleanup removed - not available in this build
        return result;
    }, (T *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // r = engine->RegisterObjectMethod(name, "bool AddPostRenderCallBack(uintptr_t func, uintptr_t argument, bool temp = false)", asMETHODPR(T, AddPostRenderCallBack, (CK_RENDEROBJECT_CALLBACK, void *, CKBOOL), CKBOOL), asCALL_THISCALL); assert(r >= 0);
    // r = engine->RegisterObjectMethod(name, "bool RemovePostRenderCallBack(uintptr_t func, uintptr_t argument)", asMETHODPR(T, RemovePostRenderCallBack, (CK_RENDEROBJECT_CALLBACK, void *), CKBOOL), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void AddPostRenderCallBack(CK_RENDEROBJECT_CALLBACK@ callback, bool temporary = false)", asFUNCTIONPR([](T *self, asIScriptFunction *scriptFunc, bool temporary) -> bool {
        scriptFunc->AddRef(); if (temporary) MarkAsTemporary(scriptFunc); return self->AddPostRenderCallBack(CKRenderObject_Callback, scriptFunc, temporary);
    }, (T *, asIScriptFunction *, bool), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void RemovePostRenderCallBack(CK_RENDEROBJECT_CALLBACK@ callback)", asFUNCTIONPR([](T *self, asIScriptFunction *scriptFunc) -> bool {
        bool result = self->RemovePostRenderCallBack(CKRenderObject_Callback, scriptFunc); if (IsMarkedAsReleasedOnce(scriptFunc)) ClearReleasedOnceMark(scriptFunc); else scriptFunc->Release(); return result;
    }, (T *, asIScriptFunction *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void RemoveAllCallbacks()", asMETHODPR(T, RemoveAllCallbacks, (), void), asCALL_THISCALL); assert(r >= 0);

    if (strcmp(name, "CKRenderObject") != 0) {
        RegisterCKObjectCast<T, CKRenderObject>(engine, name, "CKRenderObject");
    }
}

void RegisterCKRenderObject(asIScriptEngine *engine) {
    assert(engine != nullptr);

    RegisterCKRenderObjectMembers<CKRenderObject>(engine, "CKRenderObject");
}

template <typename T>
void RegisterCK2dEntityMembers(asIScriptEngine *engine, const char *name) {
    assert(engine != nullptr);

    int r = 0;

    RegisterCKRenderObjectMembers<T>(engine, name);

    r = engine->RegisterObjectMethod(name, "int GetPosition(Vx2DVector &out vect, bool hom = false, CK2dEntity@ ref = null)", asFUNCTIONPR([](T *self, Vx2DVector &vect, bool hom, CK2dEntity *ref) -> int { return self->GetPosition(vect, hom, ref); }, (T *, Vx2DVector &, bool, CK2dEntity *), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetPosition(const Vx2DVector &in vect, bool hom = false, bool keepChildren = false, CK2dEntity@ ref = null)", asFUNCTIONPR([](T *self, const Vx2DVector &vect, bool hom, bool keepChildren, CK2dEntity *ref) { self->SetPosition(vect, hom, keepChildren, ref); }, (T *, const Vx2DVector &, bool, bool, CK2dEntity *), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "int GetSize(Vx2DVector &out vect, bool hom = false)", asFUNCTIONPR([](T *self, Vx2DVector &vect, bool hom) -> int { return self->GetSize(vect, hom); }, (T *, Vx2DVector &, bool), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetSize(const Vx2DVector &in vect, bool hom = false, bool keepChildren = false)", asFUNCTIONPR([](T *self, const Vx2DVector &vect, bool hom, bool keepChildren) { self->SetSize(vect, hom, keepChildren); }, (T *, const Vx2DVector &, bool, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void SetRect(const VxRect &in rect, bool keepChildren = false)", asFUNCTIONPR([](T *self, const VxRect &rect, bool keepChildren) { self->SetRect(rect, keepChildren); }, (T *, const VxRect &, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void GetRect(VxRect &out rect)", asMETHODPR(T, GetRect, (VxRect&), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "CKERROR SetHomogeneousRect(const VxRect &in rect, bool keepChildren = false)", asFUNCTIONPR([](T *self, const VxRect &rect, bool keepChildren) { return self->SetHomogeneousRect(rect, keepChildren); }, (T *, const VxRect &, bool), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKERROR GetHomogeneousRect(VxRect &out rect)", asMETHODPR(T, GetHomogeneousRect, (VxRect&), CKERROR), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void SetSourceRect(VxRect &in rect)", asMETHODPR(T, SetSourceRect, (VxRect&), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void GetSourceRect(VxRect &out rect)", asMETHODPR(T, GetSourceRect, (VxRect&), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void UseSourceRect(bool use = true)", asFUNCTIONPR([](T *self, bool use) { self->UseSourceRect(use); }, (T *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool IsUsingSourceRect()", asFUNCTIONPR([](T *self) -> bool { return self->IsUsingSourceRect(); }, (T *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void SetPickable(bool pick = true)", asFUNCTIONPR([](T *self, bool pick) { self->SetPickable(pick); }, (T *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool IsPickable()", asFUNCTIONPR([](T *self) -> bool { return self->IsPickable(); }, (T *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void SetBackground(bool back = true)", asFUNCTIONPR([](T *self, bool back) { self->SetBackground(back); }, (T *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool IsBackground()", asFUNCTIONPR([](T *self) -> bool { return self->IsBackground(); }, (T *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void SetClipToParent(bool clip = true)", asFUNCTIONPR([](T *self, bool clip) { self->SetClipToParent(clip); }, (T *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool IsClipToParent()", asFUNCTIONPR([](T *self) -> bool { return self->IsClipToParent(); }, (T *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void SetFlags(CKDWORD flags)", asMETHODPR(T, SetFlags, (CKDWORD), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void ModifyFlags(CKDWORD add, CKDWORD remove = 0)", asMETHODPR(T, ModifyFlags, (CKDWORD, CKDWORD), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKDWORD GetFlags()", asMETHODPR(T, GetFlags, (), CKDWORD), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void EnableRatioOffset(bool ratio = true)", asFUNCTIONPR([](T *self, bool ratio) { self->EnableRatioOffset(ratio); }, (T *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool IsRatioOffset()", asFUNCTIONPR([](T *self) -> bool { return self->IsRatioOffset(); }, (T *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "bool SetParent(CK2dEntity@ parent)", asFUNCTIONPR([](T *self, CK2dEntity *parent) -> bool { return self->SetParent(parent); }, (T *, CK2dEntity *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CK2dEntity@ GetParent() const", asMETHODPR(T, GetParent, () const, CK2dEntity*), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "int GetChildrenCount() const", asMETHODPR(T, GetChildrenCount, () const, int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CK2dEntity@ GetChild(int i) const", asMETHODPR(T, GetChild, (int) const, CK2dEntity*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CK2dEntity@ HierarchyParser(CK2dEntity@ current) const", asMETHODPR(T, HierarchyParser, (CK2dEntity*) const, CK2dEntity*), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void SetMaterial(CKMaterial@ mat)", asMETHODPR(T, SetMaterial, (CKMaterial *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKMaterial@ GetMaterial()", asMETHODPR(T, GetMaterial, (), CKMaterial *), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void SetHomogeneousCoordinates(bool coord = true)", asFUNCTIONPR([](T *self, bool coord) { self->SetHomogeneousCoordinates(coord); }, (T *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool IsHomogeneousCoordinates()", asFUNCTIONPR([](T *self) -> bool { return self->IsHomogeneousCoordinates(); }, (T *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void EnableClipToCamera(bool Clip = true)", asFUNCTIONPR([](T *self, bool Clip) { self->EnableClipToCamera(Clip); }, (T *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool IsClippedToCamera()", asFUNCTIONPR([](T *self) -> bool { return self->IsClippedToCamera(); }, (T *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "int Render(CKRenderContext@ context)", asMETHODPR(T, Render, (CKRenderContext*), CKERROR), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "int Draw(CKRenderContext@ context)", asMETHODPR(T, Draw, (CKRenderContext*), CKERROR), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void GetExtents(VxRect &out srcRect, VxRect &out rect)", asMETHODPR(T, GetExtents, (VxRect&, VxRect&), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetExtents(const VxRect &in srcRect, const VxRect &in rect)", asMETHODPR(T, SetExtents, (const VxRect&, const VxRect&), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void RestoreInitialSize()", asMETHODPR(T, RestoreInitialSize, (), void), asCALL_THISCALL); assert(r >= 0);

    if (strcmp(name, "CK2dEntity") != 0) {
        RegisterCKObjectCast<T, CK2dEntity>(engine, name, "CK2dEntity");
    }
}

void RegisterCK2dEntity(asIScriptEngine *engine) {
    assert(engine != nullptr);

    RegisterCK2dEntityMembers<CK2dEntity>(engine, "CK2dEntity");
}

template <typename T>
void RegisterCKSpriteMembers(asIScriptEngine *engine, const char *name) {
    assert(engine != nullptr);

    int r = 0;

    RegisterCK2dEntityMembers<T>(engine, name);

    r = engine->RegisterObjectMethod(name, "bool Create(int width, int height, int bpp = 32, int slot = 0)", asFUNCTIONPR([](T *self, int width, int height, int bpp, int slot) -> bool { return self->Create(width, height, bpp, slot); }, (T *, int, int, int, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool LoadImage(const string &in name, int slot = 0)", asFUNCTIONPR([](T *self, const std::string &name, int slot) -> bool { return self->LoadImage(const_cast<char *>(name.c_str()), slot); }, (T *, const std::string &, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool SaveImage(const string &in name, int slot = 0, bool useFormat = false)", asFUNCTIONPR([](T *self, const std::string &name, int slot, bool useFormat) -> bool { return self->SaveImage(const_cast<char *>(name.c_str()), slot, useFormat); }, (T *, const std::string &, int, bool), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "bool LoadMovie(const string &in name, int width = 0, int height = 0, int bpp = 16)", asFUNCTIONPR([](T *self, const std::string &name, int width, int height, int bpp) -> bool { return self->LoadMovie(const_cast<char *>(name.c_str()), width, height, bpp); }, (T *, const std::string &, int, int, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "string GetMovieFileName()", asFUNCTIONPR([](T *self) -> std::string { return ScriptStringify(self->GetMovieFileName()); }, (T *), std::string), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "CKMovieReader@ GetMovieReader()", asMETHODPR(T, GetMovieReader, (), CKMovieReader*), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "NativePointer LockSurfacePtr(int slot = -1)", asFUNCTIONPR([](T *self, int slot) { return NativePointer(self->LockSurfacePtr(slot)); }, (T *, int), NativePointer), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool ReleaseSurfacePtr(int slot = -1)", asFUNCTIONPR([](T *self, int slot) -> bool { return self->ReleaseSurfacePtr(slot); }, (T *, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "string GetSlotFileName(int slot)", asFUNCTIONPR([](T *self, int slot) -> std::string { return ScriptStringify(self->GetSlotFileName(slot)); }, (T *, int), std::string), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool SetSlotFileName(int slot, const string &in filename)", asFUNCTIONPR([](T *self, int slot, const std::string &filename) -> bool { return self->SetSlotFileName(slot, const_cast<CKSTRING>(filename.c_str())); }, (T *, int, const std::string &), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "int GetWidth()", asMETHODPR(T, GetWidth, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "int GetHeight()", asMETHODPR(T, GetHeight, (), int), asCALL_THISCALL); assert(r >= 0);
#if CKVERSION == 0x13022002
    r = engine->RegisterObjectMethod(name, "int GetBitsPerPixel()", asMETHODPR(T, GetBitsPerPixel, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "int GetBytesPerLine()", asMETHODPR(T, GetBytesPerLine, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "int GetRedMask()", asMETHODPR(T, GetRedMask, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "int GetGreenMask()", asMETHODPR(T, GetGreenMask, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "int GetBlueMask()", asMETHODPR(T, GetBlueMask, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "int GetAlphaMask()", asMETHODPR(T, GetAlphaMask, (), int), asCALL_THISCALL); assert(r >= 0);
#endif

    r = engine->RegisterObjectMethod(name, "int GetSlotCount()", asMETHODPR(T, GetSlotCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool SetSlotCount(int count)", asFUNCTIONPR([](T *self, int count) -> bool { return self->SetSlotCount(count); }, (T *, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool SetCurrentSlot(int slot)", asFUNCTIONPR([](T *self, int slot) -> bool { return self->SetCurrentSlot(slot); }, (T *, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "int GetCurrentSlot()", asMETHODPR(T, GetCurrentSlot, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool ReleaseSlot(int slot)", asFUNCTIONPR([](T *self, int slot) -> bool { return self->ReleaseSlot(slot); }, (T *, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool ReleaseAllSlots()", asFUNCTIONPR([](T *self) -> bool { return self->ReleaseAllSlots(); }, (T *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "bool SetPixel(int x, int y, CKDWORD col, int slot = -1)", asFUNCTIONPR([](T *self, int x, int y, CKDWORD col, int slot) -> bool { return self->SetPixel(x, y, col, slot); }, (T *, int, int, CKDWORD, int), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKDWORD GetPixel(int x, int y, int slot = -1)", asMETHODPR(T, GetPixel, (int, int, int), CKDWORD), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "CKDWORD GetTransparentColor()", asMETHODPR(T, GetTransparentColor, (), CKDWORD), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetTransparentColor(CKDWORD Color)", asMETHODPR(T, SetTransparentColor, (CKDWORD), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetTransparent(bool transparency)", asFUNCTIONPR([](T *self, bool transparency) { self->SetTransparent(transparency); }, (T *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool IsTransparent()", asFUNCTIONPR([](T *self) -> bool { return self->IsTransparent(); }, (T *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "bool Restore(bool clamp = false)", asFUNCTIONPR([](T *self, bool clamp) -> bool { return self->Restore(clamp); }, (T *, bool), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool SystemToVideoMemory(CKRenderContext @dev, bool clamping = false)", asFUNCTIONPR([](T *self, CKRenderContext *dev, bool clamping) -> bool { return self->SystemToVideoMemory(dev, clamping); }, (T *, CKRenderContext *, bool), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool FreeVideoMemory()", asFUNCTIONPR([](T *self) -> bool { return self->FreeVideoMemory(); }, (T *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool IsInVideoMemory()", asFUNCTIONPR([](T *self) -> bool { return self->IsInVideoMemory(); }, (T *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool CopyContext(CKRenderContext @ctx, VxRect &in src, VxRect &in dest)", asFUNCTIONPR([](T *self, CKRenderContext *ctx, VxRect *src, VxRect *dest) -> bool { return self->CopyContext(ctx, src, dest); }, (T *, CKRenderContext *, VxRect *, VxRect *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "bool GetVideoTextureDesc(VxImageDescEx &out desc)", asFUNCTIONPR([](T *self, VxImageDescEx &desc) -> bool { return self->GetVideoTextureDesc(desc); }, (T *, VxImageDescEx &), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "VX_PIXELFORMAT GetVideoPixelFormat()", asMETHODPR(T, GetVideoPixelFormat, (), VX_PIXELFORMAT), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool GetSystemTextureDesc(VxImageDescEx &out desc)", asFUNCTIONPR([](T *self, VxImageDescEx &desc) -> bool { return self->GetSystemTextureDesc(desc); }, (T *, VxImageDescEx &), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetDesiredVideoFormat(VX_PIXELFORMAT pf)", asMETHODPR(T, SetDesiredVideoFormat, (VX_PIXELFORMAT), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "VX_PIXELFORMAT GetDesiredVideoFormat()", asMETHODPR(T, GetDesiredVideoFormat, (), VX_PIXELFORMAT), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "CK_TEXTURE_SAVEOPTIONS GetSaveOptions()", asMETHODPR(T, GetSaveOptions, (), CK_BITMAP_SAVEOPTIONS), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetSaveOptions(CK_TEXTURE_SAVEOPTIONS options)", asMETHODPR(T, SetSaveOptions, (CK_BITMAP_SAVEOPTIONS), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKBitmapProperties &GetSaveFormat()", asMETHODPR(T, GetSaveFormat, (), CKBitmapProperties*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetSaveFormat(CKBitmapProperties &in format)", asMETHODPR(T, SetSaveFormat, (CKBitmapProperties*), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void SetPickThreshold(int pt)", asMETHODPR(T, SetPickThreshold, (int), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "int GetPickThreshold()", asMETHODPR(T, GetPickThreshold, (), int), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "bool ToRestore()", asFUNCTIONPR([](T *self) -> bool { return self->ToRestore(); }, (T *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    if (strcmp(name, "CKSprite") != 0) {
        RegisterCKObjectCast<T, CKSprite>(engine, name, "CKSprite");
    }
}

void RegisterCKSprite(asIScriptEngine *engine) {
    assert(engine != nullptr);

    RegisterCKSpriteMembers<CKSprite>(engine, "CKSprite");
}

void RegisterCKSpriteText(asIScriptEngine *engine) {
    assert(engine != nullptr);

    int r = 0;

    RegisterCKSpriteMembers<CKSpriteText>(engine, "CKSpriteText");

    r = engine->RegisterObjectMethod("CKSpriteText", "void SetText(const string &in text)", asFUNCTIONPR([](CKSpriteText *self, const std::string &text) { self->SetText(const_cast<char *>(text.c_str())); }, (CKSpriteText *, const std::string &), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSpriteText", "string GetText()", asFUNCTIONPR([](CKSpriteText *self) -> std::string { return ScriptStringify(self->GetText()); }, (CKSpriteText *), std::string), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKSpriteText", "void SetTextColor(CKDWORD col)", asMETHODPR(CKSpriteText, SetTextColor, (CKDWORD), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSpriteText", "CKDWORD GetTextColor()", asMETHODPR(CKSpriteText, GetTextColor, (), CKDWORD), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKSpriteText", "void SetBackgroundColor(CKDWORD col)", asMETHODPR(CKSpriteText, SetBackgroundColor, (CKDWORD), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSpriteText", "CKDWORD GetBackgroundTextColor()", asMETHODPR(CKSpriteText, GetBackgroundTextColor, (), CKDWORD), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKSpriteText", "void SetFont(const string &in fontName, int fontSize = 12, int weight = 400, bool italic = false, bool underline = false)", asFUNCTIONPR([](CKSpriteText *self, const std::string &fontName, int fontSize, int weight, bool italic, bool underline) { self->SetFont(const_cast<char *>(fontName.c_str()), fontSize, weight, italic, underline); }, (CKSpriteText *, const std::string &, int, int, bool, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKSpriteText", "void SetAlign(CKSPRITETEXT_ALIGNMENT align)", asMETHODPR(CKSpriteText, SetAlign, (CKSPRITETEXT_ALIGNMENT), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSpriteText", "CKSPRITETEXT_ALIGNMENT GetAlign()", asMETHODPR(CKSpriteText, GetAlign, (), CKSPRITETEXT_ALIGNMENT), asCALL_THISCALL); assert(r >= 0);
}

template <typename T>
void RegisterCK3dEntityMembers(asIScriptEngine *engine, const char *name) {
    assert(engine != nullptr);

    int r = 0;

    RegisterCKRenderObjectMembers<T>(engine, name);

    r = engine->RegisterObjectMethod(name, "int GetChildrenCount() const", asMETHODPR(T, GetChildrenCount, () const, int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CK3dEntity@ GetChild(int pos) const", asMETHODPR(T, GetChild, (int) const, CK3dEntity *), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "bool SetParent(CK3dEntity@ parent, bool keepWorldPos = true)", asFUNCTIONPR([](T *self, CK3dEntity *parent, bool keepWorldPos) -> bool { return self->SetParent(parent, keepWorldPos); }, (T *, CK3dEntity *, bool), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CK3dEntity@ GetParent() const", asFUNCTIONPR([](T *self) -> CK3dEntity * { return self->GetParent(); }, (T *), CK3dEntity *), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "bool AddChild(CK3dEntity@ child, bool keepWorldPos = true)", asFUNCTIONPR([](T *self, CK3dEntity *child, bool keepWorldPos) -> bool { return self->AddChild(child, keepWorldPos); }, (T *, CK3dEntity *, bool), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool AddChildren(const XObjectPointerArray &in children, bool keepWorldPos = true)", asFUNCTIONPR([](T *self, const XObjectPointerArray &children, bool keepWorldPos) -> bool { return self->AddChildren(children, keepWorldPos); }, (T *, const XObjectPointerArray &, bool), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool RemoveChild(CK3dEntity@ mov)", asFUNCTIONPR([](T *self, CK3dEntity *mov) -> bool { return self->RemoveChild(mov); }, (T *, CK3dEntity *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool CheckIfSameKindOfHierarchy(CK3dEntity@ mov, bool sameOrder = false) const", asFUNCTIONPR([](T *self, CK3dEntity *mov, bool sameOrder) -> bool { return self->CheckIfSameKindOfHierarchy(mov, sameOrder); }, (T *, CK3dEntity *, bool), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CK3dEntity@ HierarchyParser(CK3dEntity@ current) const", asMETHODPR(T, HierarchyParser, (CK3dEntity *) const, CK3dEntity *), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "CKDWORD GetFlags() const", asMETHODPR(T, GetFlags, () const, CKDWORD), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetFlags(CKDWORD flags)", asMETHODPR(T, SetFlags, (CKDWORD), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void SetPickable(bool pick = true)", asFUNCTIONPR([](T *self, bool pick) { self->SetPickable(pick); }, (T *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool IsPickable() const", asFUNCTIONPR([](const T *self) -> bool { return self->IsPickable(); }, (const T *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

#if CKVERSION != 0x26052005
    r = engine->RegisterObjectMethod(name, "void SetRenderChannels(bool renderChannels = true)", asFUNCTIONPR([](T *self, bool renderChannels) { self->SetRenderChannels(renderChannels); }, (T *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool AreRenderChannelsVisible() const", asFUNCTIONPR([](const T *self) -> bool { return self->AreRenderChannelsVisible(); }, (const T *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
#endif

    r = engine->RegisterObjectMethod(name, "bool IsInViewFrustrum(CKRenderContext@ dev, CKDWORD flags = 0)", asFUNCTIONPR([](T *self, CKRenderContext *dev, CKDWORD flags) -> bool { return self->IsInViewFrustrum(dev, flags); }, (T *, CKRenderContext *, CKDWORD), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool IsInViewFrustrumHierarchic(CKRenderContext@ Dev)", asFUNCTIONPR([](T *self, CKRenderContext *Dev) -> bool { return self->IsInViewFrustrumHierarchic(Dev); }, (T *, CKRenderContext *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void IgnoreAnimations(bool ignore = true)", asFUNCTIONPR([](T *self, bool ignore) { self->IgnoreAnimations(ignore); }, (T *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool AreAnimationIgnored() const", asFUNCTIONPR([](const T *self) -> bool { return self->AreAnimationIgnored(); }, (const T *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "bool IsAllInsideFrustrum() const", asFUNCTIONPR([](const T *self) -> bool { return self->IsAllInsideFrustrum(); }, (const T *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool IsAllOutsideFrustrum() const", asFUNCTIONPR([](const T *self) -> bool { return self->IsAllOutsideFrustrum(); }, (const T *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void SetRenderAsTransparent(bool trans = true)", asFUNCTIONPR([](T *self, bool trans) { self->SetRenderAsTransparent(trans); }, (T *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "CKDWORD GetMoveableFlags() const", asMETHODPR(T, GetMoveableFlags, () const, CKDWORD), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetMoveableFlags(CKDWORD flags)", asMETHODPR(T, SetMoveableFlags, (CKDWORD), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "CKDWORD ModifyMoveableFlags(CKDWORD add, CKDWORD remove)", asMETHODPR(T, ModifyMoveableFlags, (CKDWORD, CKDWORD), CKDWORD), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "CKMesh@ GetCurrentMesh() const", asMETHODPR(T, GetCurrentMesh, () const, CKMesh*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKMesh@ SetCurrentMesh(CKMesh@ m, bool addIfNotHere = true)", asFUNCTIONPR([](T *self, CKMesh *m, bool addIfNotHere) -> CKMesh* { return self->SetCurrentMesh(m, addIfNotHere); }, (T *, CKMesh *, bool), CKMesh *), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "int GetMeshCount() const", asMETHODPR(T, GetMeshCount, () const, int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKMesh@ GetMesh(int pos) const", asMETHODPR(T, GetMesh, (int) const, CKMesh*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKERROR AddMesh(CKMesh@ mesh)", asMETHODPR(T, AddMesh, (CKMesh*), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKERROR RemoveMesh(CKMesh@ mesh)", asMETHODPR(T, RemoveMesh, (CKMesh*), CKERROR), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void LookAt(const VxVector &in pos, CK3dEntity@ ref = null, bool keepChildren = false)", asFUNCTIONPR([](T *self, const VxVector &pos, CK3dEntity *ref, bool keepChildren) { self->LookAt(&pos, ref, keepChildren); }, (T *, const VxVector &, CK3dEntity *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void Rotate(float x, float y, float z, float angle, CK3dEntity@ ref = null, bool keepChildren = false)", asFUNCTIONPR([](T *self, float x, float y, float z, float angle, CK3dEntity *ref, bool keepChildren) { self->Rotate3f(x, y, z, angle, ref, keepChildren); }, (T *, float, float, float, float, CK3dEntity *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void Rotate(const VxVector &in axis, float angle, CK3dEntity@ ref = null, bool keepChildren = false)", asFUNCTIONPR([](T *self, const VxVector &axis, float angle, CK3dEntity *ref, bool keepChildren) { self->Rotate(&axis, angle, ref, keepChildren); }, (T *, const VxVector &, float, CK3dEntity *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void Translate(float x, float y, float z, CK3dEntity@ ref = null, bool keepChildren = false)", asFUNCTIONPR([](T *self, float x, float y, float z, CK3dEntity *ref, bool keepChildren) { self->Translate3f(x, y, z, ref, keepChildren); }, (T *, float, float, float, CK3dEntity *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void Translate(const VxVector &in vect, CK3dEntity@ ref = null, bool keepChildren = false)", asFUNCTIONPR([](T *self, const VxVector &vect, CK3dEntity *ref, bool keepChildren) { self->Translate(&vect, ref, keepChildren); }, (T *, const VxVector &, CK3dEntity *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void AddScale(float x, float y, float z, bool keepChildren = false, bool local = true)", asFUNCTIONPR([](T *self, float x, float y, float z, bool keepChildren, bool local) { self->AddScale3f(x, y, z, keepChildren, local); }, (T *, float, float, float, bool, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void AddScale(const VxVector &in scale, bool keepChildren = false, bool local = true)", asFUNCTIONPR([](T *self, const VxVector &scale, bool keepChildren, bool local) { self->AddScale(&scale, keepChildren, local); }, (T *, const VxVector &, bool, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void SetPosition(float x, float y, float z, CK3dEntity@ ref = null, bool keepChildren = false)", asFUNCTIONPR([](T *self, float x, float y, float z, CK3dEntity *ref, bool keepChildren) { self->SetPosition3f(x, y, z, ref, keepChildren); }, (T *, float, float, float, CK3dEntity *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetPosition(const VxVector &in pos, CK3dEntity@ ref = null, bool keepChildren = false)", asFUNCTIONPR([](T *self, const VxVector &pos, CK3dEntity *ref, bool keepChildren) { self->SetPosition(&pos, ref, keepChildren); }, (T *, const VxVector &, CK3dEntity *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void GetPosition(VxVector&out Pos, CK3dEntity@ Ref = null) const", asMETHODPR(T, GetPosition, (VxVector *, CK3dEntity *) const, void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void SetOrientation(const VxVector &in dir, const VxVector &in up, const VxVector &in right = void, CK3dEntity@ ref = null, bool keepChildren = false)", asFUNCTIONPR([](T *self, const VxVector &dir, const VxVector &up, const VxVector *right, CK3dEntity *ref, bool keepChildren) { self->SetOrientation(&dir, &up, right, ref, keepChildren); }, (T *, const VxVector &, const VxVector &, const VxVector *, CK3dEntity *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void GetOrientation(VxVector &out dir, VxVector &out up, VxVector &out right = void, CK3dEntity@ ref = null)", asMETHODPR(T, GetOrientation, (VxVector *, VxVector *, VxVector *, CK3dEntity *), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void SetQuaternion(const VxQuaternion &in quat, CK3dEntity@ ref = null, bool keepChildren = false, bool keepScale = false)", asFUNCTIONPR([](T *self, const VxQuaternion &quat, CK3dEntity *ref, bool keepChildren, bool keepScale) { self->SetQuaternion(&quat, ref, keepChildren, keepScale); }, (T *, const VxQuaternion &, CK3dEntity *, bool, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void GetQuaternion(VxQuaternion &out quat, CK3dEntity@ ref = null)", asMETHODPR(T, GetQuaternion, (VxQuaternion*, CK3dEntity *), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void SetScale(float x, float y, float z, bool keepChildren = false, bool local = true)", asFUNCTIONPR([](T *self, float x, float y, float z, bool keepChildren, bool local) { self->SetScale3f(x, y, z, keepChildren, local); }, (T *, float, float, float, bool, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetScale(const VxVector &in scale, bool keepChildren = false, bool local = true)", asFUNCTIONPR([](T *self, const VxVector &scale, bool keepChildren, bool local) { self->SetScale(&scale, keepChildren, local); }, (T *, const VxVector &, bool, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void GetScale(VxVector &out scale, bool local = true)", asFUNCTIONPR([](T *self, VxVector &scale, bool local) { self->GetScale(&scale, local); }, (T *, VxVector&, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "bool ConstructWorldMatrix(const VxVector &in pos, const VxVector &in scale, const VxQuaternion &in quat)", asFUNCTIONPR([](T *self, const VxVector &pos, const VxVector &scale, const VxQuaternion &quat) -> bool { return self->ConstructWorldMatrix(&pos, &scale, &quat); }, (T *, const VxVector &, const VxVector &, const VxQuaternion &), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool ConstructWorldMatrixEx(const VxVector &in pos, const VxVector &in scale, const VxQuaternion &in quat, const VxQuaternion &in shear, float sign)", asFUNCTIONPR([](T *self, const VxVector &pos, const VxVector &scale, const VxQuaternion &quat, const VxQuaternion &shear, float sign) -> bool { return self->ConstructWorldMatrixEx(&pos, &scale, &quat, &shear, sign); }, (T *, const VxVector &, const VxVector &, const VxQuaternion &, const VxQuaternion &, float), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool ConstructLocalMatrix(const VxVector &in pos, const VxVector &in scale, const VxQuaternion &in quat)", asFUNCTIONPR([](T *self, const VxVector &pos, const VxVector &scale, const VxQuaternion &quat) -> bool { return self->ConstructLocalMatrix(&pos, &scale, &quat); }, (T *, const VxVector &, const VxVector &, const VxQuaternion &), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool ConstructLocalMatrixEx(const VxVector &in pos, const VxVector &in scale, const VxQuaternion &in quat, const VxQuaternion &in shear, float sign)", asFUNCTIONPR([](T *self, const VxVector &pos, const VxVector &scale, const VxQuaternion &quat, const VxQuaternion &shear, float sign) -> bool { return self->ConstructLocalMatrixEx(&pos, &scale, &quat, &shear, sign); }, (T *, const VxVector &, const VxVector &, const VxQuaternion &, const VxQuaternion &, float), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "bool Render(CKRenderContext@ dev, CKDWORD flags = CKRENDER_UPDATEEXTENTS)", asFUNCTIONPR([](T *self, CKRenderContext *dev, CKDWORD flags) -> bool { return self->Render(dev, flags); }, (T *, CKRenderContext *, CKDWORD), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "int RayIntersection(const VxVector &in pos1, const VxVector &in pos2, VxIntersectionDesc &out desc, CK3dEntity@ ref, CK_RAYINTERSECTION options = CKRAYINTERSECTION_DEFAULT)", asMETHODPR(T, RayIntersection, (const VxVector *, const VxVector *, VxIntersectionDesc *, CK3dEntity *, CK_RAYINTERSECTION), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void GetRenderExtents(VxRect &out rect) const", asMETHODPR(T, GetRenderExtents, (VxRect&) const, void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "const VxMatrix &GetLastFrameMatrix() const", asMETHODPR(T, GetLastFrameMatrix, () const, const VxMatrix &), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void SetLocalMatrix(const VxMatrix &in Mat, bool KeepChildren = false)", asFUNCTIONPR([](T *self, const VxMatrix &Mat, bool KeepChildren) { self->SetLocalMatrix(Mat, KeepChildren); }, (T *, const VxMatrix &, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "const VxMatrix &GetLocalMatrix() const", asMETHODPR(T, GetLocalMatrix, () const, const VxMatrix &), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetWorldMatrix(const VxMatrix &in Mat, bool KeepChildren = false)", asFUNCTIONPR([](T *self, const VxMatrix &Mat, bool KeepChildren) { self->SetWorldMatrix(Mat, KeepChildren); }, (T *, const VxMatrix &, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "const VxMatrix &GetWorldMatrix() const", asMETHODPR(T, GetWorldMatrix, () const, const VxMatrix &), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "const VxMatrix &GetInverseWorldMatrix() const", asMETHODPR(T, GetInverseWorldMatrix, () const, const VxMatrix &), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void Transform(VxVector &out dest, const VxVector &in src, CK3dEntity@ ref = null) const", asMETHODPR(T, Transform, (VxVector *, const VxVector *, CK3dEntity *) const, void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void InverseTransform(VxVector &out dest, const VxVector&in src, CK3dEntity@ ref = null) const", asMETHODPR(T, InverseTransform, (VxVector *, const VxVector *, CK3dEntity *) const, void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void TransformVector(VxVector &out dest, const VxVector&in src, CK3dEntity@ ref = null) const", asMETHODPR(T, TransformVector, (VxVector *, const VxVector *, CK3dEntity *) const, void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void InverseTransformVector(VxVector &out dest, const VxVector &in src, CK3dEntity@ ref = null) const", asMETHODPR(T, InverseTransformVector, (VxVector *, const VxVector *, CK3dEntity *) const, void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void TransformMany(VxVector &out dest, const VxVector &in src, int count, CK3dEntity@ ref = null) const", asMETHODPR(T, TransformMany, (VxVector *, const VxVector *, int, CK3dEntity *) const, void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void InverseTransformMany(VxVector &out dest, const VxVector &in src, int count, CK3dEntity@ ref = null) const", asMETHODPR(T, InverseTransformMany, (VxVector *, const VxVector *, int, CK3dEntity *) const, void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void ChangeReferential(CK3dEntity@ ref = null)", asMETHODPR(T, ChangeReferential, (CK3dEntity *), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "CKPlace@ GetReferencePlace() const", asMETHODPR(T, GetReferencePlace, () const, CKPlace *), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void AddObjectAnimation(CKObjectAnimation@ anim)", asMETHODPR(T, AddObjectAnimation, (CKObjectAnimation *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void RemoveObjectAnimation(CKObjectAnimation@ anim)", asMETHODPR(T, RemoveObjectAnimation, (CKObjectAnimation *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKObjectAnimation@ GetObjectAnimation(int index) const", asMETHODPR(T, GetObjectAnimation, (int) const, CKObjectAnimation *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "int GetObjectAnimationCount() const", asMETHODPR(T, GetObjectAnimationCount, () const, int), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "CKSkin@ CreateSkin()", asMETHODPR(T, CreateSkin, (), CKSkin *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool DestroySkin()", asFUNCTIONPR([](T *self) -> bool { return self->DestroySkin(); }, (T *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool UpdateSkin()", asFUNCTIONPR([](T *self) -> bool { return self->UpdateSkin(); }, (T *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "CKSkin@ GetSkin() const", asMETHODPR(T, GetSkin, () const, CKSkin *), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void UpdateBox(bool world = true)", asFUNCTIONPR([](T *self, bool world) { self->UpdateBox(world); }, (T *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "const VxBbox &GetBoundingBox(bool local = false)", asFUNCTIONPR([](T *self, bool local) -> const VxBbox& { return self->GetBoundingBox(local); }, (T *, bool), const VxBbox &), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool SetBoundingBox(const VxBbox &in bbox, bool local = false)", asFUNCTIONPR([](T *self, const VxBbox &bbox, bool local) -> bool { return self->SetBoundingBox(&bbox, local); }, (T *, const VxBbox &, bool), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "const VxBbox &GetHierarchicalBox(bool local = false)", asFUNCTIONPR([](T *self, bool local) -> const VxBbox& { return self->GetHierarchicalBox(local); }, (T *, bool), const VxBbox&), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool GetBaryCenter(VxVector&out Pos)", asFUNCTIONPR([](T *self, VxVector &Pos) -> bool { return self->GetBaryCenter(&Pos); }, (T *, VxVector &), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "float GetRadius()", asMETHODPR(T, GetRadius, (), float), asCALL_THISCALL); assert(r >= 0);

    if (strcmp(name, name) != 0) {
        RegisterCKObjectCast<T, CK3dEntity>(engine, name, name);
    }
}

void RegisterCK3dEntity(asIScriptEngine *engine) {
    assert(engine != nullptr);

    RegisterCK3dEntityMembers<CK3dEntity>(engine, "CK3dEntity");
}

template<typename T>
void RegisterCKCameraMembers(asIScriptEngine *engine, const char *name) {
    assert(engine != nullptr);

    int r = 0;

    RegisterCK3dEntityMembers<T>(engine, name);

    r = engine->RegisterObjectMethod(name, "float GetFrontPlane() const", asMETHODPR(T, GetFrontPlane, (), float), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetFrontPlane(float front)", asMETHODPR(T, SetFrontPlane, (float), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "float GetBackPlane() const", asMETHODPR(T, GetBackPlane, (), float), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetBackPlane(float back)", asMETHODPR(T, SetBackPlane, (float), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "float GetFov() const", asMETHODPR(T, GetFov, (), float), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetFov(float fov)", asMETHODPR(T, SetFov, (float), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "int GetProjectionType() const", asMETHODPR(T, GetProjectionType, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetProjectionType(int proj)", asMETHODPR(T, SetProjectionType, (int), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void SetOrthographicZoom(float zoom)", asMETHODPR(T, SetOrthographicZoom, (float), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "float GetOrthographicZoom() const", asMETHODPR(T, GetOrthographicZoom, (), float), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void SetAspectRatio(int width, int height)", asMETHODPR(T, SetAspectRatio, (int, int), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void GetAspectRatio(int &out width, int &out height)", asMETHODPR(T, GetAspectRatio, (int &, int &), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void ComputeProjectionMatrix(VxMatrix &out mat)", asMETHODPR(T, ComputeProjectionMatrix, (VxMatrix &), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void ResetRoll()", asMETHODPR(T, ResetRoll, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void Roll(float angle)", asMETHODPR(T, Roll, (float), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "CK3dEntity@ GetTarget() const", asMETHODPR(T, GetTarget, (), CK3dEntity *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetTarget(CK3dEntity@ target)", asMETHODPR(T, SetTarget, (CK3dEntity *), void), asCALL_THISCALL); assert(r >= 0);

    if (strcmp(name, name) != 0) {
        RegisterCKObjectCast<T, CKCamera>(engine, name, name);
    }
}

void RegisterCKCamera(asIScriptEngine *engine) {
    assert(engine != nullptr);

    RegisterCKCameraMembers<CKCamera>(engine, "CKCamera");
}

void RegisterCKTargetCamera(asIScriptEngine *engine) {
    assert(engine != nullptr);

    RegisterCKCameraMembers<CKTargetCamera>(engine, "CKTargetCamera");
}

void RegisterCKPlace(asIScriptEngine *engine) {
    assert(engine != nullptr);

    int r = 0;

    RegisterCK3dEntityMembers<CKPlace>(engine, "CKPlace");

    r = engine->RegisterObjectMethod("CKPlace", "CKCamera@ GetDefaultCamera() const", asMETHODPR(CKPlace, GetDefaultCamera, (), CKCamera *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPlace", "void SetDefaultCamera(CKCamera@ cam)", asMETHODPR(CKPlace, SetDefaultCamera, (CKCamera *), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKPlace", "void AddPortal(CKPlace@ place, CK3dEntity@ portal)", asMETHODPR(CKPlace, AddPortal, (CKPlace *, CK3dEntity *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPlace", "void RemovePortal(CKPlace@ place, CK3dEntity@ portal)", asMETHODPR(CKPlace, RemovePortal, (CKPlace *, CK3dEntity *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPlace", "int GetPortalCount() const", asMETHODPR(CKPlace, GetPortalCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKPlace", "CKPlace@ GetPortal(int i, CK3dEntity@ &out portal)", asMETHODPR(CKPlace, GetPortal, (int, CK3dEntity **), CKPlace *), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKPlace", "VxRect &ViewportClip()", asMETHODPR(CKPlace, ViewportClip, (), VxRect&), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKPlace", "bool ComputeBestFitBBox(CKPlace@ p2, VxMatrix &out bBoxMatrix)", asFUNCTIONPR([](CKPlace *self, CKPlace *p2, VxMatrix &bBoxMatrix) -> bool { return self->ComputeBestFitBBox(p2, bBoxMatrix); }, (CKPlace *, CKPlace *, VxMatrix &), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
}

void RegisterCKCurvePoint(asIScriptEngine *engine) {
    assert(engine != nullptr);

    int r = 0;

    RegisterCK3dEntityMembers<CKCurvePoint>(engine, "CKCurvePoint");

    r = engine->RegisterObjectMethod("CKCurvePoint", "CKCurve@ GetCurve() const", asMETHODPR(CKCurvePoint, GetCurve, (), CKCurve*), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKCurvePoint", "float GetBias() const", asMETHODPR(CKCurvePoint, GetBias, (), float), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCurvePoint", "void SetBias(float b)", asMETHODPR(CKCurvePoint, SetBias, (float), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKCurvePoint", "float GetTension() const", asMETHODPR(CKCurvePoint, GetTension, (), float), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCurvePoint", "void SetTension(float t)", asMETHODPR(CKCurvePoint, SetTension, (float), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKCurvePoint", "float GetContinuity() const", asMETHODPR(CKCurvePoint, GetContinuity, (), float), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCurvePoint", "void SetContinuity(float c)", asMETHODPR(CKCurvePoint, SetContinuity, (float), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKCurvePoint", "bool IsLinear() const", asFUNCTIONPR([](CKCurvePoint *self) -> bool { return self->IsLinear(); }, (CKCurvePoint*), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCurvePoint", "void SetLinear(bool linear = false)", asFUNCTIONPR([](CKCurvePoint *self, bool linear) { self->SetLinear(linear); }, (CKCurvePoint*, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKCurvePoint", "void UseTCB(bool use = true)", asFUNCTIONPR([](CKCurvePoint *self, bool use) { self->UseTCB(use); }, (CKCurvePoint*, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCurvePoint", "bool IsTCB() const", asFUNCTIONPR([](CKCurvePoint *self) -> bool { return self->IsTCB(); }, (CKCurvePoint*), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKCurvePoint", "float GetLength() const", asMETHODPR(CKCurvePoint, GetLength, (), float), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKCurvePoint", "void GetTangents(VxVector &out input, VxVector &out output) const", asMETHODPR(CKCurvePoint, GetTangents, (VxVector *, VxVector *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCurvePoint", "void SetTangents(const VxVector &in input, const VxVector &in output)", asMETHODPR(CKCurvePoint, SetTangents, (VxVector *, VxVector *), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKCurvePoint", "void NotifyUpdate()", asMETHODPR(CKCurvePoint, NotifyUpdate, (), void), asCALL_THISCALL); assert(r >= 0);
}

void RegisterCKSprite3D(asIScriptEngine *engine) {
    assert(engine != nullptr);

    int r = 0;

    RegisterCK3dEntityMembers<CKSprite3D>(engine, "CKSprite3D");

    r = engine->RegisterObjectMethod("CKSprite3D", "void SetMaterial(CKMaterial@ mat)", asMETHODPR(CKSprite3D, SetMaterial, (CKMaterial *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSprite3D", "CKMaterial@ GetMaterial() const", asMETHODPR(CKSprite3D, GetMaterial, (), CKMaterial *), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKSprite3D", "void SetSize(const Vx2DVector &in vect)", asMETHODPR(CKSprite3D, SetSize, (Vx2DVector&), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSprite3D", "void GetSize(Vx2DVector &out vect) const", asMETHODPR(CKSprite3D, GetSize, (Vx2DVector&), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKSprite3D", "void SetOffset(const Vx2DVector &in vect)", asMETHODPR(CKSprite3D, SetOffset, (Vx2DVector&), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSprite3D", "void GetOffset(Vx2DVector &out vect) const", asMETHODPR(CKSprite3D, GetOffset, (Vx2DVector&), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKSprite3D", "void SetUVMapping(const VxRect &in rect)", asMETHODPR(CKSprite3D, SetUVMapping, (VxRect&), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSprite3D", "void GetUVMapping(VxRect &out rect) const", asMETHODPR(CKSprite3D, GetUVMapping, (VxRect&), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKSprite3D", "void SetMode(VXSPRITE3D_TYPE mode)", asMETHODPR(CKSprite3D, SetMode, (VXSPRITE3D_TYPE), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKSprite3D", "VXSPRITE3D_TYPE GetMode() const", asMETHODPR(CKSprite3D, GetMode, (), VXSPRITE3D_TYPE), asCALL_THISCALL); assert(r >= 0);
}

template<typename T>
void RegisterCKLightMembers(asIScriptEngine *engine, const char *name) {
    assert(engine != nullptr);

    int r = 0;

    RegisterCK3dEntityMembers<T>(engine, name);

    r = engine->RegisterObjectMethod(name, "void SetColor(const VxColor &in c)", asMETHODPR(T, SetColor, (const VxColor&), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "const VxColor &GetColor() const", asMETHODPR(T, GetColor, (), const VxColor&), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void SetConstantAttenuation(float value)", asMETHODPR(T, SetConstantAttenuation, (float), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetLinearAttenuation(float value)", asMETHODPR(T, SetLinearAttenuation, (float), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetQuadraticAttenuation(float value)", asMETHODPR(T, SetQuadraticAttenuation, (float), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "float GetConstantAttenuation() const", asMETHODPR(T, GetConstantAttenuation, (), float), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "float GetLinearAttenuation() const", asMETHODPR(T, GetLinearAttenuation, (), float), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "float GetQuadraticAttenuation() const", asMETHODPR(T, GetQuadraticAttenuation, (), float), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "VXLIGHT_TYPE GetType() const", asMETHODPR(T, GetType, (), VXLIGHT_TYPE), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetType(VXLIGHT_TYPE type)", asMETHODPR(T, SetType, (VXLIGHT_TYPE), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "float GetRange() const", asMETHODPR(T, GetRange, (), float), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetRange(float value)", asMETHODPR(T, SetRange, (float), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "float GetHotSpot() const", asMETHODPR(T, GetHotSpot, (), float), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "float GetFallOff() const", asMETHODPR(T, GetFallOff, (), float), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetHotSpot(float value)", asMETHODPR(T, SetHotSpot, (float), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetFallOff(float value)", asMETHODPR(T, SetFallOff, (float), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "float GetFallOffShape() const", asMETHODPR(T, GetFallOffShape, (), float), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetFallOffShape(float value)", asMETHODPR(T, SetFallOffShape, (float), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "void Active(bool active)", asFUNCTIONPR([](T *self, bool active) { self->Active(active); }, (T *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool GetActivity() const", asFUNCTIONPR([](T *self) -> bool { return self->GetActivity(); }, (T *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetSpecularFlag(bool specular)", asFUNCTIONPR([](T *self, bool specular) { self->SetSpecularFlag(specular); }, (T *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "bool GetSpecularFlag() const", asFUNCTIONPR([](T *self) -> bool { return self->GetSpecularFlag(); }, (T *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "CK3dEntity@ GetTarget() const", asMETHODPR(T, GetTarget, (), CK3dEntity *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetTarget(CK3dEntity@ target)", asMETHODPR(T, SetTarget, (CK3dEntity *), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(name, "float GetLightPower() const", asMETHODPR(T, GetLightPower, (), float), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(name, "void SetLightPower(float power = 1.0)", asMETHODPR(T, SetLightPower, (float), void), asCALL_THISCALL); assert(r >= 0);

    if (strcmp(name, name) != 0) {
        RegisterCKObjectCast<T, CKLight>(engine, name, name);
    }
}

void RegisterCKLight(asIScriptEngine *engine) {
    assert(engine != nullptr);

    RegisterCKLightMembers<CKLight>(engine, "CKLight");
}

void RegisterCKTargetLight(asIScriptEngine *engine) {
    assert(engine != nullptr);

    RegisterCKLightMembers<CKTargetLight>(engine, "CKTargetLight");
}

void RegisterCKCharacter(asIScriptEngine *engine) {
    assert(engine != nullptr);

    int r = 0;

    RegisterCK3dEntityMembers<CKCharacter>(engine, "CKCharacter");

    r = engine->RegisterObjectMethod("CKCharacter", "CKERROR AddBodyPart(CKBodyPart@ part)", asMETHODPR(CKCharacter, AddBodyPart, (CKBodyPart *), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCharacter", "CKERROR RemoveBodyPart(CKBodyPart@ part)", asMETHODPR(CKCharacter, RemoveBodyPart, (CKBodyPart *), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCharacter", "CKBodyPart@ GetRootBodyPart()", asMETHODPR(CKCharacter, GetRootBodyPart, (), CKBodyPart*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCharacter", "CKERROR SetRootBodyPart(CKBodyPart@ part)", asMETHODPR(CKCharacter, SetRootBodyPart, (CKBodyPart *), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCharacter", "CKBodyPart@ GetBodyPart(int index)", asMETHODPR(CKCharacter, GetBodyPart, (int), CKBodyPart *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCharacter", "int GetBodyPartCount()", asMETHODPR(CKCharacter, GetBodyPartCount, (), int), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKCharacter", "CKERROR AddAnimation(CKAnimation@ anim)", asMETHODPR(CKCharacter, AddAnimation, (CKAnimation *), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCharacter", "CKERROR RemoveAnimation(CKAnimation@ anim)", asMETHODPR(CKCharacter, RemoveAnimation, (CKAnimation *), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCharacter", "CKAnimation@ GetAnimation(int index)", asMETHODPR(CKCharacter, GetAnimation, (int), CKAnimation *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCharacter", "int GetAnimationCount()", asMETHODPR(CKCharacter, GetAnimationCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCharacter", "CKAnimation@ GetWarper()", asMETHODPR(CKCharacter, GetWarper, (), CKAnimation *), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKCharacter", "CKAnimation@ GetActiveAnimation()", asMETHODPR(CKCharacter, GetActiveAnimation, (), CKAnimation *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCharacter", "CKAnimation@ GetNextActiveAnimation()", asMETHODPR(CKCharacter, GetNextActiveAnimation, (), CKAnimation *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCharacter", "CKERROR SetActiveAnimation(CKAnimation@ anim)", asMETHODPR(CKCharacter, SetActiveAnimation, (CKAnimation *), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCharacter", "CKERROR SetNextActiveAnimation(CKAnimation@ anim, CKDWORD transitionMode, float warpLength = 0.0)", asMETHODPR(CKCharacter, SetNextActiveAnimation, (CKAnimation *, CKDWORD, float), CKERROR), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKCharacter", "void ProcessAnimation(float delta = 1.0)", asMETHODPR(CKCharacter, ProcessAnimation, (float), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCharacter", "void SetAutomaticProcess(bool process = true)", asFUNCTIONPR([](CKCharacter *self, bool process) { self->SetAutomaticProcess(process); }, (CKCharacter *, bool), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCharacter", "bool IsAutomaticProcess()", asFUNCTIONPR([](CKCharacter *self) -> bool { return self->IsAutomaticProcess(); }, (CKCharacter *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCharacter", "void GetEstimatedVelocity(float delta, VxVector &out velocity)", asMETHODPR(CKCharacter, GetEstimatedVelocity, (float, VxVector *), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKCharacter", "CKERROR PlaySecondaryAnimation(CKAnimation@ anim, float startingFrame = 0.0, CK_SECONDARYANIMATION_FLAGS playFlags = CKSECONDARYANIMATION_ONESHOT, float warpLength = 5.0, int loopCount = 0)", asMETHODPR(CKCharacter, PlaySecondaryAnimation, (CKAnimation *, float, CK_SECONDARYANIMATION_FLAGS, float, int), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCharacter", "CKERROR StopSecondaryAnimation(CKAnimation@ anim, bool warp = false, float warpLength = 5.0)", asFUNCTIONPR([](CKCharacter *self, CKAnimation *anim, bool warp, float warpLength) -> CKERROR { return self->StopSecondaryAnimation(anim, warp, warpLength); }, (CKCharacter *, CKAnimation *, bool, float), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCharacter", "int GetSecondaryAnimationsCount()", asMETHODPR(CKCharacter, GetSecondaryAnimationsCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCharacter", "CKAnimation@ GetSecondaryAnimation(int index)", asMETHODPR(CKCharacter, GetSecondaryAnimation, (int), CKAnimation *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCharacter", "void FlushSecondaryAnimations()", asMETHODPR(CKCharacter, FlushSecondaryAnimations, (), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKCharacter", "void AlignCharacterWithRootPosition()", asMETHODPR(CKCharacter, AlignCharacterWithRootPosition, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCharacter", "CK3dEntity@ GetFloorReferenceObject()", asMETHODPR(CKCharacter, GetFloorReferenceObject, (), CK3dEntity *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCharacter", "void SetFloorReferenceObject(CK3dEntity@ floorRef)", asMETHODPR(CKCharacter, SetFloorReferenceObject, (CK3dEntity *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCharacter", "void SetAnimationLevelOfDetail(float lod)", asMETHODPR(CKCharacter, SetAnimationLevelOfDetail, (float), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCharacter", "float GetAnimationLevelOfDetail()", asMETHODPR(CKCharacter, GetAnimationLevelOfDetail, (), float), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCharacter", "void GetWarperParameters(CKDWORD &out transitionMode, CKAnimation@ &out animSrc, float &out frameSrc, CKAnimation@ &out animDest, float &out frameDest)", asMETHODPR(CKCharacter, GetWarperParameters, (CKDWORD *, CKAnimation **, float *, CKAnimation **, float *), void), asCALL_THISCALL); assert(r >= 0);
}

template<typename T>
void RegisterCK3dObjectMembers(asIScriptEngine *engine, const char *name) {
    assert(engine != nullptr);

    RegisterCK3dEntityMembers<T>(engine, name);

    if (strcmp(name, name) != 0) {
        RegisterCKObjectCast<T, CK3dObject>(engine, name, name);
    }
}

void RegisterCK3dObject(asIScriptEngine *engine) {
    assert(engine != nullptr);

    RegisterCK3dObjectMembers<CK3dObject>(engine, "CK3dObject");
}

void RegisterCKBodyPart(asIScriptEngine *engine) {
    assert(engine != nullptr);

    int r = 0;

    RegisterCK3dObjectMembers<CKBodyPart>(engine, "CKBodyPart");

    r = engine->RegisterObjectMethod("CKBodyPart", "CKCharacter@ GetCharacter() const", asMETHODPR(CKBodyPart, GetCharacter, () const, CKCharacter *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBodyPart", "void SetExclusiveAnimation(const CKAnimation@ anim)", asMETHODPR(CKBodyPart, SetExclusiveAnimation, (const CKAnimation *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBodyPart", "CKAnimation@ GetExclusiveAnimation() const", asMETHODPR(CKBodyPart, GetExclusiveAnimation, () const, CKAnimation *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBodyPart", "void GetRotationJoint(CKIkJoint &out rotJoint) const", asMETHODPR(CKBodyPart, GetRotationJoint, (CKIkJoint *) const, void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBodyPart", "void SetRotationJoint(const CKIkJoint &in rotJoint)", asMETHODPR(CKBodyPart, SetRotationJoint, (const CKIkJoint *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKBodyPart", "CKERROR FitToJoint()", asMETHODPR(CKBodyPart, FitToJoint, (), CKERROR), asCALL_THISCALL); assert(r >= 0);
}

void RegisterCKCurve(asIScriptEngine *engine) {
    assert(engine != nullptr);

    int r = 0;

    RegisterCK3dEntityMembers<CKCurve>(engine, "CKCurve");

    r = engine->RegisterObjectMethod("CKCurve", "float GetLength() const", asMETHODPR(CKCurve, GetLength, (), float), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKCurve", "void Open()", asMETHODPR(CKCurve, Open, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCurve", "void Close()", asMETHODPR(CKCurve, Close, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCurve", "bool IsOpen() const", asFUNCTIONPR([](CKCurve *self) -> bool { return self->IsOpen(); }, (CKCurve *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKCurve", "CKERROR GetPos(float step, VxVector &out pos, VxVector &out dir = void) const", asMETHODPR(CKCurve, GetPos, (float, VxVector *, VxVector *), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCurve", "CKERROR GetLocalPos(float step, VxVector &out pos, VxVector &out dir = void) const", asMETHODPR(CKCurve, GetLocalPos, (float, VxVector *, VxVector *), CKERROR), asCALL_THISCALL); assert(r >= 0);

    // SDK only has CKCurvePoint* overload (no int index overload)
    r = engine->RegisterObjectMethod("CKCurve", "CKERROR GetTangents(CKCurvePoint@ pt, VxVector &out input, VxVector &out output) const", asMETHODPR(CKCurve, GetTangents, (CKCurvePoint *, VxVector *, VxVector *), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCurve", "CKERROR SetTangents(CKCurvePoint@ pt, VxVector &in input, VxVector &in output)", asMETHODPR(CKCurve, SetTangents, (CKCurvePoint *, VxVector *, VxVector *), CKERROR), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKCurve", "void SetFittingCoeff(float fit)", asMETHODPR(CKCurve, SetFittingCoeff, (float), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCurve", "float GetFittingCoeff() const", asMETHODPR(CKCurve, GetFittingCoeff, (), float), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKCurve", "CKERROR RemoveControlPoint(CKCurvePoint@ pt, bool removeAll = false)", asFUNCTIONPR([](CKCurve *self, CKCurvePoint *pt, bool removeAll) -> CKERROR { return self->RemoveControlPoint(pt, removeAll); }, (CKCurve *, CKCurvePoint *, bool), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCurve", "CKERROR InsertControlPoint(CKCurvePoint@ prev, CKCurvePoint@ pt)", asMETHODPR(CKCurve, InsertControlPoint, (CKCurvePoint *, CKCurvePoint *), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCurve", "CKERROR AddControlPoint(CKCurvePoint@ pt)", asMETHODPR(CKCurve, AddControlPoint, (CKCurvePoint *), CKERROR), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKCurve", "int GetControlPointCount() const", asMETHODPR(CKCurve, GetControlPointCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCurve", "CKCurvePoint@ GetControlPoint(int pos) const", asMETHODPR(CKCurve, GetControlPoint, (int), CKCurvePoint *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCurve", "CKERROR RemoveAllControlPoints()", asMETHODPR(CKCurve, RemoveAllControlPoints, (), CKERROR), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKCurve", "CKERROR SetStepCount(int steps)", asMETHODPR(CKCurve, SetStepCount, (int), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCurve", "int GetStepCount() const", asMETHODPR(CKCurve, GetStepCount, (), int), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKCurve", "CKERROR CreateLineMesh()", asMETHODPR(CKCurve, CreateLineMesh, (), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCurve", "CKERROR UpdateMesh()", asMETHODPR(CKCurve, UpdateMesh, (), CKERROR), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKCurve", "VxColor GetColor() const", asMETHODPR(CKCurve, GetColor, (), VxColor), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKCurve", "void SetColor(const VxColor &in color)", asMETHODPR(CKCurve, SetColor, (const VxColor &), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKCurve", "void Update()", asMETHODPR(CKCurve, Update, (), void), asCALL_THISCALL); assert(r >= 0);
}

void RegisterCKGrid(asIScriptEngine *engine) {
    assert(engine != nullptr);

    int r = 0;

    RegisterCK3dEntityMembers<CKGrid>(engine, "CKGrid");

    r = engine->RegisterObjectMethod("CKGrid", "void ConstructMeshTexture(float opacity = 0.5)", asMETHODPR(CKGrid, ConstructMeshTexture, (float), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKGrid", "void DestroyMeshTexture()", asMETHODPR(CKGrid, DestroyMeshTexture, (), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKGrid", "bool IsActive() const", asFUNCTIONPR([](CKGrid *self) -> bool { return self->IsActive(); }, (CKGrid *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKGrid", "void SetHeightValidity(float heightValidity)", asMETHODPR(CKGrid, SetHeightValidity, (float), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKGrid", "float GetHeightValidity() const", asMETHODPR(CKGrid, GetHeightValidity, (), float), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKGrid", "int GetWidth() const", asMETHODPR(CKGrid, GetWidth, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKGrid", "int GetLength() const", asMETHODPR(CKGrid, GetLength, (), int), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKGrid", "void SetDimensions(int width, int length, float sizeX, float sizeY)", asMETHODPR(CKGrid, SetDimensions, (int, int, float, float), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKGrid", "float Get2dCoordsFrom3dPos(const VxVector &in pos3d, int &out x, int &out y) const", asMETHODPR(CKGrid, Get2dCoordsFrom3dPos, (const VxVector *, int *, int *), float), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKGrid", "void Get3dPosFrom2dCoords(VxVector &out pos3d, int x, int z) const", asMETHODPR(CKGrid, Get3dPosFrom2dCoords, (VxVector *, int, int), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKGrid", "CKERROR AddClassification(int classification)", asMETHODPR(CKGrid, AddClassification, (int), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKGrid", "CKERROR AddClassification(const string &in name)", asFUNCTIONPR([](CKGrid *self, const std::string &name) { return self->AddClassificationByName(const_cast<char *>(name.c_str())); }, (CKGrid *, const std::string &), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKGrid", "CKERROR RemoveClassification(int classification)", asMETHODPR(CKGrid, RemoveClassification, (int), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKGrid", "CKERROR RemoveClassification(const string &in name)", asFUNCTIONPR([](CKGrid *self, const std::string &name) { return self->RemoveClassificationByName(const_cast<char *>(name.c_str())); }, (CKGrid *, const std::string &), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKGrid", "bool HasCompatibleClass(CK3dEntity@ entity) const", asFUNCTIONPR([](CKGrid *self, CK3dEntity *entity) -> bool { return self->HasCompatibleClass(entity); }, (CKGrid *, CK3dEntity *), bool), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKGrid", "void SetGridPriority(int priority)", asMETHODPR(CKGrid, SetGridPriority, (int), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKGrid", "int GetGridPriority() const", asMETHODPR(CKGrid, GetGridPriority, (), int), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKGrid", "void SetOrientationMode(CK_GRIDORIENTATION orientation)", asMETHODPR(CKGrid, SetOrientationMode, (CK_GRIDORIENTATION), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKGrid", "CK_GRIDORIENTATION GetOrientationMode() const", asMETHODPR(CKGrid, GetOrientationMode, (), CK_GRIDORIENTATION), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("CKGrid", "CKLayer@ AddLayer(int type, int format = CKGRID_LAYER_FORMAT_NORMAL)", asMETHODPR(CKGrid, AddLayer, (int, int), CKLayer*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKGrid", "CKLayer@ AddLayer(const string &in typeName = \"\", int format = CKGRID_LAYER_FORMAT_NORMAL)", asFUNCTIONPR([](CKGrid *self, const std::string &typeName, int format) { return self->AddLayerByName(const_cast<char *>(typeName.c_str()), format); }, (CKGrid *, const std::string &, int), CKLayer *), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKGrid", "CKLayer@ GetLayer(int type) const", asMETHODPR(CKGrid, GetLayer, (int), CKLayer*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKGrid", "CKLayer@ GetLayer(const string &in typeName) const", asFUNCTIONPR([](CKGrid *self, const std::string &typeName) { return self->GetLayerByName(const_cast<char *>(typeName.c_str())); }, (CKGrid *, const std::string &), CKLayer *), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKGrid", "int GetLayerCount() const", asMETHODPR(CKGrid, GetLayerCount, (), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKGrid", "CKLayer@ GetLayerByIndex(int index) const", asMETHODPR(CKGrid, GetLayerByIndex, (int), CKLayer*), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKGrid", "CKERROR RemoveLayer(int type)", asMETHODPR(CKGrid, RemoveLayer, (int), CKERROR), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKGrid", "CKERROR RemoveLayer(const string &in typeName)", asFUNCTIONPR([](CKGrid *self, const std::string &typeName) { return self->RemoveLayerByName(const_cast<char *>(typeName.c_str())); }, (CKGrid *, const std::string &), CKERROR), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("CKGrid", "CKERROR RemoveAllLayers()", asMETHODPR(CKGrid, RemoveAllLayers, (), CKERROR), asCALL_THISCALL); assert(r >= 0);
}
