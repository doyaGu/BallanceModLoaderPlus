#ifndef BML_API_HANDLESEQUENCE_H
#define BML_API_HANDLESEQUENCE_H
#include <atomic>
#include <cstdint>
namespace BML::Api {
// Zero is a permanent exhausted sentinel. Wrapping must never make an old
// published handle designate a different object in the same loader run.
inline uint64_t NextHandle(std::atomic<uint64_t> &next) noexcept {
    auto value = next.load(std::memory_order_relaxed);
    while (value && !next.compare_exchange_weak(value, value + 1, std::memory_order_relaxed)) {
    }
    return value;
}
} // namespace BML::Api
#endif
