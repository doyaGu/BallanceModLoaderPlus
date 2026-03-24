#include "TestKernelBuilder.h"

#include "Core/ApiRegistry.h"
#include "Core/ConfigStore.h"
#include "Core/Context.h"
#include "Core/CrashDumpWriter.h"
#include "Core/DiagnosticManager.h"
#include "Core/FaultTracker.h"
#include "Core/HookRegistry.h"
#include "Core/ImcBus.h"
#include "Core/InterfaceRegistry.h"
#include "Core/KernelServices.h"
#include "Core/LeaseManager.h"
#include "Core/LocaleManager.h"
#include "Core/MemoryManager.h"
#include "Core/ProfilingManager.h"
#include "Core/ResourceApi.h"
#include "Core/RuntimeInterfaces.h"
#include "Core/SyncManager.h"
#include "Core/TimerManager.h"

namespace BML::Core::Testing {

TestKernelBuilder &TestKernelBuilder::WithConfig() {
    m_Flags |= kConfig;
    return *this;
}

TestKernelBuilder &TestKernelBuilder::WithImcBus() {
    m_Flags |= kImcBus;
    return *this;
}

TestKernelBuilder &TestKernelBuilder::WithInterfaces() {
    m_Flags |= kInterfaces;
    return *this;
}

TestKernelBuilder &TestKernelBuilder::WithTimers() {
    m_Flags |= kTimers;
    return *this;
}

TestKernelBuilder &TestKernelBuilder::WithLocale() {
    m_Flags |= kLocale;
    return *this;
}

TestKernelBuilder &TestKernelBuilder::WithHooks() {
    m_Flags |= kHooks;
    return *this;
}

TestKernelBuilder &TestKernelBuilder::WithProfiling() {
    m_Flags |= kProfiling;
    return *this;
}

TestKernelBuilder &TestKernelBuilder::WithDiagnostics() {
    m_Flags |= kDiagnostics;
    return *this;
}

TestKernelBuilder &TestKernelBuilder::WithSync() {
    m_Flags |= kSync;
    return *this;
}

TestKernelBuilder &TestKernelBuilder::WithMemory() {
    m_Flags |= kMemory;
    return *this;
}

TestKernelBuilder &TestKernelBuilder::WithAll() {
    m_Flags |= kConfig | kImcBus | kInterfaces | kTimers | kLocale |
               kHooks | kProfiling | kDiagnostics | kSync | kMemory;
    return *this;
}

TestKernelBuilder &TestKernelBuilder::RegisterConfigApis() {
    m_Flags |= kRegConfigApi;
    return *this;
}

TestKernelBuilder &TestKernelBuilder::RegisterResourceApis() {
    m_Flags |= kRegResourceApi;
    return *this;
}

TestKernelBuilder &TestKernelBuilder::RegisterAllRuntimeInterfaces() {
    m_Flags |= kRegRuntimeInterfaces;
    return *this;
}

TestKernel TestKernelBuilder::Build() {
    TestKernel kernel;
    auto &k = *kernel;

    // L0: Always created (required by Context ctor)
    k.api_registry  = std::make_unique<ApiRegistry>();
    k.crash_dump    = std::make_unique<CrashDumpWriter>();
    k.fault_tracker = std::make_unique<FaultTracker>();
    k.config        = std::make_unique<ConfigStore>(); // Always needed for Context

    // L1: Context (depends on L0)
    k.context = std::make_unique<Context>(*k.api_registry, *k.config,
                                          *k.crash_dump, *k.fault_tracker);
    k.config->BindContext(*k.context);

    // L2: Optional subsystems (order matters for DI constructors)
    if (m_Flags & kDiagnostics) {
        k.diagnostics = std::make_unique<DiagnosticManager>();
    }
    if (m_Flags & kMemory) {
        k.memory = std::make_unique<MemoryManager>();
    }
    if (m_Flags & kSync) {
        k.sync = std::make_unique<SyncManager>();
    }
    if (m_Flags & kImcBus) {
        k.imc_bus = std::make_unique<ImcBus>();
    }
    if (m_Flags & kInterfaces) {
        k.leases = std::make_unique<LeaseManager>();
        k.interface_registry = std::make_unique<InterfaceRegistry>(*k.context, *k.leases);
    }
    if (m_Flags & kTimers) {
        k.timers = std::make_unique<TimerManager>(*k.context);
    }
    if (m_Flags & kLocale) {
        k.locale = std::make_unique<LocaleManager>();
    }
    if (m_Flags & kHooks) {
        k.hooks = std::make_unique<HookRegistry>();
    }
    if (m_Flags & kProfiling) {
        // ProfilingManager requires MemoryManager
        if (!k.memory) {
            k.memory = std::make_unique<MemoryManager>();
        }
        k.profiling = std::make_unique<ProfilingManager>(*k.api_registry, *k.memory);
    }

    // L3: Bindings (ALWAYS done, never forgotten)
    k.context->BindKernel(k);
    k.context->Initialize(bmlMakeVersion(0, 4, 0));
    if (k.imc_bus) {
        k.imc_bus->BindDeps(*k.context);
    }

    // L4: API registrations
    if (m_Flags & kRegConfigApi) {
        BML::Core::RegisterConfigApis(*k.api_registry);
    }
    if (m_Flags & kRegResourceApi) {
        BML::Core::RegisterResourceApis(*k.api_registry);
    }
    if (m_Flags & kRegRuntimeInterfaces) {
        BML::Core::RegisterRuntimeInterfaces(k);
    }

    return kernel;
}

} // namespace BML::Core::Testing
