#include "Microkernel.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#include "ApiRegistration.h"
#include "ApiRegistry.h"
#include "RuntimeInterfaces.h"
#include "ConfigStore.h"
#include "Context.h"
#include "CrashDumpWriter.h"
#include "DiagnosticManager.h"
#include "FaultTracker.h"
#include "HookRegistry.h"
#include "ImcBus.h"
#include "InterfaceRegistry.h"
#include "KernelServices.h"
#include "LeaseManager.h"
#include "LocaleManager.h"
#include "Logging.h"
#include "MemoryManager.h"
#include "ProfilingManager.h"
#include "StringUtils.h"
#include "PathUtils.h"
#include "SyncManager.h"
#include "TimerManager.h"

#if defined(_WIN32)
#include <windows.h>
#endif

namespace BML::Core {
    // Defined here (not KernelServices.cpp) so that test targets linking
    // KernelServices.cpp don't pull in L0 subsystem destructors.
    KernelServices::~KernelServices() {
        // Explicit destruction order -- reverse of construction in InitializeCore().
        // IMC bus drains first while Context is still alive; then L3 -> L2 -> L1 -> L0.
        imc_bus.reset();
        context.reset();
        interface_registry.reset();
        api_registry.reset();
        config.reset();
        leases.reset();
        timers.reset();
        locale.reset();
        hooks.reset();
        crash_dump.reset();
        fault_tracker.reset();
        profiling.reset();
        sync.reset();
        memory.reset();
        diagnostics.reset();
    }

    struct RuntimeState {
        std::recursive_mutex state_mutex;
        ModuleRuntime runtime;
        ModuleBootstrapDiagnostics diagnostics;
        std::unique_ptr<KernelServices> kernel;
        bool core_initialized{false};
        bool modules_discovered{false};
        bool modules_loaded{false};
        BML_BootstrapState bootstrap_state{BML_BOOTSTRAP_STATE_NOT_STARTED};

        struct ManifestErrorSnapshot {
            std::string message;
            std::string file;
            bool has_file{false};
            int32_t line{0};
            bool has_line{false};
            int32_t column{0};
            bool has_column{false};
        };

        struct DependencyWarningSnapshot {
            std::string module_id;
            std::string dependency_id;
            std::string message;
        };

        struct DiagnosticsSnapshotStorage {
            std::vector<ManifestErrorSnapshot> manifest_storage;
            std::vector<BML_BootstrapManifestError> manifest_errors;
            std::string dependency_error_message;
            std::vector<std::string> dependency_chain_storage;
            std::vector<const char *> dependency_chain;
            std::vector<DependencyWarningSnapshot> dependency_warning_storage;
            std::vector<BML_BootstrapDependencyWarning> dependency_warnings;
            std::string load_error_module_id;
            std::string load_error_path_utf8;
            std::string load_error_message;
            std::vector<std::string> load_order_storage;
            std::vector<const char *> load_order;
            BML_BootstrapDiagnostics diagnostics{};

            void Reset() {
                manifest_storage.clear();
                manifest_errors.clear();
                dependency_error_message.clear();
                dependency_chain_storage.clear();
                dependency_chain.clear();
                dependency_warning_storage.clear();
                dependency_warnings.clear();
                load_error_module_id.clear();
                load_error_path_utf8.clear();
                load_error_message.clear();
                load_order_storage.clear();
                load_order.clear();
                diagnostics = {};
            }
        } public_diagnostics_snapshot;

        ModuleBootstrapDiagnostics internal_diagnostics_snapshot;
        bool clear_diagnostics_snapshot_on_next_query{false};
    };

    RuntimeState *CreateRuntimeState() {
        return new RuntimeState();
    }

    void DestroyRuntimeState(RuntimeState *state) {
        delete state;
    }

    KernelServices *GetRuntimeKernel(RuntimeState &state) noexcept {
        return state.kernel.get();
    }

    namespace {
        void PopulatePublicDiagnosticsSnapshot(const ModuleBootstrapDiagnostics &diag,
                                              RuntimeState::DiagnosticsSnapshotStorage &snapshot) {
            snapshot.Reset();

            snapshot.manifest_storage.reserve(diag.manifest_errors.size());
            for (const auto &error : diag.manifest_errors) {
                RuntimeState::ManifestErrorSnapshot entry;
                entry.message = error.message;
                if (error.file && !error.file->empty()) {
                    entry.file = *error.file;
                    entry.has_file = true;
                }
                if (error.line) {
                    entry.line = *error.line;
                    entry.has_line = true;
                }
                if (error.column) {
                    entry.column = *error.column;
                    entry.has_column = true;
                }
                snapshot.manifest_storage.push_back(std::move(entry));
            }

            snapshot.manifest_errors.reserve(snapshot.manifest_storage.size());
            for (const auto &entry : snapshot.manifest_storage) {
                BML_BootstrapManifestError public_entry{};
                public_entry.message = entry.message.c_str();
                public_entry.file = entry.has_file ? entry.file.c_str() : nullptr;
                public_entry.line = entry.line;
                public_entry.column = entry.column;
                public_entry.has_file = entry.has_file ? 1 : 0;
                public_entry.has_line = entry.has_line ? 1 : 0;
                public_entry.has_column = entry.has_column ? 1 : 0;
                snapshot.manifest_errors.push_back(public_entry);
            }

            snapshot.dependency_error_message = diag.dependency_error.message;
            snapshot.dependency_chain_storage = diag.dependency_error.chain;
            snapshot.dependency_chain.reserve(snapshot.dependency_chain_storage.size());
            for (const auto &entry : snapshot.dependency_chain_storage) {
                snapshot.dependency_chain.push_back(entry.c_str());
            }

            snapshot.dependency_warning_storage.reserve(diag.dependency_warnings.size());
            for (const auto &warning : diag.dependency_warnings) {
                RuntimeState::DependencyWarningSnapshot entry;
                entry.module_id = warning.mod_id;
                entry.dependency_id = warning.dependency_id;
                entry.message = warning.message;
                snapshot.dependency_warning_storage.push_back(std::move(entry));
            }

            snapshot.dependency_warnings.reserve(snapshot.dependency_warning_storage.size());
            for (const auto &entry : snapshot.dependency_warning_storage) {
                BML_BootstrapDependencyWarning public_entry{};
                public_entry.module_id = entry.module_id.empty() ? nullptr : entry.module_id.c_str();
                public_entry.dependency_id =
                    entry.dependency_id.empty() ? nullptr : entry.dependency_id.c_str();
                public_entry.message = entry.message.empty() ? nullptr : entry.message.c_str();
                snapshot.dependency_warnings.push_back(public_entry);
            }

            snapshot.load_error_module_id = diag.load_error.id;
            snapshot.load_error_path_utf8 = utils::Utf16ToUtf8(diag.load_error.path);
            snapshot.load_error_message = diag.load_error.message;
            snapshot.load_order_storage = diag.load_order;
            snapshot.load_order.reserve(snapshot.load_order_storage.size());
            for (const auto &entry : snapshot.load_order_storage) {
                snapshot.load_order.push_back(entry.c_str());
            }

            snapshot.diagnostics.manifest_errors =
                snapshot.manifest_errors.empty() ? nullptr : snapshot.manifest_errors.data();
            snapshot.diagnostics.manifest_error_count =
                static_cast<uint32_t>(snapshot.manifest_errors.size());
            snapshot.diagnostics.dependency_error.message =
                snapshot.dependency_error_message.empty()
                    ? nullptr
                    : snapshot.dependency_error_message.c_str();
            snapshot.diagnostics.dependency_error.chain =
                snapshot.dependency_chain.empty() ? nullptr : snapshot.dependency_chain.data();
            snapshot.diagnostics.dependency_error.chain_count =
                static_cast<uint32_t>(snapshot.dependency_chain.size());
            snapshot.diagnostics.dependency_warnings =
                snapshot.dependency_warnings.empty() ? nullptr : snapshot.dependency_warnings.data();
            snapshot.diagnostics.dependency_warning_count =
                static_cast<uint32_t>(snapshot.dependency_warnings.size());
            snapshot.diagnostics.load_error.module_id =
                snapshot.load_error_module_id.empty() ? nullptr : snapshot.load_error_module_id.c_str();
            snapshot.diagnostics.load_error.path_utf8 =
                snapshot.load_error_path_utf8.empty() ? nullptr : snapshot.load_error_path_utf8.c_str();
            snapshot.diagnostics.load_error.message =
                snapshot.load_error_message.empty() ? nullptr : snapshot.load_error_message.c_str();
            snapshot.diagnostics.load_error.system_code =
                static_cast<int32_t>(diag.load_error.system_code);
            snapshot.diagnostics.load_error.has_error = diag.load_error.message.empty() ? 0 : 1;
            snapshot.diagnostics.load_order =
                snapshot.load_order.empty() ? nullptr : snapshot.load_order.data();
            snapshot.diagnostics.load_order_count =
                static_cast<uint32_t>(snapshot.load_order.size());
        }

        void PublishDiagnosticsSnapshots(RuntimeState &state, const ModuleBootstrapDiagnostics &diag) {
            state.internal_diagnostics_snapshot = diag;
            PopulatePublicDiagnosticsSnapshot(diag, state.public_diagnostics_snapshot);
            state.clear_diagnostics_snapshot_on_next_query = false;
        }

        void ClearPublishedDiagnosticsSnapshots(RuntimeState &state) {
            state.internal_diagnostics_snapshot = {};
            state.public_diagnostics_snapshot.Reset();
            state.clear_diagnostics_snapshot_on_next_query = false;
        }

        inline void DebugLog(const std::string &message) {
            if (message.empty()) {
                return;
            }

#if defined(_MSC_VER)
            std::string line = "[microkernel] ";
            line += message;
            line.push_back('\n');
            OutputDebugStringA(line.c_str());
#endif

            std::fprintf(stdout, "[microkernel] %s\n", message.c_str());
            std::fflush(stdout);
        }

        std::wstring GetEnvironmentOverride() {
#if defined(_WIN32)
            DWORD required = GetEnvironmentVariableW(L"BML_MODS_DIR", nullptr, 0);
            if (required == 0)
                return {};

            std::wstring buffer;
            buffer.resize(required);
            DWORD copied = GetEnvironmentVariableW(L"BML_MODS_DIR", buffer.data(), required);
            if (copied == 0)
                return {};
            if (copied < buffer.size())
                buffer.resize(copied);
            if (!buffer.empty() && buffer.back() == L'\0')
                buffer.pop_back();
            if (buffer.empty())
                return {};
#else
            const char *raw = std::getenv("BML_MODS_DIR");
            if (!raw || raw[0] == '\0') {
                return {};
            }
            std::wstring buffer = utils::Utf8ToUtf16(raw);
            if (buffer.empty()) {
                return {};
            }
#endif

            std::filesystem::path overridePath(buffer);
            if (!overridePath.is_absolute())
                overridePath = std::filesystem::absolute(overridePath);
            return overridePath.lexically_normal().wstring();
        }

        std::wstring DetectModsDirectory() {
            auto overridePath = GetEnvironmentOverride();
            if (!overridePath.empty()) {
                DebugLog("Using BML_MODS_DIR override: " + utils::Utf16ToUtf8(overridePath));
                return overridePath;
            }
            return utils::GetRuntimeLayout().mods_directory.wstring();
        }

        std::wstring ResolveBootstrapModsDirectory(const BML_BootstrapConfig *config) {
            if (config && config->mods_dir_utf8 && config->mods_dir_utf8[0] != '\0') {
                std::wstring path = utils::Utf8ToUtf16(config->mods_dir_utf8);
                if (!path.empty()) {
                    return std::filesystem::absolute(std::filesystem::path(path)).lexically_normal().wstring();
                }
            }

            return DetectModsDirectory();
        }

        void EmitDiagnostics(const ModuleBootstrapDiagnostics &diag) {
            for (const auto &error : diag.manifest_errors) {
                std::ostringstream oss;
                oss << "Manifest error: " << error.message;
                if (error.file) {
                    oss << " (" << *error.file;
                    if (error.line)
                        oss << ":" << *error.line;
                    if (error.column)
                        oss << "," << *error.column;
                    oss << ")";
                }
                DebugLog(oss.str());
            }

            if (!diag.dependency_error.message.empty()) {
                std::ostringstream oss;
                oss << "Dependency resolution failed: " << diag.dependency_error.message;
                if (!diag.dependency_error.chain.empty()) {
                    oss << " | chain=";
                    for (size_t i = 0; i < diag.dependency_error.chain.size(); ++i) {
                        if (i)
                            oss << " -> ";
                        oss << diag.dependency_error.chain[i];
                    }
                }
                DebugLog(oss.str());
            }

            for (const auto &warning : diag.dependency_warnings) {
                std::ostringstream oss;
                oss << "Dependency warning: mod=" << warning.mod_id
                    << ", dependency=" << warning.dependency_id
                    << " - " << warning.message;
                DebugLog(oss.str());
            }

            if (!diag.load_error.message.empty()) {
                std::ostringstream oss;
                oss << "Module load failed: id=" << diag.load_error.id << ", reason=" << diag.load_error.message;
                if (!diag.load_error.path.empty()) {
                    oss << ", path=" << utils::Utf16ToUtf8(diag.load_error.path);
                }
                if (diag.load_error.system_code != 0) {
                    oss << ", code=" << diag.load_error.system_code;
                }
                DebugLog(oss.str());
            }

            if (!diag.load_order.empty()) {
                std::ostringstream oss;
                oss << "Load order (" << diag.load_order.size() << "): ";
                for (size_t i = 0; i < diag.load_order.size(); ++i) {
                    if (i)
                        oss << ", ";
                    oss << diag.load_order[i];
                }
                DebugLog(oss.str());
            }
        }

    } // namespace

    bool InitializeCore(RuntimeState &state) {
        std::lock_guard<std::recursive_mutex> lock(state.state_mutex);
        if (state.core_initialized)
            return true;

        DebugLog("Phase 0: Initializing core...");

        ClearPublishedDiagnosticsSnapshots(state);

        // Build the service graph.  All subsystems owned by KernelServices.
        state.kernel = std::make_unique<KernelServices>();
        auto &k = *state.kernel;
        // L0 - no dependencies
        k.diagnostics        = std::make_unique<DiagnosticManager>();
        k.memory             = std::make_unique<MemoryManager>();
        k.sync               = std::make_unique<SyncManager>();
        // L1 - depend on L0 (no injection yet)
        k.fault_tracker      = std::make_unique<FaultTracker>();
        k.crash_dump         = std::make_unique<CrashDumpWriter>();
        k.hooks              = std::make_unique<HookRegistry>();
        k.locale             = std::make_unique<LocaleManager>();
        k.leases             = std::make_unique<LeaseManager>();
        k.config             = std::make_unique<ConfigStore>();
        // L2 - depend on L0/L1
        k.api_registry       = std::make_unique<ApiRegistry>();
        k.profiling          = std::make_unique<ProfilingManager>(*k.api_registry, *k.memory);
        k.imc_bus            = std::make_unique<ImcBus>();
        // L3 - depend on L0/L1/L2
        k.context            = std::make_unique<Context>(*k.api_registry, *k.config, *k.crash_dump, *k.fault_tracker);
        k.context->BindKernel(k);
        k.interface_registry = std::make_unique<InterfaceRegistry>(*k.context, *k.leases);
        k.timers             = std::make_unique<TimerManager>(*k.context);
        state.runtime.BindKernel(k);

        // Initialize context with runtime version
        k.context->Initialize(bmlMakeVersion(0, 4, 0));

        // Bind runtime deps (two-phase: constructed before Context)
        k.config->BindContext(*k.context);
        k.imc_bus->BindDeps(*k.context);

        RegisterBootstrapExports(*k.api_registry);

        // Register core APIs
        RegisterCoreApis(*k.api_registry);
        RegisterRuntimeInterfaces(k);
        PublishDiagnosticsSnapshots(state, state.diagnostics);

        state.core_initialized = true;
        state.modules_discovered = false;
        state.modules_loaded = false;
        state.bootstrap_state = BML_BOOTSTRAP_STATE_CORE_INITIALIZED;
        DebugLog("Core initialized successfully");
        return true;
    }

    bool DiscoverModules(RuntimeState &state) {
        return DiscoverModulesInDirectory(state, DetectModsDirectory());
    }

    bool DiscoverModulesInDirectory(RuntimeState &state, const std::wstring &mods_dir) {
        std::lock_guard<std::recursive_mutex> lock(state.state_mutex);
        if (!state.core_initialized || !state.kernel) {
            DebugLog("DiscoverModulesInDirectory: Core not initialized");
            return false;
        }

        if (state.modules_discovered)
            return true;

        DebugLog("Phase 1: Discovering modules...");

        // Initialize crash isolation subsystems with ModLoader runtime directory.
        {
            const auto layout = utils::ResolveRuntimeLayoutFromModsDirectory(mods_dir);
            state.kernel->fault_tracker->LoadFromRuntimeDirectory(layout.runtime_directory);
            state.kernel->crash_dump->SetRuntimeDirectory(layout.runtime_directory);
        }

        ModuleBootstrapDiagnostics diag;

        // Only discover and validate, don't load DLLs yet
        if (!state.runtime.DiscoverAndValidate(mods_dir, diag)) {
            state.diagnostics = diag;
            PublishDiagnosticsSnapshots(state, diag);
            EmitDiagnostics(diag);
            DebugLog("Module discovery failed");
            // Keep context alive for later retry
            return false;
        }

        state.diagnostics = diag;
        PublishDiagnosticsSnapshots(state, diag);
        EmitDiagnostics(diag);
        state.modules_discovered = true;
        state.bootstrap_state = BML_BOOTSTRAP_STATE_MODULES_DISCOVERED;
        DebugLog("Module discovery completed successfully");
        return true;
    }

    bool LoadDiscoveredModules(RuntimeState &state,
                               const BML_Services *services) {
        std::lock_guard<std::recursive_mutex> lock(state.state_mutex);
        if (!state.modules_discovered || !state.kernel) {
            DebugLog("LoadDiscoveredModules: Modules not discovered");
            return false;
        }

        if (state.modules_loaded)
            return true;

        DebugLog("Phase 2: Loading discovered modules...");

        ModuleBootstrapDiagnostics diag;

        // Load the previously discovered modules
        if (!state.runtime.LoadDiscovered(diag, services)) {
            state.diagnostics = diag;
            PublishDiagnosticsSnapshots(state, diag);
            EmitDiagnostics(diag);
            DebugLog("Module loading failed");
            return false;
        }

        state.diagnostics = diag;
        PublishDiagnosticsSnapshots(state, diag);
        state.runtime.SetDiagnosticsCallback([&state](const ModuleBootstrapDiagnostics &new_diag) {
            std::lock_guard<std::recursive_mutex> callback_lock(state.state_mutex);
            if (state.kernel) {
                state.diagnostics = new_diag;
                PublishDiagnosticsSnapshots(state, new_diag);
                EmitDiagnostics(new_diag);
            }
        });
        state.modules_loaded = true;
        state.bootstrap_state = BML_BOOTSTRAP_STATE_READY;
        EmitDiagnostics(diag);
        DebugLog("Modules loaded successfully");
        return true;
    }

    void UpdateMicrokernel(RuntimeState &state) {
        std::lock_guard<std::recursive_mutex> lock(state.state_mutex);
        if (!state.core_initialized || !state.modules_loaded || !state.kernel)
            return;
        state.kernel->timers->Tick();
        state.runtime.Update();
    }

    void ShutdownMicrokernel(RuntimeState &state) {
        std::lock_guard<std::recursive_mutex> lock(state.state_mutex);
        if (!state.core_initialized || !state.kernel)
            return;

        DebugLog("Shutting down microkernel...");
        // Shutdown runtime (unloads modules)
        state.runtime.Shutdown();

        // Drain any IMC messages published during module detach callbacks
        ImcPump(*state.kernel, 0);

        // Gracefully shutdown all subsystems
        state.kernel->Shutdown();
        ResetTracingStateForKernel(state.kernel.get());

        DebugLog("Microkernel shut down");

        // Tear down the service graph and destroy all state in one shot.
        // KernelServices destructor guarantees reverse-declaration-order cleanup.
        state.clear_diagnostics_snapshot_on_next_query = true;
        state.runtime.SetDiagnosticsCallback({});
        state.kernel.reset();
        state.diagnostics = {};
        state.internal_diagnostics_snapshot = {};
        state.core_initialized = false;
        state.modules_discovered = false;
        state.modules_loaded = false;
        state.bootstrap_state = BML_BOOTSTRAP_STATE_SHUTDOWN;
    }

    const ModuleBootstrapDiagnostics &GetBootstrapDiagnostics(RuntimeState &state) {
        std::lock_guard<std::recursive_mutex> lock(state.state_mutex);
        if (!state.core_initialized && state.clear_diagnostics_snapshot_on_next_query) {
            ClearPublishedDiagnosticsSnapshots(state);
        }
        return state.core_initialized ? state.diagnostics : state.internal_diagnostics_snapshot;
    }

    const BML_BootstrapDiagnostics &GetPublicDiagnostics(RuntimeState &state) {
        std::lock_guard<std::recursive_mutex> lock(state.state_mutex);
        if (!state.core_initialized && state.clear_diagnostics_snapshot_on_next_query) {
            ClearPublishedDiagnosticsSnapshots(state);
        }
        return state.public_diagnostics_snapshot.diagnostics;
    }
} // namespace BML::Core
