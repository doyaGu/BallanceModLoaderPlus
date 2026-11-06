#ifndef BML_DETAIL_API_HPP
#define BML_DETAIL_API_HPP

#include "BML/Interface.h"
#include "BML/Result.hpp"

#include <new>
#include <atomic>
#include <string>
#include <string_view>
#include <vector>

namespace BML::Detail {

template <class Interface>
const Interface *Bind(const char *id, uint16_t major = 1, size_t minimumSize = sizeof(Interface),
                      bool (*accept)(const Interface *) = nullptr) noexcept {
    // Host tables have process lifetime. A failed lookup is not cached, allowing
    // a later, declared provider to become available during activation.
    static std::atomic<const Interface *> cached{nullptr};
    if (const auto *table = cached.load(std::memory_order_acquire))
        return table->Header.StructSize >= minimumSize ? table : nullptr;
    const void *table = nullptr;
    if (BML_GetInterface(id, major, &table) != BML_OK)
        return nullptr;
    const auto *typed = static_cast<const Interface *>(table);
    if (!typed || typed->Header.StructSize < minimumSize || typed->Header.MajorVersion != major ||
        !typed->Header.InterfaceId || std::strcmp(typed->Header.InterfaceId, id) != 0)
        return nullptr;
    if (accept && !accept(typed))
        return nullptr;
    cached.store(typed, std::memory_order_release);
    return typed;
}

inline bool IsText(std::string_view text) noexcept {
    return text.find('\0') == std::string_view::npos;
}

template <class Body> int Guard(Body &&body) noexcept {
    try {
        return body();
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
}

// Adapt module-local visitors to an explicit cdecl function pointer even when
// the consumer changes its compiler's default calling convention.
template <auto Function, class Signature = decltype(+Function)> struct Cdecl;
template <auto Function, class R, class... Args> struct Cdecl<Function, R (*)(Args...)> {
    static R BML_CDECL Call(Args... args) noexcept { return Function(std::forward<Args>(args)...); }
};
template <auto Function, class R, class... Args> struct Cdecl<Function, R (*)(Args...) noexcept> {
    static R BML_CDECL Call(Args... args) noexcept { return Function(std::forward<Args>(args)...); }
};

template <class Container, class Read> Result<Container> ReadBuffer(Read &&read) {
    Container value;
    size_t required = 0;
    int code = read(nullptr, 0, &required);
    if (code != BML_OK && code != BML_ERROR_BUFFER_TOO_SMALL)
        return Result<Container>::Failure(code);
    for (unsigned attempt = 0; attempt != 2; ++attempt) {
        if (required > value.max_size())
            return Result<Container>::Failure(BML_ERROR_OUT_OF_MEMORY);
        value.resize(required);
        size_t actual = 0;
        code = read(value.empty() ? nullptr : value.data(), value.size(), &actual);
        if (code == BML_OK) {
            if (actual > value.size())
                return Result<Container>::Failure(BML_ERROR_MALFORMED_MESSAGE);
            if constexpr (std::is_same_v<Container, std::string>) {
                if (!actual || value[actual - 1] != '\0')
                    return Result<Container>::Failure(BML_ERROR_MALFORMED_MESSAGE);
                value.resize(actual - 1);
            } else {
                value.resize(actual);
            }
            return Result<Container>::Success(std::move(value));
        }
        if (code != BML_ERROR_BUFFER_TOO_SMALL)
            return Result<Container>::Failure(code);
        required = actual;
    }
    return Result<Container>::Failure(BML_ERROR_BUFFER_TOO_SMALL);
}

} // namespace BML::Detail
#endif
