#ifndef BML_CORE_MOD_HANDLE_H
#define BML_CORE_MOD_HANDLE_H

#include <atomic>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "bml_types.h"
#include "bml_logging.h"
#include "bml_core.h"  // For BML_ShutdownCallback

namespace BML::Core {
    struct ModManifest;
    struct LoadedModule;

    struct LogFileCloser {
        void operator()(FILE *file) const {
            if (file) {
                fclose(file);
            }
        }
    };
} // namespace BML::Core

struct BML_Mod_T {
    struct ShutdownHook {
        BML_ShutdownCallback callback{nullptr}; // Task 2.3 - unified callback signature
        void *user_data{nullptr};
    };

    std::string id;
    BML_Version version{};
    const BML::Core::ModManifest *manifest{nullptr};
    std::vector<std::string> capabilities;
    std::vector<ShutdownHook> shutdown_hooks;
    std::wstring log_path;
    std::unique_ptr<FILE, BML::Core::LogFileCloser> log_file;
    std::atomic<int> minimum_severity{BML_LOG_INFO};
};

#endif // BML_CORE_MOD_HANDLE_H
