#include "ProfilingManager.h"
#include "ApiRegistrationMacros.h"

namespace BML::Core {
    /* Trace Events */
    void BML_API_TraceBegin(const char *name, const char *category) {
        GetKernelOrNull()->profiling->TraceBegin(name, category);
    }

    void BML_API_TraceEnd() {
        GetKernelOrNull()->profiling->TraceEnd();
    }

    void BML_API_TraceInstant(const char *name, const char *category) {
        GetKernelOrNull()->profiling->TraceInstant(name, category);
    }

    void BML_API_TraceSetThreadName(const char *name) {
        GetKernelOrNull()->profiling->TraceSetThreadName(name);
    }

    void BML_API_TraceCounter(const char *name, int64_t value) {
        GetKernelOrNull()->profiling->TraceCounter(name, value);
    }

    void BML_API_TraceFrameMark() {
        GetKernelOrNull()->profiling->TraceFrameMark();
    }

    /* Performance Counters */
    uint64_t BML_API_GetApiCallCount(const char *api_name) {
        return GetKernelOrNull()->profiling->GetApiCallCount(api_name);
    }

    uint64_t BML_API_GetTotalAllocBytes() {
        return GetKernelOrNull()->profiling->GetTotalAllocBytes();
    }

    uint64_t BML_API_GetTimestampNs() {
        return GetKernelOrNull()->profiling->GetTimestampNs();
    }

    uint64_t BML_API_GetCpuFrequency() {
        return GetKernelOrNull()->profiling->GetCpuFrequency();
    }

    /* Backend Control */
    BML_ProfilerBackend BML_API_GetProfilerBackend() {
        return GetKernelOrNull()->profiling->GetProfilerBackend();
    }

    BML_Result BML_API_SetProfilingEnabled(BML_Bool enable) {
        return GetKernelOrNull()->profiling->SetProfilingEnabled(enable);
    }

    BML_Bool BML_API_IsProfilingEnabled() {
        return GetKernelOrNull()->profiling->IsProfilingEnabled();
    }

    BML_Result BML_API_FlushProfilingData(const char *filename) {
        return GetKernelOrNull()->profiling->FlushProfilingData(filename);
    }

    /* Statistics */
    BML_Result BML_API_GetProfilingStats(BML_ProfilingStats *out_stats) {
        return GetKernelOrNull()->profiling->GetProfilingStats(out_stats);
    }

    void RegisterProfilingApis() {
        BML_BEGIN_API_REGISTRATION();

        /* Trace Events - simple functions, no error guard needed */
        BML_REGISTER_API(bmlTraceBegin, BML_API_TraceBegin);
        BML_REGISTER_API(bmlTraceEnd, BML_API_TraceEnd);
        BML_REGISTER_API(bmlTraceInstant, BML_API_TraceInstant);
        BML_REGISTER_API(bmlTraceSetThreadName, BML_API_TraceSetThreadName);
        BML_REGISTER_API(bmlTraceCounter, BML_API_TraceCounter);
        BML_REGISTER_API(bmlTraceFrameMark, BML_API_TraceFrameMark);

        /* Performance Counters - simple getters */
        BML_REGISTER_API(bmlGetApiCallCount, BML_API_GetApiCallCount);
        BML_REGISTER_API(bmlGetTotalAllocBytes, BML_API_GetTotalAllocBytes);
        BML_REGISTER_API(bmlGetTimestampNs, BML_API_GetTimestampNs);
        BML_REGISTER_API(bmlGetCpuFrequency, BML_API_GetCpuFrequency);

        /* Backend Control */
        BML_REGISTER_API(bmlGetProfilerBackend, BML_API_GetProfilerBackend);
        BML_REGISTER_API_GUARDED(bmlSetProfilingEnabled, "profiling", BML_API_SetProfilingEnabled);
        BML_REGISTER_API(bmlIsProfilingEnabled, BML_API_IsProfilingEnabled);
        BML_REGISTER_API_GUARDED(bmlFlushProfilingData, "profiling", BML_API_FlushProfilingData);

        /* Statistics */
        BML_REGISTER_API_GUARDED(bmlGetProfilingStats, "profiling.stats", BML_API_GetProfilingStats);
    }
} // namespace BML::Core
