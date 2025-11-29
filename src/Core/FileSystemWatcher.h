#ifndef BML_CORE_FILE_SYSTEM_WATCHER_H
#define BML_CORE_FILE_SYSTEM_WATCHER_H

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

namespace efsw {
    class FileWatcher;
    class FileWatchListener;
}

namespace BML::Core {
    /**
     * @brief File system change actions
     */
    enum class FileAction {
        Added,    ///< File or directory was created
        Deleted,  ///< File or directory was deleted
        Modified, ///< File was modified
        Moved     ///< File or directory was renamed/moved
    };

    /**
     * @brief File system change event
     */
    struct FileEvent {
        std::string directory;                           ///< Directory where the event occurred
        std::string filename;                            ///< Name of the affected file
        std::string old_filename;                        ///< Previous name (for Moved events)
        FileAction action;                               ///< Type of change
        std::chrono::steady_clock::time_point timestamp; ///< When the event was received
    };

    /**
     * @brief Callback type for file change events
     */
    using FileEventCallback = std::function<void(const FileEvent &)>;

    /**
     * @brief Cross-platform file system watcher using efsw
     *
     * Provides event-driven file system monitoring using native OS mechanisms
     * (inotify on Linux, IOCP on Windows, FSEvents on macOS).
     *
     * Usage:
     * @code
     * FileSystemWatcher watcher;
     * watcher.SetCallback([](const FileEvent& e) {
     *     std::cout << "File changed: " << e.filename << std::endl;
     * });
     * watcher.Watch("/path/to/directory", true);  // recursive
     * watcher.Start();
     * @endcode
     */
    class FileSystemWatcher {
    public:
        FileSystemWatcher();
        ~FileSystemWatcher();

        FileSystemWatcher(const FileSystemWatcher &) = delete;
        FileSystemWatcher &operator=(const FileSystemWatcher &) = delete;

        /**
         * @brief Add a directory to watch
         * @param path Directory path to watch
         * @param recursive Whether to watch subdirectories
         * @return Watch ID (positive) or error code (negative)
         */
        long Watch(const std::string &path, bool recursive = false);

        /**
         * @brief Stop watching a directory by ID
         * @param watch_id Watch ID returned by Watch()
         */
        void Unwatch(long watch_id);

        /**
         * @brief Stop watching a directory by path
         * @param path Directory path to stop watching
         */
        void Unwatch(const std::string &path);

        /**
         * @brief Set the callback for file change events
         * @param callback Function to call when files change
         */
        void SetCallback(FileEventCallback callback);

        /**
         * @brief Start the watcher thread
         *
         * The watcher runs in a background thread and invokes the callback
         * when file changes are detected.
         */
        void Start();

        /**
         * @brief Stop the watcher thread
         */
        void Stop();

        /**
         * @brief Check if the watcher is running
         * @return true if watching, false otherwise
         */
        bool IsRunning() const;

    private:
        class Listener;

        std::unique_ptr<efsw::FileWatcher> m_Watcher;
        std::unique_ptr<Listener> m_Listener;
        FileEventCallback m_Callback;
        mutable std::mutex m_Mutex;
        bool m_Running{false};
    };
} // namespace BML::Core

#endif // BML_CORE_FILE_SYSTEM_WATCHER_H
