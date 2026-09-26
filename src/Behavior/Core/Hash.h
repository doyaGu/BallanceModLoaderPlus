#ifndef BML_BEHAVIOR_CORE_HASH_H
#define BML_BEHAVIOR_CORE_HASH_H

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

// FNV-1a over the in-memory bytes of each value. Graph fingerprints, overlay
// identities, Layout identities, and the Pattern port shape all hash through
// here. The port shape must stay bit-identical to Detail's Edit::Shape.
namespace BML::Behavior::Internal::Fnv {

inline constexpr std::uint64_t Offset = 1469598103934665603ull;
inline constexpr std::uint64_t Prime = 1099511628211ull;

inline void Bytes(std::uint64_t &hash, const void *data,
                  std::size_t size) noexcept {
    const auto *bytes = static_cast<const unsigned char *>(data);
    for (std::size_t index = 0; index < size; ++index) {
        hash ^= bytes[index];
        hash *= Prime;
    }
}

template <class T>
void Value(std::uint64_t &hash, const T &value) noexcept {
    static_assert(std::is_trivially_copyable_v<T>);
    Bytes(hash, &value, sizeof(value));
}

// Length-prefixed, so adjacent strings cannot alias one another.
inline void Text(std::uint64_t &hash, std::string_view text) noexcept {
    Value(hash, static_cast<std::uint64_t>(text.size()));
    Bytes(hash, text.data(), text.size());
}

} // namespace BML::Behavior::Internal::Fnv

#endif // BML_BEHAVIOR_CORE_HASH_H
