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

namespace BML::Core {

// Test-specific implementations of kernel access functions.
// Production versions are in KernelServices.cpp.
namespace {
    KernelServices *g_TestKernel = nullptr;
}

KernelServices *GetKernelOrNull() noexcept {
    return g_TestKernel;
}

void InstallKernel(KernelServices *kernel) noexcept {
    g_TestKernel = kernel;
}

KernelServices::~KernelServices() = default;

namespace Testing {

TestKernel::TestKernel()
    : m_Kernel(std::make_unique<KernelServices>()) {
    InstallKernel(m_Kernel.get());
}

TestKernel::~TestKernel() {
    InstallKernel(nullptr);
    // m_Kernel destructor runs ~KernelServices() with all types complete.
}

} // namespace Testing
} // namespace BML::Core
