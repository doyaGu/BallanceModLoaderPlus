#ifndef BML_CORE_API_REGISTRATION_MACROS_H
#define BML_CORE_API_REGISTRATION_MACROS_H

#include <type_traits>

#include "ApiRegistry.h"
#include "CoreErrors.h"
#include "KernelServices.h"

namespace BML::Core {
    namespace detail {
        template <typename T>
        struct IsFunctionPointer : std::false_type {};

        template <typename R, typename... Args>
        struct IsFunctionPointer<R (*)(Args...)> : std::true_type {};

        template <auto Func, typename DomainTag, typename ApiNameTag>
        struct GuardedWrapper {
            static_assert(sizeof(Func) == 0,
                          "BML_REGISTER_API_GUARDED requires a function returning BML_Result");
        };

        template <typename... Args, BML_Result (*Func)(Args...), typename DomainTag, typename ApiNameTag>
        struct GuardedWrapper<Func, DomainTag, ApiNameTag> {
            static BML_Result Invoke(Args... args) {
                return GuardRegisteredApiResult(DomainTag::Value(), ApiNameTag::Value(), [&]() -> BML_Result {
                    return Func(args...);
                });
            }

            static constexpr auto Get() -> BML_Result (*)(Args...) {
                return &GuardedWrapper::Invoke;
            }
        };

        template <auto Func, typename DomainTag, typename ApiNameTag>
        struct VoidGuardedWrapper {
            static_assert(sizeof(Func) == 0,
                          "BML_REGISTER_API_VOID_GUARDED requires a void function");
        };

        template <typename... Args, void (*Func)(Args...), typename DomainTag, typename ApiNameTag>
        struct VoidGuardedWrapper<Func, DomainTag, ApiNameTag> {
            static void Invoke(Args... args) {
                GuardVoid(DomainTag::Value(), ApiNameTag::Value(), [&]() {
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

        inline void RegisterApi(ApiRegistry &registry, const char *name, void *pointer) {
            registry.RegisterApi(name, pointer);
        }
    } // namespace detail

#define BML_DETAIL_CONCAT_IMPL(a, b) a##b
#define BML_DETAIL_CONCAT(a, b) BML_DETAIL_CONCAT_IMPL(a, b)
#define BML_DETAIL_UNIQUE_NAME(prefix) BML_DETAIL_CONCAT(prefix, __COUNTER__)

#define BML_REGISTER_API(name, func)                                                    \
    do {                                                                               \
        using ::BML::Core::detail::IsValidFunctionPointer;                             \
        using BmlFnPtr = decltype(&func);                                              \
        static_assert(IsValidFunctionPointer<BmlFnPtr>,                                \
                      "BML_REGISTER_API expects a non-overloaded function symbol");    \
        ::BML::Core::detail::RegisterApi(                                              \
            registry, #name, reinterpret_cast<void *>(+func));                         \
    } while (0)

#define BML_DETAIL_REGISTER_API_GUARDED(name, domain, func, domain_tag, api_name_tag, wrapper_type) \
    do {                                                                               \
        struct domain_tag {                                                            \
            static constexpr const char *Value() { return domain; }                    \
        };                                                                             \
        struct api_name_tag {                                                          \
            static constexpr const char *Value() { return #name; }                     \
        };                                                                             \
        using wrapper_type = ::BML::Core::detail::GuardedWrapper<func, domain_tag, api_name_tag>; \
        ::BML::Core::detail::RegisterApi(                                              \
            registry, #name, reinterpret_cast<void *>(wrapper_type::Get()));           \
    } while (0)

#define BML_REGISTER_API_GUARDED(name, domain, func)                                   \
    BML_DETAIL_REGISTER_API_GUARDED(                                                   \
        name, domain, func,                                                            \
        BML_DETAIL_UNIQUE_NAME(_BmlDomainTag),                                         \
        BML_DETAIL_UNIQUE_NAME(_BmlApiNameTag),                                        \
        BML_DETAIL_UNIQUE_NAME(_BmlGuardWrapper))

#define BML_DETAIL_REGISTER_API_VOID_GUARDED(name, domain, func, domain_tag, api_name_tag, wrapper) \
    do {                                                                               \
        struct domain_tag {                                                            \
            static constexpr const char *Value() { return domain; }                    \
        };                                                                             \
        struct api_name_tag {                                                          \
            static constexpr const char *Value() { return #name; }                     \
        };                                                                             \
        using wrapper = ::BML::Core::detail::VoidGuardedWrapper<func, domain_tag, api_name_tag>; \
        ::BML::Core::detail::RegisterApi(                                              \
            registry, #name, reinterpret_cast<void *>(wrapper::Get()));                \
    } while (0)

#define BML_REGISTER_API_VOID_GUARDED(name, domain, func)                              \
    BML_DETAIL_REGISTER_API_VOID_GUARDED(                                              \
        name, domain, func,                                                            \
        BML_DETAIL_UNIQUE_NAME(_BmlDomainTag),                                         \
        BML_DETAIL_UNIQUE_NAME(_BmlApiNameTag),                                        \
        BML_DETAIL_UNIQUE_NAME(_BmlVoidWrapper))

#define BML_BEGIN_API_REGISTRATION() \
    auto &registry = *Kernel().api_registry

} // namespace BML::Core

#endif // BML_CORE_API_REGISTRATION_MACROS_H
