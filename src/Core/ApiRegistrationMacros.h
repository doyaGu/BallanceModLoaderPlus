#ifndef BML_CORE_API_REGISTRATION_MACROS_H
#define BML_CORE_API_REGISTRATION_MACROS_H

/**
 * @file ApiRegistrationMacros.h
 * @brief Macros for API registration with full metadata support
 *
 * This file provides consistent macros for registering APIs with the ApiRegistry.
 * All API registration should use these macros to ensure:
 * - Consistent naming convention
 * - Complete metadata (version, capabilities, threading model)
 * - Proper error handling with GuardResult
 * - Automatic ID lookup from bml_api_ids.h
 * - Type-safe function pointer casting
 * 
 * All macros now use RegisterApi() internally, ensuring complete metadata
 * is stored for every API, enabling runtime introspection and documentation.
 */

#include <type_traits>

#include "ApiRegistry.h"
#include "CoreErrors.h"
#include "bml_api_ids.h"
#include "bml_version.h"

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

        /**
         * @brief Helper to build and register API metadata
         */
        inline void RegisterApiWithMetadata(
            ApiRegistry &registry,
            const char *name,
            BML_ApiId id,
            void *pointer,
            uint64_t capabilities = 0,
            BML_ThreadingModel threading = BML_THREADING_FREE,
            const char *description = nullptr
        ) {
            ApiRegistry::ApiMetadata meta{};
            meta.name = name;
            meta.id = id;
            meta.pointer = pointer;
            meta.version_major = BML_API_VERSION_MAJOR;
            meta.version_minor = BML_API_VERSION_MINOR;
            meta.version_patch = BML_API_VERSION_PATCH;
            meta.capabilities = capabilities;
            meta.type = BML_API_TYPE_CORE;
            meta.threading = threading;
            meta.provider_mod = "BML";
            meta.description = description;
            registry.RegisterApi(meta);
        }
    } // namespace detail

#define BML_DETAIL_CONCAT_IMPL(a, b) a##b
#define BML_DETAIL_CONCAT(a, b) BML_DETAIL_CONCAT_IMPL(a, b)
#define BML_DETAIL_UNIQUE_NAME(prefix) BML_DETAIL_CONCAT(prefix, __COUNTER__)

    // =========================================================================
    // Basic Registration Macros (with full metadata)
    // =========================================================================

    /**
     * @brief Register a simple API function with full metadata (no error guard)
     *
     * Uses default capabilities (0), FREE threading model, and no description.
     */
#define BML_REGISTER_API(name, func)                                                    \
    do {                                                                               \
        using ::BML::Core::detail::IsValidFunctionPointer;                             \
        using BmlFnPtr = decltype(&func);                                              \
        static_assert(IsValidFunctionPointer<BmlFnPtr>,                                \
                      "BML_REGISTER_API expects a non-overloaded function symbol");    \
        ::BML::Core::detail::RegisterApiWithMetadata(                                  \
            registry, #name, BML_API_ID_##name,                                        \
            reinterpret_cast<void *>(+func));                                          \
    } while (0)

    /**
     * @brief Register a simple API with specific capabilities
     */
#define BML_REGISTER_API_WITH_CAPS(name, func, caps)                                   \
    do {                                                                               \
        using ::BML::Core::detail::IsValidFunctionPointer;                             \
        using BmlFnPtr = decltype(&func);                                              \
        static_assert(IsValidFunctionPointer<BmlFnPtr>,                                \
                      "BML_REGISTER_API_WITH_CAPS expects a non-overloaded function"); \
        ::BML::Core::detail::RegisterApiWithMetadata(                                  \
            registry, #name, BML_API_ID_##name,                                        \
            reinterpret_cast<void *>(+func), (caps));                                  \
    } while (0)

    /**
     * @brief Register a simple API with full customization
     */
#define BML_REGISTER_API_FULL(name, func, caps, threading, desc)                       \
    do {                                                                               \
        using ::BML::Core::detail::IsValidFunctionPointer;                             \
        using BmlFnPtr = decltype(&func);                                              \
        static_assert(IsValidFunctionPointer<BmlFnPtr>,                                \
                      "BML_REGISTER_API_FULL expects a non-overloaded function");      \
        ::BML::Core::detail::RegisterApiWithMetadata(                                  \
            registry, #name, BML_API_ID_##name,                                        \
            reinterpret_cast<void *>(+func), (caps), (threading), (desc));             \
    } while (0)

    // =========================================================================
    // Guarded Registration Macros (with full metadata)
    // =========================================================================

    /**
     * @brief Internal macro for guarded API registration with metadata
     */
#define BML_DETAIL_REGISTER_API_GUARDED_META(name, domain, func, caps, domain_tag, wrapper_type) \
    do {                                                                               \
        struct domain_tag {                                                            \
            static constexpr const char *Value() { return domain; }                    \
        };                                                                             \
        using wrapper_type = ::BML::Core::detail::GuardedWrapper<func, domain_tag>;    \
        ::BML::Core::detail::RegisterApiWithMetadata(                                  \
            registry, #name, BML_API_ID_##name,                                        \
            reinterpret_cast<void *>(wrapper_type::Get()), (caps));                    \
    } while (0)

    /**
     * @brief Register an API with error guard wrapper (default capabilities)
     */
#define BML_REGISTER_API_GUARDED(name, domain, func)                                   \
    BML_DETAIL_REGISTER_API_GUARDED_META(                                              \
        name, domain, func, 0,                                                         \
        BML_DETAIL_UNIQUE_NAME(_BmlDomainTag),                                         \
        BML_DETAIL_UNIQUE_NAME(_BmlGuardWrapper))

    /**
     * @brief Register an API with error guard and specific capabilities
     */
#define BML_REGISTER_API_GUARDED_WITH_CAPS(name, domain, func, caps)                   \
    BML_DETAIL_REGISTER_API_GUARDED_META(                                              \
        name, domain, func, caps,                                                      \
        BML_DETAIL_UNIQUE_NAME(_BmlDomainTag),                                         \
        BML_DETAIL_UNIQUE_NAME(_BmlGuardWrapper))

    /**
     * @brief Internal macro for void guarded API registration with metadata
     */
#define BML_DETAIL_REGISTER_API_VOID_GUARDED_META(name, domain, func, caps, domain_tag, wrapper) \
    do {                                                                               \
        struct domain_tag {                                                            \
            static constexpr const char *Value() { return domain; }                    \
        };                                                                             \
        using wrapper = ::BML::Core::detail::VoidGuardedWrapper<func, domain_tag>;     \
        ::BML::Core::detail::RegisterApiWithMetadata(                                  \
            registry, #name, BML_API_ID_##name,                                        \
            reinterpret_cast<void *>(wrapper::Get()), (caps));                         \
    } while (0)

    /**
     * @brief Register a void API with error guard (logs but doesn't return error)
     */
#define BML_REGISTER_API_VOID_GUARDED(name, domain, func)                              \
    BML_DETAIL_REGISTER_API_VOID_GUARDED_META(                                         \
        name, domain, func, 0,                                                         \
        BML_DETAIL_UNIQUE_NAME(_BmlDomainTag),                                         \
        BML_DETAIL_UNIQUE_NAME(_BmlVoidWrapper))

    /**
     * @brief Register a void API with error guard and specific capabilities
     */
#define BML_REGISTER_API_VOID_GUARDED_WITH_CAPS(name, domain, func, caps)              \
    BML_DETAIL_REGISTER_API_VOID_GUARDED_META(                                         \
        name, domain, func, caps,                                                      \
        BML_DETAIL_UNIQUE_NAME(_BmlDomainTag),                                         \
        BML_DETAIL_UNIQUE_NAME(_BmlVoidWrapper))

    // =========================================================================
    // Typed Registration Macro (with full metadata)
    // =========================================================================

    /**
     * @brief Internal macro for typed API registration with metadata
     */
#define BML_DETAIL_REGISTER_API_TYPED_META(name, signature, domain, func, caps,        \
    domain_tag, fn_ptr_alias, fn_ptr_value, wrapper_type) \
    do {                                                                               \
        struct domain_tag {                                                            \
            static constexpr const char *Value() { return domain; }                    \
        };                                                                             \
        using fn_ptr_alias = signature *;                                              \
        constexpr fn_ptr_alias fn_ptr_value = static_cast<fn_ptr_alias>(func);         \
        using wrapper_type = ::BML::Core::detail::GuardedWrapper<fn_ptr_value, domain_tag>; \
        ::BML::Core::detail::RegisterApiWithMetadata(                                  \
            registry, #name, BML_API_ID_##name,                                        \
            reinterpret_cast<void *>(wrapper_type::Get()), (caps));                    \
    } while (0)

    /**
     * @brief Register an API with explicit signature and error guard
     */
#define BML_REGISTER_API_TYPED(name, signature, domain, func)                          \
    BML_DETAIL_REGISTER_API_TYPED_META(                                                \
        name, signature, domain, func, 0,                                              \
        BML_DETAIL_UNIQUE_NAME(_BmlDomainTag),                                         \
        BML_DETAIL_UNIQUE_NAME(_BmlFnPtrAlias),                                        \
        BML_DETAIL_UNIQUE_NAME(_BmlFnPtrValue),                                        \
        BML_DETAIL_UNIQUE_NAME(_BmlTypedWrapper))

    /**
     * @brief Register an API with explicit signature, error guard, and capabilities
     */
#define BML_REGISTER_API_TYPED_WITH_CAPS(name, signature, domain, func, caps)          \
    BML_DETAIL_REGISTER_API_TYPED_META(                                                \
        name, signature, domain, func, caps,                                           \
        BML_DETAIL_UNIQUE_NAME(_BmlDomainTag),                                         \
        BML_DETAIL_UNIQUE_NAME(_BmlFnPtrAlias),                                        \
        BML_DETAIL_UNIQUE_NAME(_BmlFnPtrValue),                                        \
        BML_DETAIL_UNIQUE_NAME(_BmlTypedWrapper))

    // ============================================================================
    // Batch Registration Helper (with full metadata)
    // ============================================================================

    /**
     * @brief Extended API descriptor for batch registration with full metadata
     */
    struct ApiDescriptor {
        const char *name;
        void *pointer;
        BML_ApiId id;
        uint64_t capabilities;
        BML_ThreadingModel threading;
        const char *description;

        constexpr ApiDescriptor(
            const char *n, void *p, BML_ApiId i,
            uint64_t caps = 0,
            BML_ThreadingModel t = BML_THREADING_FREE,
            const char *desc = nullptr
        ) : name(n), pointer(p), id(i), capabilities(caps), threading(t), description(desc) {}
    };

    /**
     * @brief Register multiple APIs with full metadata at once
     *
     * Example:
     *   static const ApiDescriptor kApis[] = {
     *       {"bmlFoo", (void*)Impl_Foo, BML_API_ID_bmlFoo, BML_CAP_FOO},
     *       {"bmlBar", (void*)Impl_Bar, BML_API_ID_bmlBar, BML_CAP_BAR},
     *   };
     *   BML_REGISTER_API_TABLE(kApis);
     */
#define BML_REGISTER_API_TABLE(descriptors)                                            \
    for (const auto &desc : descriptors) {                                             \
        ::BML::Core::detail::RegisterApiWithMetadata(                                  \
            registry, desc.name, desc.id, desc.pointer,                                \
            desc.capabilities, desc.threading, desc.description);                      \
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
    auto &registry = ApiRegistry::Instance()

    /**
     * @brief Register a capability query API (returns BML_Result, takes caps struct)
     */
#define BML_REGISTER_CAPS_API(name, domain, func) \
    BML_REGISTER_API_GUARDED(name, domain, func)

    /**
     * @brief Register a capability query API with specific capabilities flag
     */
#define BML_REGISTER_CAPS_API_WITH_CAPS(name, domain, func, caps) \
    BML_REGISTER_API_GUARDED_WITH_CAPS(name, domain, func, caps)
} // namespace BML::Core

#endif // BML_CORE_API_REGISTRATION_MACROS_H
