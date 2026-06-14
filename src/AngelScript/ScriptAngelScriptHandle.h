#ifndef BML_SCRIPTANGELSCRIPTHANDLE_H
#define BML_SCRIPTANGELSCRIPTHANDLE_H

#include <angelscript.h>

namespace BML {

class ScriptObjectHandle {
public:
    // Adopt is for a reference already owned by native code, e.g. a plain T@
    // application parameter. Retain is for borrowed T@+ parameters that must
    // survive after the registered function returns.
    static ScriptObjectHandle Adopt(asIScriptObject *object) {
        return ScriptObjectHandle(object);
    }

    static ScriptObjectHandle Retain(asIScriptObject *object) {
        if (object)
            object->AddRef();
        return ScriptObjectHandle(object);
    }

    ScriptObjectHandle() = default;
    ~ScriptObjectHandle() {
        if (m_Object)
            m_Object->Release();
    }

    ScriptObjectHandle(const ScriptObjectHandle &) = delete;
    ScriptObjectHandle &operator=(const ScriptObjectHandle &) = delete;
    ScriptObjectHandle(ScriptObjectHandle &&other) noexcept : m_Object(other.Detach()) {}
    ScriptObjectHandle &operator=(ScriptObjectHandle &&other) noexcept {
        if (this != &other) {
            if (m_Object)
                m_Object->Release();
            m_Object = other.Detach();
        }
        return *this;
    }

    asIScriptObject *Get() const { return m_Object; }

    asIScriptObject *Detach() {
        asIScriptObject *object = m_Object;
        m_Object = nullptr;
        return object;
    }

private:
    explicit ScriptObjectHandle(asIScriptObject *object) : m_Object(object) {}

    asIScriptObject *m_Object = nullptr;
};

} // namespace BML

#endif
