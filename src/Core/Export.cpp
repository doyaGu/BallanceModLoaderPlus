#include "bml_export.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "ApiRegistrationMacros.h"
#include "RuntimeInterfaces.h"
#include "KernelServices.h"
#include "Microkernel.h"
#include "StringUtils.h"

namespace BML::Core {
    BML_Result BML_API_InterfaceRegister(BML_Mod owner, const BML_InterfaceDesc *desc);
    BML_Result BML_API_InterfaceAcquire(BML_Mod owner,
                                        const char *interface_id,
                                        const BML_Version *required_abi,
                                        const void **out_implementation,
                                        BML_InterfaceLease *out_lease);
    BML_Result BML_API_InterfaceRelease(BML_InterfaceLease lease);
    BML_Result BML_API_InterfaceUnregister(BML_Mod owner, const char *interface_id);
} // namespace BML::Core

struct BML_Runtime_T {
    std::unique_ptr<BML::Core::RuntimeState, void (*)(BML::Core::RuntimeState *)> state;
    std::wstring mods_dir;
    bool live{false};
    std::atomic<uint32_t> active_calls{0};

    BML_Runtime_T()
        : state(BML::Core::CreateRuntimeState(), &BML::Core::DestroyRuntimeState) {}
};

namespace {
    std::mutex g_RuntimeRegistryMutex;
    std::vector<BML_Runtime> g_LiveRuntimes;

    BML::Core::KernelServices *GetRuntimeKernelUnchecked(BML_Runtime runtime) {
        if (!runtime || !runtime->state) {
            return nullptr;
        }
        return BML::Core::GetRuntimeKernel(*runtime->state);
    }

    bool IsLiveRuntimeLocked(BML_Runtime runtime) {
        return runtime != nullptr &&
            std::find(g_LiveRuntimes.begin(), g_LiveRuntimes.end(), runtime) != g_LiveRuntimes.end();
    }

    class RuntimeCallGuard {
    public:
        RuntimeCallGuard() = default;

        explicit RuntimeCallGuard(BML_Runtime runtime)
            : m_Runtime(runtime) {}

        RuntimeCallGuard(const RuntimeCallGuard &) = delete;
        RuntimeCallGuard &operator=(const RuntimeCallGuard &) = delete;

        RuntimeCallGuard(RuntimeCallGuard &&other) noexcept
            : m_Runtime(other.m_Runtime) {
            other.m_Runtime = nullptr;
        }

        RuntimeCallGuard &operator=(RuntimeCallGuard &&other) noexcept {
            if (this == &other) {
                return *this;
            }
            Release();
            m_Runtime = other.m_Runtime;
            other.m_Runtime = nullptr;
            return *this;
        }

        ~RuntimeCallGuard() {
            Release();
        }

        [[nodiscard]] explicit operator bool() const noexcept {
            return m_Runtime != nullptr;
        }

        [[nodiscard]] BML_Runtime GetRuntime() const noexcept {
            return m_Runtime;
        }

        [[nodiscard]] BML::Core::KernelServices *GetKernel() const noexcept {
            return GetRuntimeKernelUnchecked(m_Runtime);
        }

    private:
        void Release() noexcept {
            if (!m_Runtime) {
                return;
            }
            m_Runtime->active_calls.fetch_sub(1, std::memory_order_acq_rel);
            m_Runtime = nullptr;
        }

        BML_Runtime m_Runtime{nullptr};
    };

    RuntimeCallGuard AcquireRuntimeCall(BML_Runtime runtime) {
        std::lock_guard<std::mutex> lock(g_RuntimeRegistryMutex);
        if (!runtime || !IsLiveRuntimeLocked(runtime) || !runtime->live) {
            return {};
        }

        runtime->active_calls.fetch_add(1, std::memory_order_acq_rel);
        return RuntimeCallGuard(runtime);
    }

    void WaitForRuntimeCallsToDrain(BML_Runtime runtime) {
        while (runtime && runtime->active_calls.load(std::memory_order_acquire) != 0) {
            std::this_thread::yield();
        }
    }

    bool IsBootstrapProcName(const char *proc_name) {
        if (!proc_name) {
            return false;
        }
        return std::strcmp(proc_name, "bmlGetProcAddress") == 0 ||
            std::strcmp(proc_name, "bmlInterfaceRegister") == 0 ||
            std::strcmp(proc_name, "bmlInterfaceAcquire") == 0 ||
            std::strcmp(proc_name, "bmlInterfaceRelease") == 0 ||
            std::strcmp(proc_name, "bmlInterfaceUnregister") == 0;
    }

    void *ResolveBootstrapProc(const char *proc_name) {
        if (!IsBootstrapProcName(proc_name)) {
            return nullptr;
        }

        if (std::strcmp(proc_name, "bmlGetProcAddress") == 0) {
            return reinterpret_cast<void *>(&::bmlGetProcAddress);
        }
        if (std::strcmp(proc_name, "bmlInterfaceRegister") == 0) {
            return reinterpret_cast<void *>(&BML::Core::BML_API_InterfaceRegister);
        }
        if (std::strcmp(proc_name, "bmlInterfaceAcquire") == 0) {
            return reinterpret_cast<void *>(&BML::Core::BML_API_InterfaceAcquire);
        }
        if (std::strcmp(proc_name, "bmlInterfaceRelease") == 0) {
            return reinterpret_cast<void *>(&BML::Core::BML_API_InterfaceRelease);
        }
        if (std::strcmp(proc_name, "bmlInterfaceUnregister") == 0) {
            return reinterpret_cast<void *>(&BML::Core::BML_API_InterfaceUnregister);
        }
        return nullptr;
    }

    bool RegisterRuntime(BML_Runtime runtime) {
        std::lock_guard<std::mutex> lock(g_RuntimeRegistryMutex);
        runtime->live = true;
        g_LiveRuntimes.push_back(runtime);
        return true;
    }

    bool BeginRuntimeDestroy(BML_Runtime runtime, BML::Core::KernelServices **out_kernel) {
        if (!out_kernel) {
            return false;
        }

        std::lock_guard<std::mutex> lock(g_RuntimeRegistryMutex);
        if (!IsLiveRuntimeLocked(runtime)) {
            *out_kernel = nullptr;
            return false;
        }

        *out_kernel = GetRuntimeKernelUnchecked(runtime);
        runtime->live = false;

        const auto it = std::find(g_LiveRuntimes.begin(), g_LiveRuntimes.end(), runtime);
        if (it != g_LiveRuntimes.end()) {
            g_LiveRuntimes.erase(it);
        }

        return true;
    }
} // namespace

BML_BEGIN_CDECLS

BML_API void *bmlGetProcAddress(const char *proc_name) {
    return ResolveBootstrapProc(proc_name);
}

BML_API BML_Result bmlRuntimeCreate(const BML_RuntimeConfig *config, BML_Runtime *out_runtime) {
    if (!out_runtime) {
        return BML_RESULT_INVALID_ARGUMENT;
    }
    *out_runtime = nullptr;

    if (config && config->struct_size < sizeof(BML_RuntimeConfig)) {
        return BML_RESULT_INVALID_SIZE;
    }

    auto runtime = std::make_unique<BML_Runtime_T>();
    if (!runtime || !runtime->state) {
        return BML_RESULT_OUT_OF_MEMORY;
    }

    if (!BML::Core::InitializeCore(*runtime->state)) {
        return BML_RESULT_FAIL;
    }

    if (config && config->mods_dir_utf8 && config->mods_dir_utf8[0] != '\0') {
        runtime->mods_dir = utils::Utf8ToUtf16(config->mods_dir_utf8);
    }

    *out_runtime = runtime.release();
    if (!RegisterRuntime(*out_runtime)) {
        BML::Core::ShutdownMicrokernel(*(*out_runtime)->state);
        delete *out_runtime;
        *out_runtime = nullptr;
        return BML_RESULT_OUT_OF_MEMORY;
    }

    if (auto *kernel = GetRuntimeKernelUnchecked(*out_runtime)) {
        kernel->runtime = *out_runtime;
        BML::Core::RefreshRuntimeInterfaces(*kernel);
    }
    return BML_RESULT_OK;
}

BML_API BML_Result bmlRuntimeDiscoverModules(BML_Runtime runtime) {
    auto guard = AcquireRuntimeCall(runtime);
    auto *kernel = guard.GetKernel();
    if (!kernel) {
        return BML_RESULT_INVALID_HANDLE;
    }

    const auto &mods_dir = guard.GetRuntime()->mods_dir;
    const bool ok = mods_dir.empty()
        ? BML::Core::DiscoverModules(*guard.GetRuntime()->state)
        : BML::Core::DiscoverModulesInDirectory(*guard.GetRuntime()->state, mods_dir);
    return ok ? BML_RESULT_OK : BML_RESULT_FAIL;
}

BML_API BML_Result bmlRuntimeLoadModules(BML_Runtime runtime) {
    auto guard = AcquireRuntimeCall(runtime);
    auto *kernel = guard.GetKernel();
    if (!kernel) {
        return BML_RESULT_INVALID_HANDLE;
    }

    const BML_Services *services = nullptr;
    if (kernel->context && kernel->context->GetServiceHub()) {
        services = &kernel->context->GetServiceHub()->Interfaces();
    }
    return BML::Core::LoadDiscoveredModules(*guard.GetRuntime()->state, services)
        ? BML_RESULT_OK
        : BML_RESULT_FAIL;
}

BML_API const BML_Services *bmlRuntimeGetServices(BML_Runtime runtime) {
    auto guard = AcquireRuntimeCall(runtime);
    auto *kernel = guard.GetKernel();
    if (!kernel || !kernel->context || !kernel->context->GetServiceHub()) {
        return nullptr;
    }
    return &kernel->context->GetServiceHub()->Interfaces();
}

BML_API void bmlRuntimeUpdate(BML_Runtime runtime) {
    auto guard = AcquireRuntimeCall(runtime);
    auto *kernel = guard.GetKernel();
    if (!kernel) {
        return;
    }

    BML::Core::UpdateMicrokernel(*guard.GetRuntime()->state);
}

BML_API const BML_BootstrapDiagnostics *bmlRuntimeGetBootstrapDiagnostics(BML_Runtime runtime) {
    auto guard = AcquireRuntimeCall(runtime);
    auto *kernel = guard.GetKernel();
    if (!kernel) {
        return nullptr;
    }

    return &BML::Core::GetPublicDiagnostics(*guard.GetRuntime()->state);
}

BML_API void bmlRuntimeDestroy(BML_Runtime runtime) {
    if (!runtime) {
        return;
    }

    BML::Core::KernelServices *kernel = nullptr;
    if (!BeginRuntimeDestroy(runtime, &kernel)) {
        return;
    }
    WaitForRuntimeCallsToDrain(runtime);

    if (kernel != nullptr) {
        BML::Core::ShutdownMicrokernel(*runtime->state);
        kernel->runtime = nullptr;
    }

    delete runtime;
}

BML_END_CDECLS

namespace BML::Core {
    void RegisterBootstrapExports(ApiRegistry &apiRegistry) {
        BML_BEGIN_API_REGISTRATION(apiRegistry);

        BML_REGISTER_API(bmlGetProcAddress, ::bmlGetProcAddress);
    }
} // namespace BML::Core
