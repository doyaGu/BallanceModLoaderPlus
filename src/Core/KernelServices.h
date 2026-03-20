#ifndef BML_CORE_KERNEL_SERVICES_H
#define BML_CORE_KERNEL_SERVICES_H

#include <memory>

namespace BML::Core {

// Forward declarations
class ApiRegistry;
class ConfigStore;
class Context;
class CrashDumpWriter;
class DiagnosticManager;
class FaultTracker;
class HookRegistry;
class InterfaceRegistry;
class LeaseManager;
class LocaleManager;
class MemoryManager;
class ProfilingManager;
class SyncManager;
class TimerManager;

/// Centralized service graph for the microkernel.
///
/// L0-L1 subsystems are owned (unique_ptr).  L2+ are non-owning
/// pointers populated from existing Meyer's singletons.
///
/// All fields are populated by Microkernel.cpp -- the constructor is
/// trivial so that KernelServices.cpp has no link dependency on any
/// subsystem .cpp file.
struct KernelServices {
    // -- L0: leaf subsystems (owned) ----------------------
    std::unique_ptr<DiagnosticManager>  diagnostics;
    std::unique_ptr<MemoryManager>      memory;
    std::unique_ptr<SyncManager>        sync;
    std::unique_ptr<ProfilingManager>   profiling;

    // -- L1: infrastructure (owned) -----------------------
    std::unique_ptr<FaultTracker>       fault_tracker;
    std::unique_ptr<CrashDumpWriter>    crash_dump;
    std::unique_ptr<HookRegistry>       hooks;
    std::unique_ptr<LocaleManager>      locale;
    std::unique_ptr<TimerManager>       timers;
    std::unique_ptr<LeaseManager>       leases;
    std::unique_ptr<ConfigStore>        config;

    // -- L2: registries (owned) ---------------------------
    std::unique_ptr<ApiRegistry>        api_registry;
    std::unique_ptr<InterfaceRegistry>  interface_registry;

    // -- L3: top-level context (owned) --------------------
    std::unique_ptr<Context>            context;

    KernelServices() = default;
    ~KernelServices();

    KernelServices(const KernelServices &) = delete;
    KernelServices &operator=(const KernelServices &) = delete;
};

/// Return the currently installed KernelServices, or nullptr.
KernelServices *GetKernelOrNull() noexcept;

/// Install (or uninstall) the global KernelServices instance.
void InstallKernel(KernelServices *kernel) noexcept;

} // namespace BML::Core

#endif // BML_CORE_KERNEL_SERVICES_H
