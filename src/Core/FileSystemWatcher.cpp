#include "FileSystemWatcher.h"

#include <efsw/efsw.hpp>

#include "Logging.h"

namespace BML::Core {
    namespace {
        constexpr char kLogCategory[] = "fs.watcher";

        FileAction ConvertAction(efsw::Action action) {
            switch (action) {
            case efsw::Actions::Add: return FileAction::Added;
            case efsw::Actions::Delete: return FileAction::Deleted;
            case efsw::Actions::Modified: return FileAction::Modified;
            case efsw::Actions::Moved: return FileAction::Moved;
            default: return FileAction::Modified;
            }
        }

        const char *ActionToString(FileAction action) {
            switch (action) {
            case FileAction::Added: return "Added";
            case FileAction::Deleted: return "Deleted";
            case FileAction::Modified: return "Modified";
            case FileAction::Moved: return "Moved";
            default: return "Unknown";
            }
        }
    }

    /**
     * @brief Internal listener implementation for efsw
     */
    class FileSystemWatcher::Listener : public efsw::FileWatchListener {
    public:
        explicit Listener(FileSystemWatcher *owner) : m_Owner(owner) {}

        void handleFileAction(efsw::WatchID watchid,
                              const std::string &dir,
                              const std::string &filename,
                              efsw::Action action,
                              std::string oldFilename) override {
            if (!m_Owner) return;

            FileEventCallback callback;
            {
                std::lock_guard lock(m_Owner->m_Mutex);
                if (!m_Owner->m_Running) return;
                callback = m_Owner->m_Callback;
            }

            if (!callback) return;

            FileEvent event;
            event.directory = dir;
            event.filename = filename;
            event.old_filename = std::move(oldFilename);
            event.action = ConvertAction(action);
            event.timestamp = std::chrono::steady_clock::now();

            CoreLog(BML_LOG_DEBUG, kLogCategory,
                    "File event: %s/%s [%s]",
                    dir.c_str(), filename.c_str(), ActionToString(event.action));

            try {
                callback(event);
            } catch (const std::exception &ex) {
                CoreLog(BML_LOG_ERROR, kLogCategory,
                        "Exception in file event callback: %s", ex.what());
            } catch (...) {
                CoreLog(BML_LOG_ERROR, kLogCategory,
                        "Unknown exception in file event callback");
            }
        }

    private:
        FileSystemWatcher *m_Owner;
    };

    FileSystemWatcher::FileSystemWatcher()
        : m_Watcher(std::make_unique<efsw::FileWatcher>()), m_Listener(std::make_unique<Listener>(this)) {}

    FileSystemWatcher::~FileSystemWatcher() {
        Stop();
    }

    long FileSystemWatcher::Watch(const std::string &path, bool recursive) {
        std::lock_guard lock(m_Mutex);

        auto id = m_Watcher->addWatch(path, m_Listener.get(), recursive);
        if (id < 0) {
            CoreLog(BML_LOG_ERROR, kLogCategory,
                    "Failed to watch path '%s': error %ld", path.c_str(), id);
        } else {
            CoreLog(BML_LOG_DEBUG, kLogCategory,
                    "Watching path '%s' (id=%ld, recursive=%s)",
                    path.c_str(), id, recursive ? "true" : "false");
        }
        return id;
    }

    void FileSystemWatcher::Unwatch(long watch_id) {
        std::lock_guard lock(m_Mutex);
        m_Watcher->removeWatch(watch_id);
        CoreLog(BML_LOG_DEBUG, kLogCategory, "Removed watch id=%ld", watch_id);
    }

    void FileSystemWatcher::Unwatch(const std::string &path) {
        std::lock_guard lock(m_Mutex);
        m_Watcher->removeWatch(path);
        CoreLog(BML_LOG_DEBUG, kLogCategory, "Removed watch for path '%s'", path.c_str());
    }

    void FileSystemWatcher::SetCallback(FileEventCallback callback) {
        std::lock_guard lock(m_Mutex);
        m_Callback = std::move(callback);
    }

    void FileSystemWatcher::Start() {
        std::lock_guard lock(m_Mutex);
        if (m_Running) return;

        m_Running = true;
        m_Watcher->watch();
        CoreLog(BML_LOG_INFO, kLogCategory, "File system watcher started");
    }

    void FileSystemWatcher::Stop() {
        std::unique_ptr<efsw::FileWatcher> old_watcher;
        {
            std::lock_guard lock(m_Mutex);
            if (!m_Running) return;
            m_Running = false;

            // Move the old watcher out so we can destroy it outside the lock
            // to avoid deadlock with the callback thread
            old_watcher = std::move(m_Watcher);
            m_Watcher = std::make_unique<efsw::FileWatcher>();
        }

        // Destroy the old watcher outside the lock
        // This will block until the background thread exits
        old_watcher.reset();

        CoreLog(BML_LOG_INFO, kLogCategory, "File system watcher stopped");
    }

    bool FileSystemWatcher::IsRunning() const {
        std::lock_guard lock(m_Mutex);
        return m_Running;
    }
} // namespace BML::Core
