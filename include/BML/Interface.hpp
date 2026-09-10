// Type-safe C++ authoring helpers for the versioned C interface registry.
//
// Interface.h remains the stable ABI seam. This header owns the repetitive C++
// side of that seam: declaring interface traits once, building a correctly headed
// table, publishing it with scoped cleanup, and retaining a typed borrowed
// reference without losing the BML status that produced it.
#ifndef BML_INTERFACE_HPP
#define BML_INTERFACE_HPP

#include "BML/Interface.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <utility>

namespace BML::Interfaces {

// Declares the common case: every member through lastRequiredMember belongs to
// the interface's minimum usable surface, and that last member must be non-null.
// Put the declaration in the shared provider/consumer header next to the plain-C
// interface struct.
#define BML_DECLARE_INTERFACE_TRAITS(name, type, id, major, lastRequiredMember)                      \
    struct name {                                                                                    \
        using Interface = type;                                                                      \
        static constexpr const char *InterfaceId() noexcept { return (id); }                         \
        static constexpr std::uint16_t MajorVersion = static_cast<std::uint16_t>(major);             \
        static constexpr std::size_t RequiredSize =                                                   \
            offsetof(type, lastRequiredMember) + sizeof(((type *) nullptr)->lastRequiredMember);     \
        static bool Accepts(const Interface &value) noexcept {                                        \
            return value.Header.StructSize >= RequiredSize && value.lastRequiredMember != nullptr;   \
        }                                                                                             \
    }

namespace Detail {

template <class Traits>
constexpr void CheckTraits() noexcept {
    using Interface = typename Traits::Interface;
    using Header = std::remove_cv_t<std::remove_reference_t<decltype(std::declval<Interface &>().Header)>>;
    static_assert(std::is_standard_layout_v<Interface>,
                  "A BML interface must be a standard-layout plain-C struct");
    static_assert(std::is_same_v<Header, BML_InterfaceHeader>,
                  "The first interface member must be named Header and have type BML_InterfaceHeader");
    static_assert(offsetof(Interface, Header) == 0,
                  "BML_InterfaceHeader must be the first interface member");
    static_assert(Traits::MajorVersion > 0, "A BML interface major version cannot be zero");
    static_assert(Traits::RequiredSize >= sizeof(BML_InterfaceHeader),
                  "The required interface surface cannot be smaller than BML_InterfaceHeader");
    static_assert(Traits::RequiredSize <= sizeof(Interface),
                  "The required interface surface cannot exceed the declared interface struct");
}

template <class Traits>
[[nodiscard]] int Validate(const typename Traits::Interface *interfacePtr) noexcept {
    CheckTraits<Traits>();
    if (!interfacePtr || !interfacePtr->Header.InterfaceId)
        return BML_ERROR_MALFORMED_MESSAGE;
    if (std::strcmp(interfacePtr->Header.InterfaceId, Traits::InterfaceId()) != 0)
        return BML_ERROR_MALFORMED_MESSAGE;
    if (interfacePtr->Header.MajorVersion != Traits::MajorVersion ||
        interfacePtr->Header.StructSize < Traits::RequiredSize || !Traits::Accepts(*interfacePtr))
        return BML_ERROR_VERSION_MISMATCH;
    return BML_OK;
}

} // namespace Detail

// Builds the provider's immutable table without making the author repeat its
// Header. Keep the returned object in static const/constexpr storage; the loader
// rejects stack and heap interface tables even when their contents are valid.
template <class Traits, class... Members>
[[nodiscard]] constexpr typename Traits::Interface MakeInterface(
    std::uint16_t minorVersion, Members &&...members) noexcept {
    Detail::CheckTraits<Traits>();
    using Interface = typename Traits::Interface;
    return Interface{BML_InterfaceHeader{sizeof(Interface), Traits::MajorVersion, minorVersion,
                                         Traits::InterfaceId()},
                     std::forward<Members>(members)...};
}

// A typed borrowed reference returned by the loader. It never owns or releases
// the provider table. Required dependency ordering keeps the provider alive
// through the consumer's OnUnload; Reset merely forgets the borrowed pointer.
template <class Traits>
class Reference final {
public:
    using Interface = typename Traits::Interface;

    Reference() noexcept { Detail::CheckTraits<Traits>(); }
    Reference(const Reference &) = delete;
    Reference &operator=(const Reference &) = delete;

    Reference(Reference &&other) noexcept
        : m_Interface(std::exchange(other.m_Interface, nullptr)),
          m_Status(std::exchange(other.m_Status, BML_ERROR_NOT_FOUND)) {}

    Reference &operator=(Reference &&other) noexcept {
        if (this != &other) {
            m_Interface = std::exchange(other.m_Interface, nullptr);
            m_Status = std::exchange(other.m_Status, BML_ERROR_NOT_FOUND);
        }
        return *this;
    }

    [[nodiscard]] int Open() noexcept {
        const void *found = nullptr;
        m_Interface = nullptr;
        m_Status = BML_GetInterface(Traits::InterfaceId(), Traits::MajorVersion, &found);
        if (m_Status != BML_OK)
            return m_Status;

        const auto *typed = static_cast<const Interface *>(found);
        m_Status = Detail::Validate<Traits>(typed);
        if (m_Status == BML_OK)
            m_Interface = typed;
        return m_Status;
    }

    void Reset() noexcept {
        m_Interface = nullptr;
        m_Status = BML_ERROR_NOT_FOUND;
    }

    [[nodiscard]] explicit operator bool() const noexcept { return m_Interface != nullptr; }
    [[nodiscard]] bool IsOpen() const noexcept { return m_Interface != nullptr; }
    [[nodiscard]] int Status() const noexcept { return m_Status; }
    [[nodiscard]] const Interface *Get() const noexcept { return m_Interface; }
    [[nodiscard]] const Interface *operator->() const noexcept {
        BML_ASSERT(m_Interface != nullptr);
        return m_Interface;
    }
    [[nodiscard]] const Interface &operator*() const noexcept {
        BML_ASSERT(m_Interface != nullptr);
        return *m_Interface;
    }

private:
    const Interface *m_Interface = nullptr;
    int m_Status = BML_ERROR_NOT_FOUND;
};

// Owns one successful registration. Close is idempotent; destruction performs
// best-effort cleanup. The loader remains the final safety net and removes an
// outstanding registration before releasing the provider DLL.
template <class Traits>
class Publication final {
public:
    using Interface = typename Traits::Interface;

    Publication() noexcept { Detail::CheckTraits<Traits>(); }
    ~Publication() { (void)Close(); }
    Publication(const Publication &) = delete;
    Publication &operator=(const Publication &) = delete;

    Publication(Publication &&other) noexcept
        : m_Interface(std::exchange(other.m_Interface, nullptr)),
          m_Status(std::exchange(other.m_Status, BML_OK)) {}

    Publication &operator=(Publication &&other) noexcept {
        if (this != &other) {
            std::swap(m_Interface, other.m_Interface);
            std::swap(m_Status, other.m_Status);
        }
        return *this;
    }

    [[nodiscard]] int Open(const Interface &interfaceValue) noexcept {
        if (m_Interface) {
            m_Status = BML_ERROR_ALREADY_EXISTS;
            return m_Status;
        }
        m_Status = Detail::Validate<Traits>(&interfaceValue);
        if (m_Status != BML_OK)
            return m_Status;
        m_Status = BML_RegisterInterface(nullptr, &interfaceValue);
        if (m_Status == BML_OK)
            m_Interface = &interfaceValue;
        return m_Status;
    }

    [[nodiscard]] int Close() noexcept {
        if (!m_Interface) {
            m_Status = BML_OK;
            return m_Status;
        }
        m_Status = BML_UnregisterInterface(nullptr, Traits::InterfaceId(), Traits::MajorVersion);
        if (m_Status == BML_OK || m_Status == BML_ERROR_NOT_FOUND)
            m_Interface = nullptr;
        return m_Status;
    }

    [[nodiscard]] explicit operator bool() const noexcept { return m_Interface != nullptr; }
    [[nodiscard]] bool IsOpen() const noexcept { return m_Interface != nullptr; }
    [[nodiscard]] int Status() const noexcept { return m_Status; }
    [[nodiscard]] const Interface *Get() const noexcept { return m_Interface; }

private:
    const Interface *m_Interface = nullptr;
    int m_Status = BML_OK;
};

} // namespace BML::Interfaces

#endif // BML_INTERFACE_HPP
