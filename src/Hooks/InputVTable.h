#ifndef BML_HOOKS_INPUT_VTABLE_H
#define BML_HOOKS_INPUT_VTABLE_H

#include <cstddef>
#include <cstring>
#include <limits>
#include <type_traits>

#include "CKInputManager.h"

#if defined(_MSC_VER)
#include "Hooks/InputVTableMsvc.h"
#define BML_INPUT_SLOT(Name) \
    (offsetof(InputManagerMethods<CKInputManager>, Name) / \
     sizeof(decltype(InputManagerMethods<CKInputManager>::Name)))
#else
#define BML_INPUT_SLOT(Name) InputVTable::Slot(&CKInputManager::Name)
#endif

namespace InputVTable {

// Only non-virtual, single-inheritance replacement methods are passed here.
// A direct member pointer stores the function address first, with zero this
// adjustment. Keep the rest of its ABI representation initialized as well.
template<typename Method>
Method Direct(void *address) {
    static_assert(std::is_member_function_pointer_v<Method>);
    static_assert(sizeof(Method) >= sizeof(address));
    Method result{};
    std::memcpy(&result, &address, sizeof(address));
    return result;
}

template<typename Method>
void *Address(Method method) {
    static_assert(std::is_member_function_pointer_v<Method>);
    static_assert(sizeof(Method) >= sizeof(void *));
    void *address = nullptr;
    std::memcpy(&address, &method, sizeof(address));
    return address;
}

#if !defined(_MSC_VER)
// Itanium's ARM variant puts the virtual bit in the this-adjustment word;
// x86 puts it in the function word. Read the compiler's actual public method
// pointer rather than assuming the legacy destructor count or slot numbers.
template<typename Method>
std::size_t Slot(Method method) {
    struct Representation { std::ptrdiff_t Function; std::ptrdiff_t Adjustment; };
    static_assert(sizeof(Method) == sizeof(Representation));
    Representation value{};
    std::memcpy(&value, &method, sizeof(value));
#if defined(__arm__) || defined(__aarch64__)
    const bool isVirtual = (value.Adjustment & 1) != 0;
    const auto adjustment = value.Adjustment >> 1;
    const auto offset = value.Function;
#else
    const bool isVirtual = (value.Function & 1) != 0;
    const auto adjustment = value.Adjustment;
    const auto offset = value.Function - 1;
#endif
    if (!isVirtual || adjustment != 0 || offset < 0 || offset % sizeof(void *) != 0)
        return (std::numeric_limits<std::size_t>::max)();
    return static_cast<std::size_t>(offset) / sizeof(void *);
}
#endif
} // namespace InputVTable

#endif // BML_HOOKS_INPUT_VTABLE_H
