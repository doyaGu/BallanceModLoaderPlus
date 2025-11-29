#include "Microkernel.h"

#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#include "ApiRegistration.h"
#include "Context.h"
#include "HotReloadCoordinator.h"  // Required for ModuleRuntime destructor
#include "ImcBus.h"
#include "Logging.h"
#include "StringUtils.h"

namespace BML::Core {
    namespace {
        struct MicrokernelState {
            ModuleRuntime runtime;
            ModuleBootstrapDiagnostics diagnostics;
            bool core_initialized{false};
            bool modules_discovered{false};
            bool modules_loaded{false};
            std::vector<BML_BootstrapManifestError> public_manifest_errors;
            std::vector<const char *> public_dependency_chain;
            std::vector<const char *> public_load_order;
            std::string load_error_path_utf8;
            BML_BootstrapDependencyError public_dependency_error{};
            BML_BootstrapLoadError public_load_error{};
            BML_BootstrapDiagnostics public_diagnostics{};
        };

        MicrokernelState &State() {
            static MicrokernelState state;
            return state;
        }

        // Use CoreLog for consistent logging
        inline void DebugLog(const std::string &message) {
            CoreLog(BML_LOG_DEBUG, "microkernel", "%s", message.c_str());
        }

        std::wstring GetEnvironmentOverride() {
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

            std::wstring path(260, L'\0');
            DWORD copied = 0;
            while (true) {
                copied = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
                if (copied == 0)
                    return {};
                if (copied < path.size() - 1)
                    break;
                path.resize(path.size() * 2, L'\0');
            }
            path.resize(copied);
            std::filesystem::path exe(path);
            // Default to ../Mods (parent of bin directory)
            return (exe.parent_path().parent_path() / L"Mods").wstring();
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

        void UpdatePublicDiagnostics(MicrokernelState &state) {
            auto &diag = state.diagnostics;

            state.public_manifest_errors.clear();
            state.public_manifest_errors.reserve(diag.manifest_errors.size());
            for (const auto &err : diag.manifest_errors) {
                BML_BootstrapManifestError entry{};
                entry.message = err.message.c_str();
                if (err.file && !err.file->empty()) {
                    entry.has_file = 1;
                    entry.file = err.file->c_str();
                }
                if (err.line) {
                    entry.has_line = 1;
                    entry.line = *err.line;
                }
                if (err.column) {
                    entry.has_column = 1;
                    entry.column = *err.column;
                }
                state.public_manifest_errors.push_back(entry);
            }

            state.public_dependency_chain.clear();
            for (const auto &id : diag.dependency_error.chain) {
                state.public_dependency_chain.push_back(id.c_str());
            }
            if (diag.dependency_error.message.empty()) {
                state.public_dependency_error = {};
            } else {
                state.public_dependency_error.message = diag.dependency_error.message.c_str();
                state.public_dependency_error.chain = state.public_dependency_chain.empty() ? nullptr : state.public_dependency_chain.data();
                state.public_dependency_error.chain_count = static_cast<uint32_t>(state.public_dependency_chain.size());
            }

            if (!diag.load_error.message.empty()) {
                state.public_load_error.has_error = 1;
                state.public_load_error.module_id = diag.load_error.id.empty() ? nullptr : diag.load_error.id.c_str();
                state.public_load_error.message = diag.load_error.message.c_str();
                state.load_error_path_utf8 = utils::Utf16ToUtf8(diag.load_error.path);
                state.public_load_error.path_utf8 = state.load_error_path_utf8.empty() ? nullptr : state.load_error_path_utf8.c_str();
                state.public_load_error.system_code = static_cast<int32_t>(diag.load_error.system_code);
            } else {
                state.public_load_error = {};
                state.load_error_path_utf8.clear();
            }

            state.public_load_order.clear();
            for (const auto &id : diag.load_order) {
                state.public_load_order.push_back(id.c_str());
            }

            state.public_diagnostics.manifest_errors = state.public_manifest_errors.empty() ? nullptr : state.public_manifest_errors.data();
            state.public_diagnostics.manifest_error_count = static_cast<uint32_t>(state.public_manifest_errors.size());
            state.public_diagnostics.dependency_error = state.public_dependency_error;
            state.public_diagnostics.load_error = state.public_load_error;
            state.public_diagnostics.load_order = state.public_load_order.empty() ? nullptr : state.public_load_order.data();
            state.public_diagnostics.load_order_count = static_cast<uint32_t>(state.public_load_order.size());
        }
    } // namespace

    bool InitializeCore() {
        auto &state = State();
        if (state.core_initialized)
            return true;

        DebugLog("Phase 0: Initializing core...");

        // Initialize context with runtime version
        auto &ctx = Context::Instance();
        ctx.Initialize({0, 4, 0});

        // Register core APIs
        RegisterCoreApis();

        state.core_initialized = true;
        DebugLog("Core initialized successfully");
        return true;
    }

    bool DiscoverModules() {
        auto &state = State();

        // Core must be initialized first
        if (!state.core_initialized) {
            DebugLog("DiscoverModules: Core not initialized");
            return false;
        }

        if (state.modules_discovered)
            return true;

        DebugLog("Phase 1: Discovering modules...");

        ModuleBootstrapDiagnostics diag;
        auto modsDir = DetectModsDirectory();

        // Only discover and validate, don't load DLLs yet
        if (!state.runtime.DiscoverAndValidate(modsDir, diag)) {
            state.diagnostics = diag;
            UpdatePublicDiagnostics(state);
            EmitDiagnostics(diag);
            DebugLog("Module discovery failed");
            // Keep context alive for later retry
            return false;
        }

        state.diagnostics = diag;
        UpdatePublicDiagnostics(state);
        EmitDiagnostics(diag);
        state.modules_discovered = true;
        DebugLog("Module discovery completed successfully");
        return true;
    }

    bool LoadDiscoveredModules() {
        auto &state = State();

        // Modules must be discovered first
        if (!state.modules_discovered) {
            DebugLog("LoadDiscoveredModules: Modules not discovered");
            return false;
        }

        if (state.modules_loaded)
            return true;

        DebugLog("Phase 2: Loading discovered modules...");

        ModuleBootstrapDiagnostics diag;

        // Load the previously discovered modules
        if (!state.runtime.LoadDiscovered(diag)) {
            state.diagnostics = diag;
            UpdatePublicDiagnostics(state);
            EmitDiagnostics(diag);
            DebugLog("Module loading failed");
            return false;
        }

        state.diagnostics = diag;
        UpdatePublicDiagnostics(state);
        state.runtime.SetDiagnosticsCallback([](const ModuleBootstrapDiagnostics &new_diag) {
            auto &sharedState = State();
            sharedState.diagnostics = new_diag;
            UpdatePublicDiagnostics(sharedState);
            EmitDiagnostics(new_diag);
        });
        state.modules_loaded = true;
        EmitDiagnostics(diag);
        DebugLog("Modules loaded successfully");
        return true;
    }

    void ShutdownMicrokernel() {
        auto &state = State();
        if (!state.core_initialized)
            return;

        DebugLog("Shutting down microkernel...");

        // Shutdown runtime (unloads modules)
        state.runtime.Shutdown();

        // Shutdown IMC bus
        ImcBus::Instance().Shutdown();

        // Cleanup context (clears all remaining resources)
        Context::Instance().Cleanup();

        // Clear diagnostics
        state.public_manifest_errors.clear();
        state.public_dependency_chain.clear();
        state.public_load_order.clear();
        state.load_error_path_utf8.clear();
        state.public_dependency_error = {};
        state.public_load_error = {};
        state.public_diagnostics = {};

        state.core_initialized = false;
        state.modules_discovered = false;
        state.modules_loaded = false;
        DebugLog("Microkernel shut down");
    }

    const ModuleBootstrapDiagnostics &GetBootstrapDiagnostics() {
        return State().diagnostics;
    }

    const BML_BootstrapDiagnostics &GetPublicDiagnostics() {
        return State().public_diagnostics;
    }
} // namespace BML::Core
