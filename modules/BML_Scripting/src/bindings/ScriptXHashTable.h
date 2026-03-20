#ifndef CK_SCRIPTXHASHTABLE_H
#define CK_SCRIPTXHASHTABLE_H

#include <angelscript.h>

#include "XString.h"
#include "XHashTable.h"
#include "XNHashTable.h"
#include "XSHashTable.h"

template<typename C, typename T, typename K>
void RegisterXHashTableEntry(asIScriptEngine *engine, const char *className, const char *keyType, const char *elementType) {
    int r = 0;
    XString decl;
    XString name = decl.Format("%sEntry", className);

    r = engine->RegisterObjectType(name.CStr(), sizeof(C), asOBJ_VALUE | asGetTypeTraits<C>()); assert(r >= 0 || r == asALREADY_REGISTERED);
    if (r == asALREADY_REGISTERED)
        return;

    decl.Format("void f(const %s &in, const %s &in)", keyType, elementType);
    r = engine->RegisterObjectBehaviour(name.CStr(), asBEHAVE_CONSTRUCT, decl.CStr(), asFUNCTIONPR([](const K &k, const T &v, C *self) { new(self) C(k, v); }, (const K &, const T &, C *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    decl.Format("void f(const %s &in)", name.CStr());
    r = engine->RegisterObjectBehaviour(name.CStr(), asBEHAVE_CONSTRUCT, decl.CStr(), asFUNCTIONPR([](const C &c, C *self) { new(self) C(c); }, (const C &, C *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour(name.CStr(), asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](C *self) { self->~C(); }, (C *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

#if CKVERSION == 0x13022002
    decl.Format("%s m_Key", keyType);
    r = engine->RegisterObjectProperty(name.CStr(), decl.CStr(), asOFFSET(C, m_Key)); assert(r >= 0);

    decl.Format("%s m_Data", elementType);
    r = engine->RegisterObjectProperty(name.CStr(), decl.CStr(), asOFFSET(C, m_Data)); assert(r >= 0);

    decl.Format("%s &m_Next", name.CStr());
    r = engine->RegisterObjectProperty(name.CStr(), decl.CStr(), asOFFSET(C, m_Next)); assert(r >= 0);
#else
    decl.Format("%s key", keyType);
    r = engine->RegisterObjectProperty(name.CStr(), decl.CStr(), asOFFSET(C, key)); assert(r >= 0);

    decl.Format("%s data", elementType);
    r = engine->RegisterObjectProperty(name.CStr(), decl.CStr(), asOFFSET(C, data)); assert(r >= 0);

    decl.Format("%s &next", name.CStr());
    r = engine->RegisterObjectProperty(name.CStr(), decl.CStr(), asOFFSET(C, next)); assert(r >= 0);
#endif

    decl.Format("%s &opAssign(const %s &in)", name.CStr(), name.CStr());
    r = engine->RegisterObjectMethod(name.CStr(), decl.CStr(), asMETHODPR(C, operator=, (const C &), C &), asCALL_THISCALL); assert(r >= 0);
}

template<typename C, typename T, typename K>
void RegisterXNHashTableEntry(asIScriptEngine *engine, const char *className, const char *keyType, const char *elementType) {
    int r = 0;
    XString decl;
    XString name = decl.Format("%sEntry", className);

    r = engine->RegisterObjectType(name.CStr(), sizeof(C), asOBJ_VALUE | asGetTypeTraits<C>()); assert(r >= 0 || r == asALREADY_REGISTERED);
    if (r == asALREADY_REGISTERED)
        return;

    decl.Format("void f(const %s &in, const %s &in)", keyType, elementType);
    r = engine->RegisterObjectBehaviour(name.CStr(), asBEHAVE_CONSTRUCT, decl.CStr(), asFUNCTIONPR([](const K &k, const T &v, C *self) { new(self) C(k, v); }, (const K &, const T &, C *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    decl.Format("void f(const %s &in)", name.CStr());
    r = engine->RegisterObjectBehaviour(name.CStr(), asBEHAVE_CONSTRUCT, decl.CStr(), asFUNCTIONPR([](const C &c, C *self) { new(self) C(c); }, (const C &, C *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour(name.CStr(), asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](C *self) { self->~C(); }, (C *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    decl.Format("%s m_Key", keyType);
    r = engine->RegisterObjectProperty(name.CStr(), decl.CStr(), asOFFSET(C, m_Key)); assert(r >= 0);

    decl.Format("%s m_Data", elementType);
    r = engine->RegisterObjectProperty(name.CStr(), decl.CStr(), asOFFSET(C, m_Data)); assert(r >= 0);

    decl.Format("%s &m_Next", name.CStr());
    r = engine->RegisterObjectProperty(name.CStr(), decl.CStr(), asOFFSET(C, m_Next)); assert(r >= 0);

    decl.Format("%s &opAssign(const %s &in)", name.CStr(), name.CStr());
    r = engine->RegisterObjectMethod(name.CStr(), decl.CStr(), asMETHODPR(C, operator=, (const C &), C &), asCALL_THISCALL); assert(r >= 0);
}

template<typename C, typename T, typename K>
void RegisterXHashTableIt(asIScriptEngine *engine, const char *className, const char *keyType, const char *elementType) {
    int r = 0;
    XString decl;

    XString name = decl.Format("%sIt", className);
    r = engine->RegisterObjectType(name.CStr(), sizeof(C), asOBJ_VALUE | asGetTypeTraits<C>());  assert(r >= 0 || r == asALREADY_REGISTERED);
    if (r == asALREADY_REGISTERED)
        return;

    r = engine->RegisterObjectBehaviour(name.CStr(), asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](C *self) { new(self) C(); }, (C *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    decl.Format("void f(const %s &in)", name.CStr());
    r = engine->RegisterObjectBehaviour(name.CStr(), asBEHAVE_CONSTRUCT, decl.CStr(), asFUNCTIONPR([](const C &c, C *self) { new(self) C(c); }, (const C &, C *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour(name.CStr(), asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](C *self) { self->~C(); }, (C *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    decl.Format("%s &opAssign(const %s &in)", name.CStr(), name.CStr());
    r = engine->RegisterObjectMethod(name.CStr(), decl.CStr(), asMETHODPR(C, operator=, (const C &), C &), asCALL_THISCALL); assert(r >= 0);

    decl.Format("bool opEquals(const %s &in) const", name.CStr());
    r = engine->RegisterObjectMethod(name.CStr(), decl.CStr(), asMETHODPR(C, operator==, (const C &) const, int), asCALL_THISCALL); assert(r >= 0);

    decl.Format("const %s &GetKey() const", keyType);
    r = engine->RegisterObjectMethod(name.CStr(), decl.CStr(), asMETHODPR(C, GetKey, () const, const K &), asCALL_THISCALL); assert(r >= 0);

    decl.Format("const %s &GetValue() const", elementType);
    r = engine->RegisterObjectMethod(name.CStr(), decl.CStr(), asMETHODPR(C, operator*, () const, const T &), asCALL_THISCALL); assert(r >= 0);

    decl.Format("%s &GetValue()", elementType);
    r = engine->RegisterObjectMethod(name.CStr(), decl.CStr(), asMETHODPR(C, operator*, (), T &), asCALL_THISCALL); assert(r >= 0);

    decl.Format("%s &opPreInc()", name.CStr());
    r = engine->RegisterObjectMethod(name.CStr(), decl.CStr(), asMETHODPR(C, operator++, (), C &), asCALL_THISCALL); assert(r >= 0);

    decl.Format("%s opPostInc(int)", name.CStr());
    r = engine->RegisterObjectMethod(name.CStr(), decl.CStr(), asMETHODPR(C, operator++, (int), C), asCALL_THISCALL); assert(r >= 0);
}

template<typename C, typename T, typename K>
void RegisterXHashTableConstIt(asIScriptEngine *engine, const char *className, const char *keyType, const char *elementType) {
    int r = 0;
    XString decl;

    XString name = decl.Format("%sConstIt", className);
    r = engine->RegisterObjectType(name.CStr(), sizeof(C), asOBJ_VALUE | asGetTypeTraits<C>());  assert(r >= 0 || r == asALREADY_REGISTERED);
    if (r == asALREADY_REGISTERED)
        return;

    r = engine->RegisterObjectBehaviour(name.CStr(), asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](C *self) { new(self) C(); }, (C *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    decl.Format("void f(const %s &in)", name.CStr());
    r = engine->RegisterObjectBehaviour(name.CStr(), asBEHAVE_CONSTRUCT, decl.CStr(), asFUNCTIONPR([](const C &c, C *self) { new(self) C(c); }, (const C &, C *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour(name.CStr(), asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](C *self) { self->~C(); }, (C *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    decl.Format("%s &opAssign(const %s &in)", name.CStr(), name.CStr());
    r = engine->RegisterObjectMethod(name.CStr(), decl.CStr(), asMETHODPR(C, operator=, (const C &), C &), asCALL_THISCALL); assert(r >= 0);

    decl.Format("bool opEquals(const %s &in) const", name.CStr());
    r = engine->RegisterObjectMethod(name.CStr(), decl.CStr(), asMETHODPR(C, operator==, (const C &) const, int), asCALL_THISCALL); assert(r >= 0);

    decl.Format("const %s &GetKey() const", keyType);
    r = engine->RegisterObjectMethod(name.CStr(), decl.CStr(), asMETHODPR(C, GetKey, () const, const K &), asCALL_THISCALL); assert(r >= 0);

    decl.Format("const %s &GetValue() const", elementType);
    r = engine->RegisterObjectMethod(name.CStr(), decl.CStr(), asMETHODPR(C, operator*, () const, const T &), asCALL_THISCALL); assert(r >= 0);

    decl.Format("%s &opPreInc()", name.CStr());
    r = engine->RegisterObjectMethod(name.CStr(), decl.CStr(), asMETHODPR(C, operator++, (), C &), asCALL_THISCALL); assert(r >= 0);

    decl.Format("%s opPostInc(int)", name.CStr());
    r = engine->RegisterObjectMethod(name.CStr(), decl.CStr(), asMETHODPR(C, operator++, (int), C), asCALL_THISCALL); assert(r >= 0);
}

template<typename C, typename T, typename K>
void RegisterXHashTablePair(asIScriptEngine *engine, const char *className, const char *keyType, const char *elementType) {
    int r = 0;
    XString decl;
    XString name = decl.Format("%sPair", className);

    r = engine->RegisterObjectType(name.CStr(), sizeof(C), asOBJ_VALUE | asGetTypeTraits<C>());  assert(r >= 0 || r == asALREADY_REGISTERED);
    if (r == asALREADY_REGISTERED)
        return;

#if CKVERSION == 0x13022002
    using HashTableIt = XHashTableIt<T, K>;
#else
    using HashTableIt = XHashTable<T, K>::Iterator;
#endif
    
    decl.Format("void f(%sIt, int)", className);
#if CKVERSION == 0x13022002
    r = engine->RegisterObjectBehaviour(name.CStr(), asBEHAVE_CONSTRUCT, decl.CStr(), asFUNCTIONPR([](XHashTableIt<T, K> it, int n, C *self) { new(self) C(it, n); }, (XHashTableIt<T, K>, int, C *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
#else
    r = engine->RegisterObjectBehaviour(name.CStr(), asBEHAVE_CONSTRUCT, decl.CStr(), asFUNCTIONPR([](HashTableIt it, int n, C *self) { new(self) C(it, n); }, (HashTableIt, int, C *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
#endif
    decl.Format("void f(const %s &in)", name.CStr());
    r = engine->RegisterObjectBehaviour(name.CStr(), asBEHAVE_CONSTRUCT, decl.CStr(), asFUNCTIONPR([](const C &c, C *self) { new(self) C(c); }, (const C &, C *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour(name.CStr(), asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](C *self) { self->~C(); }, (C *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    decl.Format("%sIt m_Iterator", className);
    r = engine->RegisterObjectProperty(name.CStr(), decl.CStr(), asOFFSET(C, m_Iterator)); assert(r >= 0);

    r = engine->RegisterObjectProperty(name.CStr(), "int m_New", asOFFSET(C, m_New)); assert(r >= 0);

    decl.Format("%s &opAssign(const %s &in)", name.CStr(), name.CStr());
    r = engine->RegisterObjectMethod(name.CStr(), decl.CStr(), asMETHODPR(C, operator=, (const C &), C &), asCALL_THISCALL); assert(r >= 0);
}

template<typename C, typename T, typename K>
void RegisterXNHashTablePair(asIScriptEngine *engine, const char *className, const char *keyType, const char *elementType) {
    int r = 0;
    XString decl;
    XString name = decl.Format("%sPair", className);

    r = engine->RegisterObjectType(name.CStr(), sizeof(C), asOBJ_VALUE | asGetTypeTraits<C>());  assert(r >= 0 || r == asALREADY_REGISTERED);
    if (r == asALREADY_REGISTERED)
        return;

    decl.Format("void f(%sIt, int)", className);
    r = engine->RegisterObjectBehaviour(name.CStr(), asBEHAVE_CONSTRUCT, decl.CStr(), asFUNCTIONPR([](XNHashTableIt<T, K> it, int n, C *self) { new(self) C(it, n); }, (XNHashTableIt<T, K>, int, C *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    decl.Format("void f(const %s &in)", name.CStr());
    r = engine->RegisterObjectBehaviour(name.CStr(), asBEHAVE_CONSTRUCT, decl.CStr(), asFUNCTIONPR([](const C &c, C *self) { new(self) C(c); }, (const C &, C *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour(name.CStr(), asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](C *self) { self->~C(); }, (C *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    decl.Format("%sIt m_Iterator", className);
    r = engine->RegisterObjectProperty(name.CStr(), decl.CStr(), asOFFSET(C, m_Iterator)); assert(r >= 0);

    r = engine->RegisterObjectProperty(name.CStr(), "int m_New", asOFFSET(C, m_New)); assert(r >= 0);

    decl.Format("%s &opAssign(const %s &in)", name.CStr(), name.CStr());
    r = engine->RegisterObjectMethod(name.CStr(), decl.CStr(), asMETHODPR(C, operator=, (const C &), C &), asCALL_THISCALL); assert(r >= 0);
}

template<typename C, typename T, typename K>
void RegisterXSHashTablePair(asIScriptEngine *engine, const char *className, const char *keyType, const char *elementType) {
    int r = 0;
    XString decl;
    XString name = decl.Format("%sPair", className);

    r = engine->RegisterObjectType(name.CStr(), sizeof(C), asOBJ_VALUE | asGetTypeTraits<C>());  assert(r >= 0 || r == asALREADY_REGISTERED);
    if (r == asALREADY_REGISTERED)
        return;

    decl.Format("void f(%sIt, int)", className);
    r = engine->RegisterObjectBehaviour(name.CStr(), asBEHAVE_CONSTRUCT, decl.CStr(), asFUNCTIONPR([](XSHashTableIt<T, K> it, int n, C *self) { new(self) C(it, n); }, (XSHashTableIt<T, K>, int, C *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    decl.Format("void f(const %s &in)", name.CStr());
    r = engine->RegisterObjectBehaviour(name.CStr(), asBEHAVE_CONSTRUCT, decl.CStr(), asFUNCTIONPR([](const C &c, C *self) { new(self) C(c); }, (const C &, C *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour(name.CStr(), asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](C *self) { self->~C(); }, (C *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    decl.Format("%sIt m_Iterator", className);
    r = engine->RegisterObjectProperty(name.CStr(), decl.CStr(), asOFFSET(C, m_Iterator)); assert(r >= 0);

    r = engine->RegisterObjectProperty(name.CStr(), "int m_New", asOFFSET(C, m_New)); assert(r >= 0);

    decl.Format("%s &opAssign(const %s &in)", name.CStr(), name.CStr());
    r = engine->RegisterObjectMethod(name.CStr(), decl.CStr(), asMETHODPR(C, operator=, (const C &), C &), asCALL_THISCALL); assert(r >= 0);
}

template<typename C, typename T, typename K>
void RegisterXHashTable(asIScriptEngine *engine, const char *className, const char *keyType, const char *elementType) {
    int r = 0;
    XString decl;

#if CKVERSION == 0x13022002
    using HashTableEntry = XHashTableEntry<T, K>;
#else
    using HashTableEntry = XHashTable<T, K>::Entry;
#endif
    RegisterXHashTableEntry<HashTableEntry, T, K>(engine, className, keyType, elementType);

#if CKVERSION == 0x13022002
    using HashTableIt = XHashTableIt<T, K>;
#else
    using HashTableIt = XHashTable<T, K>::Iterator;
#endif
    RegisterXHashTableIt<HashTableIt, T, K>(engine, className, keyType, elementType);

#if CKVERSION == 0x13022002
    using HashTableConstIt = XHashTableConstIt<T, K>;
#else
    using HashTableConstIt = XHashTable<T, K>::ConstIterator;
#endif
    RegisterXHashTableConstIt<HashTableConstIt, T, K>(engine, className, keyType, elementType);

#if CKVERSION == 0x13022002
    using HashTablePair = XHashTablePair<T, K>;
#else
    using HashTablePair = XHashTable<T, K>::Pair;
#endif
    RegisterXHashTablePair<HashTablePair, T, K>(engine, className, keyType, elementType);

    r = engine->RegisterObjectType(className, sizeof(C), asOBJ_VALUE | asGetTypeTraits<C>()); assert(r >= 0);

    r = engine->RegisterObjectBehaviour(className, asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](C *self) { new(self) C(); }, (C *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour(className, asBEHAVE_CONSTRUCT, "void f(int)", asFUNCTIONPR([](int init, C *self) { new(self) C(init); }, (int, C *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    decl.Format("void f(const %s &in)", className);
    r = engine->RegisterObjectBehaviour(className, asBEHAVE_CONSTRUCT, decl.CStr(), asFUNCTIONPR([](const C &c, C *self) { new(self) C(c); }, (const C &, C *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour(className, asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](C *self) { self->~C(); }, (C *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    decl.Format("%s &opAssign(const %s &in)", className, className);
    r = engine->RegisterObjectMethod(className, decl.CStr(), asMETHODPR(C, operator=, (const C &), C &), asCALL_THISCALL); assert(r >= 0);

    decl.Format("%s &opIndex(const %s &)", elementType, keyType);
    r = engine->RegisterObjectMethod(className, decl.CStr(), asMETHODPR(C, operator[], (const K &), T &), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(className, "void Clear()", asMETHODPR(C, Clear, (), void), asCALL_THISCALL); assert(r >= 0);

    decl.Format("bool Insert(const %s &in, const %s &in, bool override)", keyType, elementType);
    r = engine->RegisterObjectMethod(className, decl.CStr(), asMETHODPR(C, Insert, (const K &, const T &, XBOOL), XBOOL), asCALL_THISCALL); assert(r >= 0);

    decl.Format("%sIt Insert(const %s &in, const %s &in)", className, keyType, elementType);
    r = engine->RegisterObjectMethod(className, decl.CStr(), asMETHODPR(C, Insert, (const K &, const T &), HashTableIt), asCALL_THISCALL); assert(r >= 0);

    decl.Format("%sPair TestInsert(const %s &in, const %s &in)", className, keyType, elementType);
    r = engine->RegisterObjectMethod(className, decl.CStr(), asMETHODPR(C, TestInsert, (const K &, const T &), HashTablePair), asCALL_THISCALL); assert(r >= 0);

    decl.Format("%sIt InsertUnique(const %s &in, const %s &in)", className, keyType, elementType);
    r = engine->RegisterObjectMethod(className, decl.CStr(), asMETHODPR(C, InsertUnique, (const K &, const T &), HashTableIt), asCALL_THISCALL); assert(r >= 0);

    decl.Format("void Remove(const %s &in)", keyType);
    r = engine->RegisterObjectMethod(className, decl.CStr(), asMETHODPR(C, Remove, (const K &), void), asCALL_THISCALL); assert(r >= 0);

    decl.Format("%sIt Remove(const %sIt &in)", className, className);
    r = engine->RegisterObjectMethod(className, decl.CStr(), asMETHODPR(C, Remove, (const HashTableIt &), HashTableIt), asCALL_THISCALL); assert(r >= 0);

    decl.Format("%sIt Find(const %s &in)", className, keyType);
    r = engine->RegisterObjectMethod(className, decl.CStr(), asMETHODPR(C, Find, (const K &), HashTableIt), asCALL_THISCALL); assert(r >= 0);

    decl.Format("%sConstIt Find(const %s &in) const", className, keyType);
    r = engine->RegisterObjectMethod(className, decl.CStr(), asMETHODPR(C, Find, (const K &) const, HashTableConstIt), asCALL_THISCALL); assert(r >= 0);

    decl.Format("bool LookUp(const %s &in, %s &out) const", keyType, elementType);
    r = engine->RegisterObjectMethod(className, decl.CStr(), asMETHODPR(C, LookUp, (const K &, T &) const, XBOOL), asCALL_THISCALL); assert(r >= 0);

    decl.Format("bool IsHere(const %s &in) const", keyType);
    r = engine->RegisterObjectMethod(className, decl.CStr(), asMETHODPR(C, IsHere, (const K &) const, XBOOL), asCALL_THISCALL); assert(r >= 0);

    decl.Format("%sIt Begin()", className);
    r = engine->RegisterObjectMethod(className, decl.CStr(), asFUNCTIONPR([](C &table) -> HashTableIt { return table.Begin(); }, (C &), HashTableIt), asCALL_CDECL_OBJFIRST); assert( r >= 0 );

    decl.Format("%sConstIt Begin() const", className);
    r = engine->RegisterObjectMethod(className, decl.CStr(), asFUNCTIONPR([](const C &table) -> HashTableConstIt { return table.Begin(); }, (const C &), HashTableConstIt), asCALL_CDECL_OBJFIRST); assert( r >= 0 );

    decl.Format("%sIt End()", className);
    r = engine->RegisterObjectMethod(className, decl.CStr(), asFUNCTIONPR([](C &table) -> HashTableIt { return table.End(); }, (C &), HashTableIt), asCALL_CDECL_OBJFIRST); assert( r >= 0 );

    decl.Format("%sConstIt End() const", className);
    r = engine->RegisterObjectMethod(className, decl.CStr(), asFUNCTIONPR([](const C &table) -> HashTableConstIt { return table.End(); }, (const C &), HashTableConstIt), asCALL_CDECL_OBJFIRST); assert( r >= 0 );

    decl.Format("int Index(const %s &in) const", keyType);
    r = engine->RegisterObjectMethod(className, decl.CStr(), asMETHODPR(C, Index, (const K &) const, int), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(className, "int Size() const", asMETHODPR(C, Size, () const, int), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(className, "int GetMemoryOccupation(bool = false) const", asMETHODPR(C, GetMemoryOccupation, (XBOOL) const, int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(className, "void Reserve(int)", asMETHODPR(C, Reserve, (int), void), asCALL_THISCALL); assert(r >= 0);
}

template<typename C, typename T, typename K>
void RegisterXNHashTable(asIScriptEngine *engine, const char *className, const char *keyType, const char *elementType) {
    int r = 0;
    XString decl;

    using HashTable = XNHashTable<T, K>;

    using HashTableEntry = XNHashTableEntry<T, K>;
    RegisterXNHashTableEntry<HashTableEntry, T, K>(engine, className, keyType, elementType);

    using HashTableIt = typename XNHashTable<T, K>::Iterator;
    RegisterXHashTableIt<HashTableIt, T, K>(engine, className, keyType, elementType);

    using HashTableConstIt = typename XNHashTable<T, K>::ConstIterator;
    RegisterXHashTableConstIt<HashTableConstIt, T, K>(engine, className, keyType, elementType);

    using HashTablePair = typename XNHashTable<T, K>::Pair;
    RegisterXNHashTablePair<HashTablePair, T, K>(engine, className, keyType, elementType);

    r = engine->RegisterObjectType(className, sizeof(C), asOBJ_VALUE | asGetTypeTraits<C>()); assert(r >= 0);

    r = engine->RegisterObjectBehaviour(className, asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](C *self) { new(self) C(); }, (C *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour(className, asBEHAVE_CONSTRUCT, "void f(int)", asFUNCTIONPR([](int init, C *self) { new(self) C(init); }, (int, C *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    decl.Format("void f(const %s &in)", className);
    r = engine->RegisterObjectBehaviour(className, asBEHAVE_CONSTRUCT, decl.CStr(), asFUNCTIONPR([](const C &c, C *self) { new(self) C(c); }, (const C &, C *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour(className, asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](C *self) { self->~C(); }, (C *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    decl.Format("%s &opAssign(const %s &in)", className, className);
    r = engine->RegisterObjectMethod(className, decl.CStr(), asMETHODPR(C, operator=, (const C &), C &), asCALL_THISCALL); assert(r >= 0);

    decl.Format("%s &opIndex(const %s &)", elementType, keyType);
    r = engine->RegisterObjectMethod(className, decl.CStr(), asMETHODPR(HashTable, operator[], (const K &), T &), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(className, "void Clear()", asMETHODPR(HashTable, Clear, (), void), asCALL_THISCALL); assert(r >= 0);

    decl.Format("bool Insert(const %s &in, const %s &in, bool override)", keyType, elementType);
    r = engine->RegisterObjectMethod(className, decl.CStr(), asMETHODPR(HashTable, Insert, (const K &, const T &, XBOOL), XBOOL), asCALL_THISCALL); assert(r >= 0);

    decl.Format("%sIt Insert(const %s &in, const %s &in)", className, keyType, elementType);
    r = engine->RegisterObjectMethod(className, decl.CStr(), asMETHODPR(HashTable, Insert, (const K &, const T &), HashTableIt), asCALL_THISCALL); assert(r >= 0);

    decl.Format("%sPair TestInsert(const %s &in, const %s &in)", className, keyType, elementType);
    r = engine->RegisterObjectMethod(className, decl.CStr(), asMETHODPR(HashTable, TestInsert, (const K &, const T &), HashTablePair), asCALL_THISCALL); assert(r >= 0);

    decl.Format("%sIt InsertUnique(const %s &in, const %s &in)", className, keyType, elementType);
    r = engine->RegisterObjectMethod(className, decl.CStr(), asMETHODPR(HashTable, InsertUnique, (const K &, const T &), HashTableIt), asCALL_THISCALL); assert(r >= 0);

    decl.Format("void Remove(const %s &in)", keyType);
    r = engine->RegisterObjectMethod(className, decl.CStr(), asMETHODPR(HashTable, Remove, (const K &), void), asCALL_THISCALL); assert(r >= 0);

    decl.Format("%sIt Remove(const %sIt &in)", className, className);
    r = engine->RegisterObjectMethod(className, decl.CStr(), asMETHODPR(HashTable, Remove, (const HashTableIt &), HashTableIt), asCALL_THISCALL); assert(r >= 0);

    decl.Format("%sIt Find(const %s &in)", className, keyType);
    r = engine->RegisterObjectMethod(className, decl.CStr(), asMETHODPR(HashTable, Find, (const K &), HashTableIt), asCALL_THISCALL); assert(r >= 0);

    decl.Format("%sConstIt Find(const %s &in) const", className, keyType);
    r = engine->RegisterObjectMethod(className, decl.CStr(), asMETHODPR(HashTable, Find, (const K &) const, HashTableConstIt), asCALL_THISCALL); assert(r >= 0);

    decl.Format("bool LookUp(const %s &in, %s &out) const", keyType, elementType);
    r = engine->RegisterObjectMethod(className, decl.CStr(), asMETHODPR(HashTable, LookUp, (const K &, T &) const, XBOOL), asCALL_THISCALL); assert(r >= 0);

    decl.Format("bool IsHere(const %s &in) const", keyType);
    r = engine->RegisterObjectMethod(className, decl.CStr(), asMETHODPR(HashTable, IsHere, (const K &) const, XBOOL), asCALL_THISCALL); assert(r >= 0);

    decl.Format("%sIt Begin()", className);
    r = engine->RegisterObjectMethod(className, decl.CStr(), asFUNCTIONPR([](HashTable &table) -> HashTableIt { return table.Begin(); }, (HashTable &), HashTableIt), asCALL_CDECL_OBJFIRST); assert( r >= 0 );

    decl.Format("%sConstIt Begin() const", className);
    r = engine->RegisterObjectMethod(className, decl.CStr(), asFUNCTIONPR([](const HashTable &table) -> HashTableConstIt { return table.Begin(); }, (const HashTable &), HashTableConstIt), asCALL_CDECL_OBJFIRST); assert( r >= 0 );

    decl.Format("%sIt End()", className);
    r = engine->RegisterObjectMethod(className, decl.CStr(), asFUNCTIONPR([](HashTable &table) -> HashTableIt { return table.End(); }, (HashTable &), HashTableIt), asCALL_CDECL_OBJFIRST); assert( r >= 0 );

    decl.Format("%sConstIt End() const", className);
    r = engine->RegisterObjectMethod(className, decl.CStr(), asFUNCTIONPR([](const HashTable &table) -> HashTableConstIt { return table.End(); }, (const HashTable &), HashTableConstIt), asCALL_CDECL_OBJFIRST); assert( r >= 0 );

    decl.Format("int Index(const %s &in) const", keyType);
    r = engine->RegisterObjectMethod(className, decl.CStr(), asMETHODPR(HashTable, Index, (const K &) const, int), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(className, "int Size() const", asMETHODPR(HashTable, Size, () const, int), asCALL_THISCALL); assert(r >= 0);
}


template<typename C, typename T, typename K>
void RegisterXSHashTable(asIScriptEngine *engine, const char *className, const char *keyType, const char *elementType) {
    int r = 0;
    XString decl;

    using HashTable = XSHashTable<T, K>;

    using HashTableEntry = XSHashTableEntry<T, K>;
    RegisterXNHashTableEntry<HashTableEntry, T, K>(engine, className, keyType, elementType);

    using HashTableIt = XSHashTableIt<T, K>;
    RegisterXHashTableIt<HashTableIt, T, K>(engine, className, keyType, elementType);

    using HashTableConstIt = XSHashTableConstIt<T, K>;
    RegisterXHashTableConstIt<HashTableConstIt, T, K>(engine, className, keyType, elementType);

    using HashTablePair = XSHashTablePair<T, K>;
    RegisterXSHashTablePair<HashTablePair, T, K>(engine, className, keyType, elementType);

    r = engine->RegisterObjectType(className, sizeof(C), asOBJ_VALUE | asGetTypeTraits<C>()); assert(r >= 0);

    r = engine->RegisterObjectBehaviour(className, asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](C *self) { new(self) C(); }, (C *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour(className, asBEHAVE_CONSTRUCT, "void f(int)", asFUNCTIONPR([](int init, C *self) { new(self) C(init); }, (int, C *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    decl.Format("void f(const %s &in)", className);
    r = engine->RegisterObjectBehaviour(className, asBEHAVE_CONSTRUCT, decl.CStr(), asFUNCTIONPR([](const C &c, C *self) { new(self) C(c); }, (const C &, C *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectBehaviour(className, asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](C *self) { self->~C(); }, (C *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    decl.Format("%s &opAssign(const %s &in)", className, className);
    r = engine->RegisterObjectMethod(className, decl.CStr(), asMETHODPR(C, operator=, (const C &), C &), asCALL_THISCALL); assert(r >= 0);

    decl.Format("%s &opIndex(const %s &)", elementType, keyType);
    r = engine->RegisterObjectMethod(className, decl.CStr(), asMETHODPR(HashTable, operator[], (const K &), T &), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(className, "void Clear()", asMETHODPR(HashTable, Clear, (), void), asCALL_THISCALL); assert(r >= 0);

    decl.Format("bool Insert(const %s &in, const %s &in, bool override)", keyType, elementType);
    r = engine->RegisterObjectMethod(className, decl.CStr(), asMETHODPR(HashTable, Insert, (const K &, const T &, XBOOL), XBOOL), asCALL_THISCALL); assert(r >= 0);

    decl.Format("%sIt Insert(const %s &in, const %s &in)", className, keyType, elementType);
    r = engine->RegisterObjectMethod(className, decl.CStr(), asMETHODPR(HashTable, Insert, (const K &, const T &), HashTableIt), asCALL_THISCALL); assert(r >= 0);

    decl.Format("%sPair TestInsert(const %s &in, const %s &in)", className, keyType, elementType);
    r = engine->RegisterObjectMethod(className, decl.CStr(), asMETHODPR(HashTable, TestInsert, (const K &, const T &), HashTablePair), asCALL_THISCALL); assert(r >= 0);

    decl.Format("%sIt InsertUnique(const %s &in, const %s &in)", className, keyType, elementType);
    r = engine->RegisterObjectMethod(className, decl.CStr(), asMETHODPR(HashTable, InsertUnique, (const K &, const T &), HashTableIt), asCALL_THISCALL); assert(r >= 0);

    decl.Format("void Remove(const %s &in)", keyType);
    r = engine->RegisterObjectMethod(className, decl.CStr(), asMETHODPR(HashTable, Remove, (const K &), void), asCALL_THISCALL); assert(r >= 0);

    decl.Format("%sIt Remove(const %sIt &in)", className, className);
    r = engine->RegisterObjectMethod(className, decl.CStr(), asMETHODPR(HashTable, Remove, (const HashTableIt &), HashTableIt), asCALL_THISCALL); assert(r >= 0);

    decl.Format("%sIt Find(const %s &in)", className, keyType);
    r = engine->RegisterObjectMethod(className, decl.CStr(), asMETHODPR(HashTable, Find, (const K &), HashTableIt), asCALL_THISCALL); assert(r >= 0);

    decl.Format("%sConstIt Find(const %s &in) const", className, keyType);
    r = engine->RegisterObjectMethod(className, decl.CStr(), asMETHODPR(HashTable, Find, (const K &) const, HashTableConstIt), asCALL_THISCALL); assert(r >= 0);

    decl.Format("bool LookUp(const %s &in, %s &out) const", keyType, elementType);
    r = engine->RegisterObjectMethod(className, decl.CStr(), asMETHODPR(HashTable, LookUp, (const K &, T &) const, XBOOL), asCALL_THISCALL); assert(r >= 0);

    decl.Format("bool IsHere(const %s &in) const", keyType);
    r = engine->RegisterObjectMethod(className, decl.CStr(), asMETHODPR(HashTable, IsHere, (const K &) const, XBOOL), asCALL_THISCALL); assert(r >= 0);

    decl.Format("%sIt Begin()", className);
    r = engine->RegisterObjectMethod(className, decl.CStr(), asFUNCTIONPR([](HashTable &table) -> HashTableIt { return table.Begin(); }, (HashTable &), HashTableIt), asCALL_CDECL_OBJFIRST); assert( r >= 0 );

    decl.Format("%sConstIt Begin() const", className);
    r = engine->RegisterObjectMethod(className, decl.CStr(), asFUNCTIONPR([](const HashTable &table) -> HashTableConstIt { return table.Begin(); }, (const HashTable &), HashTableConstIt), asCALL_CDECL_OBJFIRST); assert( r >= 0 );

    decl.Format("%sIt End()", className);
    r = engine->RegisterObjectMethod(className, decl.CStr(), asFUNCTIONPR([](HashTable &table) -> HashTableIt { return table.End(); }, (HashTable &), HashTableIt), asCALL_CDECL_OBJFIRST); assert( r >= 0 );

    decl.Format("%sConstIt End() const", className);
    r = engine->RegisterObjectMethod(className, decl.CStr(), asFUNCTIONPR([](const HashTable &table) -> HashTableConstIt { return table.End(); }, (const HashTable &), HashTableConstIt), asCALL_CDECL_OBJFIRST); assert( r >= 0 );

    decl.Format("int Index(const %s &in) const", keyType);
    r = engine->RegisterObjectMethod(className, decl.CStr(), asMETHODPR(HashTable, Index, (const K &) const, int), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod(className, "int Size() const", asMETHODPR(HashTable, Size, () const, int), asCALL_THISCALL); assert(r >= 0);
}

#endif // CK_SCRIPTXHASHTABLE_H
