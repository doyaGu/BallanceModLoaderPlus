#ifndef BML_CORE_FAULT_TRACKER_H
#define BML_CORE_FAULT_TRACKER_H

#include <string>

namespace BML::Core {
    /**
     * @brief Persistent fault tracking for module crashes.
     *
     * Writes ModLoader/fault_log.json after each crash event.
     * On startup, provides module skip list for ModuleDiscovery.
     */
    class FaultTracker {
    public:
        struct FaultRecord {
            int fault_count = 0;
            std::string last_fault;      // ISO 8601 timestamp
            std::string last_code;       // hex exception code
            bool disabled = false;
        };

        /** Load fault_log.json from the given directory. */
        void Load(const std::wstring &base_dir);

        /** Record a crash event for a module. Writes JSON immediately. */
        void RecordFault(const std::string &module_id,
                         unsigned long exception_code);

        /** Check if a module should be skipped on startup. */
        bool IsDisabled(const std::string &module_id) const;

        /** Re-enable a disabled module. */
        void Enable(const std::string &module_id);

        /** Get fault count for a module. */
        int GetFaultCount(const std::string &module_id) const;

        void Shutdown();

        FaultTracker() = default;
    };
} // namespace BML::Core

#endif /* BML_CORE_FAULT_TRACKER_H */
