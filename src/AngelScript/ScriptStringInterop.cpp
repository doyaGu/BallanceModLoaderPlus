#include "ScriptStringInterop.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <vector>

namespace BML::ScriptStringInterop {

namespace {

asITypeInfo *GetStringType(asIScriptEngine *engine) {
    return engine ? engine->GetTypeInfoByDecl("string") : nullptr;
}

asIStringFactory *GetStringFactory(asIScriptEngine *engine) {
    if (!engine) {
        return nullptr;
    }
    asIStringFactory *factory = nullptr;
    if (engine->GetStringFactory(nullptr, &factory) < 0) {
        return nullptr;
    }
    return factory;
}

using AsGetActiveContextFn = asIScriptContext *(__cdecl *)();

AsGetActiveContextFn ResolveAsGetActiveContext() {
    static AsGetActiveContextFn function = []() -> AsGetActiveContextFn {
        HMODULE module = GetModuleHandleA("AngelScript.dll");
        if (!module) {
            return nullptr;
        }

        FARPROC proc = GetProcAddress(module, "asGetActiveContext");
        if (!proc) {
            proc = GetProcAddress(module, "_asGetActiveContext");
        }
        return reinterpret_cast<AsGetActiveContextFn>(proc);
    }();
    return function;
}

} // namespace

ScriptStringObject::ScriptStringObject(asIScriptEngine *engine, const std::string &value)
    : m_Engine(engine), m_Type(GetStringType(engine)) {
    asIStringFactory *factory = GetStringFactory(engine);
    if (!factory || !m_Type) {
        return;
    }

    const void *constant = factory->GetStringConstant(value.data(), static_cast<asUINT>(value.size()));
    if (!constant) {
        return;
    }

    m_Object = engine->CreateScriptObjectCopy(const_cast<void *>(constant), m_Type);
    factory->ReleaseStringConstant(constant);
}

ScriptStringObject::~ScriptStringObject() {
    Reset();
}

ScriptStringObject::ScriptStringObject(ScriptStringObject &&other) noexcept
    : m_Engine(other.m_Engine), m_Type(other.m_Type), m_Object(other.m_Object) {
    other.m_Engine = nullptr;
    other.m_Type = nullptr;
    other.m_Object = nullptr;
}

ScriptStringObject &ScriptStringObject::operator=(ScriptStringObject &&other) noexcept {
    if (this != &other) {
        Reset();
        m_Engine = other.m_Engine;
        m_Type = other.m_Type;
        m_Object = other.m_Object;
        other.m_Engine = nullptr;
        other.m_Type = nullptr;
        other.m_Object = nullptr;
    }
    return *this;
}

void ScriptStringObject::Reset() {
    if (m_Engine && m_Type && m_Object) {
        m_Engine->ReleaseScriptObject(m_Object, m_Type);
    }
    m_Engine = nullptr;
    m_Type = nullptr;
    m_Object = nullptr;
}

bool ReadStringObject(asIScriptEngine *engine, const void *object, std::string &out) {
    out.clear();
    asIStringFactory *factory = GetStringFactory(engine);
    if (!factory || !object) {
        return false;
    }

    asUINT length = 0;
    if (factory->GetRawStringData(object, nullptr, &length) < 0) {
        return false;
    }
    if (length == 0) {
        return true;
    }

    std::vector<char> bytes(length);
    asUINT copyLength = length;
    if (factory->GetRawStringData(object, bytes.data(), &copyLength) < 0) {
        return false;
    }
    out.assign(bytes.data(), bytes.data() + copyLength);
    return true;
}

bool AssignStringObject(asIScriptEngine *engine, void *object, const std::string &value) {
    if (!engine || !object) {
        return false;
    }

    ScriptStringObject source(engine, value);
    if (!source) {
        return false;
    }

    return engine->AssignScriptObject(object, source.Get(), source.Type()) >= 0;
}

bool SetReturnString(asIScriptGeneric *gen, const std::string &value) {
    if (!gen) {
        return false;
    }

    ScriptStringObject source(gen->GetEngine(), value);
    if (!source) {
        return false;
    }

    return gen->SetReturnObject(source.Get()) >= 0;
}

bool ReadContextReturnString(asIScriptContext *context, std::string &out) {
    out.clear();
    if (!context || context->GetState() != asEXECUTION_FINISHED) {
        return false;
    }
    asIScriptEngine *engine = context->GetEngine();
    return ReadStringObject(engine, context->GetReturnObject(), out);
}

void RaiseActiveException(const char *message) {
    AsGetActiveContextFn getActiveContext = ResolveAsGetActiveContext();
    asIScriptContext *context = getActiveContext ? getActiveContext() : nullptr;
    if (context) {
        context->SetException(message ? message : "AngelScript native binding failed.");
    }
}

} // namespace BML::ScriptStringInterop
