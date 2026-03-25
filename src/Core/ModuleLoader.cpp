#include "ModuleLoader.h"

#include "bml_export.h"

#include <filesystem>
#include <unordered_set>
#if !defined(_WIN32)
#include <dlfcn.h>
#endif

#include "Context.h"
#include "ApiRegistry.h"
#include "ConfigStore.h"
#include "CrashDumpWriter.h"
#include "ExtensionStateHooks.h"
#include "FaultTracker.h"
#include "InterfaceRegistry.h"
#include "KernelServices.h"
#include "ModuleLifecycle.h"
#include "Logging.h"
#include "ResourceApi.h"
#include "StringUtils.h"

namespace BML::Core {
    namespace {
        constexpr char kModuleLoaderLogCategory[] = "module.loader";

        const char *DefaultModuleExtension() {
#if defined(_WIN32)
            return ".dll";
#elif defined(__APPLE__)
            return ".dylib";
#else
            return ".so";
#endif
        }

        std::wstring ResolveEntryPath(const ModManifest &manifest) {
            std::string entry = manifest.package.entry;
            if (entry.empty()) {
                entry = manifest.package.id;
                if (!entry.empty()) {
                    entry += DefaultModuleExtension();
                }
            }

            std::wstring wideEntry = utils::Utf8ToUtf16(entry);
            if (wideEntry.empty())
                return {};

            std::filesystem::path entryPath(wideEntry);
            if (entryPath.is_relative()) {
                std::filesystem::path base(manifest.directory);
                entryPath = base / entryPath;
            }
            auto resolved = entryPath.lexically_normal();

            // Validate that resolved path stays within the module directory
            auto baseNorm = std::filesystem::path(manifest.directory).lexically_normal();
            auto rel = resolved.lexically_relative(baseNorm);
            if (rel.empty() || rel.native().starts_with(L"..")) {
                CoreLog(BML_LOG_ERROR, kModuleLoaderLogCategory,
                        "Module entry path escapes module directory: %s",
                        manifest.package.id.c_str());
                return {};
            }
            return resolved.wstring();
        }

        std::string FormatSystemMessage(long code) {
#if defined(_WIN32)
            if (code == 0)
                return {};

            LPWSTR buffer = nullptr;
            DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
            DWORD length = FormatMessageW(flags,
                                          nullptr,
                                          static_cast<DWORD>(code),
                                          MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                          reinterpret_cast<LPWSTR>(&buffer),
                                          0,
                                          nullptr);
            std::wstring message_w;
            if (length != 0 && buffer) {
                message_w.assign(buffer, buffer + length);
                LocalFree(buffer);
                while (!message_w.empty() && (message_w.back() == L'\r' || message_w.back() == L'\n' || message_w.back() == L'.')) {
                    message_w.pop_back();
                }
                return utils::Utf16ToUtf8(message_w);
            }
            return "system error " + std::to_string(code);
#else
            (void) code;
            const char *message = dlerror();
            return message ? message : "dynamic loader error";
#endif
        }

        void Rollback(std::vector<LoadedModule> &loaded, Context &context, KernelServices &kernel) {
            UnloadModules(loaded, context, kernel);
        }

        void RecordModuleFailure(ModuleLoadError &out_error,
                                 const ResolvedNode &node,
                                 const std::wstring &path,
                                 const std::string &message,
                                 long system_code) {
            if (!out_error.message.empty()) {
                return;
            }

            out_error.id = node.id;
            out_error.path = path;
            out_error.message = message;
            out_error.system_code = system_code;
        }

        void CleanupFailedAttach(KernelServices &kernel, const std::string &module_id, BML_Mod mod) {
            if (module_id.empty() || !mod) {
                return;
            }

            kernel.config->FlushAndRelease(mod);
            RunExtensionProviderCleanupHook(module_id);
            kernel.api_registry->UnregisterByProvider(module_id);
            UnregisterResourceTypesForProvider(module_id);
            CleanupModuleKernelState(kernel, module_id, mod);
            kernel.context->RemoveCreatedModHandle(mod);
        }

#if defined(_MSC_VER) && !defined(__MINGW32__)
        // SEH-safe entrypoint invocation (no C++ destructors on stack)
        static BML_Result InvokeEntrypointSEH(PFN_BML_ModEntrypoint entrypoint,
                                               BML_ModEntrypointCommand action,
                                               void *args,
                                               unsigned long *out_seh_code) {
            *out_seh_code = 0;
            BML_Result result = BML_RESULT_INTERNAL_ERROR;
            __try {
                result = entrypoint(action, args);
            } __except (
                (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ||
                 GetExceptionCode() == EXCEPTION_ILLEGAL_INSTRUCTION ||
                 GetExceptionCode() == EXCEPTION_STACK_OVERFLOW ||
                 GetExceptionCode() == EXCEPTION_INT_DIVIDE_BY_ZERO ||
                 GetExceptionCode() == EXCEPTION_FLT_DIVIDE_BY_ZERO ||
                 GetExceptionCode() == EXCEPTION_ARRAY_BOUNDS_EXCEEDED)
                    ? EXCEPTION_EXECUTE_HANDLER
                    : EXCEPTION_CONTINUE_SEARCH) {
                *out_seh_code = GetExceptionCode();
                return BML_RESULT_INTERNAL_ERROR;
            }
            return result;
        }
#endif
    } // namespace

    bool LoadModules(const std::vector<ResolvedNode> &order,
                     Context &context,
                     KernelServices &kernel,
                     const BML_Services *services,
                     std::vector<LoadedModule> &out_modules,
                     ModuleLoadError &out_error) {
        out_modules.clear();
        out_error = {}; // Clear error state upfront

        // Empty order is trivially successful
        if (order.empty()) {
            return true;
        }

        BML_Context ctx = context.GetHandle();
        if (!ctx) {
            out_error.message = "Context handle is null";
            return false;
        }
        if (!services) {
            out_error.message = "runtime service bundle is null";
            return false;
        }

        // Track failed module IDs for cascade-skip. When module A fails
        // (script compile error, SEH crash, etc.), any module B that has a
        // non-optional dependency on A is automatically skipped.
        std::unordered_set<std::string> failed_ids;

        auto hasDependencyFailed = [&](const ModManifest &manifest) -> bool {
            if (failed_ids.empty()) return false;
            for (const auto &dep : manifest.dependencies) {
                if (!dep.optional && failed_ids.count(dep.id))
                    return true;
            }
            return false;
        };

        for (const auto &node : order) {
            if (!node.manifest) {
                out_error.id = node.id;
                out_error.message = "Resolved node missing manifest";
                Rollback(out_modules, context, kernel);
                return false;
            }

            // Cascade-skip: if a required dependency failed, skip this module.
            if (hasDependencyFailed(*node.manifest)) {
                CoreLog(BML_LOG_WARN, kModuleLoaderLogCategory,
                        "Skipping '%s': required dependency failed to load",
                        node.id.c_str());
                failed_ids.insert(node.id);
                continue;
            }

            auto entryPath = ResolveEntryPath(*node.manifest);
            if (entryPath.empty()) {
                RecordModuleFailure(out_error, node, {}, "Unable to resolve entry path", 0);
                failed_ids.insert(node.id);
                continue;
            }

            // Check if a runtime provider can handle this entry file.
            std::string entryPathUtf8 = utils::Utf16ToUtf8(entryPath);
            const auto *provider = context.FindRuntimeProvider(entryPathUtf8);

            if (provider) {
                // ---- Non-native module: delegate to runtime provider ----
                auto modHandle = context.CreateModHandle(*node.manifest);
                if (!modHandle) {
                    RecordModuleFailure(out_error, node, entryPath, "Failed to create module handle", 0);
                    failed_ids.insert(node.id);
                    continue;
                }

                BML_Result initResult = provider->AttachModule(
                    modHandle.get(), services,
                    entryPathUtf8.c_str(),
                    modHandle->directory_utf8.c_str());

                if (initResult != BML_RESULT_OK) {
                    CoreLog(BML_LOG_ERROR, kModuleLoaderLogCategory,
                            "Runtime provider failed to attach '%s': %d",
                            node.id.c_str(), static_cast<int>(initResult));
                    RecordModuleFailure(out_error,
                                        node,
                                        entryPath,
                                        "Runtime provider attach returned " + std::to_string(initResult),
                                        0);
                    CleanupFailedAttach(kernel, node.id, modHandle.get());
                    failed_ids.insert(node.id);
                    continue;
                }

                // Validate [interfaces] for provider-loaded modules too.
                for (const auto &iface : node.manifest->interfaces) {
                    if (!kernel.interface_registry->Exists(iface.interface_id.c_str())) {
                        CoreLog(BML_LOG_WARN, kModuleLoaderLogCategory,
                                "Module '%s' declared [interfaces] interface '%s' "
                                "but did not register it during attach",
                                node.manifest->package.id.c_str(),
                                iface.interface_id.c_str());
                    }
                }

                LoadedModule loaded;
                loaded.id = node.id;
                loaded.manifest = node.manifest;
                loaded.handle = nullptr;
                loaded.entrypoint = nullptr;
                loaded.path = entryPath;
                loaded.mod_handle = std::move(modHandle);
                loaded.runtime = provider;
                out_modules.emplace_back(std::move(loaded));
                continue;
            }

            // ---- Native DLL module: existing path ----
            HMODULE handle = nullptr;
#if defined(_WIN32)
            handle = LoadLibraryW(entryPath.c_str());
#else
            if (!entryPathUtf8.empty()) {
                dlerror(); // Clear any stale error before dlopen/dlsym.
                handle = dlopen(entryPathUtf8.c_str(), RTLD_NOW);
            }
#endif
            if (!handle) {
                long system_code =
#if defined(_WIN32)
                    static_cast<long>(GetLastError());
#else
                    0;
#endif
                RecordModuleFailure(out_error,
                                    node,
                                    entryPath,
                                    "LoadLibrary failed: " + FormatSystemMessage(system_code),
                                    system_code);
                failed_ids.insert(node.id);
                continue;
            }

            auto closeModule = [](HMODULE module_handle) {
#if defined(_WIN32)
                FreeLibrary(module_handle);
#else
                dlclose(module_handle);
#endif
            };

            auto getSymbol = [](HMODULE module_handle, const char *symbol) -> void * {
#if defined(_WIN32)
                return reinterpret_cast<void *>(::GetProcAddress(module_handle, symbol));
#else
                dlerror();
                return dlsym(module_handle, symbol);
#endif
            };

            auto entrypoint = reinterpret_cast<PFN_BML_ModEntrypoint>(getSymbol(handle, "BML_ModEntrypoint"));
            if (!entrypoint) {
                RecordModuleFailure(out_error, node, entryPath, "BML_ModEntrypoint export not found", 0);
                closeModule(handle);
                failed_ids.insert(node.id);
                continue;
            }

            BML_Result initResult = BML_RESULT_OK;

            auto modHandle = context.CreateModHandle(*node.manifest);
            if (!modHandle) {
                RecordModuleFailure(out_error, node, entryPath, "Failed to create module handle", 0);
                closeModule(handle);
                failed_ids.insert(node.id);
                continue;
            }

            // Call BML_ModEntrypoint attach with BML_Mod handle
            BML_ModAttachArgs attach{};
            attach.struct_size = sizeof(attach);
            attach.api_version = BML_MOD_ENTRYPOINT_API_VERSION;
            attach.context = ctx;
            attach.mod = modHandle.get();
            attach.services = services;

#if defined(_MSC_VER) && !defined(__MINGW32__)
            unsigned long seh_code = 0;
            initResult = InvokeEntrypointSEH(entrypoint, BML_MOD_ENTRYPOINT_ATTACH, &attach, &seh_code);
            if (seh_code != 0) {
                char seh_msg[128];
                std::snprintf(seh_msg, sizeof(seh_msg),
                              "Module crashed during attach (SEH code 0x%08lX)", seh_code);
                RecordModuleFailure(out_error, node, entryPath, seh_msg, static_cast<long>(seh_code));
                CoreLog(BML_LOG_ERROR, kModuleLoaderLogCategory,
                        "%s: %s", node.id.c_str(), seh_msg);
                kernel.crash_dump->WriteDumpOnce(node.id, seh_code);
                kernel.fault_tracker->RecordFault(node.id, seh_code);
                CleanupFailedAttach(kernel, node.id, modHandle.get());
                closeModule(handle);
                failed_ids.insert(node.id);
                continue;
            }
#else
            initResult = entrypoint(BML_MOD_ENTRYPOINT_ATTACH, &attach);
#endif
            if (initResult != BML_RESULT_OK) {
                RecordModuleFailure(out_error,
                                    node,
                                    entryPath,
                                    "BML_ModEntrypoint attach returned " + std::to_string(initResult),
                                    0);
                CleanupFailedAttach(kernel, node.id, modHandle.get());
                closeModule(handle);
                failed_ids.insert(node.id);
                continue;
            }

            // Validate [interfaces] immediately after this module attaches,
            // before subsequent modules try to Acquire<>() the interface.
            for (const auto &iface : node.manifest->interfaces) {
                if (!kernel.interface_registry->Exists(iface.interface_id.c_str())) {
                    CoreLog(BML_LOG_WARN, kModuleLoaderLogCategory,
                            "Module '%s' declared [interfaces] interface '%s' "
                            "but did not register it during attach",
                            node.manifest->package.id.c_str(),
                            iface.interface_id.c_str());
                }
            }

            LoadedModule loaded;
            loaded.id = node.id;
            loaded.manifest = node.manifest;
            loaded.handle = handle;
            loaded.entrypoint = entrypoint;
            loaded.path = entryPath;
            loaded.mod_handle = std::move(modHandle);
            out_modules.emplace_back(std::move(loaded));
        }

        return true;
    }

    void UnloadModules(std::vector<LoadedModule> &modules, Context &context, KernelServices &kernel) {
        (void)context;
        for (auto it = modules.rbegin(); it != modules.rend(); ++it) {
            if (it->runtime && it->mod_handle) {
                // Non-native module: delegate to runtime provider.
                // Verify the provider is still registered (it may have been
                // invalidated if the provider module crashed/was disabled).
                std::string entryUtf8 = utils::Utf16ToUtf8(it->path);
                bool provider_alive =
                    (kernel.context->FindRuntimeProvider(entryUtf8) == it->runtime);

                if (provider_alive) {
                    try {
                        if (it->runtime->PrepareDetach)
                            it->runtime->PrepareDetach(it->mod_handle.get());
                        it->runtime->DetachModule(it->mod_handle.get());
                    } catch (const std::exception &ex) {
                        CoreLog(BML_LOG_ERROR, kModuleLoaderLogCategory,
                                "Exception during provider detach of '%s': %s",
                                it->id.c_str(), ex.what());
                    } catch (...) {
                        CoreLog(BML_LOG_ERROR, kModuleLoaderLogCategory,
                                "Unknown exception during provider detach of '%s'",
                                it->id.c_str());
                    }
                } else {
                    CoreLog(BML_LOG_WARN, kModuleLoaderLogCategory,
                            "Provider for '%s' is no longer registered; "
                            "skipping script detach, kernel cleanup only",
                            it->id.c_str());
                }

                // Core cleanup runs unconditionally (IMC, timers, hooks, leases).
                CleanupModuleKernelState(kernel, it->id, it->mod_handle.get());

            } else if (it->entrypoint && it->mod_handle) {
                // Native DLL module: existing path.
                std::string diagnostic;
                BML_Result prepare_result = PrepareModuleForDetach(kernel,
                                                                  it->id,
                                                                  it->mod_handle.get(),
                                                                  it->entrypoint,
                                                                  &diagnostic);
                if (prepare_result != BML_RESULT_OK) {
                    CoreLog(BML_LOG_WARN, kModuleLoaderLogCategory,
                            "Detach gate for '%s' failed during shutdown: %s (result %d); forcing final detach",
                            it->id.c_str(),
                            diagnostic.empty() ? "no diagnostic" : diagnostic.c_str(),
                            static_cast<int>(prepare_result));
                }

                BML_ModDetachArgs detach{};
                detach.struct_size = sizeof(detach);
                detach.api_version = BML_MOD_ENTRYPOINT_API_VERSION;
                detach.mod = it->mod_handle.get();
                try {
                    it->entrypoint(BML_MOD_ENTRYPOINT_DETACH, &detach);
                } catch (const std::exception &ex) {
                    CoreLog(BML_LOG_ERROR, kModuleLoaderLogCategory,
                            "Exception during detach of '%s': %s",
                            it->id.c_str(), ex.what());
                } catch (...) {
                    CoreLog(BML_LOG_ERROR, kModuleLoaderLogCategory,
                            "Unknown exception during detach of '%s'",
                            it->id.c_str());
                }

                CleanupModuleKernelState(kernel, it->id, it->mod_handle.get());
            }

            if (it->handle) {
#if defined(_WIN32)
                FreeLibrary(it->handle);
#else
                dlclose(it->handle);
#endif
                it->handle = nullptr;
            }
        }
        modules.clear();
    }
} // namespace BML::Core
