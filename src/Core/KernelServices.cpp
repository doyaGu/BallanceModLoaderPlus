#include "KernelServices.h"

#include "ApiRegistry.h"
#include "ConfigStore.h"
#include "Context.h"
#include "CrashDumpWriter.h"
#include "DiagnosticManager.h"
#include "FaultTracker.h"
#include "HookRegistry.h"
#include "ImcBus.h"
#include "InterfaceRegistry.h"
#include "LeaseManager.h"
#include "LocaleManager.h"
#include "MemoryManager.h"
#include "ProfilingManager.h"
#include "SyncManager.h"
#include "TimerManager.h"

namespace BML::Core {

namespace {
    KernelServices *g_Kernel = nullptr;
}

KernelServices *GetKernelOrNull() noexcept {
    return g_Kernel;
}

void InstallKernel(KernelServices *kernel) noexcept {
    g_Kernel = kernel;
}

void KernelServices::Shutdown() {
    // Graceful shutdown (order matters)
    // NOTE: This method is only intended to be called from Microkernel.cpp,
    // where all subsystem .cpp files are linked. Test targets should NOT
    // call this method; they just let the default destructor clean up.
    if (imc_bus)             imc_bus->Shutdown();
    if (timers)              timers->Shutdown();
    if (locale)              locale->Shutdown();
    if (hooks)               hooks->Shutdown();
    if (crash_dump)          crash_dump->Shutdown();
    if (fault_tracker)       fault_tracker->Shutdown();
    if (context)             context->Cleanup();
    if (leases)              leases->Reset();
    if (interface_registry)  interface_registry->Clear();
    if (api_registry)        api_registry->Clear();
}

// NOTE: ~KernelServices() is defined in TestKernelSupport.cpp for test targets,
// and in Microkernel.cpp for BML.dll (where all subsystem .cpp files are available).

} // namespace BML::Core
