/**
 * @file bml_config.hpp
 * @brief BML C++ Configuration Wrapper
 *
 * Provides type-safe, RAII-friendly access to BML configuration.
 */

#ifndef BML_CONFIG_HPP
#define BML_CONFIG_HPP

#include "bml_config.h"
#include "bml_assert.hpp"

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
     *
     * @note All string parameters must be null-terminated C strings.
     */
    class Config {
    public:
        /**
         * @brief Construct a config wrapper for a mod
         * @param mod The mod handle
         */
        explicit Config(BML_Mod mod, const BML_CoreConfigInterface *configInterface = nullptr)
            : m_Mod(mod), m_ConfigInterface(configInterface) {}

        // ========================================================================
        // String Accessors
        // ========================================================================

        /**
         * @brief Get a string configuration value
         * @param category Configuration category (null-terminated)
         * @param key Configuration key (null-terminated)
         * @return The value if found, std::nullopt otherwise
         */
        std::optional<std::string> GetString(const char *category, const char *key) const {
            BML_ConfigKey cfg_key = {sizeof(BML_ConfigKey), category, key};
            BML_ConfigValue value = {sizeof(BML_ConfigValue), BML_CONFIG_STRING, {}, 0, 0};
            auto result = m_ConfigInterface->Get(m_Mod, &cfg_key, &value);
            if (result == BML_RESULT_OK && value.type == BML_CONFIG_STRING) {
                return std::string(value.data.string_value);
            }
            return std::nullopt;
        }

        /**
         * @brief Set a string configuration value
         * @param category Configuration category (null-terminated)
         * @param key Configuration key (null-terminated)
         * @param value Value to set (null-terminated)
         * @return true if successful
         */
        bool SetString(const char *category, const char *key, const char *value) {
            BML_ConfigKey cfg_key = {sizeof(BML_ConfigKey), category, key};
            BML_ConfigValue cfg_value = {sizeof(BML_ConfigValue), BML_CONFIG_STRING, {}, 0, 0};
            cfg_value.data.string_value = value;
            return m_ConfigInterface->Set(m_Mod, &cfg_key, &cfg_value) == BML_RESULT_OK;
        }

        // ========================================================================
        // Int Accessors
        // ========================================================================

        /**
         * @brief Get an integer configuration value
         * @param category Configuration category (null-terminated)
         * @param key Configuration key (null-terminated)
         * @return The value if found, std::nullopt otherwise
         */
        std::optional<int32_t> GetInt(const char *category, const char *key) const {
            BML_ConfigKey cfg_key = {sizeof(BML_ConfigKey), category, key};
            BML_ConfigValue value = {sizeof(BML_ConfigValue), BML_CONFIG_INT, {}, 0, 0};
            auto result = m_ConfigInterface->Get(m_Mod, &cfg_key, &value);
            if (result == BML_RESULT_OK && value.type == BML_CONFIG_INT) {
                return value.data.int_value;
            }
            return std::nullopt;
        }

        /**
         * @brief Set an integer configuration value
         * @param category Configuration category (null-terminated)
         * @param key Configuration key (null-terminated)
         * @param value Value to set
         * @return true if successful
         */
        bool SetInt(const char *category, const char *key, int32_t value) {
            BML_ConfigKey cfg_key = {sizeof(BML_ConfigKey), category, key};
            BML_ConfigValue cfg_value = {sizeof(BML_ConfigValue), BML_CONFIG_INT, {}, 0, 0};
            cfg_value.data.int_value = value;
            return m_ConfigInterface->Set(m_Mod, &cfg_key, &cfg_value) == BML_RESULT_OK;
        }

        // ========================================================================
        // Float Accessors
        // ========================================================================

        /**
         * @brief Get a float configuration value
         * @param category Configuration category (null-terminated)
         * @param key Configuration key (null-terminated)
         * @return The value if found, std::nullopt otherwise
         */
        std::optional<float> GetFloat(const char *category, const char *key) const {
            BML_ConfigKey cfg_key = {sizeof(BML_ConfigKey), category, key};
            BML_ConfigValue value = {sizeof(BML_ConfigValue), BML_CONFIG_FLOAT, {}, 0, 0};
            auto result = m_ConfigInterface->Get(m_Mod, &cfg_key, &value);
            if (result == BML_RESULT_OK && value.type == BML_CONFIG_FLOAT) {
                return value.data.float_value;
            }
            return std::nullopt;
        }

        /**
         * @brief Set a float configuration value
         * @param category Configuration category (null-terminated)
         * @param key Configuration key (null-terminated)
         * @param value Value to set
         * @return true if successful
         */
        bool SetFloat(const char *category, const char *key, float value) {
            BML_ConfigKey cfg_key = {sizeof(BML_ConfigKey), category, key};
            BML_ConfigValue cfg_value = {sizeof(BML_ConfigValue), BML_CONFIG_FLOAT, {}, 0, 0};
            cfg_value.data.float_value = value;
            return m_ConfigInterface->Set(m_Mod, &cfg_key, &cfg_value) == BML_RESULT_OK;
        }

        // ========================================================================
        // Bool Accessors
        // ========================================================================

        /**
         * @brief Get a boolean configuration value
         * @param category Configuration category (null-terminated)
         * @param key Configuration key (null-terminated)
         * @return The value if found, std::nullopt otherwise
         */
        std::optional<bool> GetBool(const char *category, const char *key) const {
            BML_ConfigKey cfg_key = {sizeof(BML_ConfigKey), category, key};
            BML_ConfigValue value = {sizeof(BML_ConfigValue), BML_CONFIG_BOOL, {}, 0, 0};
            auto result = m_ConfigInterface->Get(m_Mod, &cfg_key, &value);
            if (result == BML_RESULT_OK && value.type == BML_CONFIG_BOOL) {
                return value.data.bool_value != 0;
            }
            return std::nullopt;
        }

        /**
         * @brief Set a boolean configuration value
         * @param category Configuration category (null-terminated)
         * @param key Configuration key (null-terminated)
         * @param value Value to set
         * @return true if successful
         */
        bool SetBool(const char *category, const char *key, bool value) {
            BML_ConfigKey cfg_key = {sizeof(BML_ConfigKey), category, key};
            BML_ConfigValue cfg_value = {sizeof(BML_ConfigValue), BML_CONFIG_BOOL, {}, 0, 0};
            cfg_value.data.bool_value = value ? 1 : 0;
            return m_ConfigInterface->Set(m_Mod, &cfg_key, &cfg_value) == BML_RESULT_OK;
        }

        // ========================================================================
        // Default-Value Shortcuts (v1.1 vtable)
        // ========================================================================

        /**
         * @brief Get an integer, returning default_value if not found
         */
        int32_t GetInt(const char *category, const char *key, int32_t default_value) const {
            int32_t out = default_value;
            m_ConfigInterface->GetInt(m_Mod, category, key, default_value, &out);
            return out;
        }

        /**
         * @brief Get a float, returning default_value if not found
         */
        float GetFloat(const char *category, const char *key, float default_value) const {
            float out = default_value;
            m_ConfigInterface->GetFloat(m_Mod, category, key, default_value, &out);
            return out;
        }

        /**
         * @brief Get a bool, returning default_value if not found
         */
        bool GetBool(const char *category, const char *key, bool default_value) const {
            BML_Bool out = default_value ? BML_TRUE : BML_FALSE;
            m_ConfigInterface->GetBool(m_Mod, category, key, out, &out);
            return out != 0;
        }

        /**
         * @brief Get a string, returning default_value if not found
         * @warning The returned string is a copy, unlike the C API variant
         */
        std::string GetString(const char *category, const char *key, const char *default_value) const {
            const char *out = default_value;
            m_ConfigInterface->GetString(m_Mod, category, key, default_value, &out);
            return out ? std::string(out) : std::string();
        }

        // ========================================================================
        // Operations
        // ========================================================================

        /**
         * @brief Reset a configuration value to its default
         * @param category Configuration category (null-terminated)
         * @param key Configuration key (null-terminated)
         * @return true if successful
         */
        bool Reset(const char *category, const char *key) {
            BML_ConfigKey cfg_key = {sizeof(BML_ConfigKey), category, key};
            return m_ConfigInterface->Reset(m_Mod, &cfg_key) == BML_RESULT_OK;
        }

    private:
        BML_Mod m_Mod;
        const BML_CoreConfigInterface *m_ConfigInterface;
    };

    // ============================================================================
    // ConfigBatch - RAII wrapper for atomic multi-key config updates
    // ============================================================================

    /**
     * @brief RAII wrapper for atomic configuration batch operations
     *
     * Groups multiple config changes into an atomic transaction that either
     * succeeds entirely or fails without partial changes.
     *
     * Example:
     *   {
     *       bml::ConfigBatch batch(mod);
     *       if (batch.Valid()) {
     *           batch.Set("video", "width", 1920);
     *           batch.Set("video", "height", 1080);
     *           batch.Set("video", "fullscreen", true);
     *           batch.Commit();  // All or nothing
     *       }
     *   }  // Automatically discards if not committed
     *
     * @note All string parameters must be null-terminated C strings.
     */
    class ConfigBatch {
    public:
        /**
         * @brief Begin a new configuration batch
         * @param mod The mod handle
         */
        explicit ConfigBatch(BML_Mod mod, const BML_CoreConfigInterface *configInterface = nullptr)
            : m_Batch(nullptr), m_Committed(false), m_ConfigInterface(configInterface) {
            BML_ASSERT(configInterface);
            m_ConfigInterface->BatchBegin(mod, &m_Batch);
        }

        ~ConfigBatch() {
            if (m_Batch && !m_Committed) m_ConfigInterface->BatchDiscard(m_Batch);
        }

        // Non-copyable, movable
        ConfigBatch(const ConfigBatch &) = delete;
        ConfigBatch &operator=(const ConfigBatch &) = delete;

        ConfigBatch(ConfigBatch &&other) noexcept
            : m_Batch(other.m_Batch),
              m_Committed(other.m_Committed),
              m_ConfigInterface(other.m_ConfigInterface) {
            other.m_Batch = nullptr;
            other.m_Committed = true;
            other.m_ConfigInterface = nullptr;
        }

        ConfigBatch &operator=(ConfigBatch &&other) noexcept {
            if (this != &other) {
                if (m_Batch && !m_Committed) m_ConfigInterface->BatchDiscard(m_Batch);
                m_Batch = other.m_Batch;
                m_Committed = other.m_Committed;
                m_ConfigInterface = other.m_ConfigInterface;
                other.m_Batch = nullptr;
                other.m_Committed = true;
                other.m_ConfigInterface = nullptr;
            }
            return *this;
        }

        /**
         * @brief Check if the batch was successfully created
         * @return true if batch handle is valid
         */
        [[nodiscard]] bool Valid() const { return m_Batch != nullptr; }

        /**
         * @brief Check if the batch has been committed
         * @return true if already committed
         */
        [[nodiscard]] bool IsCommitted() const { return m_Committed; }

        /**
         * @brief Queue a generic config value change
         * @param key Configuration key
         * @param value Configuration value
         * @return true if successfully queued
         */
        bool Set(const BML_ConfigKey *key, const BML_ConfigValue *value) {
            if (!m_Batch || m_Committed) return false;
            return m_ConfigInterface->BatchSet(m_Batch, key, value) == BML_RESULT_OK;
        }

        /**
         * @brief Queue an integer config value change
         */
        bool Set(const char *category, const char *key, int32_t value) {
            BML_ConfigKey cfg_key = {sizeof(BML_ConfigKey), category, key};
            BML_ConfigValue cfg_value = {sizeof(BML_ConfigValue), BML_CONFIG_INT, {}, 0, 0};
            cfg_value.data.int_value = value;
            return Set(&cfg_key, &cfg_value);
        }

        /**
         * @brief Queue a float config value change
         */
        bool Set(const char *category, const char *key, float value) {
            BML_ConfigKey cfg_key = {sizeof(BML_ConfigKey), category, key};
            BML_ConfigValue cfg_value = {sizeof(BML_ConfigValue), BML_CONFIG_FLOAT, {}, 0, 0};
            cfg_value.data.float_value = value;
            return Set(&cfg_key, &cfg_value);
        }

        /**
         * @brief Queue a boolean config value change
         */
        bool Set(const char *category, const char *key, bool value) {
            BML_ConfigKey cfg_key = {sizeof(BML_ConfigKey), category, key};
            BML_ConfigValue cfg_value = {sizeof(BML_ConfigValue), BML_CONFIG_BOOL, {}, 0, 0};
            cfg_value.data.bool_value = value ? 1 : 0;
            return Set(&cfg_key, &cfg_value);
        }

        /**
         * @brief Queue a string config value change
         */
        bool Set(const char *category, const char *key, const char *value) {
            BML_ConfigKey cfg_key = {sizeof(BML_ConfigKey), category, key};
            BML_ConfigValue cfg_value = {sizeof(BML_ConfigValue), BML_CONFIG_STRING, {}, 0, 0};
            cfg_value.data.string_value = value;
            return Set(&cfg_key, &cfg_value);
        }

        /**
         * @brief Atomically commit all queued changes
         * @return true if all changes were successfully applied
         */
        bool Commit() {
            if (!m_Batch || m_Committed) return false;
            BML_Result result = m_ConfigInterface->BatchCommit(m_Batch);
            if (result == BML_RESULT_OK) {
                m_Committed = true;
                m_Batch = nullptr; // Handle consumed by commit
                return true;
            }
            return false;
        }

        /**
         * @brief Discard all queued changes without applying
         * @return true if successfully discarded
         */
        bool Discard() {
            if (!m_Batch || m_Committed) return false;
            BML_Result result = m_ConfigInterface->BatchDiscard(m_Batch);
            if (result == BML_RESULT_OK) {
                m_Committed = true; // Mark as consumed
                m_Batch = nullptr;
                return true;
            }
            return false;
        }

        /**
         * @brief Get the underlying handle (for advanced usage)
         */
        [[nodiscard]] BML_ConfigBatch Handle() const { return m_Batch; }

    private:
        BML_ConfigBatch m_Batch;
        bool m_Committed;
        const BML_CoreConfigInterface *m_ConfigInterface;
    };

} // namespace bml

#endif /* BML_CONFIG_HPP */
