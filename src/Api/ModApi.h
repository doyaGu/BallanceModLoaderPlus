#ifndef BML_API_MODAPI_H
#define BML_API_MODAPI_H

#include "BML/Mod.h"
#include "Loader/ModContext.h"
#include <new>

namespace BML::Api {
const BML_ModContextInterface &ModContextInterface();

template <class Body> int WithModContext(BML_ModContextHandle handle, Body &&body) noexcept {
    try {
        ModContextLease context;
        if (!context)
            return BML_ERROR_FROZEN;
        if (!context->IsMainThread())
            return BML_ERROR_WRONG_THREAD;
        ModRecord *owner = context->GetModLoader().ResolveModContext(handle);
        if (!owner)
            return BML_ERROR_INVALID_HANDLE;
        auto lock = context->GetModLoader().LockModInvocation();
        return context->GetModLoader().InvokeAs(owner, [&] { return body(*context, *owner); });
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
}

inline bool AcceptsModWork(ModContext &context, ModRecord &owner) noexcept {
    if (context.GetModLoader().IsShuttingDown())
        return false;
    const auto *native = context.GetModLoader().FindNativeMod(&owner);
    if (native)
        return native->IsLoading() || native->IsLoaded();
    return context.GetModLoader().CanDispatchOwner(&owner);
}

template <class T> void WriteOutput(T *out, const T &value, size_t knownPrefix) noexcept {
    const size_t written = std::min(out->StructSize, knownPrefix);
    std::memcpy(out, &value, written);
    out->StructSize = written;
}

inline int CopyText(std::string_view text, char *buffer, size_t capacity,
                    size_t *required) noexcept {
    if (!required || (!buffer && capacity))
        return BML_ERROR_INVALID_PARAMETER;
    if (text.size() == SIZE_MAX)
        return BML_ERROR_INVALID_PARAMETER;
    *required = text.size() + 1;
    if (capacity < *required)
        return BML_ERROR_BUFFER_TOO_SMALL;
    std::memcpy(buffer, text.data(), text.size());
    buffer[text.size()] = '\0';
    return BML_OK;
}
} // namespace BML::Api
#endif
