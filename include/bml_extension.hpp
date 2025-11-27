/**
 * @file bml_extension.hpp
 * @brief BML C++ Extension Wrapper
 * 
 * Provides type-safe access to BML extension registration and loading.
 */

#ifndef BML_EXTENSION_HPP
#define BML_EXTENSION_HPP

#include "bml_loader.h"

#include <string_view>
#include <optional>

namespace bml {

// ============================================================================
// Extension Wrapper
// ============================================================================

/**
 * @brief Wrapper for BML extension management
 * 
 * Example:
 *   bml::Extension ext;
 *   
 *   // Register an extension
 *   ext.Register("my_extension", 1, 0, &api_table, sizeof(api_table));
 *   
 *   // Load an extension
 *   if (auto api = ext.Load<MyExtensionAPI>("other_extension")) {
 *       api->SomeFunction();
 *   }
 */
class Extension {
public:
    Extension() = default;
    
    // ========================================================================
    // Registration
    // ========================================================================
    
    /**
     * @brief Register an extension API
     * @param name Extension name
     * @param major Major version
     * @param minor Minor version
     * @param api_table Pointer to API table
     * @param api_size Size of API table
     * @return true if successful
     */
    bool Register(std::string_view name, uint32_t major, uint32_t minor, void* api_table, size_t api_size) {
        if (!bmlExtensionRegister) return false;
        return bmlExtensionRegister(name.data(), major, minor, api_table, api_size) == BML_RESULT_OK;
    }
    
    // ========================================================================
    // Query
    // ========================================================================
    
    /**
     * @brief Query extension information
     * @param name Extension name
     * @return Extension info if found and supported
     */
    std::optional<BML_ExtensionInfo> Query(std::string_view name) const {
        if (!bmlExtensionQuery) return std::nullopt;
        BML_ExtensionInfo info;
        BML_Bool supported;
        auto result = bmlExtensionQuery(name.data(), &info, &supported);
        if (result == BML_RESULT_OK && supported) {
            return info;
        }
        return std::nullopt;
    }
    
    /**
     * @brief Check if an extension is available
     * @param name Extension name
     * @return true if extension is available
     */
    bool IsAvailable(std::string_view name) const {
        return Query(name).has_value();
    }
    
    // ========================================================================
    // Loading
    // ========================================================================
    
    /**
     * @brief Load an extension API (untyped)
     * @param name Extension name
     * @return Pointer to API table if successful
     */
    std::optional<void*> Load(std::string_view name) const {
        if (!bmlExtensionLoad) return std::nullopt;
        void* api_table = nullptr;
        auto result = bmlExtensionLoad(name.data(), &api_table);
        if (result == BML_RESULT_OK && api_table) {
            return api_table;
        }
        return std::nullopt;
    }
    
    /**
     * @brief Load an extension API (typed)
     * @tparam T API table type
     * @param name Extension name
     * @return Pointer to typed API table if successful
     */
    template<typename T>
    std::optional<T*> Load(std::string_view name) const {
        auto ptr = Load(name);
        if (ptr) {
            return static_cast<T*>(*ptr);
        }
        return std::nullopt;
    }
    
    /**
     * @brief Load an extension API with version requirements
     * @param name Extension name
     * @param req_major Required major version
     * @param req_minor Required minor version
     * @param out_major Output: actual major version (optional)
     * @param out_minor Output: actual minor version (optional)
     * @return Pointer to API table if successful
     */
    std::optional<void*> LoadVersioned(std::string_view name, uint32_t req_major, uint32_t req_minor,
                                       uint32_t* out_major = nullptr, uint32_t* out_minor = nullptr) const {
        if (!bmlExtensionLoadVersioned) return std::nullopt;
        void* api_table = nullptr;
        auto result = bmlExtensionLoadVersioned(name.data(), req_major, req_minor, &api_table, out_major, out_minor);
        if (result == BML_RESULT_OK && api_table) {
            return api_table;
        }
        return std::nullopt;
    }
    
    /**
     * @brief Load an extension API with version requirements (typed)
     * @tparam T API table type
     * @param name Extension name
     * @param req_major Required major version
     * @param req_minor Required minor version
     * @param out_major Output: actual major version (optional)
     * @param out_minor Output: actual minor version (optional)
     * @return Pointer to typed API table if successful
     */
    template<typename T>
    std::optional<T*> LoadVersioned(std::string_view name, uint32_t req_major, uint32_t req_minor,
                                    uint32_t* out_major = nullptr, uint32_t* out_minor = nullptr) const {
        auto ptr = LoadVersioned(name, req_major, req_minor, out_major, out_minor);
        if (ptr) {
            return static_cast<T*>(*ptr);
        }
        return std::nullopt;
    }
};

} // namespace bml

#endif /* BML_EXTENSION_HPP */
