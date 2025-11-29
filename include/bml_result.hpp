/**
 * @file bml_result.hpp
 * @brief BML C++ Result Type and Error Handling
 * 
 * Provides monadic Result type for safer error handling in C++.
 */

#ifndef BML_RESULT_HPP
#define BML_RESULT_HPP

#include "bml_errors.h"

#include <optional>
#include <utility>

namespace bml {
    // ============================================================================
    // Result Type
    // ============================================================================

    /**
     * @brief Result type for functions that may fail
     *
     * Similar to Rust's Result<T, E> or C++23's std::expected.
     *
     * Example:
     *   bml::Result<std::string> GetValue() {
     *       if (error) return BML_RESULT_NOT_FOUND;
     *       return std::string("value");
     *   }
     *
     *   // Usage 1: value_or
     *   auto val = GetValue().ValueOr("default");
     *
     *   // Usage 2: check and throw
     *   auto val = GetValue().Value(); // throws on error
     *
     *   // Usage 3: explicit check
     *   auto result = GetValue();
     *   if (result) {
     *       use(result.Value());
     *   } else {
     *       handle_error(result.Error());
     *   }
     */
    template <typename T>
    class Result {
    public:
        /**
         * @brief Construct a successful result
         * @param value The value
         */
        Result(T value) : m_Value(std::move(value)), m_Error(BML_RESULT_OK) {}

        /**
         * @brief Construct a failed result
         * @param error The error code
         */
        Result(BML_Result error) : m_Error(error) {}

        /**
         * @brief Check if result is successful
         */
        bool Ok() const noexcept { return m_Error == BML_RESULT_OK; }

        /**
         * @brief Check if result is successful (bool conversion)
         */
        explicit operator bool() const noexcept { return Ok(); }

        // ========================================================================
        // Value Access
        // ========================================================================

        /**
         * @brief Get the value (lvalue reference)
         * @throws bml::Exception if result is an error
         */
        T &Value() & {
            if (!Ok()) throw Exception(m_Error);
            return *m_Value;
        }

        /**
         * @brief Get the value (const lvalue reference)
         * @throws bml::Exception if result is an error
         */
        const T &Value() const & {
            if (!Ok()) throw Exception(m_Error);
            return *m_Value;
        }

        /**
         * @brief Get the value (rvalue reference)
         * @throws bml::Exception if result is an error
         */
        T &&Value() && {
            if (!Ok()) throw Exception(m_Error);
            return std::move(*m_Value);
        }

        /**
         * @brief Get the value or a default
         * @param default_value Value to return if result is an error
         * @return The value or default
         */
        T ValueOr(T default_value) const & {
            return Ok() ? *m_Value : std::move(default_value);
        }

        /**
         * @brief Get the value or a default (move semantics)
         * @param default_value Value to return if result is an error
         * @return The value or default
         */
        T ValueOr(T default_value) && {
            return Ok() ? std::move(*m_Value) : std::move(default_value);
        }

        // ========================================================================
        // Error Access
        // ========================================================================

        /**
         * @brief Get the error code
         */
        BML_Result Error() const noexcept { return m_Error; }

        /**
         * @brief Get the error message
         */
        const char *ErrorMessage() const noexcept {
            return bmlGetErrorString ? bmlGetErrorString(m_Error) : "Unknown error";
        }

        // ========================================================================
        // Monadic Operations
        // ========================================================================

        /**
         * @brief Transform the value if successful
         * @tparam F Callable: U(T)
         * @param f Transformation function
         * @return Result<U>
         */
        template <typename F>
        auto map(F &&f) const & -> Result<decltype(f(std::declval<const T &>()))> {
            using U = decltype(f(std::declval<const T &>()));
            if (Ok()) {
                return Result<U>(f(*m_Value));
            }
            return Result<U>(m_Error);
        }

        /**
         * @brief Transform the value if successful (move)
         * @tparam F Callable: U(T)
         * @param f Transformation function
         * @return Result<U>
         */
        template <typename F>
        auto map(F &&f) && -> Result<decltype(f(std::declval<T>()))> {
            using U = decltype(f(std::declval<T>()));
            if (Ok()) {
                return Result<U>(f(std::move(*m_Value)));
            }
            return Result<U>(m_Error);
        }

        /**
         * @brief Chain another operation that may fail
         * @tparam F Callable: Result<U>(T)
         * @param f Chained operation
         * @return Result<U>
         */
        template <typename F>
        auto and_then(F &&f) const & -> decltype(f(std::declval<const T &>())) {
            if (Ok()) {
                return f(*m_Value);
            }
            using ResultType = decltype(f(std::declval<const T &>()));
            return ResultType(m_Error);
        }

        /**
         * @brief Chain another operation that may fail (move)
         * @tparam F Callable: Result<U>(T)
         * @param f Chained operation
         * @return Result<U>
         */
        template <typename F>
        auto and_then(F &&f) && -> decltype(f(std::declval<T>())) {
            if (Ok()) {
                return f(std::move(*m_Value));
            }
            using ResultType = decltype(f(std::declval<T>()));
            return ResultType(m_Error);
        }

        /**
         * @brief Transform the error if failed
         * @tparam F Callable: T(BML_Result)
         * @param f Recovery function
         * @return Result<T>
         */
        template <typename F>
        Result<T> or_else(F &&f) const & {
            if (Ok()) {
                return *this;
            }
            return Result<T>(f(m_Error));
        }

    private:
        std::optional<T> m_Value;
        BML_Result m_Error;
    };

    /**
     * @brief Specialization for void (just success/failure)
     */
    template <>
    class Result<void> {
    public:
        Result() : m_Error(BML_RESULT_OK) {}

        Result(BML_Result error) : m_Error(error) {}

        bool Ok() const noexcept { return m_Error == BML_RESULT_OK; }
        explicit operator bool() const noexcept { return Ok(); }

        BML_Result Error() const noexcept { return m_Error; }

        const char *ErrorMessage() const noexcept {
            return bmlGetErrorString ? bmlGetErrorString(m_Error) : "Unknown error";
        }

        void Value() const {
            if (!Ok()) throw Exception(m_Error);
        }

    private:
        BML_Result m_Error;
    };

    // ============================================================================
    // Helper Functions
    // ============================================================================

    /**
     * @brief Create a successful result
     * @param value The value
     * @return Result<T>
     */
    template <typename T>
    inline Result<T> Ok(T value) {
        return Result<T>(std::move(value));
    }

    /**
     * @brief Create a successful void result
     * @return Result<void>
     */
    inline Result<void> Ok() {
        return Result<void>();
    }

    /**
     * @brief Create a failed result
     * @param error The error code
     * @return Result<T>
     */
    template <typename T>
    inline Result<T> Err(BML_Result error) {
        return Result<T>(error);
    }

    /**
     * @brief Create a failed void result
     * @param error The error code
     * @return Result<void>
     */
    inline Result<void> Err(BML_Result error) {
        return Result<void>(error);
    }

    // ============================================================================
    // Utility Macros
    // ============================================================================

    /**
     * @brief Try an operation, return error on failure
     *
     * Usage:
     *   Result<int> DoSomething() {
     *       BML_TRY(SomeOperation());
     *       return 42;
     *   }
     */
#define BML_TRY(expr) do { \
    auto _result = (expr); \
    if (!_result.Ok()) return _result.Error(); \
} while(0)

    /**
     * @brief Try an operation, assign value on success
     *
     * Usage:
     *   Result<int> DoSomething() {
     *       int value;
     *       BML_TRY_ASSIGN(value, GetValue());
     *       return value * 2;
     *   }
     */
#define BML_TRY_ASSIGN(var, expr) do { \
    auto _result = (expr); \
    if (!_result.Ok()) return _result.Error(); \
    var = std::move(_result).Value(); \
} while(0)
} // namespace bml

#endif /* BML_RESULT_HPP */

