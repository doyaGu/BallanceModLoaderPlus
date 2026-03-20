#include "ScriptXString.h"

#include <string>

#include "XBitArray.h"

static std::string XBitArrayConvertToString(XBitArray &self) {
    std::string str(self.Size(), '0');
    self.ConvertToString(&str[0]);
    return str;
}

void RegisterXBitArray(asIScriptEngine *engine) {
    assert(engine != nullptr);

    int r = 0;

    r = engine->RegisterObjectType("XBitArray", sizeof(XBitArray), asOBJ_VALUE | asGetTypeTraits<XBitArray>()); assert(r >= 0);

    // Constructors
    r = engine->RegisterObjectBehaviour("XBitArray", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](XBitArray *self) { new(self) XBitArray(); }, (XBitArray *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("XBitArray", asBEHAVE_CONSTRUCT, "void f(const XBitArray &in other)", asFUNCTIONPR([](const XBitArray &array, XBitArray *self) { new(self) XBitArray(array); }, (const XBitArray &, XBitArray *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("XBitArray", asBEHAVE_CONSTRUCT, "void f(int)", asFUNCTIONPR([](int initialize, XBitArray *self) { new(self) XBitArray(initialize); }, (int, XBitArray *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    // Destructor
    r = engine->RegisterObjectBehaviour("XBitArray", asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](XBitArray *self) { self->~XBitArray(); }, (XBitArray *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    // Methods
    r = engine->RegisterObjectMethod("XBitArray", "XBitArray &opAssign(const XBitArray &in other)", asMETHODPR(XBitArray, operator=, (const XBitArray &), XBitArray &), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XBitArray", "XBitArray &opSubAssign(const XBitArray &in other)", asMETHODPR(XBitArray, operator-=, (XBitArray &), XBitArray &), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XBitArray", "int opIndex(int index) const", asMETHOD(XBitArray, operator[]), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("XBitArray", "void CheckSize(int n)", asMETHOD(XBitArray, CheckSize), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XBitArray", "void CheckSameSize(const XBitArray &in other)", asMETHOD(XBitArray, CheckSameSize), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XBitArray", "int IsSet(int n) const", asMETHOD(XBitArray, IsSet), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XBitArray", "void AppendBits(int n, int v, int count)", asMETHOD(XBitArray, AppendBits), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XBitArray", "void Set(int n)", asMETHOD(XBitArray, Set), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XBitArray", "int TestSet(int n)", asMETHOD(XBitArray, TestSet), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XBitArray", "void Unset(int n)", asMETHOD(XBitArray, Unset), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XBitArray", "int TestUnset(int n)", asMETHOD(XBitArray, TestUnset), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XBitArray", "int Size() const", asMETHOD(XBitArray, Size), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XBitArray", "void Clear()", asMETHOD(XBitArray, Clear), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XBitArray", "void Fill()", asMETHOD(XBitArray, Fill), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XBitArray", "void And(const XBitArray &in other)", asMETHOD(XBitArray, And), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XBitArray", "bool CheckCommon(const XBitArray &in other) const", asMETHOD(XBitArray, CheckCommon), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XBitArray", "void Or(const XBitArray &in other)", asMETHOD(XBitArray, Or), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XBitArray", "void XOr(const XBitArray &in other)", asMETHOD(XBitArray, XOr), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XBitArray", "void Invert()", asMETHOD(XBitArray, Invert), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XBitArray", "int BitSet() const", asMETHOD(XBitArray, BitSet), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XBitArray", "int GetSetBitPosition(int n) const", asMETHOD(XBitArray, GetSetBitPosition), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XBitArray", "int GetUnsetBitPosition(int n) const", asMETHOD(XBitArray, GetUnsetBitPosition), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XBitArray", "string ConvertToString() const", asFUNCTION(XBitArrayConvertToString), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectMethod("XBitArray", "int GetMemoryOccupation(bool addStatic = false) const", asMETHOD(XBitArray, GetMemoryOccupation), asCALL_THISCALL); assert(r >= 0);
}
