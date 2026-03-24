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
/// The constructor installs an empty KernelServices for tests that still need
/// a process-wide bootstrap stub. The destructor uninstalls and destroys the
/// kernel, cleaning up all subsystems that were populated.
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
    TestKernel(TestKernel &&other) noexcept : m_Kernel(std::move(other.m_Kernel)) {}
    TestKernel &operator=(TestKernel &&other) noexcept {
        if (this != &other) m_Kernel = std::move(other.m_Kernel);
        return *this;
    }

    KernelServices &operator*()  noexcept { return *m_Kernel; }
    KernelServices *operator->() noexcept { return m_Kernel.get(); }
    KernelServices *get()        noexcept { return m_Kernel.get(); }

private:
    std::unique_ptr<KernelServices> m_Kernel;
};

} // namespace BML::Core::Testing

#endif // BML_TESTS_TEST_KERNEL_H
