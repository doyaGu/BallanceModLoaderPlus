/**
 * @file bml_test.hpp
 * @brief BML Test Support - C++ wrapper for mod unit testing
 *
 * Provides bml::test::TestContext, a RAII wrapper around the BML Core bootstrap
 * that lets mod authors unit-test their code without running Ballance.
 *
 * Usage:
 * @code
 * #include <gtest/gtest.h>
 * #include <bml_test.hpp>
 *
 * class MyModTest : public ::testing::Test {
 * protected:
 *     bml::test::TestContext ctx;
 * };
 *
 * TEST_F(MyModTest, CanPublishAndReceive) {
 *     BML_Mod mod = ctx.CreateMockMod("com.test.mymod");
 *     // ... subscribe, publish, tick, assert ...
 * }
 * @endcode
 *
 * Link against `bml_test` (static library) which links BML.dll.
 */

#ifndef BML_TEST_HPP
#define BML_TEST_HPP

#include "bml_test.h"
#include "bml_core.h"
#include "bml_services.hpp"

#include <memory>
#include <mutex>
#include <string>
#include <vector>

// Forward declaration -- full definition in Core/ModHandle.h
struct BML_Mod_T;

namespace bml::test {

/**
 * @brief RAII test context that bootstraps BML Core for unit testing.
 *
 * Constructor calls bmlBootstrap with SKIP_DISCOVERY | SKIP_LOAD.
 * Destructor calls bmlShutdown. Multiple TestContexts may be created
 * sequentially (not concurrently).
 */
class TestContext {
public:
    /**
     * @brief Bootstrap BML Core in test mode.
     *
     * Initializes the microkernel with SKIP_DISCOVERY | SKIP_LOAD flags,
     * loads the bootstrap API pointers, and optionally installs a log
     * capture sink.
     */
    TestContext();

    /**
     * @brief Shut down BML Core and clean up.
     *
     * Clears any active log capture, destroys mock module handles,
     * and calls bmlShutdown().
     */
    ~TestContext();

    TestContext(const TestContext &) = delete;
    TestContext &operator=(const TestContext &) = delete;

    /** @brief Get the raw BML context handle. */
    BML_Context Handle() const;

    /**
     * @brief Create a mock module handle for testing.
     * @param id Module ID string (e.g. "com.test.mymod")
     * @return Valid BML_Mod handle
     */
    BML_Mod CreateMockMod(const char *id);

    /**
     * @brief Pump IMC messages.
     *
     * Call after Publish to deliver messages to subscribers.
     */
    void Tick();

    /**
     * @brief Publish to a topic by name.
     * @param topic Topic name string
     * @param data  Payload (may be nullptr)
     * @param size  Payload size
     * @return true on success
     */
    bool Publish(const char *topic, const void *data = nullptr, size_t size = 0);

    /**
     * @brief Publish typed data to a topic by name.
     * @tparam T Payload type (must be trivially copyable)
     * @param topic Topic name string
     * @param data  Payload value
     * @return true on success
     */
    template <typename T>
    bool Publish(const char *topic, const T &data) {
        return Publish(topic, &data, sizeof(T));
    }

    /**
     * @brief Enable log capture to an internal buffer.
     * @param min_level Minimum severity to capture (default: BML_LOG_TRACE)
     */
    void EnableLogCapture(BML_LogSeverity min_level = BML_LOG_TRACE);

    /** @brief Get the list of captured log messages. */
    const std::vector<std::string> &CapturedLogs() const;

    /** @brief Clear captured log messages. */
    void ClearLogs();

    /**
     * @brief Check if any captured log contains the given substring.
     * @param substr Substring to search for
     * @return true if found in any captured log
     */
    bool HasLog(const std::string &substr) const;

    /** @brief Get the runtime service hub. */
    const bml::RuntimeServiceHub *Hub() const;

    /**
     * @brief Create a ModuleServices instance for a mock module.
     * @param mod Module handle from CreateMockMod
     * @return Bound ModuleServices instance
     */
    bml::ModuleServices CreateServices(BML_Mod mod);

private:
    struct LogCaptureState {
        std::vector<std::string> logs;
        std::mutex mutex;
        BML_LogSeverity min_level{BML_LOG_TRACE};
    };

    bool m_Initialized{false};
    bool m_LogCaptureEnabled{false};
    LogCaptureState m_LogCapture;

    // Mock module handle storage (kept alive until shutdown)
    std::vector<std::unique_ptr<BML_Mod_T>> m_MockMods;
    BML_Mod m_DefaultMod{nullptr};

    // Cached service hub pointer (stable for runtime lifetime)
    mutable const bml::RuntimeServiceHub *m_CachedHub{nullptr};

    // IMC API function pointers (cached for performance)
    PFN_BML_ImcGetTopicId m_GetTopicId{nullptr};
    PFN_BML_ImcPublish m_Publish{nullptr};
    PFN_BML_ImcPump m_Pump{nullptr};

    // Core API function pointers
    PFN_BML_SetLogFilter m_SetLogFilter{nullptr};
    PFN_BML_RegisterLogSinkOverride m_RegisterLogSinkOverride{nullptr};
    PFN_BML_ClearLogSinkOverride m_ClearLogSinkOverride{nullptr};
    PFN_BML_GetGlobalContext m_GetGlobalContext{nullptr};
    PFN_BML_GetHostModule m_GetHostModule{nullptr};

    static void LogCaptureDispatch(BML_Context ctx,
                                   const BML_LogMessageInfo *info,
                                   void *user_data);
};

} // namespace bml::test

#endif /* BML_TEST_HPP */
