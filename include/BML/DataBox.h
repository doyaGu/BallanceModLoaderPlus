/**
 * @file DataBox.h
 * @brief Thread-safe storage for arbitrary user data slots keyed by a size_t "type".
 */
#ifndef BML_DATABOX_H
#define BML_DATABOX_H

#include <mutex>
#include <unordered_map>
#include <utility>

namespace BML {
    /**
     * @brief A container for storing user data pointers keyed by a caller-provided "type".
     *
     * Notes:
     *  - Uses an unordered_map<size_t, void*> under a single mutex: O(1) avg lookups.
     *  - Never casts pointers to integers (avoids 32/64-bit truncation pitfalls). :contentReference[oaicite:2]{index=2}
     *  - SetData returns the previous pointer (or nullptr if none).
     *  - No ownership semantics: callers manage the lifetime of stored pointers.
     */
    class DataBox {
    public:
        DataBox() = default;
        DataBox(const DataBox &) = delete;
        DataBox &operator=(const DataBox &) = delete;

        /**
         * @brief Get the pointer stored for @p type, or nullptr if absent.
         */
        void *GetData(std::size_t type) const noexcept {
            std::lock_guard<std::mutex> g(m_RWLock);
            const auto it = m_UserData.find(type);
            return (it == m_UserData.end()) ? nullptr : it->second;
        }

        /**
         * @brief Set the pointer for @p type. Returns the previous pointer (or nullptr).
         */
        void *SetData(void *data, std::size_t type) noexcept {
            std::lock_guard<std::mutex> g(m_RWLock);
            auto &slot = m_UserData[type];
            void *prev = slot;
            slot = data;
            return prev;
        }

        /**
         * @brief Remove an entry; returns the removed pointer (or nullptr if none).
         */
        void *RemoveData(std::size_t type) noexcept {
            std::lock_guard<std::mutex> g(m_RWLock);
            const auto it = m_UserData.find(type);
            if (it == m_UserData.end()) return nullptr;
            void *prev = it->second;
            m_UserData.erase(it);
            return prev;
        }

        /// Clear all entries (does not free what pointers point to).
        void Clear() noexcept {
            std::lock_guard<std::mutex> g(m_RWLock);
            m_UserData.clear();
        }

        /// Number of stored entries.
        std::size_t Size() const noexcept {
            std::lock_guard<std::mutex> g(m_RWLock);
            return m_UserData.size();
        }

    private:
        mutable std::mutex m_RWLock;
        std::unordered_map<std::size_t, void *> m_UserData;
    };
} // namespace BML

#endif // BML_DATABOX_H
