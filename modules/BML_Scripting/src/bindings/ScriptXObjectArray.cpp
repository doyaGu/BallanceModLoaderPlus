#include "ScriptXObjectArray.h"

#include "XObjectArray.h"

#include "ScriptXArray.h"

void RegisterXSObjectPointerArray(asIScriptEngine *engine) {
    int r = 0;

    RegisterXSArray<XSObjectPointerArray, CKObject *>(engine, "XSObjectPointerArray", "CKObject@");

    r = engine->RegisterObjectMethod("XSObjectPointerArray", "bool AddIfNotHere(CKObject@ obj)", asMETHODPR(XSObjectPointerArray, AddIfNotHere, (CKObject *), CKBOOL), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XSObjectPointerArray", "CKObject@ GetObject(uint i) const", asMETHODPR(XSObjectPointerArray, GetObject, (unsigned int) const, CKObject *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XSObjectPointerArray", "int RemoveObject(CKObject@ obj)", asMETHODPR(XSObjectPointerArray, RemoveObject, (CKObject *), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XSObjectPointerArray", "bool FindObject(CKObject@ obj) const", asMETHODPR(XSObjectPointerArray, FindObject, (CKObject *) const, CKBOOL), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XSObjectPointerArray", "bool Check()", asMETHODPR(XSObjectPointerArray, Check, (), CKBOOL), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XSObjectPointerArray", "void Load(CKContext@ context, CKStateChunk@ chunk)", asMETHODPR(XSObjectPointerArray, Load, (CKContext *, CKStateChunk *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XSObjectPointerArray", "void Save(CKStateChunk@ chunk) const", asMETHODPR(XSObjectPointerArray, Save, (CKStateChunk *) const, void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XSObjectPointerArray", "void Prepare(CKDependenciesContext &out context) const", asMETHODPR(XSObjectPointerArray, Prepare, (CKDependenciesContext &) const, void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XSObjectPointerArray", "void Remap(CKDependenciesContext &out context)", asMETHODPR(XSObjectPointerArray, Remap, (CKDependenciesContext &), void), asCALL_THISCALL); assert(r >= 0);
}

void RegisterXSObjectArray(asIScriptEngine *engine) {
    int r = 0;

    RegisterXSArray<XSObjectArray, CK_ID>(engine, "XSObjectArray", "CK_ID");

    r = engine->RegisterObjectMethod("XSObjectArray", "void ConvertToObjects(CKContext@ context, XSObjectPointerArray &out objArray) const", asMETHODPR(XSObjectArray, ConvertToObjects, (CKContext *Context, XSArray<CKObject *> &) const, void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XSObjectArray", "void ConvertFromObjects(const XSObjectPointerArray &in objArray)", asMETHODPR(XSObjectArray, ConvertFromObjects, (const XSArray<CKObject *> &), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XSObjectArray", "bool Check(CKContext@ context)", asMETHODPR(XSObjectArray, Check, (CKContext *), CKBOOL), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XSObjectArray", "bool AddIfNotHere(CK_ID id)", asMETHODPR(XSObjectArray, AddIfNotHere, (CK_ID), CKBOOL), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XSObjectArray", "bool AddIfNotHere(CKObject@ obj)", asMETHODPR(XSObjectArray, AddIfNotHere, (CKObject *), CKBOOL), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XSObjectArray", "CKObject@ GetObject(CKContext@ context, uint i) const", asMETHODPR(XSObjectArray, GetObject, (CKContext *, unsigned int) const, CKObject *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XSObjectArray", "CK_ID GetObjectID(uint i) const", asMETHODPR(XSObjectArray, GetObjectID, (unsigned int) const, CK_ID), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XSObjectArray", "int RemoveObject(CKObject@ obj)", asMETHODPR(XSObjectArray, RemoveObject, (CKObject *), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XSObjectArray", "bool FindObject(CKObject@ obj) const", asMETHODPR(XSObjectArray, FindObject, (CKObject *) const, CKBOOL), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XSObjectArray", "bool FindID(CK_ID id) const", asMETHODPR(XSObjectArray, FindID, (CK_ID) const, CKBOOL), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XSObjectArray", "void Load(CKStateChunk@ chunk)", asMETHODPR(XSObjectArray, Load, (CKStateChunk *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XSObjectArray", "void Save(CKStateChunk@ chunk, CKContext@ context) const", asMETHODPR(XSObjectArray, Save, (CKStateChunk *, CKContext *) const, void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XSObjectArray", "void Prepare(CKDependenciesContext &out context) const", asMETHODPR(XSObjectArray, Prepare, (CKDependenciesContext &) const, void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XSObjectArray", "void Remap(CKDependenciesContext &out context)", asMETHODPR(XSObjectArray, Remap, (CKDependenciesContext &), void), asCALL_THISCALL); assert(r >= 0);
}

void RegisterXObjectPointerArray(asIScriptEngine *engine) {
    int r = 0;

    RegisterXArray<XObjectPointerArray, CKObject *>(engine, "XObjectPointerArray", "CKObject@");

    r = engine->RegisterObjectMethod("XObjectPointerArray", "bool AddIfNotHere(CKObject@ obj)", asMETHODPR(XObjectPointerArray, AddIfNotHere, (CKObject *), CKBOOL), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XObjectPointerArray", "CKObject@ GetObject(uint i) const", asMETHODPR(XObjectPointerArray, GetObject, (unsigned int) const, CKObject *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XObjectPointerArray", "int RemoveObject(CKObject@ obj)", asMETHODPR(XObjectPointerArray, RemoveObject, (CKObject *), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XObjectPointerArray", "bool FindObject(CKObject@ obj) const", asMETHODPR(XObjectPointerArray, FindObject, (CKObject *) const, CKBOOL), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XObjectPointerArray", "bool Check()", asMETHODPR(XObjectPointerArray, Check, (), CKBOOL), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XObjectPointerArray", "void Load(CKContext@ context, CKStateChunk@ chunk)", asMETHODPR(XObjectPointerArray, Load, (CKContext *, CKStateChunk *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XObjectPointerArray", "void Save(CKStateChunk@ chunk) const", asMETHODPR(XObjectPointerArray, Save, (CKStateChunk *) const, void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XObjectPointerArray", "void Prepare(CKDependenciesContext &out context) const", asMETHODPR(XObjectPointerArray, Prepare, (CKDependenciesContext &) const, void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XObjectPointerArray", "void Remap(CKDependenciesContext &out context)", asMETHODPR(XObjectPointerArray, Remap, (CKDependenciesContext &), void), asCALL_THISCALL); assert(r >= 0);
}

void RegisterXObjectArray(asIScriptEngine *engine) {
    int r = 0;

    RegisterXArray<XObjectArray, CK_ID>(engine, "XObjectArray", "CK_ID");

    r = engine->RegisterObjectMethod("XObjectArray", "void ConvertToObjects(CKContext@ context, XObjectPointerArray &out objArray) const", asMETHODPR(XObjectArray, ConvertToObjects, (CKContext *Context, XObjectPointerArray &) const, void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XObjectArray", "void ConvertFromObjects(const XObjectPointerArray &in objArray)", asMETHODPR(XObjectArray, ConvertFromObjects, (const XObjectPointerArray &), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XObjectArray", "bool Check(CKContext@ obj)", asMETHODPR(XObjectArray, Check, (CKContext *), CKBOOL), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XObjectArray", "bool AddIfNotHere(CK_ID id)", asMETHODPR(XObjectArray, AddIfNotHere, (CK_ID), CKBOOL), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XObjectArray", "bool AddIfNotHere(CKObject@ obj)", asMETHODPR(XObjectArray, AddIfNotHere, (CKObject *), CKBOOL), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XObjectArray", "CKObject@ GetObject(CKContext@ context, uint i) const", asMETHODPR(XObjectArray, GetObject, (CKContext *, unsigned int) const, CKObject *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XObjectArray", "CK_ID GetObjectID(uint i) const", asMETHODPR(XObjectArray, GetObjectID, (unsigned int) const, CK_ID), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XObjectArray", "int RemoveObject(CKObject@ obj)", asMETHODPR(XObjectArray, RemoveObject, (CKObject *), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XObjectArray", "bool FindObject(CKObject@ obj) const", asMETHODPR(XObjectArray, FindObject, (CKObject *) const, CKBOOL), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XObjectArray", "bool FindID(CK_ID id) const", asMETHODPR(XObjectArray, FindID, (CK_ID) const, CKBOOL), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XObjectArray", "void Load(CKStateChunk@ chunk)", asMETHODPR(XObjectArray, Load, (CKStateChunk *), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XObjectArray", "void Save(CKStateChunk@ chunk, CKContext@ context) const", asMETHODPR(XObjectArray, Save, (CKStateChunk *, CKContext *) const, void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XObjectArray", "void Prepare(CKDependenciesContext &out context) const", asMETHODPR(XObjectArray, Prepare, (CKDependenciesContext &) const, void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XObjectArray", "void Remap(CKDependenciesContext &out context)", asMETHODPR(XObjectArray, Remap, (CKDependenciesContext &), void), asCALL_THISCALL); assert(r >= 0);
}
