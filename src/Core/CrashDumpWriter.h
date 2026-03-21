#ifndef BML_CORE_CRASH_DUMP_WRITER_H
#define BML_CORE_CRASH_DUMP_WRITER_H

#include <memory>
#include <string>

namespace BML::Core {
    /**
     * @brief Writes minidump files on first SEH catch per session.
     *
     * Dumps go to <base_dir>/ModLoader/CrashDumps/<timestamp>.dmp
     */
    class CrashDumpWriter {
    public:
        CrashDumpWriter();
        ~CrashDumpWriter();

        CrashDumpWriter(const CrashDumpWriter &) = delete;
        CrashDumpWriter &operator=(const CrashDumpWriter &) = delete;

        /** Set the base directory (typically the game directory). */
        void SetBaseDir(const std::wstring &base_dir);

        /**
         * @brief Write a minidump if this is the first crash in this session.
         *
         * @param faulting_module_id Module that crashed
         * @param exception_code     Win32 exception code
         */
        void WriteDumpOnce(const std::string &faulting_module_id,
                           unsigned long exception_code);

        void Shutdown();

    private:
        struct Impl;
        std::unique_ptr<Impl> m_Impl;
    };
} // namespace BML::Core

#endif /* BML_CORE_CRASH_DUMP_WRITER_H */
