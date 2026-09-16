// C++ result for BML operations that report a status code and, optionally,
// a value. A module may supply its own diagnostic type without making the
// result container depend on that module. Failure always reports a negative
// BML error code, even if its caller accidentally passes a success code.
#ifndef BML_RESULT_HPP
#define BML_RESULT_HPP

#include "BML/Defines.h"

#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

namespace BML {

template <class T, class TStatus = std::monostate>
class Result {
public:
    Result() = default;

    [[nodiscard]] explicit operator bool() const noexcept {
        return m_Code == BML_OK && m_Value.has_value();
    }
    [[nodiscard]] int Code() const noexcept { return m_Code; }
    [[nodiscard]] const TStatus &GetStatus() const noexcept { return m_Status; }

    [[nodiscard]] bool HasValue() const noexcept { return m_Value.has_value(); }
    [[nodiscard]] T &Value() & { return m_Value.value(); }
    [[nodiscard]] const T &Value() const & { return m_Value.value(); }
    template <class U = T,
              std::enable_if_t<std::is_copy_constructible_v<U>, int> = 0>
    [[nodiscard]] T Value() && {
        if constexpr (std::is_move_constructible_v<T>)
            return std::move(m_Value).value();
        else
            return m_Value.value();
    }
    template <class U = T,
              std::enable_if_t<!std::is_copy_constructible_v<U>, int> = 0>
    [[nodiscard]] T Value() && = delete;
    [[nodiscard]] const T &&Value() const && = delete;

    // Consuming the value preserves the code and diagnostic for inspection.
    [[nodiscard]] T Take() {
        if constexpr (std::is_move_constructible_v<T>) {
            T value(std::move(m_Value).value());
            m_Value.reset();
            return value;
        } else {
            T value(m_Value.value());
            m_Value.reset();
            return static_cast<const T &>(value);
        }
    }
    [[nodiscard]] T *operator->() { return &Value(); }
    [[nodiscard]] const T *operator->() const { return &Value(); }
    [[nodiscard]] T &operator*() & { return Value(); }
    [[nodiscard]] const T &operator*() const & { return Value(); }

    static Result Success(T value, TStatus status = {}) {
        Result result;
        result.m_Code = BML_OK;
        result.m_Status = std::move(status);
        if constexpr (std::is_move_constructible_v<T>)
            result.m_Value.emplace(std::move(value));
        else
            result.m_Value.emplace(value);
        return result;
    }

    static Result Failure(int code, TStatus status = {}) {
        Result result;
        result.m_Code = code < BML_OK ? code : BML_ERROR_FAIL;
        result.m_Status = std::move(status);
        return result;
    }

private:
    int m_Code = BML_ERROR_FAIL;
    TStatus m_Status;
    std::optional<T> m_Value;
};

template <class TStatus>
class Result<void, TStatus> {
public:
    Result() = default;

    [[nodiscard]] explicit operator bool() const noexcept {
        return m_Code == BML_OK;
    }
    [[nodiscard]] int Code() const noexcept { return m_Code; }
    [[nodiscard]] const TStatus &GetStatus() const noexcept { return m_Status; }

    static Result Success(TStatus status = {}) {
        Result result;
        result.m_Code = BML_OK;
        result.m_Status = std::move(status);
        return result;
    }

    static Result Failure(int code, TStatus status = {}) {
        Result result;
        result.m_Code = code < BML_OK ? code : BML_ERROR_FAIL;
        result.m_Status = std::move(status);
        return result;
    }

private:
    int m_Code = BML_ERROR_FAIL;
    TStatus m_Status;
};

} // namespace BML

#endif // BML_RESULT_HPP
