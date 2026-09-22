#include "IvpTestAdapter.h"

#include "BML/IVP.h"

#include <cstdint>
#include <cstring>

extern "C" uintptr_t BML_IvpTestResolveRetailCall(
    std::uint32_t rva) noexcept;

namespace {

int BML_CDECL ResolveRva(std::uint32_t rva, uintptr_t *out) noexcept {
    if (!out)
        return BML_ERROR_INVALID_PARAMETER;
    *out = BML_IvpTestResolveRetailCall(rva);
    return *out ? BML_OK : BML_ERROR_NOT_FOUND;
}

const BML_IvpInterface Interface{
    BML_IFACE_HEADER(BML_IvpInterface, BML_IVP_INTERFACE_ID,
                     BML_IVP_INTERFACE_MAJOR, BML_IVP_INTERFACE_MINOR),
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    &ResolveRva,
    nullptr,
    nullptr,
};

} // namespace

extern "C" int BML_CDECL BML_GetInterface(
    const char *interfaceId, std::uint16_t major, const void **out) {
    if (!out)
        return BML_ERROR_INVALID_PARAMETER;
    *out = nullptr;
    if (!interfaceId)
        return BML_ERROR_INVALID_PARAMETER;
    if (std::strcmp(interfaceId, BML_IVP_INTERFACE_ID) != 0)
        return BML_ERROR_NOT_FOUND;
    if (major != BML_IVP_INTERFACE_MAJOR)
        return BML_ERROR_VERSION_MISMATCH;
    *out = &Interface;
    return BML_OK;
}
