#ifndef BML_CORE_API_REGISTRATION_H
#define BML_CORE_API_REGISTRATION_H

namespace BML::Core {
    void RegisterCoreApis();
    void RegisterLoggingApis();
    void RegisterConfigApis();
    void RegisterImcApis();
    void RegisterResourceApis();
    void RegisterExtensionApis();
    void RegisterMemoryApis();
    void RegisterDiagnosticApis();
    void RegisterSyncApis();
    void RegisterCapabilityApis();
    void RegisterTracingApis();
    void RegisterProfilingApis();
} // namespace BML::Core

#endif // BML_CORE_API_REGISTRATION_H
