/**
 * @file Macros.h
 * @brief VTable hooking macros for BML_Input module
 * 
 * Self-contained macros for virtual method hooking without BMLPlus dependencies.
 */

#ifndef BML_INPUT_MACROS_H
#define BML_INPUT_MACROS_H

#define CP_HOOK_CLASS_NAME(Class) \
    Class##Hook

#define CP_CLASS_VTABLE_NAME(Class) \
    Class##VTable

#define CP_FUNC_HOOK_NAME(Name) \
    Name##Hook

#define CP_FUNC_TYPE_NAME(Name) \
    Name##Func

#define CP_DECLARE_METHOD_PTR(Class, Ret, Name, Args) \
    typedef Ret(Class::*CP_FUNC_TYPE_NAME(Name)) Args; \
    CP_FUNC_TYPE_NAME(Name) Name

#define CP_DECLARE_METHOD_HOOK(Ret, Name, Args) \
    Ret CP_FUNC_HOOK_NAME(Name) Args

#define CP_CALL_METHOD_PTR(Ptr, Func, ...) \
    (Ptr->*Func)(__VA_ARGS__)

#endif // BML_INPUT_MACROS_H
