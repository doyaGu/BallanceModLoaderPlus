/**
 * @file bml_config.hpp
 * @brief BML C++ Configuration Wrapper
 * 
 * Provides type-safe, RAII-friendly access to BML configuration.
 */

#ifndef BML_CONFIG_HPP
#define BML_CONFIG_HPP

#include "bml_config.h"

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
        explicit Config(BML_Mod mod) : m_Mod(mod) {}

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
            BML_ConfigKey cfg_key = {sizeof(BML_ConfigKey), category.data(), key.data()};
            BML_ConfigValue value = {sizeof(BML_ConfigValue), BML_CONFIG_STRING, {}};
            auto result = bmlConfigGet(m_Mod, &cfg_key, &value);
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
            BML_ConfigKey cfg_key = {sizeof(BML_ConfigKey), category.data(), key.data()};
            BML_ConfigValue cfg_value = {sizeof(BML_ConfigValue), BML_CONFIG_STRING, {}};
            cfg_value.data.string_value = value.data();
            return bmlConfigSet(m_Mod, &cfg_key, &cfg_value) == BML_RESULT_OK;
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
            BML_ConfigKey cfg_key = {sizeof(BML_ConfigKey), category.data(), key.data()};
            BML_ConfigValue value = {sizeof(BML_ConfigValue), BML_CONFIG_INT, {}};
            auto result = bmlConfigGet(m_Mod, &cfg_key, &value);
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
            BML_ConfigKey cfg_key = {sizeof(BML_ConfigKey), category.data(), key.data()};
            BML_ConfigValue cfg_value = {sizeof(BML_ConfigValue), BML_CONFIG_INT, {}};
            cfg_value.data.int_value = value;
            return bmlConfigSet(m_Mod, &cfg_key, &cfg_value) == BML_RESULT_OK;
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
            BML_ConfigKey cfg_key = {sizeof(BML_ConfigKey), category.data(), key.data()};
            BML_ConfigValue value = {sizeof(BML_ConfigValue), BML_CONFIG_FLOAT, {}};
            auto result = bmlConfigGet(m_Mod, &cfg_key, &value);
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
            BML_ConfigKey cfg_key = {sizeof(BML_ConfigKey), category.data(), key.data()};
            BML_ConfigValue cfg_value = {sizeof(BML_ConfigValue), BML_CONFIG_FLOAT, {}};
            cfg_value.data.float_value = value;
            return bmlConfigSet(m_Mod, &cfg_key, &cfg_value) == BML_RESULT_OK;
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
            BML_ConfigKey cfg_key = {sizeof(BML_ConfigKey), category.data(), key.data()};
            BML_ConfigValue value = {sizeof(BML_ConfigValue), BML_CONFIG_BOOL, {}};
            auto result = bmlConfigGet(m_Mod, &cfg_key, &value);
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
            BML_ConfigKey cfg_key = {sizeof(BML_ConfigKey), category.data(), key.data()};
            BML_ConfigValue cfg_value = {sizeof(BML_ConfigValue), BML_CONFIG_BOOL, {}};
            cfg_value.data.bool_value = value ? 1 : 0;
            return bmlConfigSet(m_Mod, &cfg_key, &cfg_value) == BML_RESULT_OK;
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
            BML_ConfigKey cfg_key = {sizeof(BML_ConfigKey), category.data(), key.data()};
            return bmlConfigReset(m_Mod, &cfg_key) == BML_RESULT_OK;
        }

    private:
        BML_Mod m_Mod;
    };

    // ============================================================================
    // Config Capability Queries
    // ============================================================================

    /**
     * @brief Get configuration store capabilities
     * @return Optional containing capabilities if successful
     */
    inline std::optional<BML_ConfigStoreCaps> GetConfigCaps() {
        if (!bmlConfigGetCaps) return std::nullopt;
        BML_ConfigStoreCaps caps = BML_CONFIG_STORE_CAPS_INIT;
        if (bmlConfigGetCaps(&caps) == BML_RESULT_OK) {
            return caps;
        }
        return std::nullopt;
    }

    /**
     * @brief Check if a specific config capability is available
     * @param flag The capability flag to check (BML_ConfigCapabilityFlags)
     * @return true if the capability is available
     */
    inline bool HasConfigCap(BML_ConfigCapabilityFlags flag) {
        auto caps = GetConfigCaps();
        return caps && (caps->feature_flags & flag);
    }

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
     */
    class ConfigBatch {
    public:
        /**
         * @brief Begin a new configuration batch
         * @param mod The mod handle
         */
        explicit ConfigBatch(BML_Mod mod) : m_Batch(nullptr), m_Committed(false) {
            if (bmlConfigBatchBegin) {
                bmlConfigBatchBegin(mod, &m_Batch);
            }
        }

        ~ConfigBatch() {
            if (m_Batch && !m_Committed && bmlConfigBatchDiscard) {
                bmlConfigBatchDiscard(m_Batch);
            }
        }

        // Non-copyable, movable
        ConfigBatch(const ConfigBatch &) = delete;
        ConfigBatch &operator=(const ConfigBatch &) = delete;

        ConfigBatch(ConfigBatch &&other) noexcept
            : m_Batch(other.m_Batch), m_Committed(other.m_Committed) {
            other.m_Batch = nullptr;
            other.m_Committed = true;
        }

        ConfigBatch &operator=(ConfigBatch &&other) noexcept {
            if (this != &other) {
                if (m_Batch && !m_Committed && bmlConfigBatchDiscard) {
                    bmlConfigBatchDiscard(m_Batch);
                }
                m_Batch = other.m_Batch;
                m_Committed = other.m_Committed;
                other.m_Batch = nullptr;
                other.m_Committed = true;
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
            if (!m_Batch || m_Committed || !bmlConfigBatchSet) return false;
            return bmlConfigBatchSet(m_Batch, key, value) == BML_RESULT_OK;
        }

        /**
         * @brief Queue an integer config value change
         */
        bool Set(std::string_view category, std::string_view key, int32_t value) {
            BML_ConfigKey cfg_key = {sizeof(BML_ConfigKey), category.data(), key.data()};
            BML_ConfigValue cfg_value = {sizeof(BML_ConfigValue), BML_CONFIG_INT, {}};
            cfg_value.data.int_value = value;
            return Set(&cfg_key, &cfg_value);
        }

        /**
         * @brief Queue a float config value change
         */
        bool Set(std::string_view category, std::string_view key, float value) {
            BML_ConfigKey cfg_key = {sizeof(BML_ConfigKey), category.data(), key.data()};
            BML_ConfigValue cfg_value = {sizeof(BML_ConfigValue), BML_CONFIG_FLOAT, {}};
            cfg_value.data.float_value = value;
            return Set(&cfg_key, &cfg_value);
        }

        /**
         * @brief Queue a boolean config value change
         */
        bool Set(std::string_view category, std::string_view key, bool value) {
            BML_ConfigKey cfg_key = {sizeof(BML_ConfigKey), category.data(), key.data()};
            BML_ConfigValue cfg_value = {sizeof(BML_ConfigValue), BML_CONFIG_BOOL, {}};
            cfg_value.data.bool_value = value ? 1 : 0;
            return Set(&cfg_key, &cfg_value);
        }

        /**
         * @brief Queue a string config value change
         */
        bool Set(std::string_view category, std::string_view key, std::string_view value) {
            BML_ConfigKey cfg_key = {sizeof(BML_ConfigKey), category.data(), key.data()};
            BML_ConfigValue cfg_value = {sizeof(BML_ConfigValue), BML_CONFIG_STRING, {}};
            cfg_value.data.string_value = value.data();
            return Set(&cfg_key, &cfg_value);
        }

        /**
         * @brief Atomically commit all queued changes
         * @return true if all changes were successfully applied
         */
        bool Commit() {
            if (!m_Batch || m_Committed || !bmlConfigBatchCommit) return false;
            BML_Result result = bmlConfigBatchCommit(m_Batch);
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
            if (!m_Batch || m_Committed || !bmlConfigBatchDiscard) return false;
            BML_Result result = bmlConfigBatchDiscard(m_Batch);
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
    };

    /**
     * @brief Check if batch config operations are supported
     * @return true if batch operations are available
     */
    inline bool HasConfigBatch() {
        return HasConfigCap(BML_CONFIG_CAP_BATCH);
    }
} // namespace bml

#endif /* BML_CONFIG_HPP */
