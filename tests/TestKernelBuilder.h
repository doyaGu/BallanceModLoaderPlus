#pragma once

#include "TestKernel.h"

namespace BML::Core::Testing {

/// Builder for composable test kernel construction.
///
/// Usage:
///   TestKernel kernel = TestKernelBuilder()
///       .WithConfig()
///       .WithImcBus()
///       .RegisterConfigApis()
///       .Build();
///
/// All subsystems are created with proper dependency ordering.
/// BindKernel() and Initialize() are automatically called.
class TestKernelBuilder {
public:
    TestKernelBuilder() = default;

    // Composable subsystem selection
    TestKernelBuilder &WithConfig();
    TestKernelBuilder &WithImcBus();
    TestKernelBuilder &WithInterfaces();
    TestKernelBuilder &WithTimers();
    TestKernelBuilder &WithLocale();
    TestKernelBuilder &WithHooks();
    TestKernelBuilder &WithProfiling();
    TestKernelBuilder &WithDiagnostics();
    TestKernelBuilder &WithSync();
    TestKernelBuilder &WithMemory();

    // All subsystems
    TestKernelBuilder &WithAll();

    // API registration (applied after Build)
    TestKernelBuilder &RegisterConfigApis();
    TestKernelBuilder &RegisterResourceApis();
    TestKernelBuilder &RegisterAllRuntimeInterfaces();

    // Build and return initialized TestKernel
    TestKernel Build();

private:
    enum Flags : uint32_t {
        kConfig       = 1 << 0,
        kImcBus       = 1 << 1,
        kInterfaces   = 1 << 2,
        kTimers       = 1 << 3,
        kLocale       = 1 << 4,
        kHooks        = 1 << 5,
        kProfiling    = 1 << 6,
        kDiagnostics  = 1 << 7,
        kSync         = 1 << 8,
        kMemory       = 1 << 9,
        kRegConfigApi = 1 << 16,
        kRegResourceApi = 1 << 17,
        kRegRuntimeInterfaces = 1 << 18,
    };
    uint32_t m_Flags = 0;
};

} // namespace BML::Core::Testing
