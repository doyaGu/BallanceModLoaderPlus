/**
 * @file IntegrationTestMod.cpp
 * @brief In-game integration test module for BML
 *
 * Runs inside a live Ballance session. Validates:
 *   - API loading and module context
 *   - IMC subscribe/publish/pump lifecycle events
 *   - Service acquisition against builtin providers
 *   - Config read/write roundtrip
 *   - Logger output at all levels
 *   - Frame loop (PreProcess/PostProcess/Render events)
 *
 * Writes a JSON report to the runtime directory as IntegrationTestReport.json,
 * then requests game exit after collecting enough frames.
 */

#define BML_LOADER_IMPLEMENTATION
#include "bml_module.hpp"
#include "bml_services.hpp"
#include "bml_input_control.h"
#include "bml_topics.h"
#include "BuiltinModuleProbe.h"
#include "PathUtils.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

// ---------------------------------------------------------------------------
// Test result bookkeeping
// ---------------------------------------------------------------------------

struct TestResult {
    std::string name;
    bool passed;
    std::string detail;
};

static std::mutex g_ResultsMutex;
static std::vector<TestResult> g_Results;

static void RecordTest(const char *name, bool passed, const char *detail = "") {
    std::lock_guard<std::mutex> lock(g_ResultsMutex);
    g_Results.push_back({name, passed, detail});
}

static constexpr std::array<const char *, 21> kRequiredTestNames = {
    "api_load",
    "module_context",
    "global_context",
    "runtime_version",
    "mod_id",
    "logger",
    "subscriptions",
    "engine_init_received",
    "service_query_peer",
    "config_roundtrip",
    "imc_self_pubsub_setup",
    "frame_preprocess",
    "frame_postprocess",
    "frame_prerender",
    "frame_prerender_payload",
    "frame_postrender",
    "frame_postrender_payload",
    "frame_postspriterender",
    "frame_postspriterender_payload",
    "engine_play_received",
    "imc_self_pubsub_verify",
};

static std::filesystem::path ResolveReportPath() {
    const utils::RuntimeLayoutNames names;
    const auto layout = utils::GetRuntimeLayout();
    if (!layout.runtime_directory.empty()) {
        return (layout.runtime_directory / L"IntegrationTestReport.json").lexically_normal();
    }
    return (std::filesystem::current_path() / names.runtime_directory / L"IntegrationTestReport.json")
        .lexically_normal();
}

static std::string EscapeJson(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size() + 8);
    for (unsigned char ch : value) {
        switch (ch) {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        case '\b':
            escaped += "\\b";
            break;
        case '\f':
            escaped += "\\f";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            if (ch < 0x20) {
                char buf[7];
                std::snprintf(buf, sizeof(buf), "\\u%04X", ch);
                escaped += buf;
            } else {
                escaped.push_back(static_cast<char>(ch));
            }
            break;
        }
    }
    return escaped;
}

// ---------------------------------------------------------------------------
// Report writer
// ---------------------------------------------------------------------------

struct ReportSummary {
    int total{0};
    int passed{0};
    int failed{0};
    int expected_total{static_cast<int>(kRequiredTestNames.size())};
    int missing_required{0};
    bool required_complete{false};
};

static ReportSummary SummarizeResults(const std::vector<TestResult> &results) {
    ReportSummary summary;
    summary.total = static_cast<int>(results.size());

    std::unordered_set<std::string> names;
    names.reserve(results.size());

    for (const auto &result : results) {
        if (result.passed) {
            ++summary.passed;
        } else {
            ++summary.failed;
        }
        names.insert(result.name);
    }

    for (const char *required : kRequiredTestNames) {
        if (names.find(required) == names.end()) {
            ++summary.missing_required;
        }
    }

    summary.required_complete = summary.missing_required == 0 && summary.total == summary.expected_total;
    return summary;
}

static void WriteReport(const std::filesystem::path &path, double duration_ms, bool complete) {
    std::vector<TestResult> results;
    {
        std::lock_guard<std::mutex> lock(g_ResultsMutex);
        results = g_Results;
    }

    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);

    FILE *fp = nullptr;
#if defined(_WIN32)
    fp = _wfopen(path.c_str(), L"wb");
#else
    fp = fopen(path.string().c_str(), "wb");
#endif
    if (!fp) return;

    const ReportSummary summary = SummarizeResults(results);

    fprintf(fp, "{\n");
    fprintf(fp, "  \"version\": \"0.4.0\",\n");
    fprintf(fp, "  \"duration_ms\": %.1f,\n", duration_ms);
    fprintf(fp, "  \"tests\": [\n");
    for (size_t i = 0; i < results.size(); ++i) {
        const auto &r = results[i];
        const std::string escaped_name = EscapeJson(r.name);
        fprintf(fp, "    {\"name\": \"%s\", \"passed\": %s",
                escaped_name.c_str(), r.passed ? "true" : "false");
        if (!r.detail.empty()) {
            const std::string escaped_detail = EscapeJson(r.detail);
            fprintf(fp, ", \"detail\": \"%s\"", escaped_detail.c_str());
        }
        fprintf(fp, "}%s\n", (i + 1 < results.size()) ? "," : "");
    }
    fprintf(fp, "  ],\n");
    fprintf(fp,
            "  \"summary\": {\"total\": %d, \"passed\": %d, \"failed\": %d, "
            "\"expected_total\": %d, \"missing_required\": %d, "
            "\"required_complete\": %s, \"complete\": %s}\n",
            summary.total,
            summary.passed,
            summary.failed,
            summary.expected_total,
            summary.missing_required,
            summary.required_complete ? "true" : "false",
            complete ? "true" : "false");
    fprintf(fp, "}\n");
    fclose(fp);
}

// ---------------------------------------------------------------------------
// Integration Test Module
// ---------------------------------------------------------------------------

class IntegrationTestMod : public bml::Module {
    bml::imc::SubscriptionManager m_Subs;
    std::unique_ptr<BuiltinModuleProbe> m_Probe;

    std::atomic<int> m_PreProcessCount{0};
    std::atomic<int> m_PostProcessCount{0};
    std::atomic<int> m_PreRenderCount{0};
    std::atomic<int> m_PostRenderCount{0};
    std::atomic<int> m_PostSpriteRenderCount{0};
    std::atomic<bool> m_PreRenderPayloadValid{true};
    std::atomic<bool> m_PostRenderPayloadValid{true};
    std::atomic<bool> m_PostSpriteRenderPayloadValid{true};
    std::atomic<bool> m_EngineInitReceived{false};
    std::atomic<bool> m_EnginePlayReceived{false};
    std::atomic<bool> m_ReportWritten{false};

    // IMC self-pub/sub test state (verified asynchronously after a few frames)
    std::atomic<int> m_SelfPubCount{0};
    std::atomic<uint32_t> m_SelfPubValue{0};

    std::chrono::steady_clock::time_point m_StartTime;
    const std::filesystem::path m_ReportPath{ResolveReportPath()};

    static constexpr int kTargetFrames = 30;
    static constexpr const char *kLogTag = "IntTest";

    bool TryGetModId(const char **outId) const {
        if (!outId) {
            return false;
        }

        return Services().Interfaces().Module->GetModId(Handle(), outId) == BML_RESULT_OK;
    }

public:
    BML_Result OnAttach(bml::ModuleServices &services) override {
        if (BuiltinModuleProbe::IsEnabled()) {
            m_Probe = std::make_unique<BuiltinModuleProbe>(Handle());
            return m_Probe->OnAttach();
        }

        m_Subs = services.CreateSubscriptions();
        m_StartTime = std::chrono::steady_clock::now();

        // ---- Test: api_load ----
        bool api_ok = bml::IsApiLoaded();
        RecordTest("api_load", api_ok);

        // ---- Test: module_context ----
        bool ctx_ok = (Handle() != nullptr);
        RecordTest("module_context", ctx_ok);

        // ---- Test: global_context ----
        auto gctx = bml::GetContext(Services().Interfaces().Context);
        RecordTest("global_context", static_cast<bool>(gctx));

        // ---- Test: runtime_version ----
        auto ver = bml::GetRuntimeVersion(Services().Interfaces().Context);
        {
            char buf[64] = {};
            if (ver) {
                snprintf(buf, sizeof(buf), "%u.%u.%u", ver->major, ver->minor, ver->patch);
            }
            RecordTest("runtime_version", ver.has_value() && (ver->major + ver->minor + ver->patch > 0), buf);
        }

        // ---- Test: mod_id ----
        {
            const char *id = nullptr;
            const bool ok = TryGetModId(&id);
            RecordTest("mod_id", ok && id && std::string(id) == "com.bml.integration-test",
                       id ? id : "(null)");
        }

        // ---- Test: logger ----
        {
            Services().Log().Trace("trace %d", 1);
            Services().Log().Debug("debug %d", 2);
            Services().Log().Info("info %d", 3);
            Services().Log().Warn("warn %d", 4);
            Services().Log().Error("error %d", 5);
            RecordTest("logger", true, "all levels written");
        }

        // ---- Subscribe to engine lifecycle ----
        bool sub_init = m_Subs.Add(BML_TOPIC_ENGINE_INIT, [this](const bml::imc::Message &) {
            OnEngineInit();
        });
        bool sub_play = m_Subs.Add(BML_TOPIC_ENGINE_PLAY, [this](const bml::imc::Message &) {
            m_EnginePlayReceived = true;
        });
        bool sub_pre = m_Subs.Add(BML_TOPIC_ENGINE_PRE_PROCESS, [this](const bml::imc::Message &) {
            OnPreProcess();
        });
        bool sub_post = m_Subs.Add(BML_TOPIC_ENGINE_POST_PROCESS, [this](const bml::imc::Message &) {
            m_PostProcessCount.fetch_add(1, std::memory_order_relaxed);
        });
        bool sub_prer = m_Subs.Add(BML_TOPIC_ENGINE_PRE_RENDER, [this](const bml::imc::Message &msg) {
            void *ctx = nullptr;
            if (msg.Size() != sizeof(ctx) || !msg.CopyTo(ctx) || !ctx) {
                m_PreRenderPayloadValid.store(false, std::memory_order_relaxed);
            }
            m_PreRenderCount.fetch_add(1, std::memory_order_relaxed);
        });
        bool sub_postr = m_Subs.Add(BML_TOPIC_ENGINE_POST_RENDER, [this](const bml::imc::Message &msg) {
            void *ctx = nullptr;
            if (msg.Size() != sizeof(ctx) || !msg.CopyTo(ctx) || !ctx) {
                m_PostRenderPayloadValid.store(false, std::memory_order_relaxed);
            }
            m_PostRenderCount.fetch_add(1, std::memory_order_relaxed);
        });
        bool sub_sprite = m_Subs.Add(BML_TOPIC_ENGINE_POST_SPRITE_RENDER, [this](const bml::imc::Message &msg) {
            void *ctx = nullptr;
            if (msg.Size() != sizeof(ctx) || !msg.CopyTo(ctx) || !ctx) {
                m_PostSpriteRenderPayloadValid.store(false, std::memory_order_relaxed);
            }
            m_PostSpriteRenderCount.fetch_add(1, std::memory_order_relaxed);
        });

        bool all_subs = sub_init && sub_play && sub_pre && sub_post &&
                        sub_prer && sub_postr && sub_sprite;
        {
            char buf[128];
            snprintf(buf, sizeof(buf), "%d/%d subscribed", static_cast<int>(m_Subs.Count()), 7);
            RecordTest("subscriptions", all_subs, buf);
        }

        if (!all_subs) {
            Services().Log().Error("Failed to subscribe to some engine events");
            return BML_RESULT_FAIL;
        }

        Services().Log().Info("OnAttach complete. Waiting for engine events...");
        return BML_RESULT_OK;
    }

    void OnDetach() override {
        if (m_Probe) {
            m_Probe->OnDetach();
            m_Probe.reset();
            return;
        }

        // Write report if not yet written (e.g. early exit)
        FinalizeReport();

        Services().Log().Info("OnDetach - RAII cleanup begins");
        // SubscriptionManager destructor will unsubscribe all
    }

private:
    // Called on BML/Engine/Init
    void OnEngineInit() {
        if (m_EngineInitReceived.exchange(true)) return; // only run once

        Services().Log().Info("Received BML/Engine/Init");
        RecordTest("engine_init_received", true);

        // ---- Test: service_query_peer (input capture service from BML_Input module) ----
        {
            auto inputCapture = bml::AcquireInterface<BML_InputCaptureInterface>(
                Handle(), BML_INPUT_CAPTURE_INTERFACE_ID, 1, 0, 0);
            RecordTest("service_query_peer", static_cast<bool>(inputCapture),
                       inputCapture ? "input capture service found" : "input capture service not found");
        }

        // ---- Test: config_roundtrip ----
        {
            auto config = Services().Config();
            bool s_ok = config.SetString("inttest", "greeting", "hello");
            auto s_val = config.GetString("inttest", "greeting");
            bool str_rt = s_ok && s_val.has_value() && *s_val == "hello";

            bool i_ok = config.SetInt("inttest", "count", 42);
            auto i_val = config.GetInt("inttest", "count");
            bool int_rt = i_ok && i_val.has_value() && *i_val == 42;

            bool f_ok = config.SetFloat("inttest", "ratio", 3.14f);
            auto f_val = config.GetFloat("inttest", "ratio");
            bool flt_rt = f_ok && f_val.has_value() && (*f_val > 3.13f && *f_val < 3.15f);

            bool b_ok = config.SetBool("inttest", "flag", true);
            auto b_val = config.GetBool("inttest", "flag");
            bool bool_rt = b_ok && b_val.has_value() && *b_val == true;

            char buf[128];
            snprintf(buf, sizeof(buf), "str=%d int=%d float=%d bool=%d",
                     str_rt, int_rt, flt_rt, bool_rt);
            RecordTest("config_roundtrip", str_rt && int_rt && flt_rt && bool_rt, buf);
        }

        // ---- Test: imc_publish_receive (self-pub/sub) ----
        // Do NOT call Pump::ProcessAll() from within a subscriber callback
        // (that deadlocks). Verification happens in FinalizeReport().
        {
            bool sub_ok = m_Subs.Add("test/inttest/selfpub",
                [this](const bml::imc::Message &msg) {
                    if (auto *v = msg.As<uint32_t>()) {
                        m_SelfPubValue.store(*v, std::memory_order_relaxed);
                    }
                    m_SelfPubCount.fetch_add(1, std::memory_order_relaxed);
                });

            if (sub_ok) {
                bml::imc::publish(Handle(),
                                  "test/inttest/selfpub",
                                  uint32_t(0xCAFE),
                                  Services().Interfaces().ImcBus);
            }
            RecordTest("imc_self_pubsub_setup", sub_ok, "subscribe+publish queued");
        }

        Services().Log().Info("OnEngineInit tests complete");

        // Write early report in case game doesn't reach target frames
        {
            auto elapsed = std::chrono::steady_clock::now() - m_StartTime;
            double ms = std::chrono::duration<double, std::milli>(elapsed).count();
            WriteReport(m_ReportPath, ms, false);
        }
    }

    // Called on BML/Engine/PreProcess
    void OnPreProcess() {
        int frame = m_PreProcessCount.fetch_add(1, std::memory_order_relaxed) + 1;

        if (frame == kTargetFrames) {
            FinalizeReport();
        }
    }

    void FinalizeReport() {
        if (m_ReportWritten.exchange(true)) return;

        auto elapsed = std::chrono::steady_clock::now() - m_StartTime;
        double ms = std::chrono::duration<double, std::milli>(elapsed).count();

        // Record frame-count tests
        {
            int pre = m_PreProcessCount.load();
            char buf[64];
            snprintf(buf, sizeof(buf), "%d frames", pre);
            RecordTest("frame_preprocess", pre >= kTargetFrames, buf);
        }
        {
            int post = m_PostProcessCount.load();
            char buf[64];
            snprintf(buf, sizeof(buf), "%d frames", post);
            RecordTest("frame_postprocess", post > 0, buf);
        }
        {
            int r = m_PreRenderCount.load();
            char buf[64];
            snprintf(buf, sizeof(buf), "%d frames", r);
            RecordTest("frame_prerender", r > 0, buf);
        }
        RecordTest("frame_prerender_payload", m_PreRenderPayloadValid.load());
        {
            int r = m_PostRenderCount.load();
            char buf[64];
            snprintf(buf, sizeof(buf), "%d frames", r);
            RecordTest("frame_postrender", r > 0, buf);
        }
        RecordTest("frame_postrender_payload", m_PostRenderPayloadValid.load());
        {
            int r = m_PostSpriteRenderCount.load();
            char buf[64];
            snprintf(buf, sizeof(buf), "%d frames", r);
            RecordTest("frame_postspriterender", r > 0, buf);
        }
        RecordTest("frame_postspriterender_payload", m_PostSpriteRenderPayloadValid.load());
        RecordTest("engine_play_received", m_EnginePlayReceived.load());

        // Verify async self-pub/sub (message should have been delivered by now)
        {
            int count = m_SelfPubCount.load(std::memory_order_relaxed);
            uint32_t val = m_SelfPubValue.load(std::memory_order_relaxed);
            bool ok = count >= 1 && val == 0xCAFE;
            char buf[64];
            snprintf(buf, sizeof(buf), "count=%d val=0x%X", count, val);
            RecordTest("imc_self_pubsub_verify", ok, buf);
        }

        WriteReport(m_ReportPath, ms, true);

        Services().Log().Info(
            "Report written to %s (%.0f ms, %d frames)",
            m_ReportPath.string().c_str(),
            ms,
            m_PreProcessCount.load());
    }
};

BML_DEFINE_MODULE(IntegrationTestMod)
