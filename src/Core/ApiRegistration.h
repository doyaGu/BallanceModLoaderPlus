#ifndef BML_CORE_API_REGISTRATION_H
#define BML_CORE_API_REGISTRATION_H

namespace BML::Core {
    void RegisterBootstrapExports();
    void RegisterBuiltinInterfaces();
    void RegisterCoreApis();
    void RegisterLoggingApis();
    void RegisterConfigApis();
    void RegisterImcApis();
    void RegisterResourceApis();
    void RegisterInterfaceApis();
    void RegisterMemoryApis();
    void RegisterDiagnosticApis();
    void RegisterSyncApis();
    void RegisterTracingApis();
    void RegisterProfilingApis();
    void RegisterTimerApis();
    void RegisterLocaleApis();
    void RegisterHookApis();
} // namespace BML::Core

#endif // BML_CORE_API_REGISTRATION_H
