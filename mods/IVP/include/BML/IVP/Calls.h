#ifndef BML_IVP_CALLS_H
#define BML_IVP_CALLS_H

#include "BML/IVP.h"

#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>

namespace BML::IVP::ABI {

#if defined(_WIN32)
static_assert(sizeof(void *) == 4,
              "Ballance's native IVP ABI is available only to x86 Mods");
#endif

// Addresses in this enum have been checked against Ballance's retail x86 DLL.
// The full IDA name/RVA catalog remains available through bml.ivp.
enum class Address : std::uint32_t {
#include "BML/IVP/detail/AddressEntries.inc"
};

#include "BML/IVP/detail/DataAddresses.inc"

template <typename Function>
[[nodiscard]] inline Function Resolve(Address address) noexcept {
    static_assert(std::is_pointer_v<Function>);
    return BML::IVP::ResolveRva<Function>(static_cast<std::uint32_t>(address));
}

template <typename Return, typename... Arguments>
inline Return InvokeThis(Address address, void *self, Arguments... arguments) {
    using Function = Return (__thiscall *)(void *, Arguments...);
    Function function = Resolve<Function>(address);
    if constexpr (std::is_void_v<Return>) {
        if (function)
            function(self, std::forward<Arguments>(arguments)...);
    } else {
        return function
            ? function(self, std::forward<Arguments>(arguments)...)
            : Return{};
    }
}

template <typename Return, typename Fallback, typename... Arguments>
inline Return InvokeThisOr(
    Address address, void *self, Fallback &&fallback,
    Arguments... arguments) {
    using Function = Return (__thiscall *)(void *, Arguments...);
    Function function = Resolve<Function>(address);
    if (function) {
        if constexpr (std::is_void_v<Return>) {
            function(self, std::forward<Arguments>(arguments)...);
            return;
        } else {
            return function(self, std::forward<Arguments>(arguments)...);
        }
    }
    if constexpr (std::is_void_v<Return>) {
        std::forward<Fallback>(fallback)();
    } else {
        return std::forward<Fallback>(fallback)();
    }
}

template <typename Return, typename... Arguments>
inline Return Invoke(Address address, Arguments... arguments) {
    using Function = Return (__cdecl *)(Arguments...);
    Function function = Resolve<Function>(address);
    if constexpr (std::is_void_v<Return>) {
        if (function)
            function(std::forward<Arguments>(arguments)...);
    } else {
        return function
            ? function(std::forward<Arguments>(arguments)...)
            : Return{};
    }
}

template <typename Return, typename... Arguments>
inline Return InvokeStdcall(Address address, Arguments... arguments) {
    using Function = Return (__stdcall *)(Arguments...);
    Function function = Resolve<Function>(address);
    if constexpr (std::is_void_v<Return>) {
        if (function)
            function(std::forward<Arguments>(arguments)...);
    } else {
        return function ? function(std::forward<Arguments>(arguments)...)
                        : Return{};
    }
}

// A retail constructor may leave an object pointing at a vtable owned by
// physics_RT.dll. Its scalar deleting destructor consequently releases the
// allocation through that DLL as well. Public heap-constructible IVP classes
// use this pair so allocation and deletion never straddle the UCRT/MSVCRT
// boundary. These helpers deliberately do not fall back to the caller's heap.
[[nodiscard]] inline void *RetailOperatorNew(std::size_t size) {
    void *memory = Invoke<void *>(Address::OperatorNew,
                                  static_cast<unsigned int>(size));
    if (!memory)
        throw std::bad_alloc();
    return memory;
}

[[nodiscard]] inline void *RetailOperatorNew(
    std::size_t size, const std::nothrow_t &) noexcept {
    try {
        return RetailOperatorNew(size);
    } catch (...) {
        return nullptr;
    }
}

inline void RetailOperatorDelete(void *memory) noexcept {
    if (memory)
        Invoke<void>(Address::OperatorDelete, memory);
}

} // namespace BML::IVP::ABI

// This macro adds no data member or base and therefore cannot alter an IVP
// layout. Array allocation is rejected because Ballance has no array-cookie
// ownership contract for engine objects. The placement overload remains
// available for the audited raw-memory factories used by the compatibility
// layer; placement storage must never be handed to an engine deleting dtor.
#define BML_IVP_RETAIL_ALLOCATED_OBJECT                                      \
    static void *operator new(std::size_t size) {                            \
        return BML::IVP::ABI::RetailOperatorNew(size);                       \
    }                                                                         \
    static void *operator new(std::size_t size,                              \
                              const std::nothrow_t &tag) noexcept {           \
        return BML::IVP::ABI::RetailOperatorNew(size, tag);                  \
    }                                                                         \
    static void operator delete(void *memory) noexcept {                     \
        BML::IVP::ABI::RetailOperatorDelete(memory);                         \
    }                                                                         \
    static void operator delete(void *memory, std::size_t) noexcept {        \
        BML::IVP::ABI::RetailOperatorDelete(memory);                         \
    }                                                                         \
    static void operator delete(void *memory,                                \
                                const std::nothrow_t &) noexcept {            \
        BML::IVP::ABI::RetailOperatorDelete(memory);                         \
    }                                                                         \
    static void *operator new(std::size_t, void *memory) noexcept {          \
        return memory;                                                        \
    }                                                                         \
    static void operator delete(void *, void *) noexcept {}                  \
    static void *operator new[](std::size_t) = delete;                       \
    static void operator delete[](void *) = delete

#define BML_IVP_RETAIL_DEALLOCATED_OBJECT                                    \
    static void operator delete(void *memory) noexcept {                     \
        BML::IVP::ABI::RetailOperatorDelete(memory);                         \
    }                                                                         \
    static void operator delete(void *memory, std::size_t) noexcept {        \
        BML::IVP::ABI::RetailOperatorDelete(memory);                         \
    }

#endif // BML_IVP_CALLS_H
