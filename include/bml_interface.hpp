#ifndef BML_INTERFACE_HPP
#define BML_INTERFACE_HPP

#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "bml_builtin_interfaces.h"
#include "bml_interface.h"

namespace bml {
    inline BML_Result RegisterInterface(PFN_BML_InterfaceRegister reg,
                                        const BML_InterfaceDesc *desc) {
        if (!desc || !reg) {
            return BML_RESULT_NOT_SUPPORTED;
        }
        return reg(desc);
    }

    inline BML_Result UnregisterInterface(PFN_BML_InterfaceUnregister unreg,
                                          const char *interfaceId) {
        if (!interfaceId || !unreg) {
            return BML_RESULT_NOT_SUPPORTED;
        }
        return unreg(interfaceId);
    }

    template <typename T>
    BML_InterfaceDesc MakeInterfaceDesc(const char *interfaceId,
                                        const T *implementation,
                                        uint16_t abiMajor,
                                        uint16_t abiMinor = 0,
                                        uint16_t abiPatch = 0,
                                        uint64_t flags = 0,
                                        uint64_t capabilities = 0) {
        BML_InterfaceDesc desc = BML_INTERFACE_DESC_INIT;
        desc.interface_id = interfaceId;
        desc.abi_version = bmlMakeVersion(abiMajor, abiMinor, abiPatch);
        desc.implementation = implementation;
        desc.implementation_size = sizeof(T);
        desc.flags = flags;
        desc.capabilities = capabilities;
        return desc;
    }

    /**
     * @brief Fluent builder for BML_InterfaceDesc.
     *
     *   auto desc = bml::InterfaceDescBuilder(&myVtable, "com.example.foo", 1, 0)
     *       .Capabilities(MY_CAP)
     *       .Description("My interface")
     *       .Category("rendering")
     *       .Metadata({{"vendor.url", "https://example.com"}})
     *       .Build();
     */
    class InterfaceDescBuilder {
    public:
        template <typename T>
        InterfaceDescBuilder(const T *impl, const char *id,
                             uint16_t major, uint16_t minor = 0, uint16_t patch = 0) {
            m_Desc = BML_INTERFACE_DESC_INIT;
            m_Desc.interface_id = id;
            m_Desc.abi_version = bmlMakeVersion(major, minor, patch);
            m_Desc.implementation = impl;
            m_Desc.implementation_size = sizeof(T);
        }

        InterfaceDescBuilder &Flags(uint64_t f) { m_Desc.flags = f; return *this; }
        InterfaceDescBuilder &Capabilities(uint64_t c) { m_Desc.capabilities = c; return *this; }
        InterfaceDescBuilder &Description(const char *d) { m_Desc.description = d; return *this; }
        InterfaceDescBuilder &Category(const char *c) { m_Desc.category = c; return *this; }
        InterfaceDescBuilder &SupersededBy(const char *id) { m_Desc.superseded_by = id; return *this; }

        InterfaceDescBuilder &Metadata(std::initializer_list<BML_InterfaceMetadata> entries) {
            m_Metadata.assign(entries.begin(), entries.end());
            m_Desc.metadata = m_Metadata.data();
            m_Desc.metadata_count = static_cast<uint32_t>(m_Metadata.size());
            return *this;
        }

        const BML_InterfaceDesc &Build() { return m_Desc; }

    private:
        BML_InterfaceDesc m_Desc{};
        std::vector<BML_InterfaceMetadata> m_Metadata;
    };

    template <typename T>
    class InterfaceLease {
    public:
        InterfaceLease() = default;

        InterfaceLease(const T *implementation, BML_InterfaceLease lease)
            : m_Implementation(implementation), m_Lease(lease) {
        }

        InterfaceLease(const InterfaceLease &) = delete;
        InterfaceLease &operator=(const InterfaceLease &) = delete;

        InterfaceLease(InterfaceLease &&other) noexcept
            : m_Implementation(other.m_Implementation), m_Lease(other.m_Lease) {
            other.m_Implementation = nullptr;
            other.m_Lease = nullptr;
        }

        InterfaceLease &operator=(InterfaceLease &&other) noexcept {
            if (this != &other) {
                Reset();
                m_Implementation = other.m_Implementation;
                m_Lease = other.m_Lease;
                other.m_Implementation = nullptr;
                other.m_Lease = nullptr;
            }
            return *this;
        }

        ~InterfaceLease() {
            Reset();
        }

        const T *Get() const noexcept { return m_Implementation; }
        const T *operator->() const noexcept { return m_Implementation; }
        explicit operator bool() const noexcept { return m_Implementation != nullptr; }

        void Reset() {
            if (m_Lease && bmlInterfaceRelease) {
                bmlInterfaceRelease(m_Lease);
            }
            m_Implementation = nullptr;
            m_Lease = nullptr;
        }

    private:
        const T *m_Implementation = nullptr;
        BML_InterfaceLease m_Lease = nullptr;
    };

    class PublishedInterface {
    public:
        PublishedInterface() = default;

        PublishedInterface(PFN_BML_InterfaceRegister reg,
                           PFN_BML_InterfaceUnregister unreg,
                           const BML_InterfaceDesc &desc) {
            if (!reg || !unreg || !desc.interface_id) {
                return;
            }

            if (reg(&desc) != BML_RESULT_OK) {
                return;
            }

            m_InterfaceId = desc.interface_id;
            m_Unregister = unreg;
            m_Valid = true;
        }

        ~PublishedInterface() {
            (void) Reset();
        }

        PublishedInterface(const PublishedInterface &) = delete;
        PublishedInterface &operator=(const PublishedInterface &) = delete;

        PublishedInterface(PublishedInterface &&other) noexcept
            : m_InterfaceId(std::move(other.m_InterfaceId)),
              m_Unregister(other.m_Unregister),
              m_Valid(other.m_Valid) {
            other.m_Unregister = nullptr;
            other.m_Valid = false;
        }

        PublishedInterface &operator=(PublishedInterface &&other) noexcept {
            if (this != &other) {
                (void) Reset();
                m_InterfaceId = std::move(other.m_InterfaceId);
                m_Unregister = other.m_Unregister;
                m_Valid = other.m_Valid;
                other.m_Unregister = nullptr;
                other.m_Valid = false;
            }
            return *this;
        }

        explicit operator bool() const noexcept { return m_Valid; }

        BML_Result Reset() {
            if (!m_Valid) {
                return BML_RESULT_OK;
            }
            if (!m_Unregister || m_InterfaceId.empty()) {
                return BML_RESULT_NOT_SUPPORTED;
            }

            const BML_Result result = m_Unregister(m_InterfaceId.c_str());
            if (result == BML_RESULT_OK) {
                m_InterfaceId.clear();
                m_Unregister = nullptr;
                m_Valid = false;
            }
            return result;
        }

    private:
        std::string m_InterfaceId;
        PFN_BML_InterfaceUnregister m_Unregister = nullptr;
        bool m_Valid = false;
    };

    class ContributionLease {
    public:
        ContributionLease() = default;

        ContributionLease(const BML_HostRuntimeInterface *hostRuntime,
                          BML_InterfaceRegistration registration)
            : m_HostRuntime(hostRuntime),
              m_Registration(registration) {
        }

        ~ContributionLease() {
            (void) Reset();
        }

        ContributionLease(const ContributionLease &) = delete;
        ContributionLease &operator=(const ContributionLease &) = delete;

        ContributionLease(ContributionLease &&other) noexcept
            : m_HostRuntime(other.m_HostRuntime),
              m_Registration(other.m_Registration) {
            other.m_HostRuntime = nullptr;
            other.m_Registration = nullptr;
        }

        ContributionLease &operator=(ContributionLease &&other) noexcept {
            if (this != &other) {
                (void) Reset();
                m_HostRuntime = other.m_HostRuntime;
                m_Registration = other.m_Registration;
                other.m_HostRuntime = nullptr;
                other.m_Registration = nullptr;
            }
            return *this;
        }

        explicit operator bool() const noexcept { return m_Registration != nullptr; }

        BML_Result Reset() {
            if (!m_Registration) {
                return BML_RESULT_OK;
            }
            if (!m_HostRuntime || !m_HostRuntime->UnregisterContribution) {
                return BML_RESULT_NOT_SUPPORTED;
            }

            const BML_Result result = m_HostRuntime->UnregisterContribution(m_Registration);
            if (result == BML_RESULT_OK) {
                m_HostRuntime = nullptr;
                m_Registration = nullptr;
            }
            return result;
        }

    private:
        const BML_HostRuntimeInterface *m_HostRuntime = nullptr;
        BML_InterfaceRegistration m_Registration = nullptr;
    };

    // ========================================================================
    // Interface Reflection (via BML_CoreDiagnosticInterface)
    // ========================================================================

    inline bool InterfaceExists(const BML_CoreDiagnosticInterface *diag,
                                const char *interfaceId) {
        if (!diag || !diag->InterfaceExists || !interfaceId) return false;
        return diag->InterfaceExists(interfaceId) != BML_FALSE;
    }

    inline bool IsInterfaceCompatible(const BML_CoreDiagnosticInterface *diag,
                                      const char *interfaceId,
                                      uint16_t reqMajor,
                                      uint16_t reqMinor = 0,
                                      uint16_t reqPatch = 0) {
        if (!diag || !diag->IsInterfaceCompatible || !interfaceId) return false;
        BML_Version required = bmlMakeVersion(reqMajor, reqMinor, reqPatch);
        return diag->IsInterfaceCompatible(interfaceId, &required) != BML_FALSE;
    }

    inline std::optional<BML_InterfaceRuntimeDesc> GetInterfaceDescriptor(
        const BML_CoreDiagnosticInterface *diag,
        const char *interfaceId) {
        if (!diag || !diag->GetInterfaceDescriptor || !interfaceId) return std::nullopt;
        BML_InterfaceRuntimeDesc desc = BML_INTERFACE_RUNTIME_DESC_INIT;
        if (diag->GetInterfaceDescriptor(interfaceId, &desc) != BML_FALSE) {
            return desc;
        }
        return std::nullopt;
    }

    inline uint32_t GetInterfaceCount(const BML_CoreDiagnosticInterface *diag) {
        if (!diag || !diag->GetInterfaceCount) return 0;
        return diag->GetInterfaceCount();
    }

    inline uint32_t GetInterfaceLeaseCount(const BML_CoreDiagnosticInterface *diag,
                                            const char *interfaceId) {
        if (!diag || !diag->GetLeaseCount || !interfaceId) return 0;
        return diag->GetLeaseCount(interfaceId);
    }

    // -- Lambda-friendly enumeration ------------------------------------------

    namespace detail {
        template <typename Fn>
        void EnumerateWithLambda(
            void (*cEnumFn)(PFN_BML_InterfaceRuntimeEnumerator, void *, uint64_t),
            Fn &&fn, uint64_t flags) {
            if (!cEnumFn) return;
            auto trampoline = [](const BML_InterfaceRuntimeDesc *desc, void *ctx) {
                (*static_cast<std::remove_reference_t<Fn> *>(ctx))(*desc);
            };
            cEnumFn(trampoline, &fn, flags);
        }

        template <typename Fn>
        void EnumerateProviderWithLambda(
            void (*cEnumFn)(const char *, PFN_BML_InterfaceRuntimeEnumerator, void *),
            const char *id, Fn &&fn) {
            if (!cEnumFn || !id) return;
            auto trampoline = [](const BML_InterfaceRuntimeDesc *desc, void *ctx) {
                (*static_cast<std::remove_reference_t<Fn> *>(ctx))(*desc);
            };
            cEnumFn(id, trampoline, &fn);
        }

        template <typename Fn>
        void EnumerateCapWithLambda(
            void (*cEnumFn)(uint64_t, PFN_BML_InterfaceRuntimeEnumerator, void *),
            uint64_t caps, Fn &&fn) {
            if (!cEnumFn) return;
            auto trampoline = [](const BML_InterfaceRuntimeDesc *desc, void *ctx) {
                (*static_cast<std::remove_reference_t<Fn> *>(ctx))(*desc);
            };
            cEnumFn(caps, trampoline, &fn);
        }
    } // namespace detail

    /**
     * @brief Enumerate interfaces matching flag mask via lambda.
     *
     *   bml::ForEachInterface(diag, [](const BML_InterfaceRuntimeDesc &d) {
     *       printf("%s by %s\n", d.interface_id, d.provider_id);
     *   });
     */
    template <typename Fn>
    void ForEachInterface(const BML_CoreDiagnosticInterface *diag, Fn &&fn,
                          uint64_t requiredFlags = 0) {
        if (!diag || !diag->EnumerateInterfaces) return;
        detail::EnumerateWithLambda(diag->EnumerateInterfaces,
                                    std::forward<Fn>(fn), requiredFlags);
    }

    template <typename Fn>
    void ForEachInterfaceByProvider(const BML_CoreDiagnosticInterface *diag,
                                    const char *providerId, Fn &&fn) {
        if (!diag || !diag->EnumerateByProvider) return;
        detail::EnumerateProviderWithLambda(diag->EnumerateByProvider,
                                            providerId, std::forward<Fn>(fn));
    }

    template <typename Fn>
    void ForEachInterfaceByCapability(const BML_CoreDiagnosticInterface *diag,
                                      uint64_t requiredCaps, Fn &&fn) {
        if (!diag || !diag->EnumerateByCapability) return;
        detail::EnumerateCapWithLambda(diag->EnumerateByCapability,
                                       requiredCaps, std::forward<Fn>(fn));
    }

    // -- Collection returns ---------------------------------------------------

    inline std::vector<BML_InterfaceRuntimeDesc> ListInterfaces(
        const BML_CoreDiagnosticInterface *diag, uint64_t requiredFlags = 0) {
        std::vector<BML_InterfaceRuntimeDesc> result;
        ForEachInterface(diag, [&](const BML_InterfaceRuntimeDesc &d) {
            result.push_back(d);
        }, requiredFlags);
        return result;
    }

    inline std::vector<BML_InterfaceRuntimeDesc> ListInterfacesByProvider(
        const BML_CoreDiagnosticInterface *diag, const char *providerId) {
        std::vector<BML_InterfaceRuntimeDesc> result;
        ForEachInterfaceByProvider(diag, providerId, [&](const BML_InterfaceRuntimeDesc &d) {
            result.push_back(d);
        });
        return result;
    }

    inline std::vector<BML_InterfaceRuntimeDesc> ListInterfacesByCapability(
        const BML_CoreDiagnosticInterface *diag, uint64_t requiredCaps) {
        std::vector<BML_InterfaceRuntimeDesc> result;
        ForEachInterfaceByCapability(diag, requiredCaps, [&](const BML_InterfaceRuntimeDesc &d) {
            result.push_back(d);
        });
        return result;
    }

    // -- Predicate search -----------------------------------------------------

    /**
     * @brief Find the first interface matching a predicate.
     *
     *   auto desc = bml::FindInterface(diag, [](const BML_InterfaceRuntimeDesc &d) {
     *       return d.capabilities & MY_CAP_FLAG;
     *   });
     */
    template <typename Pred>
    std::optional<BML_InterfaceRuntimeDesc> FindInterface(
        const BML_CoreDiagnosticInterface *diag, Pred &&pred, uint64_t requiredFlags = 0) {
        std::optional<BML_InterfaceRuntimeDesc> found;
        ForEachInterface(diag, [&](const BML_InterfaceRuntimeDesc &d) {
            if (!found && pred(d)) {
                found = d;
            }
        }, requiredFlags);
        return found;
    }

    // -- Error handling (via diagnostic interface) ----------------------------

    inline std::optional<BML_ErrorInfo> GetLastError(const BML_CoreDiagnosticInterface *diag) {
        if (!diag || !diag->GetLastError) return std::nullopt;
        BML_ErrorInfo info = BML_ERROR_INFO_INIT;
        if (diag->GetLastError(&info) == BML_RESULT_OK) {
            return info;
        }
        return std::nullopt;
    }

    inline void ClearLastError(const BML_CoreDiagnosticInterface *diag) {
        if (diag && diag->ClearLastError) {
            diag->ClearLastError();
        }
    }

    inline const char *GetErrorString(const BML_CoreDiagnosticInterface *diag, BML_Result code) {
        if (!diag || !diag->GetErrorString) return nullptr;
        return diag->GetErrorString(code);
    }

    // ========================================================================
    // Interface Acquisition
    // ========================================================================

    template <typename T>
    InterfaceLease<T> AcquireInterface(
        const char *interfaceId,
        uint16_t reqMajor,
        uint16_t reqMinor = 0,
        uint16_t reqPatch = 0) {
        if (!interfaceId || !bmlInterfaceAcquire) {
            return {};
        }

        const void *implementation = nullptr;
        BML_InterfaceLease lease = nullptr;
        BML_Version required = bmlMakeVersion(reqMajor, reqMinor, reqPatch);
        if (bmlInterfaceAcquire(interfaceId, &required, &implementation, &lease) != BML_RESULT_OK) {
            return {};
        }

        return InterfaceLease<T>(static_cast<const T *>(implementation), lease);
    }

    // ========================================================================
    // Interface Traits (compile-time type -> ID + version binding)
    // ========================================================================

    /**
     * @brief Compile-time trait that binds a vtable struct to its interface ID
     *        and ABI version. Specialize via BML_DEFINE_INTERFACE_TRAIT.
     */
    template <typename T>
    struct InterfaceTrait {
        // Unspecialized: static_assert fires on Acquire<T>() with unknown type
    };

    /**
     * @brief Acquire an interface using compile-time traits.
     *
     *   auto logging = bml::Acquire<BML_CoreLoggingInterface>();
     *   if (logging) logging->LogVa(ctx, BML_LOG_INFO, "tag", "msg", args);
     */
    template <typename T>
    InterfaceLease<T> Acquire() {
        using Trait = InterfaceTrait<T>;
        return AcquireInterface<T>(Trait::Id, Trait::AbiMajor, Trait::AbiMinor);
    }
} // namespace bml

// ============================================================================
// BML_DEFINE_INTERFACE_TRAIT -- works at global scope or in any C++ header.
//
// 3rd-party example:
//   #include <bml_interface.hpp>
//   BML_DEFINE_INTERFACE_TRAIT(MyInterface, MY_INTERFACE_ID, 1, 0)
//
//   // Then consumers can do:
//   auto lease = Services().Acquire<MyInterface>();
// ============================================================================

#define BML_DEFINE_INTERFACE_TRAIT(Type, IdMacro, Major, Minor)              \
    namespace bml { template <> struct InterfaceTrait<Type> {                \
        static constexpr const char *Id = IdMacro;                          \
        static constexpr uint16_t AbiMajor = (Major);                       \
        static constexpr uint16_t AbiMinor = (Minor);                       \
    }; }

// Core interface traits
BML_DEFINE_INTERFACE_TRAIT(BML_CoreContextInterface,      BML_CORE_CONTEXT_INTERFACE_ID,       1, 0)
BML_DEFINE_INTERFACE_TRAIT(BML_CoreModuleInterface,       BML_CORE_MODULE_INTERFACE_ID,        1, 0)
BML_DEFINE_INTERFACE_TRAIT(BML_CoreLoggingInterface,      BML_CORE_LOGGING_INTERFACE_ID,       1, 0)
BML_DEFINE_INTERFACE_TRAIT(BML_CoreConfigInterface,       BML_CORE_CONFIG_INTERFACE_ID,        1, 0)
BML_DEFINE_INTERFACE_TRAIT(BML_CoreMemoryInterface,       BML_CORE_MEMORY_INTERFACE_ID,        1, 0)
BML_DEFINE_INTERFACE_TRAIT(BML_CoreResourceInterface,     BML_CORE_RESOURCE_INTERFACE_ID,      1, 0)
BML_DEFINE_INTERFACE_TRAIT(BML_CoreDiagnosticInterface,   BML_CORE_DIAGNOSTIC_INTERFACE_ID,    1, 0)
BML_DEFINE_INTERFACE_TRAIT(BML_ImcBusInterface,           BML_IMC_BUS_INTERFACE_ID,            1, 0)
BML_DEFINE_INTERFACE_TRAIT(BML_CoreTimerInterface,        BML_CORE_TIMER_INTERFACE_ID,         1, 0)
BML_DEFINE_INTERFACE_TRAIT(BML_CoreHookRegistryInterface, BML_CORE_HOOK_REGISTRY_INTERFACE_ID, 1, 0)
BML_DEFINE_INTERFACE_TRAIT(BML_CoreLocaleInterface,       BML_CORE_LOCALE_INTERFACE_ID,        1, 0)
BML_DEFINE_INTERFACE_TRAIT(BML_HostRuntimeInterface,      BML_CORE_HOST_RUNTIME_INTERFACE_ID,  1, 0)

// Module interface traits (forward-declared; full definitions in module headers)
struct BML_InputCaptureInterface;
struct BML_ConsoleCommandRegistry;
struct BML_UIDrawRegistry;
struct BML_NetworkInterface;

BML_DEFINE_INTERFACE_TRAIT(BML_InputCaptureInterface,  "bml.input.capture",            1, 0)
BML_DEFINE_INTERFACE_TRAIT(BML_ConsoleCommandRegistry, "bml.console.command_registry", 1, 0)
BML_DEFINE_INTERFACE_TRAIT(BML_UIDrawRegistry,         "bml.ui.draw_registry",         1, 0)
BML_DEFINE_INTERFACE_TRAIT(BML_NetworkInterface,        "bml.network",                  1, 0)

#endif /* BML_INTERFACE_HPP */
