#include "CrashDumpWriter.h"

#include "KernelServices.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#endif

#include "Logging.h"
#include "PathUtils.h"
#include "StringUtils.h"
#include "TimeUtils.h"

namespace BML::Core {
    namespace {
        constexpr char kLogCategory[] = "crash.dump";
    } // namespace

    struct CrashDumpWriter::Impl {
        std::mutex mutex;
        std::filesystem::path runtime_directory;
        std::atomic<bool> dump_written{false};
    };

    CrashDumpWriter::CrashDumpWriter() : m_Impl(std::make_unique<Impl>()) {}
    CrashDumpWriter::~CrashDumpWriter() = default;

    void CrashDumpWriter::SetRuntimeDirectory(const std::filesystem::path &runtimeDirectory) {
        auto &impl = *m_Impl;
        std::lock_guard lock(impl.mutex);
        impl.runtime_directory = runtimeDirectory.empty()
            ? std::filesystem::path()
            : runtimeDirectory.lexically_normal();
    }

    void CrashDumpWriter::WriteDumpOnce(const std::string &faulting_module_id,
                                         unsigned long exception_code) {
        auto &impl = *m_Impl;

        // Only write one dump per session.
        // NOTE: This runs after a SEH catch, so process state may be partially
        // corrupted (especially after EXCEPTION_STACK_OVERFLOW). The filesystem
        // and MiniDumpWriteDump calls are best-effort. An out-of-process crash
        // handler would be more reliable but is outside the current scope.
        bool expected = false;
        if (!impl.dump_written.compare_exchange_strong(expected, true))
            return;

#if defined(_WIN32)
        std::lock_guard lock(impl.mutex);

        if (impl.runtime_directory.empty()) {
            CoreLog(BML_LOG_WARN, kLogCategory,
                    "Cannot write crash dump: runtime directory not set");
            return;
        }

        const auto layout = utils::ResolveRuntimeLayoutFromRuntimeDirectory(impl.runtime_directory);
        std::filesystem::path dump_dir = layout.crash_dumps_directory;

        std::error_code ec;
        std::filesystem::create_directories(dump_dir, ec);
        if (ec) {
            CoreLog(BML_LOG_WARN, kLogCategory,
                    "Failed to create dump directory: %s", ec.message().c_str());
            return;
        }

        const std::wstring filename = utils::Utf8ToUtf16(
            "crash_" + utils::GetCurrentLocalCompactTimestamp() + ".dmp");
        std::filesystem::path dump_path = dump_dir / filename;

        HANDLE file = CreateFileW(dump_path.c_str(),
                                   GENERIC_WRITE,
                                   0,
                                   nullptr,
                                   CREATE_ALWAYS,
                                   FILE_ATTRIBUTE_NORMAL,
                                   nullptr);
        if (file == INVALID_HANDLE_VALUE) {
            CoreLog(BML_LOG_WARN, kLogCategory,
                    "Failed to create dump file: %s",
                    utils::Utf16ToUtf8(dump_path.wstring()).c_str());
            return;
        }

        MINIDUMP_TYPE dump_type = static_cast<MINIDUMP_TYPE>(
            MiniDumpNormal | MiniDumpWithThreadInfo);

        BOOL result = MiniDumpWriteDump(
            GetCurrentProcess(),
            GetCurrentProcessId(),
            file,
            dump_type,
            nullptr,  // No exception info available (SEH already caught)
            nullptr,
            nullptr);

        CloseHandle(file);

        if (result) {
            CoreLog(BML_LOG_ERROR, kLogCategory,
                    "Crash dump written: %s (module=%s, code=0x%08lX)",
                    utils::Utf16ToUtf8(dump_path.wstring()).c_str(),
                    faulting_module_id.c_str(),
                    exception_code);
        } else {
            CoreLog(BML_LOG_WARN, kLogCategory,
                    "MiniDumpWriteDump failed (error %lu)", GetLastError());
            std::filesystem::remove(dump_path, ec);
        }
#else
        (void)faulting_module_id;
        (void)exception_code;
        CoreLog(BML_LOG_WARN, kLogCategory,
                "Crash dump not supported on this platform");
#endif
    }

    void CrashDumpWriter::Shutdown() {
        auto &impl = *m_Impl;
        std::lock_guard lock(impl.mutex);
        impl.runtime_directory.clear();
        impl.dump_written.store(false);
    }
} // namespace BML::Core
