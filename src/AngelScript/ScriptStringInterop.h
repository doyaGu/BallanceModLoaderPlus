#ifndef BML_SCRIPT_STRING_INTEROP_H
#define BML_SCRIPT_STRING_INTEROP_H

#ifdef GetObject
#define BML_SCRIPT_STRING_INTEROP_RESTORE_GETOBJECT
#pragma push_macro("GetObject")
#undef GetObject
#endif

#include <angelscript.h>

#include <cstdint>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

namespace BML::ScriptStringInterop {

bool ReadStringObject(asIScriptEngine *engine, const void *object, std::string &out);
bool AssignStringObject(asIScriptEngine *engine, void *object, const std::string &value);
bool SetReturnString(asIScriptGeneric *gen, const std::string &value);
bool ReadContextReturnString(asIScriptContext *context, std::string &out);
void RaiseActiveException(const char *message);

class ScriptStringObject {
public:
    ScriptStringObject() = default;
    ScriptStringObject(asIScriptEngine *engine, const std::string &value);
    ~ScriptStringObject();

    ScriptStringObject(const ScriptStringObject &) = delete;
    ScriptStringObject &operator=(const ScriptStringObject &) = delete;

    ScriptStringObject(ScriptStringObject &&other) noexcept;
    ScriptStringObject &operator=(ScriptStringObject &&other) noexcept;

    void Reset();
    void *Get() const { return m_Object; }
    asITypeInfo *Type() const { return m_Type; }
    explicit operator bool() const { return m_Object != nullptr; }

private:
    asIScriptEngine *m_Engine = nullptr;
    asITypeInfo *m_Type = nullptr;
    void *m_Object = nullptr;
};

template <typename T>
struct UnsupportedGenericArgument;

template <typename T>
struct GenericValueReader {
    static T Read(asIScriptGeneric *gen, asUINT index, bool &ok) {
        using U = std::remove_cv_t<T>;
        if constexpr (std::is_same_v<U, bool>) {
            return gen->GetArgByte(index) != 0;
        } else if constexpr (std::is_floating_point_v<U> && sizeof(U) == sizeof(float)) {
            return static_cast<T>(gen->GetArgFloat(index));
        } else if constexpr (std::is_floating_point_v<U> && sizeof(U) == sizeof(double)) {
            return static_cast<T>(gen->GetArgDouble(index));
        } else if constexpr (std::is_integral_v<U> && sizeof(U) <= sizeof(asBYTE)) {
            return static_cast<T>(gen->GetArgByte(index));
        } else if constexpr (std::is_integral_v<U> && sizeof(U) <= sizeof(asWORD)) {
            return static_cast<T>(gen->GetArgWord(index));
        } else if constexpr ((std::is_integral_v<U> || std::is_enum_v<U>) && sizeof(U) <= sizeof(asDWORD)) {
            return static_cast<T>(gen->GetArgDWord(index));
        } else if constexpr ((std::is_integral_v<U> || std::is_enum_v<U>) && sizeof(U) <= sizeof(asQWORD)) {
            return static_cast<T>(gen->GetArgQWord(index));
        } else if constexpr (std::is_pointer_v<U>) {
            void *object = gen->GetArgObject(index);
            if (!object) {
                object = gen->GetArgAddress(index);
            }
            return static_cast<T>(object);
        } else {
            void *object = gen->GetArgObject(index);
            if (!object) {
                object = gen->GetArgAddress(index);
            }
            if (!object) {
                ok = false;
                return T{};
            }
            return *static_cast<T *>(object);
        }
    }
};

template <typename T>
class GenericArg {
public:
    GenericArg(asIScriptGeneric *gen, asUINT index, bool &ok)
        : m_Value(GenericValueReader<T>::Read(gen, index, ok)) {}

    T Get() { return m_Value; }
    bool WriteBack(asIScriptGeneric *) { return true; }

private:
    T m_Value{};
};

template <typename T>
class GenericArg<T &> {
public:
    GenericArg(asIScriptGeneric *gen, asUINT index, bool &ok)
        : m_Value(static_cast<T *>(gen->GetArgAddress(index))) {
        if (!m_Value) {
            m_Value = static_cast<T *>(gen->GetArgObject(index));
        }
        if (!m_Value) {
            ok = false;
        }
    }

    T &Get() { return *m_Value; }
    bool WriteBack(asIScriptGeneric *) { return m_Value != nullptr; }

private:
    T *m_Value = nullptr;
};

template <typename T>
class GenericArg<const T &> {
public:
    GenericArg(asIScriptGeneric *gen, asUINT index, bool &ok)
        : m_Value(static_cast<const T *>(gen->GetArgAddress(index))) {
        if (!m_Value) {
            m_Value = static_cast<const T *>(gen->GetArgObject(index));
        }
        if (!m_Value) {
            ok = false;
        }
    }

    const T &Get() const { return *m_Value; }
    bool WriteBack(asIScriptGeneric *) { return m_Value != nullptr; }

private:
    const T *m_Value = nullptr;
};

template <>
class GenericArg<const std::string &> {
public:
    GenericArg(asIScriptGeneric *gen, asUINT index, bool &ok)
        : m_Engine(gen ? gen->GetEngine() : nullptr) {
        void *object = gen ? gen->GetArgObject(index) : nullptr;
        if (!object && gen) {
            object = gen->GetArgAddress(index);
        }
        ok = ok && ReadStringObject(m_Engine, object, m_Value);
    }

    const std::string &Get() const { return m_Value; }
    bool WriteBack(asIScriptGeneric *) { return true; }

private:
    asIScriptEngine *m_Engine = nullptr;
    std::string m_Value;
};

template <>
class GenericArg<std::string &> {
public:
    GenericArg(asIScriptGeneric *gen, asUINT index, bool &ok)
        : m_Engine(gen ? gen->GetEngine() : nullptr),
          m_Object(gen ? gen->GetArgObject(index) : nullptr) {
        if (!m_Object && gen) {
            m_Object = gen->GetArgAddress(index);
        }
        ok = ok && ReadStringObject(m_Engine, m_Object, m_Value);
    }

    std::string &Get() { return m_Value; }

    bool WriteBack(asIScriptGeneric *) {
        return AssignStringObject(m_Engine, m_Object, m_Value);
    }

private:
    asIScriptEngine *m_Engine = nullptr;
    void *m_Object = nullptr;
    std::string m_Value;
};

template <typename R>
bool SetGenericReturn(asIScriptGeneric *gen, R &&value) {
    using T = std::remove_cv_t<std::remove_reference_t<R>>;
    if constexpr (std::is_same_v<T, bool>) {
        return gen->SetReturnByte(value ? 1 : 0) >= 0;
    } else if constexpr (std::is_floating_point_v<T> && sizeof(T) == sizeof(float)) {
        return gen->SetReturnFloat(static_cast<float>(value)) >= 0;
    } else if constexpr (std::is_floating_point_v<T> && sizeof(T) == sizeof(double)) {
        return gen->SetReturnDouble(static_cast<double>(value)) >= 0;
    } else if constexpr (std::is_integral_v<T> && sizeof(T) <= sizeof(asBYTE)) {
        return gen->SetReturnByte(static_cast<asBYTE>(value)) >= 0;
    } else if constexpr (std::is_integral_v<T> && sizeof(T) <= sizeof(asWORD)) {
        return gen->SetReturnWord(static_cast<asWORD>(value)) >= 0;
    } else if constexpr ((std::is_integral_v<T> || std::is_enum_v<T>) && sizeof(T) <= sizeof(asDWORD)) {
        return gen->SetReturnDWord(static_cast<asDWORD>(value)) >= 0;
    } else if constexpr ((std::is_integral_v<T> || std::is_enum_v<T>) && sizeof(T) <= sizeof(asQWORD)) {
        return gen->SetReturnQWord(static_cast<asQWORD>(value)) >= 0;
    } else if constexpr (std::is_pointer_v<T>) {
        return gen->SetReturnAddress(reinterpret_cast<void *>(value)) >= 0;
    } else if constexpr (std::is_same_v<T, std::string>) {
        return SetReturnString(gen, value);
    } else {
        return gen->SetReturnObject(&value) >= 0;
    }
}

template <typename Tuple, std::size_t... I>
bool WriteBackTuple(Tuple &tuple, asIScriptGeneric *gen, std::index_sequence<I...>) {
    bool ok = true;
    ((ok = std::get<I>(tuple).WriteBack(gen) && ok), ...);
    return ok;
}

template <auto Function>
struct GenericFunction;

template <typename R, typename... Args, R (*Function)(Args...)>
struct GenericFunction<Function> {
    static void Call(asIScriptGeneric *gen) {
        bool ok = true;
        auto args = MakeArgs(gen, ok, std::index_sequence_for<Args...>{});
        if (!ok) {
            RaiseActiveException("Failed to marshal AngelScript generic function arguments.");
            return;
        }

        if constexpr (std::is_void_v<R>) {
            CallVoid(args, std::index_sequence_for<Args...>{});
            if (!WriteBackTuple(args, gen, std::index_sequence_for<Args...>{})) {
                RaiseActiveException("Failed to marshal AngelScript generic function outputs.");
            }
        } else {
            R result = CallReturn(args, std::index_sequence_for<Args...>{});
            if (!WriteBackTuple(args, gen, std::index_sequence_for<Args...>{}) ||
                !SetGenericReturn(gen, result)) {
                RaiseActiveException("Failed to marshal AngelScript generic function result.");
            }
        }
    }

private:
    template <std::size_t... I>
    static std::tuple<GenericArg<Args>...> MakeArgs(asIScriptGeneric *gen,
                                                    bool &ok,
                                                    std::index_sequence<I...>) {
        return std::tuple<GenericArg<Args>...>{GenericArg<Args>(gen, static_cast<asUINT>(I), ok)...};
    }

    template <typename Tuple, std::size_t... I>
    static void CallVoid(Tuple &args, std::index_sequence<I...>) {
        Function(std::get<I>(args).Get()...);
    }

    template <typename Tuple, std::size_t... I>
    static R CallReturn(Tuple &args, std::index_sequence<I...>) {
        return Function(std::get<I>(args).Get()...);
    }
};

template <auto Method>
struct GenericMethod;

template <typename C, typename R, typename... Args, R (C::*Method)(Args...)>
struct GenericMethod<Method> {
    static void Call(asIScriptGeneric *gen) {
        C *self = static_cast<C *>(gen ? gen->GetObject() : nullptr);
        if (!self) {
            RaiseActiveException("Missing AngelScript generic method object.");
            return;
        }
        bool ok = true;
        auto args = MakeArgs(gen, ok, std::index_sequence_for<Args...>{});
        if (!ok) {
            RaiseActiveException("Failed to marshal AngelScript generic method arguments.");
            return;
        }

        if constexpr (std::is_void_v<R>) {
            CallVoid(self, args, std::index_sequence_for<Args...>{});
            if (!WriteBackTuple(args, gen, std::index_sequence_for<Args...>{})) {
                RaiseActiveException("Failed to marshal AngelScript generic method outputs.");
            }
        } else {
            R result = CallReturn(self, args, std::index_sequence_for<Args...>{});
            if (!WriteBackTuple(args, gen, std::index_sequence_for<Args...>{}) ||
                !SetGenericReturn(gen, result)) {
                RaiseActiveException("Failed to marshal AngelScript generic method result.");
            }
        }
    }

private:
    template <std::size_t... I>
    static std::tuple<GenericArg<Args>...> MakeArgs(asIScriptGeneric *gen,
                                                    bool &ok,
                                                    std::index_sequence<I...>) {
        return std::tuple<GenericArg<Args>...>{GenericArg<Args>(gen, static_cast<asUINT>(I), ok)...};
    }

    template <typename Tuple, std::size_t... I>
    static void CallVoid(C *self, Tuple &args, std::index_sequence<I...>) {
        (self->*Method)(std::get<I>(args).Get()...);
    }

    template <typename Tuple, std::size_t... I>
    static R CallReturn(C *self, Tuple &args, std::index_sequence<I...>) {
        return (self->*Method)(std::get<I>(args).Get()...);
    }
};

template <typename C, typename R, typename... Args, R (C::*Method)(Args...) const>
struct GenericMethod<Method> {
    static void Call(asIScriptGeneric *gen) {
        const C *self = static_cast<const C *>(gen ? gen->GetObject() : nullptr);
        if (!self) {
            RaiseActiveException("Missing AngelScript generic method object.");
            return;
        }
        bool ok = true;
        auto args = MakeArgs(gen, ok, std::index_sequence_for<Args...>{});
        if (!ok) {
            RaiseActiveException("Failed to marshal AngelScript generic method arguments.");
            return;
        }

        if constexpr (std::is_void_v<R>) {
            CallVoid(self, args, std::index_sequence_for<Args...>{});
            if (!WriteBackTuple(args, gen, std::index_sequence_for<Args...>{})) {
                RaiseActiveException("Failed to marshal AngelScript generic method outputs.");
            }
        } else {
            R result = CallReturn(self, args, std::index_sequence_for<Args...>{});
            if (!WriteBackTuple(args, gen, std::index_sequence_for<Args...>{}) ||
                !SetGenericReturn(gen, result)) {
                RaiseActiveException("Failed to marshal AngelScript generic method result.");
            }
        }
    }

private:
    template <std::size_t... I>
    static std::tuple<GenericArg<Args>...> MakeArgs(asIScriptGeneric *gen,
                                                    bool &ok,
                                                    std::index_sequence<I...>) {
        return std::tuple<GenericArg<Args>...>{GenericArg<Args>(gen, static_cast<asUINT>(I), ok)...};
    }

    template <typename Tuple, std::size_t... I>
    static void CallVoid(const C *self, Tuple &args, std::index_sequence<I...>) {
        (self->*Method)(std::get<I>(args).Get()...);
    }

    template <typename Tuple, std::size_t... I>
    static R CallReturn(const C *self, Tuple &args, std::index_sequence<I...>) {
        return (self->*Method)(std::get<I>(args).Get()...);
    }
};

template <auto Function>
asSFuncPtr GenericFunctionPtr() {
    return asFUNCTION(GenericFunction<Function>::Call);
}

template <auto Method>
asSFuncPtr GenericMethodPtr() {
    return asFUNCTION(GenericMethod<Method>::Call);
}

template <auto Function>
struct GenericObjectFirstFunction;

template <typename C, typename R, typename... Args, R (*Function)(C *, Args...)>
struct GenericObjectFirstFunction<Function> {
    static void Call(asIScriptGeneric *gen) {
        C *self = static_cast<C *>(gen ? gen->GetObject() : nullptr);
        if (!self) {
            RaiseActiveException("Missing AngelScript generic object-first method object.");
            return;
        }
        bool ok = true;
        auto args = MakeArgs(gen, ok, std::index_sequence_for<Args...>{});
        if (!ok) {
            RaiseActiveException("Failed to marshal AngelScript generic object-first method arguments.");
            return;
        }

        if constexpr (std::is_void_v<R>) {
            CallVoid(self, args, std::index_sequence_for<Args...>{});
            if (!WriteBackTuple(args, gen, std::index_sequence_for<Args...>{})) {
                RaiseActiveException("Failed to marshal AngelScript generic object-first method outputs.");
            }
        } else {
            R result = CallReturn(self, args, std::index_sequence_for<Args...>{});
            if (!WriteBackTuple(args, gen, std::index_sequence_for<Args...>{}) ||
                !SetGenericReturn(gen, result)) {
                RaiseActiveException("Failed to marshal AngelScript generic object-first method result.");
            }
        }
    }

private:
    template <std::size_t... I>
    static std::tuple<GenericArg<Args>...> MakeArgs(asIScriptGeneric *gen,
                                                    bool &ok,
                                                    std::index_sequence<I...>) {
        return std::tuple<GenericArg<Args>...>{GenericArg<Args>(gen, static_cast<asUINT>(I), ok)...};
    }

    template <typename Tuple, std::size_t... I>
    static void CallVoid(C *self, Tuple &args, std::index_sequence<I...>) {
        Function(self, std::get<I>(args).Get()...);
    }

    template <typename Tuple, std::size_t... I>
    static R CallReturn(C *self, Tuple &args, std::index_sequence<I...>) {
        return Function(self, std::get<I>(args).Get()...);
    }
};

template <auto Function>
asSFuncPtr GenericObjectFirstFunctionPtr() {
    return asFUNCTION(GenericObjectFirstFunction<Function>::Call);
}

template <typename C, std::string C::*Member>
struct GenericStringFieldAccessor {
    static void Get(asIScriptGeneric *gen) {
        const C *self = static_cast<const C *>(gen ? gen->GetObject() : nullptr);
        if (!self || !SetReturnString(gen, self->*Member)) {
            RaiseActiveException("Failed to marshal AngelScript string property getter.");
        }
    }

    static void Set(asIScriptGeneric *gen) {
        C *self = static_cast<C *>(gen ? gen->GetObject() : nullptr);
        if (!self) {
            RaiseActiveException("Missing AngelScript string property object.");
            return;
        }

        bool ok = true;
        GenericArg<const std::string &> value(gen, 0, ok);
        if (!ok) {
            RaiseActiveException("Failed to marshal AngelScript string property setter.");
            return;
        }
        self->*Member = value.Get();
    }
};

template <typename C, std::string C::*Member>
asSFuncPtr GenericStringFieldGetterPtr() {
    asGENFUNC_t function = &GenericStringFieldAccessor<C, Member>::Get;
    return asFunctionPtr(function);
}

template <typename C, std::string C::*Member>
asSFuncPtr GenericStringFieldSetterPtr() {
    asGENFUNC_t function = &GenericStringFieldAccessor<C, Member>::Set;
    return asFunctionPtr(function);
}

} // namespace BML::ScriptStringInterop

#define BML_AS_GENERIC_FUNCTION(function) \
    (::BML::ScriptStringInterop::GenericFunctionPtr<function>())

#define BML_AS_GENERIC_METHOD(method) \
    (::BML::ScriptStringInterop::GenericMethodPtr<method>())

#define BML_AS_GENERIC_OBJECT_FIRST_FUNCTION(function) \
    (::BML::ScriptStringInterop::GenericObjectFirstFunctionPtr<function>())

#define BML_AS_STRING_FIELD_GETTER(type, member) \
    (::BML::ScriptStringInterop::GenericStringFieldGetterPtr<type, &type::member>())

#define BML_AS_STRING_FIELD_SETTER(type, member) \
    (::BML::ScriptStringInterop::GenericStringFieldSetterPtr<type, &type::member>())

#ifdef BML_SCRIPT_STRING_INTEROP_RESTORE_GETOBJECT
#pragma pop_macro("GetObject")
#undef BML_SCRIPT_STRING_INTEROP_RESTORE_GETOBJECT
#endif

#endif
