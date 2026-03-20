/**
 * @file TestContext.cpp
 * @brief Implementation of bml::test::TestContext
 *
 * Bootstraps BML Core in test mode (no CK2/Virtools, no module discovery)
 * and provides convenience utilities for mod unit testing.
 *
 * Uses only exported BML.dll APIs and bmlGetProcAddress lookups.
 * Defines BML_LOADER_IMPLEMENTATION to provide the global API pointer storage
 * so that test executables linking bml_test do not need to define it themselves.
 */

// Provide the global API pointer definitions (bmlInterfaceAcquire, etc.)
#define BML_LOADER_IMPLEMENTATION
#include "bml_loader.h"

#include "bml_test.hpp"

#include <cstring>
#include <memory>
#include <string>
#include <vector>

// BML public headers
#include "bml_bootstrap.h"
#include "bml_builtin_interfaces.h"
#include "bml_imc.h"
#include "bml_logging.h"

// ModHandle.h is a header-only struct definition (no linked symbols)
#include "Core/ModHandle.h"

namespace bml::test {

// ============================================================================
// TestContext - Construction / Destruction
// ============================================================================

TestContext::TestContext() {
    BML_BootstrapConfig config = BML_BOOTSTRAP_CONFIG_INIT;
    config.flags = BML_BOOTSTRAP_FLAG_SKIP_DISCOVERY | BML_BOOTSTRAP_FLAG_SKIP_LOAD;

    BML_Result res = bmlBootstrap(&config);
    if (BML_FAILED(res)) {
        return;
    }

    // Load the bootstrap loader globals (bmlInterfaceAcquire, bmlInterfaceRelease, etc.)
    res = BML_BOOTSTRAP_LOAD(bmlGetProcAddress);
    if (BML_FAILED(res)) {
        bmlShutdown();
        return;
    }

    // Cache frequently-used API function pointers via public proc lookup
    m_GetTopicId = reinterpret_cast<PFN_BML_ImcGetTopicId>(
        bmlGetProcAddress("bmlImcGetTopicId"));
    m_Publish = reinterpret_cast<PFN_BML_ImcPublish>(
        bmlGetProcAddress("bmlImcPublish"));
    m_Pump = reinterpret_cast<PFN_BML_ImcPump>(
        bmlGetProcAddress("bmlImcPump"));
    m_SetCurrentModule = reinterpret_cast<PFN_BML_SetCurrentModule>(
        bmlGetProcAddress("bmlSetCurrentModule"));
    m_SetLogFilter = reinterpret_cast<PFN_BML_SetLogFilter>(
        bmlGetProcAddress("bmlSetLogFilter"));
    m_RegisterLogSinkOverride = reinterpret_cast<PFN_BML_RegisterLogSinkOverride>(
        bmlGetProcAddress("bmlRegisterLogSinkOverride"));
    m_ClearLogSinkOverride = reinterpret_cast<PFN_BML_ClearLogSinkOverride>(
        bmlGetProcAddress("bmlClearLogSinkOverride"));
    m_GetGlobalContext = reinterpret_cast<PFN_BML_GetGlobalContext>(
        bmlGetProcAddress("bmlGetGlobalContext"));

    m_Initialized = true;

    // Set the synthetic host module as current so that IMC subscribe calls work in tests.
    auto getHostModule = reinterpret_cast<BML_Mod (*)(void)>(
        bmlGetProcAddress("bmlGetHostModule"));
    if (getHostModule && m_SetCurrentModule) {
        m_DefaultMod = getHostModule();
        if (m_DefaultMod) {
            m_SetCurrentModule(m_DefaultMod);
        }
    }
}

TestContext::~TestContext() {
    if (!m_Initialized) {
        return;
    }

    // Clear log capture before shutdown
    if (m_LogCaptureEnabled && m_ClearLogSinkOverride) {
        m_ClearLogSinkOverride();
        m_LogCaptureEnabled = false;
    }

    // Clear TLS module binding
    if (m_SetCurrentModule) {
        m_SetCurrentModule(nullptr);
    }

    // Destroy mock module handles before shutdown
    m_MockMods.clear();

    // Unload API pointers before shutting down the runtime
    bmlUnloadAPI();

    bmlShutdown();
    m_Initialized = false;
}

// ============================================================================
// TestContext - Context Handle
// ============================================================================

BML_Context TestContext::Handle() const {
    if (!m_Initialized || !m_GetGlobalContext) {
        return nullptr;
    }
    return m_GetGlobalContext();
}

// ============================================================================
// TestContext - Mock Module Creation
// ============================================================================

BML_Mod TestContext::CreateMockMod(const char *id) {
    if (!m_Initialized || !id) {
        return nullptr;
    }

    auto mod = std::make_unique<BML_Mod_T>();
    mod->id = id;
    mod->version = bmlMakeVersion(1, 0, 0);

    BML_Mod raw = mod.get();
    m_MockMods.push_back(std::move(mod));
    return raw;
}

// ============================================================================
// TestContext - Tick
// ============================================================================

void TestContext::Tick() {
    if (!m_Initialized) {
        return;
    }

    // Pump IMC messages (process all pending) via public API
    if (m_Pump) {
        m_Pump(0);
    }
}

// ============================================================================
// TestContext - Publish
// ============================================================================

bool TestContext::Publish(const char *topic, const void *data, size_t size) {
    if (!m_Initialized || !topic || !m_GetTopicId || !m_Publish) {
        return false;
    }

    BML_TopicId topic_id = BML_TOPIC_ID_INVALID;
    BML_Result res = m_GetTopicId(topic, &topic_id);
    if (BML_FAILED(res) || topic_id == BML_TOPIC_ID_INVALID) {
        return false;
    }

    res = m_Publish(topic_id, data, size);
    return BML_SUCCEEDED(res);
}

// ============================================================================
// TestContext - Log Capture
// ============================================================================

void TestContext::EnableLogCapture(BML_LogSeverity min_level) {
    if (!m_Initialized || m_LogCaptureEnabled || !m_RegisterLogSinkOverride) {
        return;
    }

    m_LogCapture.min_level = min_level;
    m_LogCapture.logs.clear();

    // Set the global log filter to pass everything through
    if (m_SetLogFilter) {
        m_SetLogFilter(min_level);
    }

    BML_LogSinkOverrideDesc desc{};
    desc.struct_size = sizeof(BML_LogSinkOverrideDesc);
    desc.dispatch = &TestContext::LogCaptureDispatch;
    desc.on_shutdown = nullptr;
    desc.user_data = &m_LogCapture;
    desc.flags = 0; // Don't suppress default output in tests

    BML_Result res = m_RegisterLogSinkOverride(&desc);
    if (BML_SUCCEEDED(res)) {
        m_LogCaptureEnabled = true;
    }
}

const std::vector<std::string> &TestContext::CapturedLogs() const {
    return m_LogCapture.logs;
}

void TestContext::ClearLogs() {
    std::lock_guard<std::mutex> lock(m_LogCapture.mutex);
    m_LogCapture.logs.clear();
}

bool TestContext::HasLog(const std::string &substr) const {
    for (const auto &log : m_LogCapture.logs) {
        if (log.find(substr) != std::string::npos) {
            return true;
        }
    }
    return false;
}

void TestContext::LogCaptureDispatch(BML_Context /*ctx*/,
                                     const BML_LogMessageInfo *info,
                                     void *user_data) {
    if (!info || !user_data) {
        return;
    }

    auto *state = static_cast<LogCaptureState *>(user_data);
    if (info->severity < state->min_level) {
        return;
    }

    const char *line = info->formatted_line ? info->formatted_line
                     : info->message        ? info->message
                                            : "";

    std::lock_guard<std::mutex> lock(state->mutex);
    state->logs.emplace_back(line);
}

// ============================================================================
// TestContext - Service Access
// ============================================================================

const bml::RuntimeServiceHub *TestContext::Hub() const {
    if (!m_Initialized) {
        return nullptr;
    }

    // The service hub is exposed via bmlGetServiceHub (registered in CoreApi.cpp).
    // Its signature is: const void* (*)(void)
    // The returned pointer is stable for the runtime lifetime.
    if (!m_CachedHub) {
        using PFN_GetServiceHub = const void *(*)(void);
        auto getServiceHub = reinterpret_cast<PFN_GetServiceHub>(
            bmlGetProcAddress("bmlGetServiceHub"));
        if (getServiceHub) {
            m_CachedHub = static_cast<const bml::RuntimeServiceHub *>(getServiceHub());
        }
    }
    return m_CachedHub;
}

bml::ModuleServices TestContext::CreateServices(BML_Mod mod) {
    return bml::ModuleServices(mod, Hub());
}

} // namespace bml::test
