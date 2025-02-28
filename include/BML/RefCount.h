/**
 * @file RefCount.h
 * @brief The class implements reference counting
 */
#ifndef BML_REFCOUNT_H
#define BML_REFCOUNT_H

#include <atomic>

namespace BML {
    /**
     * @class RefCount
     * @brief The class implements reference counting
     */
    class RefCount {
    public:
        RefCount() : m_RefCount(1) {} /**< Default Constructor */
        explicit RefCount(int value) : m_RefCount(value) {} /**< Constructor */
        ~RefCount() = default; /**< Destructor */

        RefCount &operator++() { AddRef(); return *this; } /**< Operator++ override */
        RefCount &operator--() { Release(); return *this; } /**< Operator-- override */

        /**
         * @brief Increment reference count.
         * @return Current reference count.
         */
        int AddRef() { return m_RefCount.fetch_add(1, std::memory_order_relaxed); }

        /**
         * @brief Decrement reference count.
         * @return Current reference count.
         */
        int Release() { return m_RefCount.fetch_sub(1, std::memory_order_acq_rel); }

        /**
         * @brief Get reference count.
         * @return Current reference count.
         */
        int GetCount() { return m_RefCount.load(std::memory_order_relaxed); }

    private:
        std::atomic<int> m_RefCount; /**< Reference count. */
    };
}

#endif // BML_REFCOUNT_H
