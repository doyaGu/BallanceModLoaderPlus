#ifndef BML_MACROS_H
#define BML_MACROS_H

#define CP_HOOK_CLASS_NAME(Class) \
    Class##Hook

#define CP_HOOK_CLASS(Class) \
    class CP_HOOK_CLASS_NAME(Class) : public Class

#define CP_CLASS_VTABLE_NAME(Class) \
    Class##VTable

#define CP_HOOK_METHOD_NAME(Class, Name) \
    CP_HOOK_CLASS_NAME(Class)::Name##Hook

#define CP_FUNC_HOOK_NAME(Name) \
    Name##Hook

#define CP_FUNC_TYPE_NAME(Name) \
    Name##Func

#define CP_FUNC_PTR_NAME(Name) \
    s_##Name##Func

#define CP_FUNC_ORIG_PTR_NAME(Name) \
    s_##Name##FuncOrig

#define CP_FUNC_TARGET_PTR_NAME(Name) \
    s_##Name##FuncTarget

#define CP_DECLARE_FUNCTION_PTRS(Ret, Name, Args) \
    typedef Ret(*CP_FUNC_TYPE_NAME(Name)) Args; \
    extern CP_FUNC_TYPE_NAME(Name) CP_FUNC_PTR_NAME(Name); \
    extern CP_FUNC_TYPE_NAME(Name) CP_FUNC_ORIG_PTR_NAME(Name); \
    extern CP_FUNC_TYPE_NAME(Name) CP_FUNC_TARGET_PTR_NAME(Name)

#define CP_DEFINE_FUNCTION_PTRS(Name) \
    CP_FUNC_TYPE_NAME(Name) CP_FUNC_PTR_NAME(Name) = reinterpret_cast<CP_FUNC_TYPE_NAME(Name)>(&CP_FUNC_HOOK_NAME(Name)); \
    CP_FUNC_TYPE_NAME(Name) CP_FUNC_ORIG_PTR_NAME(Name) = nullptr; \
    CP_FUNC_TYPE_NAME(Name) CP_FUNC_TARGET_PTR_NAME(Name) = nullptr;

#define CP_DECLARE_METHOD_PTR(Class, Ret, Name, Args) \
    typedef Ret(Class::*CP_FUNC_TYPE_NAME(Name)) Args; \
    CP_FUNC_TYPE_NAME(Name) Name

#define CP_DECLARE_METHOD_HOOK(Ret, Name, Args) \
    Ret CP_FUNC_HOOK_NAME(Name) Args

#define CP_DECLARE_METHOD_PTRS(Class, Ret, Name, Args) \
    typedef Ret(Class::*CP_FUNC_TYPE_NAME(Name)) Args; \
    static CP_FUNC_TYPE_NAME(Name) CP_FUNC_PTR_NAME(Name); \
    static CP_FUNC_TYPE_NAME(Name) CP_FUNC_ORIG_PTR_NAME(Name); \
    static CP_FUNC_TYPE_NAME(Name) CP_FUNC_TARGET_PTR_NAME(Name)

#define CP_DEFINE_METHOD_PTRS(Class, Name) \
    Class::CP_FUNC_TYPE_NAME(Name) Class::CP_FUNC_PTR_NAME(Name) = \
        reinterpret_cast<Class::CP_FUNC_TYPE_NAME(Name)>(&Class::Name); \
    Class::CP_FUNC_TYPE_NAME(Name) Class::CP_FUNC_ORIG_PTR_NAME(Name) = nullptr; \
    Class::CP_FUNC_TYPE_NAME(Name) Class::CP_FUNC_TARGET_PTR_NAME(Name) = nullptr;

#define CP_DEFINE_METHOD_HOOK_PTRS(Class, Name) \
    CP_HOOK_CLASS_NAME(Class)::CP_FUNC_TYPE_NAME(Name) CP_HOOK_CLASS_NAME(Class)::CP_FUNC_PTR_NAME(Name) = \
        reinterpret_cast<CP_HOOK_CLASS_NAME(Class)::CP_FUNC_TYPE_NAME(Name)>(&CP_HOOK_CLASS_NAME(Class)::CP_FUNC_HOOK_NAME(Name)); \
    CP_HOOK_CLASS_NAME(Class)::CP_FUNC_TYPE_NAME(Name) CP_HOOK_CLASS_NAME(Class)::CP_FUNC_ORIG_PTR_NAME(Name) = nullptr; \
    CP_HOOK_CLASS_NAME(Class)::CP_FUNC_TYPE_NAME(Name) CP_HOOK_CLASS_NAME(Class)::CP_FUNC_TARGET_PTR_NAME(Name) = nullptr;

#define CP_CALL_FUNCTION(Func, ...) \
    (*Func)(__VA_ARGS__)

#define CP_CALL_FUNCTION_ORIG(Name, ...) \
    (*CP_FUNC_ORIG_PTR_NAME(Name))(__VA_ARGS__)

#define CP_CALL_METHOD(Ref, Func, ...) \
    ((&Ref)->*Func)(__VA_ARGS__)

#define CP_CALL_METHOD_PTR(Ptr, Func, ...) \
    (Ptr->*Func)(__VA_ARGS__)

#define CP_CALL_METHOD_ORIG(Name, ...) \
    (this->*CP_FUNC_ORIG_PTR_NAME(Name))(__VA_ARGS__)

#define CP_CALL_METHOD_ORIG_FORCE(Class, Name, ...) \
    (((Class *)this)->*CP_FUNC_ORIG_PTR_NAME(Name))(__VA_ARGS__)

#endif // BML_MACROS_H
