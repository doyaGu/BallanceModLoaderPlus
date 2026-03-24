#ifndef BML_CORE_API_REGISTRATION_H
#define BML_CORE_API_REGISTRATION_H

namespace BML::Core {
    class ApiRegistry;
    struct KernelServices;

    void RegisterBootstrapExports(ApiRegistry &registry);
    void RegisterRuntimeInterfaces(KernelServices &kernel);
    void RegisterCoreApis(ApiRegistry &registry);
    void RegisterLoggingApis(ApiRegistry &registry);
    void RegisterConfigApis(ApiRegistry &registry);
    void RegisterImcApis(ApiRegistry &registry);
    void RegisterResourceApis(ApiRegistry &registry);
    void RegisterInterfaceApis(ApiRegistry &registry);
    void RegisterMemoryApis(ApiRegistry &registry);
    void RegisterDiagnosticApis(ApiRegistry &registry);
    void RegisterSyncApis(ApiRegistry &registry);
    void RegisterTracingApis(ApiRegistry &registry);
    void RegisterProfilingApis(ApiRegistry &registry);
    void RegisterTimerApis(ApiRegistry &registry);
    void RegisterLocaleApis(ApiRegistry &registry);
    void RegisterHookApis(ApiRegistry &registry);

} // namespace BML::Core

#endif // BML_CORE_API_REGISTRATION_H
