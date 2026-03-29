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
#include "bml_services.hpp"
#include "bml_imc.h"
#include "bml_logging.h"

// ModHandle.h is a header-only struct definition (no linked symbols)
#include "Core/ModHandle.h"

namespace bml::test {
// ============================================================================
// TestContext - Construction / Destruction
// ============================================================================

TestContext::TestContext() {
    BML_RuntimeConfig config = BML_RUNTIME_CONFIG_INIT;

    BML_Result res = bmlRuntimeCreate(&config, &m_Runtime);
    if (BML_FAILED(res)) {
        return;
    }

    m_CachedHub = bmlRuntimeGetServices(m_Runtime);
    if (!m_CachedHub) {
        bmlRuntimeDestroy(m_Runtime);
        m_Runtime = nullptr;
        return;
    }
    bmlBindServices(m_CachedHub);

    m_GetTopicId = m_CachedHub->ImcBus ? m_CachedHub->ImcBus->GetTopicId : nullptr;
    m_Publish = m_CachedHub->ImcBus ? m_CachedHub->ImcBus->Publish : nullptr;
    m_Pump = m_CachedHub->ImcBus ? m_CachedHub->ImcBus->Pump : nullptr;
    m_SetLogFilter = m_CachedHub->Logging ? m_CachedHub->Logging->SetLogFilter : nullptr;
    m_RegisterLogSinkOverride = m_CachedHub->Logging
        ? m_CachedHub->Logging->RegisterSinkOverride : nullptr;
    m_ClearLogSinkOverride = m_CachedHub->Logging
        ? m_CachedHub->Logging->ClearSinkOverride : nullptr;
    m_Context = m_CachedHub->Context ? m_CachedHub->Context->Context : nullptr;
    m_FindModuleById = m_CachedHub->Module ? m_CachedHub->Module->FindModuleById : nullptr;

    m_Initialized = true;

    if (m_FindModuleById && m_Context) {
        m_DefaultMod = m_FindModuleById(m_Context, "ModLoader");
    }
}

TestContext::~TestContext() {
    if (!m_Initialized) {
        return;
    }

    // Clear log capture before shutdown
    if (m_LogCaptureEnabled && m_ClearLogSinkOverride && m_DefaultMod) {
        m_ClearLogSinkOverride(m_DefaultMod);
        m_LogCaptureEnabled = false;
    }

    // Destroy mock module handles before shutdown
    m_MockMods.clear();

    // Unload API pointers before shutting down the runtime
    bmlUnloadAPI();

    if (m_Runtime) {
        bmlRuntimeDestroy(m_Runtime);
        m_Runtime = nullptr;
    }
    m_Initialized = false;
}

// ============================================================================
// TestContext - Context Handle
// ============================================================================

BML_Context TestContext::Handle() const {
    if (!m_Initialized) {
        return nullptr;
    }
    return m_Context;
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
    if (m_Pump && m_Context) {
        m_Pump(m_Context, 0);
    }
}

// ============================================================================
// TestContext - Publish
// ============================================================================

bool TestContext::Publish(const char *topic, const void *data, size_t size) {
    if (!m_Initialized || !topic || !m_GetTopicId) {
        return false;
    }

    BML_TopicId topic_id = BML_TOPIC_ID_INVALID;
    BML_Result res = m_GetTopicId(m_Context, topic, &topic_id);
    if (BML_FAILED(res) || topic_id == BML_TOPIC_ID_INVALID) {
        return false;
    }

    if (!m_DefaultMod || !m_Publish) {
        return false;
    }
    res = m_Publish(m_DefaultMod, topic_id, data, size, BML_PAYLOAD_TYPE_NONE);
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
    if (m_DefaultMod && m_SetLogFilter) {
        m_SetLogFilter(m_DefaultMod, min_level);
    }

    BML_LogSinkOverrideDesc desc{};
    desc.struct_size = sizeof(BML_LogSinkOverrideDesc);
    desc.dispatch = &TestContext::LogCaptureDispatch;
    desc.on_shutdown = nullptr;
    desc.user_data = &m_LogCapture;
    desc.flags = 0; // Don't suppress default output in tests

    BML_Result res = m_RegisterLogSinkOverride(m_DefaultMod, &desc);
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

const BML_Services *TestContext::Hub() const {
    if (!m_Initialized) {
        return nullptr;
    }
    return m_CachedHub;
}

bml::ModuleServices TestContext::CreateServices(BML_Mod mod) {
    return bml::ModuleServices(mod, Hub());
}

} // namespace bml::test
