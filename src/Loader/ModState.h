#ifndef BML_LOADER_MODSTATE_H
#define BML_LOADER_MODSTATE_H

#include <atomic>

namespace BML {
// Shared admission state for native and script owners. Backend-specific
// diagnostics and VM/module teardown are kept outside this state.
class ModState {
public:
    bool IsLoaded() const noexcept { return m_Loaded.load(std::memory_order_acquire); }

    bool IsFailed() const noexcept { return m_Failed.load(std::memory_order_acquire); }

    void MarkLoaded(bool loaded) noexcept { m_Loaded.store(loaded, std::memory_order_release); }

    void Fail() noexcept { m_Failed.store(true, std::memory_order_release); }

    void ClearFailure() noexcept { m_Failed.store(false, std::memory_order_release); }

private:
    std::atomic_bool m_Loaded{false}, m_Failed{false};
};
} // namespace BML
#endif
