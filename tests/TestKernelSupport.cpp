// Test-specific KernelServices destructor.
//
// BML.dll gets ~KernelServices() from Microkernel.cpp.  Test targets
// don't link Microkernel.cpp, so they use this definition instead.
//
// All subsystem destructors are now inline (= default in their headers)
// because the previously-incomplete types (DeadlockDetector, FixedBlockPool)
// are now fully defined in headers included by the subsystem headers.

#include "Core/KernelServices.h"
#include "TestKernel.h"

#include "Core/ApiRegistry.h"
#include "Core/ConfigStore.h"
#include "Core/Context.h"
#include "Core/CrashDumpWriter.h"
#include "Core/DiagnosticManager.h"
#include "Core/FaultTracker.h"
#include "Core/HookRegistry.h"
#include "Core/ImcBus.h"
#include "Core/InterfaceRegistry.h"
#include "Core/LeaseManager.h"
#include "Core/LocaleManager.h"
#include "Core/MemoryManager.h"
#include "Core/ProfilingManager.h"
#include "Core/SyncManager.h"
#include "Core/TimerManager.h"

#include <atomic>
#include <cassert>

namespace BML::Core {
namespace {
    std::atomic<KernelServices *> g_TestKernel{nullptr};
}

KernelServices::~KernelServices() {
    // Mirror the production teardown order so IMC shutdown callbacks still see
    // a live Context when the test kernel is destroyed without an explicit shutdown.
    imc_bus.reset();
    context.reset();
    interface_registry.reset();
    api_registry.reset();
    config.reset();
    leases.reset();
    timers.reset();
    locale.reset();
    hooks.reset();
    crash_dump.reset();
    fault_tracker.reset();
    profiling.reset();
    sync.reset();
    memory.reset();
    diagnostics.reset();
}

namespace Testing {
KernelServices *GetInstalledTestBootstrapKernel() noexcept {
    return g_TestKernel.load(std::memory_order_acquire);
}

TestKernel::TestKernel()
    : m_Kernel(std::make_unique<KernelServices>()) {
    g_TestKernel.store(m_Kernel.get(), std::memory_order_release);
}

TestKernel::~TestKernel() {
    auto *installed = m_Kernel.get();
    m_Kernel.reset();
    if (g_TestKernel.load(std::memory_order_acquire) == installed) {
        g_TestKernel.store(nullptr, std::memory_order_release);
    }
}

} // namespace Testing
} // namespace BML::Core
