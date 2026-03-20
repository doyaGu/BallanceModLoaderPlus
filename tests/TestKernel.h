#ifndef BML_TESTS_TEST_KERNEL_H
#define BML_TESTS_TEST_KERNEL_H

#include <memory>

#include "Core/KernelServices.h"

namespace BML::Core::Testing {

/// RAII guard that creates a fresh KernelServices for each test.
///
/// Usage in a Google Test fixture:
///
///   class MyTest : public ::testing::Test {
///   protected:
///       TestKernel kernel_;
///
///       void SetUp() override {
///           // Populate only the subsystems this test needs:
///           kernel_->api_registry = std::make_unique<ApiRegistry>();
///           kernel_->config       = std::make_unique<ConfigStore>();
///           // etc.
///       }
///       // No TearDown needed -- TestKernel destructor handles cleanup.
///   };
///
/// The constructor installs an empty KernelServices so that Instance()
/// methods resolve through it (instead of fallback static locals).
/// The destructor uninstalls and destroys the kernel, cleaning up all
/// subsystems that were populated.
///
/// NOTE: TestKernelSupport.cpp provides the KernelServices destructor
/// for test link targets.  It must be compiled into every test target
/// that uses TestKernel.
class TestKernel {
public:
    TestKernel();
    ~TestKernel();

    TestKernel(const TestKernel &) = delete;
    TestKernel &operator=(const TestKernel &) = delete;

    KernelServices &operator*()  noexcept { return *m_Kernel; }
    KernelServices *operator->() noexcept { return m_Kernel.get(); }
    KernelServices *get()        noexcept { return m_Kernel.get(); }

private:
    std::unique_ptr<KernelServices> m_Kernel;
};

} // namespace BML::Core::Testing

#endif // BML_TESTS_TEST_KERNEL_H
