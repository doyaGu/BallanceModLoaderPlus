/**
 * @file Macros.h
 * @brief Hook macros for BML_Render module
 */

#ifndef BML_RENDER_MACROS_H
#define BML_RENDER_MACROS_H

#define CP_HOOK_CLASS_NAME(Class) \
    Class##Hook

#define CP_CLASS_VTABLE_NAME(Class) \
    Class##VTable

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

#define CP_CALL_METHOD_PTR(Ptr, Func, ...) \
    (Ptr->*Func)(__VA_ARGS__)

#define CP_CALL_METHOD_ORIG(Name, ...) \
    (this->*CP_FUNC_ORIG_PTR_NAME(Name))(__VA_ARGS__)

// MinHook-based member function hook macros
#define CP_ADD_METHOD_HOOK(Name, Base, Offset) \
    { CP_FUNC_TARGET_PTR_NAME(Name) = ForceReinterpretCast<CP_FUNC_TYPE_NAME(Name)>(Base, Offset); } \
    if ((MH_CreateHook(*reinterpret_cast<LPVOID *>(&CP_FUNC_TARGET_PTR_NAME(Name)), \
                       *reinterpret_cast<LPVOID *>(&CP_FUNC_PTR_NAME(Name)), \
                       reinterpret_cast<LPVOID *>(&CP_FUNC_ORIG_PTR_NAME(Name))) != MH_OK || \
        MH_EnableHook(*reinterpret_cast<LPVOID *>(&CP_FUNC_TARGET_PTR_NAME(Name))) != MH_OK)) \
            return false;

#define CP_REMOVE_METHOD_HOOK(Name) \
    MH_DisableHook(*reinterpret_cast<void **>(&CP_FUNC_TARGET_PTR_NAME(Name))); \
    MH_RemoveHook(*reinterpret_cast<void **>(&CP_FUNC_TARGET_PTR_NAME(Name)));

#endif // BML_RENDER_MACROS_H
