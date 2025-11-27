#include "ModuleLoader.h"

#include "bml_export.h"

#include <filesystem>

#include "Context.h"
#include "StringUtils.h"

namespace BML::Core {
    namespace {
        std::wstring ResolveEntryPath(const ModManifest &manifest) {
            std::string entry = manifest.package.entry;
            if (entry.empty()) {
                entry = manifest.package.id;
                if (!entry.empty()) {
                    entry += ".dll";
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
            return entryPath.lexically_normal().wstring();
        }

        std::string FormatSystemMessage(DWORD code) {
            if (code == 0)
                return {};

            LPWSTR buffer = nullptr;
            DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
            DWORD length = FormatMessageW(flags,
                                          nullptr,
                                          code,
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
        }

        void Rollback(std::vector<LoadedModule> &loaded, Context &context) {
            UnloadModules(loaded, context.GetHandle());
        }
    } // namespace

    bool LoadModules(const std::vector<ResolvedNode> &order,
                     Context &context,
                     PFN_BML_GetProcAddress get_proc,
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
        if (!get_proc) {
            out_error.message = "get_proc callback is null";
            return false;
        }

        for (const auto &node : order) {
            if (!node.manifest) {
                out_error.id = node.id;
                out_error.message = "Resolved node missing manifest";
                Rollback(out_modules, context);
                return false;
            }

            auto dllPath = ResolveEntryPath(*node.manifest);
            if (dllPath.empty()) {
                out_error.id = node.id;
                out_error.message = "Unable to resolve entry path";
                Rollback(out_modules, context);
                return false;
            }
            out_error.path = dllPath;

            HMODULE handle = LoadLibraryW(dllPath.c_str());
            if (!handle) {
                out_error.id = node.id;
                out_error.system_code = static_cast<long>(GetLastError());
                out_error.message = "LoadLibrary failed: " + FormatSystemMessage(static_cast<DWORD>(out_error.system_code));
                Rollback(out_modules, context);
                return false;
            }

            auto entrypoint = reinterpret_cast<PFN_BML_ModEntrypoint>(GetProcAddress(handle, "BML_ModEntrypoint"));
            if (!entrypoint) {
                out_error.id = node.id;
                out_error.message = "BML_ModEntrypoint export not found";
                out_error.system_code = 0;
                FreeLibrary(handle);
                Rollback(out_modules, context);
                return false;
            }

            BML_Result initResult = BML_RESULT_OK;

            auto modHandle = context.CreateModHandle(*node.manifest);
            if (!modHandle) {
                out_error.id = node.id;
                out_error.message = "Failed to create module handle";
                out_error.system_code = 0;
                FreeLibrary(handle);
                Rollback(out_modules, context);
                return false;
            }

            struct ModuleScope {
                explicit ModuleScope(BML_Mod_T *mod) : previous(Context::GetCurrentModule()) {
                    Context::SetCurrentModule(mod);
                }

                ~ModuleScope() {
                    Context::SetCurrentModule(previous);
                }

                BML_Mod previous;
            } scope(modHandle.get());

            // Call BML_ModEntrypoint attach with BML_Mod handle
            BML_ModAttachArgs attach{};
            attach.struct_size = sizeof(attach);
            attach.api_version = BML_MOD_ENTRYPOINT_API_VERSION;
            attach.mod = modHandle.get();
            attach.get_proc = get_proc;
            attach.get_proc_by_id = &bmlGetProcAddressById; // Fast path (v0.4.1+)
            attach.get_api_id = &bmlGetApiId;               // ID query (v0.4.1+)
            attach.reserved = nullptr;
            initResult = entrypoint(BML_MOD_ENTRYPOINT_ATTACH, &attach);
            if (initResult != BML_RESULT_OK) {
                out_error.id = node.id;
                out_error.message = "BML_ModEntrypoint attach returned " + std::to_string(initResult);
                out_error.system_code = 0;
                FreeLibrary(handle);
                Rollback(out_modules, context);
                return false;
            }

            LoadedModule loaded;
            loaded.id = node.id;
            loaded.manifest = node.manifest;
            loaded.handle = handle;
            loaded.entrypoint = entrypoint;
            loaded.path = dllPath;
            loaded.mod_handle = std::move(modHandle);
            out_modules.emplace_back(std::move(loaded));
        }

        out_error = {};
        return true;
    }

    void UnloadModules(std::vector<LoadedModule> &modules, BML_Context ctx) {
        for (auto it = modules.rbegin(); it != modules.rend(); ++it) {
            struct ModuleScope {
                explicit ModuleScope(BML_Mod_T *mod) : previous(Context::GetCurrentModule()) {
                    Context::SetCurrentModule(mod);
                }

                ~ModuleScope() {
                    Context::SetCurrentModule(previous);
                }

                BML_Mod previous;
            } scope(it->mod_handle ? it->mod_handle.get() : nullptr);

            if (it->entrypoint && it->mod_handle) {
                BML_ModDetachArgs detach{};
                detach.struct_size = sizeof(detach);
                detach.api_version = BML_MOD_ENTRYPOINT_API_VERSION;
                detach.mod = it->mod_handle.get();
                detach.reserved = nullptr;
                try {
                    it->entrypoint(BML_MOD_ENTRYPOINT_DETACH, &detach);
                } catch (const std::exception &ex) {
                    std::string msg = "[BML ModuleLoader] Exception during detach of '" + it->id + "': " + ex.what() + "\n";
                    OutputDebugStringA(msg.c_str());
                } catch (...) {
                    std::string msg = "[BML ModuleLoader] Unknown exception during detach of '" + it->id + "'\n";
                    OutputDebugStringA(msg.c_str());
                }
            }
            if (it->handle) {
                FreeLibrary(it->handle);
                it->handle = nullptr;
            }
        }
        modules.clear();
    }
} // namespace BML::Core
