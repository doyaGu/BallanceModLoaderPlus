#include "ProfilingManager.h"
#include "ApiRegistrationMacros.h"
#include "bml_capabilities.h"

namespace BML::Core {

namespace {

/* Trace Events */
void Impl_TraceBegin(const char *name, const char *category) {
    ProfilingManager::Instance().TraceBegin(name, category);
}

void Impl_TraceEnd() {
    ProfilingManager::Instance().TraceEnd();
}

void Impl_TraceInstant(const char *name, const char *category) {
    ProfilingManager::Instance().TraceInstant(name, category);
}

void Impl_TraceSetThreadName(const char *name) {
    ProfilingManager::Instance().TraceSetThreadName(name);
}

void Impl_TraceCounter(const char *name, int64_t value) {
    ProfilingManager::Instance().TraceCounter(name, value);
}

void Impl_TraceFrameMark() {
    ProfilingManager::Instance().TraceFrameMark();
}

/* Performance Counters */
uint64_t Impl_GetApiCallCount(const char *api_name) {
    return ProfilingManager::Instance().GetApiCallCount(api_name);
}

uint64_t Impl_GetTotalAllocBytes() {
    return ProfilingManager::Instance().GetTotalAllocBytes();
}

uint64_t Impl_GetTimestampNs() {
    return ProfilingManager::Instance().GetTimestampNs();
}

uint64_t Impl_GetCpuFrequency() {
    return ProfilingManager::Instance().GetCpuFrequency();
}

/* Backend Control */
BML_ProfilerBackend Impl_GetProfilerBackend() {
    return ProfilingManager::Instance().GetProfilerBackend();
}

BML_Result Impl_SetProfilingEnabled(BML_Bool enable) {
    return ProfilingManager::Instance().SetProfilingEnabled(enable);
}

BML_Bool Impl_IsProfilingEnabled() {
    return ProfilingManager::Instance().IsProfilingEnabled();
}

BML_Result Impl_FlushProfilingData(const char *filename) {
    return ProfilingManager::Instance().FlushProfilingData(filename);
}

/* Statistics */
BML_Result Impl_GetProfilingStats(BML_ProfilingStats *out_stats) {
    return ProfilingManager::Instance().GetProfilingStats(out_stats);
}

BML_Result Impl_GetProfilingCaps(BML_ProfilingCaps *out_caps) {
    return ProfilingManager::Instance().GetProfilingCaps(out_caps);
}

} // namespace

/* Register all profiling APIs */
void RegisterProfilingApis() {
    BML_BEGIN_API_REGISTRATION();

    /* Trace Events - simple functions, no error guard needed */
    BML_REGISTER_API_WITH_CAPS(bmlTraceBegin, Impl_TraceBegin, BML_CAP_PROFILING_TRACE);
    BML_REGISTER_API_WITH_CAPS(bmlTraceEnd, Impl_TraceEnd, BML_CAP_PROFILING_TRACE);
    BML_REGISTER_API_WITH_CAPS(bmlTraceInstant, Impl_TraceInstant, BML_CAP_PROFILING_TRACE);
    BML_REGISTER_API_WITH_CAPS(bmlTraceSetThreadName, Impl_TraceSetThreadName, BML_CAP_PROFILING_TRACE);
    BML_REGISTER_API_WITH_CAPS(bmlTraceCounter, Impl_TraceCounter, BML_CAP_PROFILING_TRACE);
    BML_REGISTER_API_WITH_CAPS(bmlTraceFrameMark, Impl_TraceFrameMark, BML_CAP_PROFILING_TRACE);

    /* Performance Counters - simple getters */
    BML_REGISTER_API_WITH_CAPS(bmlGetApiCallCount, Impl_GetApiCallCount, BML_CAP_PROFILING_STATS);
    BML_REGISTER_API_WITH_CAPS(bmlGetTotalAllocBytes, Impl_GetTotalAllocBytes, BML_CAP_PROFILING_STATS);
    BML_REGISTER_API_WITH_CAPS(bmlGetTimestampNs, Impl_GetTimestampNs, BML_CAP_PROFILING_STATS);
    BML_REGISTER_API_WITH_CAPS(bmlGetCpuFrequency, Impl_GetCpuFrequency, BML_CAP_PROFILING_STATS);

    /* Backend Control */
    BML_REGISTER_API_WITH_CAPS(bmlGetProfilerBackend, Impl_GetProfilerBackend, BML_CAP_PROFILING_TRACE);
    BML_REGISTER_API_GUARDED_WITH_CAPS(bmlSetProfilingEnabled, "profiling", Impl_SetProfilingEnabled, BML_CAP_PROFILING_TRACE);
    BML_REGISTER_API_WITH_CAPS(bmlIsProfilingEnabled, Impl_IsProfilingEnabled, BML_CAP_PROFILING_TRACE);
    BML_REGISTER_API_GUARDED_WITH_CAPS(bmlFlushProfilingData, "profiling", Impl_FlushProfilingData, BML_CAP_PROFILING_TRACE);

    /* Statistics */
    BML_REGISTER_CAPS_API_WITH_CAPS(bmlGetProfilingStats, "profiling.stats", Impl_GetProfilingStats, BML_CAP_PROFILING_STATS);
    BML_REGISTER_CAPS_API_WITH_CAPS(bmlGetProfilingCaps, "profiling.caps", Impl_GetProfilingCaps, BML_CAP_PROFILING_TRACE | BML_CAP_PROFILING_STATS);
}

} // namespace BML::Core
