#include "ProfilingManager.h"
#include "ApiRegistrationMacros.h"
#include "bml_capabilities.h"

namespace BML::Core {
    /* Trace Events */
    void BML_API_TraceBegin(const char *name, const char *category) {
        ProfilingManager::Instance().TraceBegin(name, category);
    }

    void BML_API_TraceEnd() {
        ProfilingManager::Instance().TraceEnd();
    }

    void BML_API_TraceInstant(const char *name, const char *category) {
        ProfilingManager::Instance().TraceInstant(name, category);
    }

    void BML_API_TraceSetThreadName(const char *name) {
        ProfilingManager::Instance().TraceSetThreadName(name);
    }

    void BML_API_TraceCounter(const char *name, int64_t value) {
        ProfilingManager::Instance().TraceCounter(name, value);
    }

    void BML_API_TraceFrameMark() {
        ProfilingManager::Instance().TraceFrameMark();
    }

    /* Performance Counters */
    uint64_t BML_API_GetApiCallCount(const char *api_name) {
        return ProfilingManager::Instance().GetApiCallCount(api_name);
    }

    uint64_t BML_API_GetTotalAllocBytes() {
        return ProfilingManager::Instance().GetTotalAllocBytes();
    }

    uint64_t BML_API_GetTimestampNs() {
        return ProfilingManager::Instance().GetTimestampNs();
    }

    uint64_t BML_API_GetCpuFrequency() {
        return ProfilingManager::Instance().GetCpuFrequency();
    }

    /* Backend Control */
    BML_ProfilerBackend BML_API_GetProfilerBackend() {
        return ProfilingManager::Instance().GetProfilerBackend();
    }

    BML_Result BML_API_SetProfilingEnabled(BML_Bool enable) {
        return ProfilingManager::Instance().SetProfilingEnabled(enable);
    }

    BML_Bool BML_API_IsProfilingEnabled() {
        return ProfilingManager::Instance().IsProfilingEnabled();
    }

    BML_Result BML_API_FlushProfilingData(const char *filename) {
        return ProfilingManager::Instance().FlushProfilingData(filename);
    }

    /* Statistics */
    BML_Result BML_API_GetProfilingStats(BML_ProfilingStats *out_stats) {
        return ProfilingManager::Instance().GetProfilingStats(out_stats);
    }

    BML_Result BML_API_ProfilingGetCaps(BML_ProfilingCaps *out_caps) {
        return ProfilingManager::Instance().GetProfilingCaps(out_caps);
    }

    void RegisterProfilingApis() {
        BML_BEGIN_API_REGISTRATION();

        /* Trace Events - simple functions, no error guard needed */
        BML_REGISTER_API_WITH_CAPS(bmlTraceBegin, BML_API_TraceBegin, BML_CAP_PROFILING_TRACE);
        BML_REGISTER_API_WITH_CAPS(bmlTraceEnd, BML_API_TraceEnd, BML_CAP_PROFILING_TRACE);
        BML_REGISTER_API_WITH_CAPS(bmlTraceInstant, BML_API_TraceInstant, BML_CAP_PROFILING_TRACE);
        BML_REGISTER_API_WITH_CAPS(bmlTraceSetThreadName, BML_API_TraceSetThreadName, BML_CAP_PROFILING_TRACE);
        BML_REGISTER_API_WITH_CAPS(bmlTraceCounter, BML_API_TraceCounter, BML_CAP_PROFILING_TRACE);
        BML_REGISTER_API_WITH_CAPS(bmlTraceFrameMark, BML_API_TraceFrameMark, BML_CAP_PROFILING_TRACE);

        /* Performance Counters - simple getters */
        BML_REGISTER_API_WITH_CAPS(bmlGetApiCallCount, BML_API_GetApiCallCount, BML_CAP_PROFILING_STATS);
        BML_REGISTER_API_WITH_CAPS(bmlGetTotalAllocBytes, BML_API_GetTotalAllocBytes, BML_CAP_PROFILING_STATS);
        BML_REGISTER_API_WITH_CAPS(bmlGetTimestampNs, BML_API_GetTimestampNs, BML_CAP_PROFILING_STATS);
        BML_REGISTER_API_WITH_CAPS(bmlGetCpuFrequency, BML_API_GetCpuFrequency, BML_CAP_PROFILING_STATS);

        /* Backend Control */
        BML_REGISTER_API_WITH_CAPS(bmlGetProfilerBackend, BML_API_GetProfilerBackend, BML_CAP_PROFILING_TRACE);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlSetProfilingEnabled, "profiling", BML_API_SetProfilingEnabled, BML_CAP_PROFILING_TRACE);
        BML_REGISTER_API_WITH_CAPS(bmlIsProfilingEnabled, BML_API_IsProfilingEnabled, BML_CAP_PROFILING_TRACE);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlFlushProfilingData, "profiling", BML_API_FlushProfilingData, BML_CAP_PROFILING_TRACE);

        /* Statistics */
        BML_REGISTER_CAPS_API_WITH_CAPS(bmlGetProfilingStats, "profiling.stats", BML_API_GetProfilingStats, BML_CAP_PROFILING_STATS);
        BML_REGISTER_CAPS_API_WITH_CAPS(bmlProfilingGetCaps, "profiling.caps", BML_API_ProfilingGetCaps, BML_CAP_PROFILING_TRACE | BML_CAP_PROFILING_STATS);
    }
} // namespace BML::Core
