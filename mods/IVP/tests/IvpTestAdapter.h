#ifndef BML_TESTS_IVP_TEST_ADAPTER_H
#define BML_TESTS_IVP_TEST_ADAPTER_H

#include "BML/IVP/Calls.h"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace BML::IVP::Test {

struct RetailCallBinding {
    ABI::Address Address;
    uintptr_t Target;
};

template <typename Function>
RetailCallBinding Bind(ABI::Address address, Function function) noexcept {
    static_assert(std::is_pointer_v<Function>);
    static_assert(std::is_function_v<std::remove_pointer_t<Function>>);
    return {address, reinterpret_cast<uintptr_t>(function)};
}

template <std::size_t Count>
uintptr_t Resolve(
    std::uint32_t rva,
    const RetailCallBinding (&bindings)[Count]) noexcept {
    const auto address = static_cast<ABI::Address>(rva);
    for (const RetailCallBinding &binding : bindings) {
        if (binding.Address == address)
            return binding.Target;
    }
    return 0;
}

} // namespace BML::IVP::Test

#endif // BML_TESTS_IVP_TEST_ADAPTER_H
