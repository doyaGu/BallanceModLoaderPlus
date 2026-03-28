#ifndef BML_CORE_KERNEL_SERVICES_H
#define BML_CORE_KERNEL_SERVICES_H

#include <atomic>
#include <memory>

#include "bml_types.h"

namespace BML::Core {

// Forward declarations
class ApiRegistry;
class ConfigStore;
class Context;
class CrashDumpWriter;
class DiagnosticManager;
class FaultTracker;
class HookRegistry;
class ImcBus;
class InterfaceRegistry;
class LeaseManager;
class LocaleManager;
class MemoryManager;
class ProfilingManager;
class SyncManager;
class TimerManager;

/// Centralized service graph for the microkernel.
///
/// All subsystems are owned via unique_ptr.  Field declaration order
/// mirrors the dependency layers (L0 -> L3) so that the default
/// reverse-declaration-order destruction is reasonable, though the
/// explicit destructor in Microkernel.cpp defines the authoritative
/// teardown sequence.
///
/// All fields are populated by Microkernel.cpp -- the constructor is
/// trivial so that KernelServices.cpp has no link dependency on any
/// subsystem .cpp file.
struct KernelServices {
    // -- L0: leaf subsystems (no dependencies) ------------
    std::unique_ptr<DiagnosticManager>  diagnostics;
    std::unique_ptr<MemoryManager>      memory;
    std::unique_ptr<SyncManager>        sync;

    // -- L1: infrastructure (depend on L0 only) -----------
    std::unique_ptr<FaultTracker>       fault_tracker;
    std::unique_ptr<CrashDumpWriter>    crash_dump;
    std::unique_ptr<HookRegistry>       hooks;
    std::unique_ptr<LocaleManager>      locale;
    std::unique_ptr<LeaseManager>       leases;
    std::unique_ptr<ConfigStore>        config;

    // -- L2: registries + message bus (depend on L0/L1) ---
    std::unique_ptr<ApiRegistry>        api_registry;
    std::unique_ptr<ProfilingManager>   profiling;          // ApiRegistry&, MemoryManager&
    std::unique_ptr<ImcBus>             imc_bus;

    // -- L3: context-dependent subsystems -----------------
    std::unique_ptr<Context>            context;            // ApiRegistry&, ConfigStore&, ...
    std::unique_ptr<InterfaceRegistry>  interface_registry;  // Context&, LeaseManager&
    std::unique_ptr<TimerManager>       timers;              // Context&
    BML_Runtime                         runtime{nullptr};

    KernelServices() = default;
    ~KernelServices();

    void Shutdown();

    KernelServices(const KernelServices &) = delete;
    KernelServices &operator=(const KernelServices &) = delete;
};

/// Clear runtime-owned tracing state for a kernel that is shutting down.
void ResetTracingStateForKernel(KernelServices *kernel);

} // namespace BML::Core

#endif // BML_CORE_KERNEL_SERVICES_H
