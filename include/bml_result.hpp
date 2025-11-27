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

// Note: Exception class is defined in bml_errors.h

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
 *   auto val = GetValue().value_or("default");
 *   
 *   // Usage 2: check and throw
 *   auto val = GetValue().value(); // throws on error
 *   
 *   // Usage 3: explicit check
 *   auto result = GetValue();
 *   if (result) {
 *       use(result.value());
 *   } else {
 *       handle_error(result.error());
 *   }
 */
template<typename T>
class Result {
public:
    /**
     * @brief Construct a successful result
     * @param value The value
     */
    Result(T value) : m_value(std::move(value)), m_error(BML_RESULT_OK) {}
    
    /**
     * @brief Construct a failed result
     * @param error The error code
     */
    Result(BML_Result error) : m_error(error) {}
    
    /**
     * @brief Check if result is successful
     */
    bool ok() const noexcept { return m_error == BML_RESULT_OK; }
    
    /**
     * @brief Check if result is successful (bool conversion)
     */
    explicit operator bool() const noexcept { return ok(); }
    
    // ========================================================================
    // Value Access
    // ========================================================================
    
    /**
     * @brief Get the value (lvalue reference)
     * @throws bml::Exception if result is an error
     */
    T& value() & { 
        if (!ok()) throw Exception(m_error);
        return *m_value;
    }
    
    /**
     * @brief Get the value (const lvalue reference)
     * @throws bml::Exception if result is an error
     */
    const T& value() const& {
        if (!ok()) throw Exception(m_error);
        return *m_value;
    }
    
    /**
     * @brief Get the value (rvalue reference)
     * @throws bml::Exception if result is an error
     */
    T&& value() && {
        if (!ok()) throw Exception(m_error);
        return std::move(*m_value);
    }
    
    /**
     * @brief Get the value or a default
     * @param default_value Value to return if result is an error
     * @return The value or default
     */
    T value_or(T default_value) const& {
        return ok() ? *m_value : std::move(default_value);
    }
    
    /**
     * @brief Get the value or a default (move semantics)
     * @param default_value Value to return if result is an error
     * @return The value or default
     */
    T value_or(T default_value) && {
        return ok() ? std::move(*m_value) : std::move(default_value);
    }
    
    // ========================================================================
    // Error Access
    // ========================================================================
    
    /**
     * @brief Get the error code
     */
    BML_Result error() const noexcept { return m_error; }
    
    /**
     * @brief Get the error message
     */
    const char* error_message() const noexcept {
        return bmlGetErrorString ? bmlGetErrorString(m_error) : "Unknown error";
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
    template<typename F>
    auto map(F&& f) const& -> Result<decltype(f(std::declval<const T&>()))> {
        using U = decltype(f(std::declval<const T&>()));
        if (ok()) {
            return Result<U>(f(*m_value));
        }
        return Result<U>(m_error);
    }
    
    /**
     * @brief Transform the value if successful (move)
     * @tparam F Callable: U(T)
     * @param f Transformation function
     * @return Result<U>
     */
    template<typename F>
    auto map(F&& f) && -> Result<decltype(f(std::declval<T>()))> {
        using U = decltype(f(std::declval<T>()));
        if (ok()) {
            return Result<U>(f(std::move(*m_value)));
        }
        return Result<U>(m_error);
    }
    
    /**
     * @brief Chain another operation that may fail
     * @tparam F Callable: Result<U>(T)
     * @param f Chained operation
     * @return Result<U>
     */
    template<typename F>
    auto and_then(F&& f) const& -> decltype(f(std::declval<const T&>())) {
        if (ok()) {
            return f(*m_value);
        }
        using ResultType = decltype(f(std::declval<const T&>()));
        return ResultType(m_error);
    }
    
    /**
     * @brief Chain another operation that may fail (move)
     * @tparam F Callable: Result<U>(T)
     * @param f Chained operation
     * @return Result<U>
     */
    template<typename F>
    auto and_then(F&& f) && -> decltype(f(std::declval<T>())) {
        if (ok()) {
            return f(std::move(*m_value));
        }
        using ResultType = decltype(f(std::declval<T>()));
        return ResultType(m_error);
    }
    
    /**
     * @brief Transform the error if failed
     * @tparam F Callable: T(BML_Result)
     * @param f Recovery function
     * @return Result<T>
     */
    template<typename F>
    Result<T> or_else(F&& f) const& {
        if (ok()) {
            return *this;
        }
        return Result<T>(f(m_error));
    }
    
private:
    std::optional<T> m_value;
    BML_Result m_error;
};

/**
 * @brief Specialization for void (just success/failure)
 */
template<>
class Result<void> {
public:
    Result() : m_error(BML_RESULT_OK) {}
    Result(BML_Result error) : m_error(error) {}
    
    bool ok() const noexcept { return m_error == BML_RESULT_OK; }
    explicit operator bool() const noexcept { return ok(); }
    
    BML_Result error() const noexcept { return m_error; }
    
    const char* error_message() const noexcept {
        return bmlGetErrorString ? bmlGetErrorString(m_error) : "Unknown error";
    }
    
    void value() const {
        if (!ok()) throw Exception(m_error);
    }
    
private:
    BML_Result m_error;
};

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Create a successful result
 * @param value The value
 * @return Result<T>
 */
template<typename T>
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
template<typename T>
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
    if (!_result.ok()) return _result.error(); \
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
    if (!_result.ok()) return _result.error(); \
    var = std::move(_result).value(); \
} while(0)

} // namespace bml

#endif /* BML_RESULT_HPP */
