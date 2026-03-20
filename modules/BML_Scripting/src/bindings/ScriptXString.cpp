#include "ScriptXString.h"

#include <cassert>
#include <string>
#include <unordered_map>

#include "XString.h"

namespace std {
    // BKDR hash function for XString
    template<>
    struct hash<XString> {
        size_t operator()(const XString &str) const noexcept {
            size_t hash = 0;

            if (str.Length() != 0) {
                const char *pch = str.CStr();
                while (char ch = *pch++) {
                    hash = hash * 131 + ch;
                }
            }

            return hash;
        }
    };
}

class CXStringFactory : public asIStringFactory {
public:
    CXStringFactory() = default;

    ~CXStringFactory() override {
        // The script engine must release each string 
        // constant that it has requested
        assert(m_StringCache.empty());
    }

    const void *GetStringConstant(const char *data, asUINT length) override {
        // The string factory might be modified from multiple 
        // threads, so it is necessary to use a mutex.
        asAcquireExclusiveLock();

        XString str(data, (int) length);
        auto it = m_StringCache.find(str);
        if (it != m_StringCache.end())
            it->second++;
        else
            it = m_StringCache.insert(XStringCache::value_type(str, 1)).first;

        asReleaseExclusiveLock();

        return reinterpret_cast<const void *>(&it->first);
    }

    int ReleaseStringConstant(const void *str) override {
        if (!str)
            return asERROR;

        int ret = asSUCCESS;

        // The string factory might be modified from multiple 
        // threads, so it is necessary to use a mutex.
        asAcquireExclusiveLock();

        auto it = m_StringCache.find(*reinterpret_cast<const XString *>(str));
        if (it == m_StringCache.end())
            ret = asERROR;
        else {
            it->second--;
            if (it->second == 0)
                m_StringCache.erase(it);
        }

        asReleaseExclusiveLock();

        return ret;
    }

    int GetRawStringData(const void *str, char *data, asUINT *length) const override {
        if (!str)
            return asERROR;

        if (length)
            *length = (asUINT) reinterpret_cast<const XString *>(str)->Length();

        if (data)
            memcpy(data, reinterpret_cast<const XString *>(str)->CStr(), reinterpret_cast<const XString *>(str)->Length());

        return asSUCCESS;
    }

    // THe access to the string cache is protected with the common mutex provided by AngelScript
    typedef std::unordered_map<XString, int> XStringCache;
    XStringCache m_StringCache;
};

static CXStringFactory *s_StringFactory = nullptr;

CXStringFactory *GetXStringFactorySingleton() {
    if (!s_StringFactory) {
        // The following instance will be destroyed by the global 
        // CXStringFactoryCleaner instance upon application shutdown
        s_StringFactory = new CXStringFactory();
    }
    return s_StringFactory;
}

class CXStringFactoryCleaner {
public:
    ~CXStringFactoryCleaner() {
        if (s_StringFactory) {
            // Only delete the string factory if the m_StringCache is empty
            // If it is not empty, it means that someone might still attempt
            // to release string constants, so if we delete the string factory
            // the application might crash. Not deleting the cache would
            // lead to a memory leak, but since this is only happens when the
            // application is shutting down anyway, it is not important.
            if (s_StringFactory->m_StringCache.empty()) {
                delete s_StringFactory;
                s_StringFactory = nullptr;
            }
        }
    }
};

static CXStringFactoryCleaner s_StringCleaner;

static bool CheckStringSize(const std::string &str) {
    if (str.size() >= UINT16_MAX) {
        // Set a script exception
        asIScriptContext *ctx = asGetActiveContext();
        ctx->SetException("Oversized string");
        return false;
    }
    return true;
}

static void ConstructXString(const std::string &str, XString *self) {
    if (!CheckStringSize(str))
        return;

    new(self) XString(str.c_str());
}

static XString &XStringAssign(const std::string &str, XString &self) {
    if (!CheckStringSize(str))
        return self;

    self = str.c_str();
    return self;
}

static bool XStringEquals(const XString &str, const XString &self) {
    return self.Compare(str) == 0;
}

static bool XStringEquals(const std::string &str, const XString &self) {
    if (!CheckStringSize(str))
        return false;

    return self.Compare(str.c_str()) == 0;
}

static int XStringCmp(const XString &str, const XString &self) {
    return self.Compare(str);
}

static int XStringCmp(const std::string &str, const XString &self) {
    if (!CheckStringSize(str))
        return 0;

    return self.Compare(str.c_str());
}

static XString &XStringAddAssign(const std::string &str, XString &self) {
    if (!CheckStringSize(str))
        return self;

    return self += str.c_str();
}

static XString XStringAdd(const std::string &str, const XString &self) {
    if (!CheckStringSize(str))
        return self;

    return self + str.c_str();
}

static char *XStringCharAt(const XWORD i, XString &self) {
    if (i >= self.Length()) {
        // Set a script exception
        asIScriptContext *ctx = asGetActiveContext();
        ctx->SetException("Out of range");
        return nullptr;
    }

    return &self[i];
}

static std::string XStringCastToString(const XString &str) {
    return str.CStr();
}

static XString StringCastToXString(const std::string &str) {
    if (str.size() >= UINT16_MAX) {
        // Set a script exception
        asIScriptContext *ctx = asGetActiveContext();
        ctx->SetException("Oversized string");
        return {};
    }

    return str.c_str();
}

void RegisterXString(asIScriptEngine *engine) {
    assert(engine != nullptr);

    int r = 0;

    r = engine->RegisterObjectType("XString", sizeof(XString), asOBJ_VALUE | asGetTypeTraits<XString>()); assert(r >= 0);

    // r = engine->RegisterStringFactory("XString", GetXStringFactorySingleton()); assert(r >= 0);

    // Constructors
    r = engine->RegisterObjectBehaviour("XString", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR([](XString *self) { new(self) XString(); }, (XString *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("XString", asBEHAVE_CONSTRUCT, "void f(const XString &in other)", asFUNCTIONPR([](const XBaseString &other, XString *self) { new(self) XString(other); }, (const XBaseString &, XString *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("XString", asBEHAVE_CONSTRUCT, "void f(const string &in str)", asFUNCTIONPR(ConstructXString, (const std::string &, XString *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("XString", asBEHAVE_CONSTRUCT, "void f(int length)", asFUNCTIONPR([](int length, XString *self) { new(self) XString(length); }, (int, XString *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    // Destructor
    r = engine->RegisterObjectBehaviour("XString", asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR([](XString *self) { self->~XString(); }, (XString *), void), asCALL_CDECL_OBJLAST); assert(r >= 0);

    // Methods
    r = engine->RegisterObjectMethod("XString", "XString &opAssign(const XString &in other)", asMETHODPR(XString, operator=, (const XString &), XString &), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XString", "XString &opAssign(const string &in str)", asFUNCTIONPR(XStringAssign, (const std::string &, XString &), XString &), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectMethod("XString", "bool opEquals(const XString &in other) const", asFUNCTIONPR(XStringEquals, (const XString &, const XString &), bool), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectMethod("XString", "bool opEquals(const string &in str) const", asFUNCTIONPR(XStringEquals, (const std::string &, const XString &), bool), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectMethod("XString", "int opCmp(const XString &in other) const", asFUNCTIONPR(XStringCmp, (const XString &, const XString &), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectMethod("XString", "int opCmp(const string &in str) const", asFUNCTIONPR(XStringCmp, (const std::string &, const XString &), int), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = engine->RegisterObjectMethod("XString", "XString &opAddAssign(const XString &in str)", asMETHODPR(XString, operator+=, (const XString&), XString &), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XString", "XString &opAddAssign(const string &in str)", asFUNCTIONPR(XStringAddAssign, (const std::string &, XString &), XString &), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectMethod("XString", "XString &opAddAssign(uint8 v)", asMETHODPR(XString, operator+=, (const char), XString &), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XString", "XString &opAddAssign(int v)", asMETHODPR(XString, operator+=, (const int), XString &), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XString", "XString &opAddAssign(float v)", asMETHODPR(XString, operator+=, (const float), XString &), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("XString", "XString opAdd(const XString &in str) const", asMETHODPR(XString, operator+, (const XBaseString &) const, XString), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XString", "XString opAdd(const string &in str) const", asFUNCTIONPR(XStringAdd, (const std::string &, const XString &), XString), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectMethod("XString", "XString opAdd(uint8 v) const", asMETHODPR(XString, operator+, (const char) const, XString), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XString", "XString opAdd(int v) const", asMETHODPR(XString, operator+, (const int) const, XString), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XString", "XString opAdd(float v) const", asMETHODPR(XString, operator+, (const float) const, XString), asCALL_THISCALL); assert(r >= 0);

    // Register the index operator, both as a mutator and as an inspector
    // Note that we don't register the operator[] directly, as it doesn't do bounds checking
    r = engine->RegisterObjectMethod("XString", "uint8 &opIndex(uint16 index)", asFUNCTION(XStringCharAt), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectMethod("XString", "const uint8 &opIndex(uint16 index) const", asFUNCTION(XStringCharAt), asCALL_CDECL_OBJLAST); assert(r >= 0);

    r = engine->RegisterObjectMethod("XString", "string opImplConv() const", asFUNCTION(XStringCastToString), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectMethod("string", "XString opImplConv() const", asFUNCTION(StringCastToXString), asCALL_CDECL_OBJLAST); assert(r >= 0);

#if CKVERSION == 0x13022002
    r = engine->RegisterObjectMethod("XString", "bool Empty() const", asMETHODPR(XString, Empty, () const, XBOOL), asCALL_THISCALL); assert(r >= 0);
#endif
    r = engine->RegisterObjectMethod("XString", "uint16 Length() const", asMETHODPR(XString, Length, () const, XWORD), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XString", "uint16 Capacity() const", asMETHODPR(XString, Capacity, (), XWORD), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XString", "void Resize(uint16 length)", asMETHODPR(XString, Resize, (XWORD), void), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XString", "void Reserve(uint16 length)", asMETHODPR(XString, Reserve, (XWORD), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("XString", "XString &ToUpper()", asMETHODPR(XString, ToUpper, (), XString &), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XString", "XString &ToLower()", asMETHODPR(XString, ToLower, (), XString &), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("XString", "int Compare(const XString &in str) const", asMETHODPR(XString, Compare, (const XBaseString &) const, int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XString", "int NCompare(const XString &in str, int n) const", asMETHODPR(XString, NCompare, (const XBaseString &, const int) const, int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XString", "int ICompare(const XString &in str) const", asMETHODPR(XString, ICompare, (const XBaseString &) const, int), asCALL_THISCALL); assert(r >= 0);
#if CKVERSION == 0x13022002
    r = engine->RegisterObjectMethod("XString", "int NICompare(const XString &in str, int n) const", asMETHODPR(XString, NICompare, (const XBaseString &, const int) const, int), asCALL_THISCALL); assert(r >= 0);
#endif

    r = engine->RegisterObjectMethod("XString", "XString &Trim()", asMETHODPR(XString, Trim, (), XString &), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XString", "XString &Strip()", asMETHODPR(XString, Strip, (), XString &), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XString", "bool Contains(const XString &in str) const", asMETHODPR(XString, Contains, (const XBaseString &) const, XBOOL), asCALL_THISCALL); assert(r >= 0);
#if CKVERSION == 0x13022002
    r = engine->RegisterObjectMethod("XString", "bool StartsWith(const XString &in str) const", asMETHODPR(XString, StartsWith, (const XBaseString &) const, XBOOL), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XString", "bool IStartsWith(const XString &in str) const", asMETHODPR(XString, IStartsWith, (const XBaseString &) const, XBOOL), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XString", "bool EndsWith(const XString &in str) const", asMETHODPR(XString, EndsWith, (const XBaseString &) const, XBOOL), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XString", "bool IEndsWith(const XString &in str) const", asMETHODPR(XString, IEndsWith, (const XBaseString &) const, XBOOL), asCALL_THISCALL); assert(r >= 0);
#endif
    r = engine->RegisterObjectMethod("XString", "uint16 Find(uint8 c, uint16 start = 0) const", asMETHODPR(XString, Find, (char, XWORD) const, XWORD), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XString", "uint16 Find(const XString &in str, uint16 start = 0) const", asMETHODPR(XString, Find, (const XBaseString &, XWORD) const, XWORD), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XString", "uint16 RFind(uint8 c, uint16 start = 0) const", asMETHODPR(XString, RFind, (char, XWORD) const, XWORD), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XString", "XString Substring(uint16 start, uint16 length = 0) const", asMETHODPR(XString, Substring, (XWORD, XWORD) const, XString), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XString", "XString &Crop(uint16 start, uint16 length)", asMETHODPR(XString, Crop, (XWORD, XWORD), XString &), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XString", "XString &Cut(uint16 start, uint16 length)", asMETHODPR(XString, Cut, (XWORD, XWORD), XString &), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XString", "int Replace(uint8 src, uint8 dest)", asMETHODPR(XString, Replace, (char, char), int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("XString", "int Replace(const XString &in srt, const XString &in dest)", asMETHODPR(XString, Replace, (const XBaseString &, const XBaseString &), int), asCALL_THISCALL); assert(r >= 0);
}
