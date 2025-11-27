#ifndef BML_CORE_API_REGISTRATION_MACROS_H
#define BML_CORE_API_REGISTRATION_MACROS_H

/**
 * @file ApiRegistrationMacros.h
 * @brief Unified macros for API registration
 *
 * This file provides consistent macros for registering APIs with the ApiRegistry.
 * All API registration should use these macros to ensure:
 * - Consistent naming convention
 * - Proper error handling with GuardResult
 * - Automatic ID lookup from bml_api_ids.h
 * - Type-safe function pointer casting
 */

#include <type_traits>

#include "ApiRegistry.h"
#include "CoreErrors.h"
#include "bml_api_ids.h"

namespace BML::Core {

namespace detail {

template <typename T>
struct IsFunctionPointer : std::false_type {};

template <typename R, typename... Args>
struct IsFunctionPointer<R (*)(Args...)> : std::true_type {};

template <auto Func, typename DomainTag>
struct GuardedWrapper {
    static_assert(sizeof(Func) == 0,
                  "BML_REGISTER_API_GUARDED requires a function returning BML_Result");
};

template <typename... Args, BML_Result (*Func)(Args...), typename DomainTag>
struct GuardedWrapper<Func, DomainTag> {
    static BML_Result Invoke(Args... args) {
        return GuardResult(DomainTag::Value(), [&]() -> BML_Result {
            return Func(args...);
        });
    }

    static constexpr auto Get() -> BML_Result (*)(Args...) {
        return &GuardedWrapper::Invoke;
    }
};

template <auto Func, typename DomainTag>
struct VoidGuardedWrapper {
    static_assert(sizeof(Func) == 0,
                  "BML_REGISTER_API_VOID_GUARDED requires a void function");
};

template <typename... Args, void (*Func)(Args...), typename DomainTag>
struct VoidGuardedWrapper<Func, DomainTag> {
    static void Invoke(Args... args) {
        GuardVoid(DomainTag::Value(), [&]() {
            Func(args...);
        });
    }

    static constexpr auto Get() -> void (*)(Args...) {
        return &VoidGuardedWrapper::Invoke;
    }
};

template <typename FnPtr>
constexpr bool IsValidFunctionPointer =
    IsFunctionPointer<std::remove_reference_t<FnPtr>>::value;

} // namespace detail

#define BML_DETAIL_CONCAT_IMPL(a, b) a##b
#define BML_DETAIL_CONCAT(a, b) BML_DETAIL_CONCAT_IMPL(a, b)
#define BML_DETAIL_UNIQUE_NAME(prefix) BML_DETAIL_CONCAT(prefix, __COUNTER__)

// =========================================================================
// Basic Registration Macros
// =========================================================================

/**
 * @brief Register a simple API function (no error guard)
 */
#define BML_REGISTER_API(name, func)                                                    \
    do {                                                                               \
        using ::BML::Core::detail::IsValidFunctionPointer;                             \
        using BmlFnPtr = decltype(&func);                                              \
        static_assert(IsValidFunctionPointer<BmlFnPtr>,                                \
                      "BML_REGISTER_API expects a non-overloaded function symbol");  \
        registry.Register(#name, reinterpret_cast<void *>(+func), BML_API_ID_##name);  \
    } while (0)

// =========================================================================
// Guarded Registration Macros
// =========================================================================

#define BML_DETAIL_REGISTER_API_GUARDED(name, domain, func, domain_tag, wrapper_type)   \
    do {                                                                               \
        struct domain_tag {                                                            \
            static constexpr const char *Value() { return domain; }                    \
        };                                                                             \
        using wrapper_type = ::BML::Core::detail::GuardedWrapper<                      \
            func, domain_tag>;                                                         \
        registry.Register(#name,                                                       \
                          reinterpret_cast<void *>(wrapper_type::Get()),               \
                          BML_API_ID_##name);                                         \
    } while (0)

/**
 * @brief Register an API with error guard wrapper
 */
#define BML_REGISTER_API_GUARDED(name, domain, func)                                    \
    BML_DETAIL_REGISTER_API_GUARDED(                                                   \
        name, domain, func,                                                            \
        BML_DETAIL_UNIQUE_NAME(_BmlDomainTag),                                         \
        BML_DETAIL_UNIQUE_NAME(_BmlGuardWrapper))

#define BML_DETAIL_REGISTER_API_VOID_GUARDED(name, domain, func, domain_tag, wrapper)   \
    do {                                                                               \
        struct domain_tag {                                                            \
            static constexpr const char *Value() { return domain; }                    \
        };                                                                             \
        using wrapper = ::BML::Core::detail::VoidGuardedWrapper<                        \
            func, domain_tag>;                                                         \
        registry.Register(#name,                                                       \
                          reinterpret_cast<void *>(wrapper::Get()),                    \
                          BML_API_ID_##name);                                         \
    } while (0)

/**
 * @brief Register a void API with error guard (logs but doesn't return error)
 */
#define BML_REGISTER_API_VOID_GUARDED(name, domain, func)                               \
    BML_DETAIL_REGISTER_API_VOID_GUARDED(                                              \
        name, domain, func,                                                            \
        BML_DETAIL_UNIQUE_NAME(_BmlDomainTag),                                         \
        BML_DETAIL_UNIQUE_NAME(_BmlVoidWrapper))

// =========================================================================
// Typed Registration Macro
// =========================================================================

#define BML_DETAIL_REGISTER_API_TYPED(name, signature, domain, func, domain_tag,        \
                                      fn_ptr_alias, fn_ptr_value, wrapper_type)        \
    do {                                                                               \
        struct domain_tag {                                                            \
            static constexpr const char *Value() { return domain; }                    \
        };                                                                             \
        using fn_ptr_alias = signature *;                                              \
        constexpr fn_ptr_alias fn_ptr_value = static_cast<fn_ptr_alias>(func);         \
        using wrapper_type = ::BML::Core::detail::GuardedWrapper<                      \
            fn_ptr_value, domain_tag>;                                                 \
        registry.Register(#name,                                                       \
                          reinterpret_cast<void *>(wrapper_type::Get()),               \
                          BML_API_ID_##name);                                         \
    } while (0)

/**
 * @brief Register an API with explicit signature and error guard
 */
#define BML_REGISTER_API_TYPED(name, signature, domain, func)                          \
    BML_DETAIL_REGISTER_API_TYPED(                                                     \
        name, signature, domain, func,                                                 \
        BML_DETAIL_UNIQUE_NAME(_BmlDomainTag),                                         \
        BML_DETAIL_UNIQUE_NAME(_BmlFnPtrAlias),                                        \
        BML_DETAIL_UNIQUE_NAME(_BmlFnPtrValue),                                        \
        BML_DETAIL_UNIQUE_NAME(_BmlTypedWrapper))

// ============================================================================
// Batch Registration Helper
// ============================================================================

/**
 * @brief API descriptor for batch registration
 */
struct SimpleApiDescriptor {
    const char* name;
    void* pointer;
    BML_ApiId id;
};

/**
 * @brief Register multiple simple APIs at once
 * 
 * Example:
 *   static const SimpleApiDescriptor kApis[] = {
 *       {"bmlFoo", (void*)Impl_Foo, BML_API_ID_bmlFoo},
 *       {"bmlBar", (void*)Impl_Bar, BML_API_ID_bmlBar},
 *   };
 *   BML_REGISTER_API_BATCH(kApis);
 */
#define BML_REGISTER_API_BATCH(descriptors) \
    for (const auto& desc : descriptors) { \
        registry.Register(desc.name, desc.pointer, desc.id); \
    }

// ============================================================================
// Common Patterns
// ============================================================================

/**
 * @brief Start API registration block
 * 
 * Creates local 'registry' reference for use in registration macros.
 */
#define BML_BEGIN_API_REGISTRATION() \
    auto& registry = ApiRegistry::Instance()

/**
 * @brief Register a capability query API (returns BML_Result, takes caps struct)
 */
#define BML_REGISTER_CAPS_API(name, domain, func) \
    BML_REGISTER_API_GUARDED(name, domain, func)

} // namespace BML::Core

#endif // BML_CORE_API_REGISTRATION_MACROS_H
