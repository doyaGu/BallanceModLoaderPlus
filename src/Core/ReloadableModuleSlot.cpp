#include "ReloadableModuleSlot.h"

#include <cstring>
#include <system_error>

#include "Context.h"
#include "Logging.h"
#include "ModManifest.h"
#include "StringUtils.h"

namespace BML::Core {

    namespace {
        constexpr char kLogCategory[] = "hot.reload.slot";

        std::filesystem::file_time_type GetFileWriteTime(const std::wstring& path) {
            std::error_code ec;
            auto time = std::filesystem::last_write_time(path, ec);
            if (ec) return std::filesystem::file_time_type::min();
            return time;
        }

        bool FileExists(const std::wstring& path) {
            std::error_code ec;
            return std::filesystem::exists(path, ec);
        }

        const char* ReloadResultToString(ReloadResult result) {
            switch (result) {
                case ReloadResult::Success: return "Success";
                case ReloadResult::NoChange: return "NoChange";
                case ReloadResult::LoadFailed: return "LoadFailed";
                case ReloadResult::EntrypointMissing: return "EntrypointMissing";
                case ReloadResult::InitFailed: return "InitFailed";
                case ReloadResult::Crashed: return "Crashed";
                case ReloadResult::RolledBack: return "RolledBack";
                default: return "Unknown";
            }
        }

        const char* ReloadFailureToString(ReloadFailure failure) {
            switch (failure) {
                case ReloadFailure::None: return "None";
                case ReloadFailure::SegFault: return "SegFault";
                case ReloadFailure::IllegalInstruction: return "IllegalInstruction";
                case ReloadFailure::StackOverflow: return "StackOverflow";
                case ReloadFailure::BadImage: return "BadImage";
                case ReloadFailure::StateInvalidated: return "StateInvalidated";
                case ReloadFailure::UserError: return "UserError";
                case ReloadFailure::InitialFailure: return "InitialFailure";
                case ReloadFailure::SystemError: return "SystemError";
                case ReloadFailure::Other: return "Other";
                default: return "Unknown";
            }
        }
    }

    ReloadableModuleSlot::ReloadableModuleSlot() = default;

    ReloadableModuleSlot::~ReloadableModuleSlot() {
        Shutdown();
    }

    bool ReloadableModuleSlot::Initialize(const ReloadableSlotConfig& config) {
        if (config.dll_path.empty()) {
            CoreLog(BML_LOG_ERROR, kLogCategory, "DLL path is empty");
            return false;
        }

        if (!FileExists(config.dll_path)) {
            CoreLog(BML_LOG_ERROR, kLogCategory, "DLL does not exist: %ls",
                    config.dll_path.c_str());
            return false;
        }

        if (!config.context) {
            CoreLog(BML_LOG_ERROR, kLogCategory, "Context is null");
            return false;
        }

        m_Config = config;
        m_Version = 0;
        m_NextVersion = 1;
        m_LastWorkingVersion = 0;
        m_LastWriteTime = GetFileWriteTime(config.dll_path);
        m_LastFailure = ReloadFailure::None;
        m_LastSystemError = 0;

        // Ensure temp directory exists
        if (!config.temp_directory.empty()) {
            std::error_code ec;
            std::filesystem::create_directories(config.temp_directory, ec);
            if (ec) {
                CoreLog(BML_LOG_WARN, kLogCategory,
                        "Failed to create temp directory: %s", ec.message().c_str());
            }
        }

        CoreLog(BML_LOG_DEBUG, kLogCategory, "Initialized slot for: %ls",
                config.dll_path.c_str());
        return true;
    }

    void ReloadableModuleSlot::Shutdown() {
        if (m_Handle) {
            UnloadCurrent(false, true);  // Not rollback, is close
        }
        CleanupVersionFiles();
        m_Config = {};
        m_Version = 0;
        m_NextVersion = 1;
        m_LastWorkingVersion = 0;
        m_UserData = nullptr;
        m_ModHandle.reset();

        CoreLog(BML_LOG_DEBUG, kLogCategory, "Slot shutdown complete");
    }

    bool ReloadableModuleSlot::HasChanged() const {
        if (m_Config.dll_path.empty()) return false;
        auto current = GetFileWriteTime(m_Config.dll_path);
        return current > m_LastWriteTime;
    }

    ReloadResult ReloadableModuleSlot::Reload() {
        if (!HasChanged()) {
            return ReloadResult::NoChange;
        }
        return ForceReload();
    }

    ReloadResult ReloadableModuleSlot::ForceReload() {
        CoreLog(BML_LOG_INFO, kLogCategory,
                "Reloading module '%s' (version %u -> %u)",
                GetModId().c_str(), m_Version, m_NextVersion);

        m_LastFailure = ReloadFailure::None;
        m_LastSystemError = 0;

        // Unload current if loaded
        if (m_Handle) {
            if (!UnloadCurrent(false, false)) {
                // Unload crashed or failed, try rollback
                CoreLog(BML_LOG_WARN, kLogCategory,
                        "Unload failed, attempting rollback");
                if (TryRollback()) {
                    return ReloadResult::RolledBack;
                }
                return ReloadResult::Crashed;
            }
        }

        // Load new version
        if (!LoadVersion(m_NextVersion, false)) {
            // Load failed, try rollback if we have a previous version
            if (m_LastWorkingVersion > 0) {
                CoreLog(BML_LOG_WARN, kLogCategory,
                        "Load failed, attempting rollback to version %u",
                        m_LastWorkingVersion);
                if (TryRollback()) {
                    return ReloadResult::RolledBack;
                }
            } else {
                m_LastFailure = ReloadFailure::InitialFailure;
            }
            return ReloadResult::LoadFailed;
        }

        m_LastFailure = ReloadFailure::None;
        CoreLog(BML_LOG_INFO, kLogCategory,
                "Successfully reloaded to version %u", m_Version);
        return ReloadResult::Success;
    }

    const std::string& ReloadableModuleSlot::GetModId() const {
        static const std::string empty;
        if (m_Config.manifest) {
            return m_Config.manifest->package.id;
        }
        return empty;
    }

    std::wstring ReloadableModuleSlot::MakeVersionPath(unsigned int version) const {
        std::filesystem::path original(m_Config.dll_path);
        std::wstring stem = original.stem().wstring();
        std::wstring ext = original.extension().wstring();

        std::filesystem::path temp_dir;
        if (m_Config.temp_directory.empty()) {
            temp_dir = original.parent_path();
        } else {
            temp_dir = m_Config.temp_directory;
        }

        return (temp_dir / (stem + std::to_wstring(version) + ext)).wstring();
    }

    bool ReloadableModuleSlot::CopyDllToTemp(unsigned int version) {
        auto dest = MakeVersionPath(version);
        std::error_code ec;

        // Remove existing file first
        std::filesystem::remove(dest, ec);

        // Copy the DLL
        if (!std::filesystem::copy_file(m_Config.dll_path, dest,
                std::filesystem::copy_options::overwrite_existing, ec)) {
            CoreLog(BML_LOG_ERROR, kLogCategory,
                    "Failed to copy DLL to '%ls': %s",
                    dest.c_str(), ec.message().c_str());
            m_LastFailure = ReloadFailure::SystemError;
            return false;
        }

        // Also copy PDB if it exists (for debugging)
        std::filesystem::path pdb_src(m_Config.dll_path);
        pdb_src.replace_extension(L".pdb");
        if (std::filesystem::exists(pdb_src, ec)) {
            std::filesystem::path pdb_dest(dest);
            pdb_dest.replace_extension(L".pdb");
            std::filesystem::copy_file(pdb_src, pdb_dest,
                std::filesystem::copy_options::overwrite_existing, ec);
            // Ignore PDB copy failure, it's not critical
        }

        CoreLog(BML_LOG_DEBUG, kLogCategory,
                "Copied DLL to '%ls'", dest.c_str());
        return true;
    }

    HMODULE ReloadableModuleSlot::LoadDll(const std::wstring& path) {
        HMODULE handle = LoadLibraryW(path.c_str());
        if (!handle) {
            m_LastSystemError = GetLastError();
            CoreLog(BML_LOG_ERROR, kLogCategory,
                    "LoadLibrary failed for '%ls': error %lu",
                    path.c_str(), m_LastSystemError);
        }
        return handle;
    }

    bool ReloadableModuleSlot::LoadVersion(unsigned int version, bool is_rollback) {
        std::wstring dll_path;

        if (!is_rollback) {
            // Copy to temp with version number
            if (!CopyDllToTemp(version)) {
                m_LastFailure = ReloadFailure::BadImage;
                return false;
            }
            dll_path = MakeVersionPath(version);
            m_LastWorkingVersion = m_Version;
            m_NextVersion = version + 1;
        } else {
            // Use existing versioned copy for rollback
            dll_path = MakeVersionPath(version);
            if (!FileExists(dll_path)) {
                CoreLog(BML_LOG_ERROR, kLogCategory,
                        "Rollback version %u not found at '%ls'",
                        version, dll_path.c_str());
                m_LastFailure = ReloadFailure::BadImage;
                return false;
            }
            // Don't rollback to this version again if it crashes
            m_LastWorkingVersion = version > 0 ? version - 1 : 0;
        }

        // Load the DLL
        HMODULE handle = LoadDll(dll_path);
        if (!handle) {
            m_LastFailure = ReloadFailure::BadImage;
            return false;
        }

        // Find entrypoint
        auto entrypoint = reinterpret_cast<PFN_BML_ModEntrypoint>(
            GetProcAddress(handle, "BML_ModEntrypoint"));
        if (!entrypoint) {
            CoreLog(BML_LOG_ERROR, kLogCategory,
                    "BML_ModEntrypoint not found in '%ls'", dll_path.c_str());
            FreeLibrary(handle);
            m_LastFailure = ReloadFailure::BadImage;
            return false;
        }

        // Create mod handle if needed
        if (!m_ModHandle && m_Config.context && m_Config.manifest) {
            m_ModHandle = m_Config.context->CreateModHandle(*m_Config.manifest);
        }

        m_Handle = handle;
        m_Entrypoint = entrypoint;
        m_Version = version;
        m_LastWriteTime = GetFileWriteTime(m_Config.dll_path);

        // Restore state if this is a reload (not first load)
        if (is_rollback) {
            RestoreState();  // From backup (future)
        } else if (version > 1) {
            RestoreState();  // From current snapshot (future)
        }

        // Call entrypoint with ATTACH operation
        int result = InvokeEntrypoint(ReloadOp::Load);
        if (result < 0) {
            CoreLog(BML_LOG_ERROR, kLogCategory,
                    "Entrypoint attach returned %d", result);
            FreeLibrary(handle);
            m_Handle = nullptr;
            m_Entrypoint = nullptr;
            m_LastFailure = ReloadFailure::UserError;
            return false;
        }

        CoreLog(BML_LOG_DEBUG, kLogCategory,
                "Successfully loaded version %u from '%ls'",
                version, dll_path.c_str());
        return true;
    }

    bool ReloadableModuleSlot::UnloadCurrent(bool is_rollback, bool is_close) {
        if (!m_Handle) return true;

        int result = 0;
        if (!is_rollback) {
            // Call entrypoint with DETACH operation
            result = InvokeEntrypoint(is_close ? ReloadOp::Close : ReloadOp::Unload);

            if (result >= 0) {
                // Save state only if unload succeeded
                SaveState();
                BackupState();
            } else {
                CoreLog(BML_LOG_WARN, kLogCategory,
                        "Entrypoint detach returned %d", result);
            }
        }

        FreeLibrary(m_Handle);
        m_Handle = nullptr;
        m_Entrypoint = nullptr;

        return result >= 0;
    }

    bool ReloadableModuleSlot::TryRollback() {
        if (m_LastWorkingVersion == 0) {
            CoreLog(BML_LOG_ERROR, kLogCategory,
                    "Cannot rollback: no previous working version");
            m_LastFailure = ReloadFailure::InitialFailure;
            return false;
        }

        CoreLog(BML_LOG_WARN, kLogCategory,
                "Rolling back to version %u", m_LastWorkingVersion);

        if (LoadVersion(m_LastWorkingVersion, true)) {
            m_LastFailure = ReloadFailure::None;
            return true;
        }
        return false;
    }

    void ReloadableModuleSlot::SaveState() {
        // Currently, only userdata pointer is preserved (already in m_UserData)
        // Future: could call mod entrypoint to serialize state
    }

    void ReloadableModuleSlot::RestoreState() {
        // Currently, userdata pointer is already preserved
        // Future: could call mod entrypoint to deserialize state
    }

    void ReloadableModuleSlot::BackupState() {
        // Future: backup current state for rollback scenarios
    }

    int ReloadableModuleSlot::FilterException(unsigned long code, ReloadFailure& out_failure) {
        switch (code) {
            case EXCEPTION_ACCESS_VIOLATION:
                out_failure = ReloadFailure::SegFault;
                return EXCEPTION_EXECUTE_HANDLER;
            case EXCEPTION_ILLEGAL_INSTRUCTION:
                out_failure = ReloadFailure::IllegalInstruction;
                return EXCEPTION_EXECUTE_HANDLER;
            case EXCEPTION_STACK_OVERFLOW:
                out_failure = ReloadFailure::StackOverflow;
                return EXCEPTION_EXECUTE_HANDLER;
            case EXCEPTION_DATATYPE_MISALIGNMENT:
            case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
            case EXCEPTION_INT_DIVIDE_BY_ZERO:
            case EXCEPTION_INT_OVERFLOW:
            case EXCEPTION_FLT_DENORMAL_OPERAND:
            case EXCEPTION_FLT_DIVIDE_BY_ZERO:
            case EXCEPTION_FLT_INEXACT_RESULT:
            case EXCEPTION_FLT_INVALID_OPERATION:
            case EXCEPTION_FLT_OVERFLOW:
            case EXCEPTION_FLT_STACK_CHECK:
            case EXCEPTION_FLT_UNDERFLOW:
                out_failure = ReloadFailure::Other;
                return EXCEPTION_EXECUTE_HANDLER;
            default:
                // Don't handle unknown exceptions
                return EXCEPTION_CONTINUE_SEARCH;
        }
    }

    int ReloadableModuleSlot::InvokeEntrypoint(ReloadOp op) {
        if (!m_Entrypoint || !m_ModHandle) return -1;

        ReloadFailure caught_failure = ReloadFailure::None;
        int result = -1;

        // Set current module context
        BML_Mod previous_mod = Context::GetCurrentModule();
        Context::SetCurrentModule(m_ModHandle.get());

#if defined(_MSC_VER) && !defined(__MINGW32__)
        // Use SEH to catch crashes
        __try {
#endif
            if (op == ReloadOp::Load) {
                BML_ModAttachArgs attach{};
                attach.struct_size = sizeof(attach);
                attach.api_version = BML_MOD_ENTRYPOINT_API_VERSION;
                attach.mod = m_ModHandle.get();
                attach.get_proc = m_Config.get_proc;
                attach.get_proc_by_id = &bmlGetProcAddressById;
                attach.get_api_id = &bmlGetApiId;
                attach.reserved = nullptr;
                result = m_Entrypoint(BML_MOD_ENTRYPOINT_ATTACH, &attach);
            } else {
                BML_ModDetachArgs detach{};
                detach.struct_size = sizeof(detach);
                detach.api_version = BML_MOD_ENTRYPOINT_API_VERSION;
                detach.mod = m_ModHandle.get();
                detach.reserved = nullptr;
                result = m_Entrypoint(BML_MOD_ENTRYPOINT_DETACH, &detach);
            }
#if defined(_MSC_VER) && !defined(__MINGW32__)
        } __except (FilterException(GetExceptionCode(), caught_failure)) {
            CoreLog(BML_LOG_ERROR, kLogCategory,
                    "Exception caught in entrypoint: %s",
                    ReloadFailureToString(caught_failure));
            m_LastFailure = caught_failure;
            result = -1;
        }
#endif

        Context::SetCurrentModule(previous_mod);
        return result;
    }

    void ReloadableModuleSlot::CleanupVersionFiles() {
        if (m_Config.dll_path.empty()) return;

        // Clean up versioned DLL copies
        for (unsigned int v = 1; v <= m_Version + 5; ++v) {
            auto path = MakeVersionPath(v);
            std::error_code ec;
            if (std::filesystem::remove(path, ec)) {
                CoreLog(BML_LOG_DEBUG, kLogCategory,
                        "Removed versioned DLL: %ls", path.c_str());
            }

            // Also remove PDB
            std::filesystem::path pdb(path);
            pdb.replace_extension(L".pdb");
            std::filesystem::remove(pdb, ec);
        }
    }

} // namespace BML::Core
