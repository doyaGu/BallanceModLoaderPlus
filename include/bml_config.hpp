/**
 * @file bml_config.hpp
 * @brief BML C++ Configuration Wrapper
 * 
 * Provides type-safe, RAII-friendly access to BML configuration.
 */

#ifndef BML_CONFIG_HPP
#define BML_CONFIG_HPP

#include "bml_loader.h"

#include <string>
#include <string_view>
#include <optional>

namespace bml {

// ============================================================================
// Config Wrapper (RAII, typed accessors)
// ============================================================================

/**
 * @brief Type-safe wrapper for BML configuration access
 * 
 * Example:
 *   bml::Config config(mod);
 *   config.SetString("mymod", "key", "value");
 *   auto value = config.GetString("mymod", "key").value_or("default");
 */
class Config {
public:
    /**
     * @brief Construct a config wrapper for a mod
     * @param mod The mod handle
     */
    explicit Config(BML_Mod mod) : m_mod(mod) {}
    
    // ========================================================================
    // String Accessors
    // ========================================================================
    
    /**
     * @brief Get a string configuration value
     * @param category Configuration category
     * @param key Configuration key
     * @return The value if found, std::nullopt otherwise
     */
    std::optional<std::string> GetString(std::string_view category, std::string_view key) const {
        if (!bmlConfigGet) return std::nullopt;
        BML_ConfigKey cfg_key = { sizeof(BML_ConfigKey), category.data(), key.data() };
        BML_ConfigValue value = { sizeof(BML_ConfigValue), BML_CONFIG_STRING, {} };
        auto result = bmlConfigGet(m_mod, &cfg_key, &value);
        if (result == BML_RESULT_OK && value.type == BML_CONFIG_STRING) {
            return std::string(value.data.string_value);
        }
        return std::nullopt;
    }
    
    /**
     * @brief Set a string configuration value
     * @param category Configuration category
     * @param key Configuration key
     * @param value Value to set
     * @return true if successful
     */
    bool SetString(std::string_view category, std::string_view key, std::string_view value) {
        if (!bmlConfigSet) return false;
        BML_ConfigKey cfg_key = { sizeof(BML_ConfigKey), category.data(), key.data() };
        BML_ConfigValue cfg_value = { sizeof(BML_ConfigValue), BML_CONFIG_STRING, {} };
        cfg_value.data.string_value = value.data();
        return bmlConfigSet(m_mod, &cfg_key, &cfg_value) == BML_RESULT_OK;
    }
    
    // ========================================================================
    // Int Accessors
    // ========================================================================
    
    /**
     * @brief Get an integer configuration value
     * @param category Configuration category
     * @param key Configuration key
     * @return The value if found, std::nullopt otherwise
     */
    std::optional<int32_t> GetInt(std::string_view category, std::string_view key) const {
        if (!bmlConfigGet) return std::nullopt;
        BML_ConfigKey cfg_key = { sizeof(BML_ConfigKey), category.data(), key.data() };
        BML_ConfigValue value = { sizeof(BML_ConfigValue), BML_CONFIG_INT, {} };
        auto result = bmlConfigGet(m_mod, &cfg_key, &value);
        if (result == BML_RESULT_OK && value.type == BML_CONFIG_INT) {
            return value.data.int_value;
        }
        return std::nullopt;
    }
    
    /**
     * @brief Set an integer configuration value
     * @param category Configuration category
     * @param key Configuration key
     * @param value Value to set
     * @return true if successful
     */
    bool SetInt(std::string_view category, std::string_view key, int32_t value) {
        if (!bmlConfigSet) return false;
        BML_ConfigKey cfg_key = { sizeof(BML_ConfigKey), category.data(), key.data() };
        BML_ConfigValue cfg_value = { sizeof(BML_ConfigValue), BML_CONFIG_INT, {} };
        cfg_value.data.int_value = value;
        return bmlConfigSet(m_mod, &cfg_key, &cfg_value) == BML_RESULT_OK;
    }
    
    // ========================================================================
    // Float Accessors
    // ========================================================================
    
    /**
     * @brief Get a float configuration value
     * @param category Configuration category
     * @param key Configuration key
     * @return The value if found, std::nullopt otherwise
     */
    std::optional<float> GetFloat(std::string_view category, std::string_view key) const {
        if (!bmlConfigGet) return std::nullopt;
        BML_ConfigKey cfg_key = { sizeof(BML_ConfigKey), category.data(), key.data() };
        BML_ConfigValue value = { sizeof(BML_ConfigValue), BML_CONFIG_FLOAT, {} };
        auto result = bmlConfigGet(m_mod, &cfg_key, &value);
        if (result == BML_RESULT_OK && value.type == BML_CONFIG_FLOAT) {
            return value.data.float_value;
        }
        return std::nullopt;
    }
    
    /**
     * @brief Set a float configuration value
     * @param category Configuration category
     * @param key Configuration key
     * @param value Value to set
     * @return true if successful
     */
    bool SetFloat(std::string_view category, std::string_view key, float value) {
        if (!bmlConfigSet) return false;
        BML_ConfigKey cfg_key = { sizeof(BML_ConfigKey), category.data(), key.data() };
        BML_ConfigValue cfg_value = { sizeof(BML_ConfigValue), BML_CONFIG_FLOAT, {} };
        cfg_value.data.float_value = value;
        return bmlConfigSet(m_mod, &cfg_key, &cfg_value) == BML_RESULT_OK;
    }
    
    // ========================================================================
    // Bool Accessors
    // ========================================================================
    
    /**
     * @brief Get a boolean configuration value
     * @param category Configuration category
     * @param key Configuration key
     * @return The value if found, std::nullopt otherwise
     */
    std::optional<bool> GetBool(std::string_view category, std::string_view key) const {
        if (!bmlConfigGet) return std::nullopt;
        BML_ConfigKey cfg_key = { sizeof(BML_ConfigKey), category.data(), key.data() };
        BML_ConfigValue value = { sizeof(BML_ConfigValue), BML_CONFIG_BOOL, {} };
        auto result = bmlConfigGet(m_mod, &cfg_key, &value);
        if (result == BML_RESULT_OK && value.type == BML_CONFIG_BOOL) {
            return value.data.bool_value != 0;
        }
        return std::nullopt;
    }
    
    /**
     * @brief Set a boolean configuration value
     * @param category Configuration category
     * @param key Configuration key
     * @param value Value to set
     * @return true if successful
     */
    bool SetBool(std::string_view category, std::string_view key, bool value) {
        if (!bmlConfigSet) return false;
        BML_ConfigKey cfg_key = { sizeof(BML_ConfigKey), category.data(), key.data() };
        BML_ConfigValue cfg_value = { sizeof(BML_ConfigValue), BML_CONFIG_BOOL, {} };
        cfg_value.data.bool_value = value ? 1 : 0;
        return bmlConfigSet(m_mod, &cfg_key, &cfg_value) == BML_RESULT_OK;
    }
    
    // ========================================================================
    // Operations
    // ========================================================================
    
    /**
     * @brief Reset a configuration value to its default
     * @param category Configuration category
     * @param key Configuration key
     * @return true if successful
     */
    bool Reset(std::string_view category, std::string_view key) {
        if (!bmlConfigReset) return false;
        BML_ConfigKey cfg_key = { sizeof(BML_ConfigKey), category.data(), key.data() };
        return bmlConfigReset(m_mod, &cfg_key) == BML_RESULT_OK;
    }
    
private:
    BML_Mod m_mod;
};

} // namespace bml

#endif /* BML_CONFIG_HPP */
