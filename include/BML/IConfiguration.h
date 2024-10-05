/**
 * @file IConfiguration.h
 * @brief The interface of configuration.
 */
#ifndef BML_ICONFIGURATION_H
#define BML_ICONFIGURATION_H

#include <cstdint>

namespace BML {
    inline namespace v1 {
        class IConfigurationSection;
        class IConfigurationEntry;

        /**
         * @interface IConfiguration
         * @brief The utility interface of configuration.
         * @warning Mod should have no classes implementing this interface.
         */
        class IConfiguration {
        public:
            /**
             * @brief Increase the reference count of the configuration object.
             * @return The new reference count.
             */
            virtual int AddRef() const = 0;

            /**
             * @brief Decrease the reference count of the configuration object.
             * @return The new reference count.
             */
            virtual int Release() const = 0;

            /**
             * @brief Get the name of the configuration.
             * @return The name as a null-terminated string.
             */
            virtual const char *GetName() const = 0;

            /**
             * @brief Clear all entries and sections.
             */
            virtual void Clear() = 0;

            /**
             * @brief Get the number of entries in the configuration.
             * @return The number of entries.
             */
            virtual size_t GetNumberOfEntries() const = 0;

            /**
             * @brief Get the number of sections in the configuration.
             * @return The number of sections.
             */
            virtual size_t GetNumberOfSections() const = 0;

            /**
             * @brief Get the total number of entries, including nested sections.
             * @return The number of entries.
             */
            virtual size_t GetNumberOfEntriesRecursive() const = 0;

            /**
             * @brief Get the total number of sections, including nested sections.
             * @return The number of sections.
             */
            virtual size_t GetNumberOfSectionsRecursive() const = 0;

            /**
             * @brief Retrieves the configuration entry at the specified index.
             * @param index The index of the entry to retrieve.
             * @return The configuration entry if found, nullptr otherwise.
             */
            virtual IConfigurationEntry *GetEntry(size_t index) const = 0;

            /**
             * @brief Retrieves the configuration section at the specified index.
             * @param index The index of the section to retrieve.
             * @return The configuration section if found, nullptr otherwise.
             */
            virtual IConfigurationSection *GetSection(size_t index) const = 0;

            /**
             * @brief Retrieves the configuration entry with the specified name.
             * @param name The name of the entry to retrieve.
             * @return The configuration entry if found, nullptr otherwise.
             */
            virtual IConfigurationEntry *GetEntry(const char *name) = 0;

            /**
             * @brief Retrieves the configuration section with the specified name.
             *
             * @param name The name of the section to retrieve.
             * @return The configuration section if found, nullptr otherwise.
             */
            virtual IConfigurationSection *GetSection(const char *name) = 0;

            /**
             * @brief Add an entry to the configuration.
             * @param parent The parent section name or nullptr for the root section.
             * @param name The name of the entry.
             * @return The configuration entry.
             */
            virtual IConfigurationEntry *AddEntry(const char *parent, const char *name) = 0;

            /**
             * @brief Add a boolean entry to the configuration.
             * @param parent The parent section name or nullptr for the root section.
             * @param name The name of the entry.
             * @param value The default value for the entry.
             * @return The configuration entry.
             */
            virtual IConfigurationEntry *AddEntryBool(const char *parent, const char *name, bool value) = 0;

            /**
             * @brief Add an unsigned integer entry to the configuration.
             * @param parent The parent section name or nullptr for the root section.
             * @param name The name of the entry.
             * @param value The default value for the entry.
             * @return The configuration entry.
             */
            virtual IConfigurationEntry *AddEntryUint32(const char *parent, const char *name, uint32_t value) = 0;

            /**
             * @brief Add an entry to the configuration.
             * @param parent The parent name of the entry.
             * @param name The name of the entry.
             * @param value The default value for the entry.
             * @return The configuration entry.
             */
            virtual IConfigurationEntry *AddEntryInt32(const char *parent, const char *name, int32_t value) = 0;

            /**
             * @brief Add an entry to the configuration.
             * @param parent The parent name of the entry.
             * @param name The name of the entry.
             * @param value The default value for the entry.
             * @return The configuration entry.
             */
            virtual IConfigurationEntry *AddEntryUint64(const char *parent, const char *name, uint64_t value) = 0;

            /**
             * @brief Add an entry to the configuration.
             * @param parent The parent name of the entry.
             * @param name The name of the entry.
             * @param value The default value for the entry.
             * @return The configuration entry.
             */
            virtual IConfigurationEntry *AddEntryInt64(const char *parent, const char *name, int64_t value) = 0;

            /**
             * @brief Add an entry to the configuration.
             * @param parent The parent name of the entry.
             * @param name The name of the entry.
             * @param value The default value for the entry.
             * @return The configuration entry.
             */
            virtual IConfigurationEntry *AddEntryFloat(const char *parent, const char *name, float value) = 0;

            /**
             * @brief Add an entry to the configuration.
             * @param parent The parent name of the entry.
             * @param name The name of the entry.
             * @param value The default value for the entry.
             * @return The configuration entry.
             */
            virtual IConfigurationEntry *AddEntryDouble(const char *parent, const char *name, double value) = 0;

            /**
             * @brief Add an entry to the configuration.
             * @param parent The parent name of the entry.
             * @param name The name of the entry.
             * @param value The default value for the entry.
             * @return The configuration entry.
             */
            virtual IConfigurationEntry *AddEntryString(const char *parent, const char *name, const char *value) = 0;

            /**
             * @brief Add a new section in the configuration.
             * @param parent The parent section name or nullptr for the root section.
             * @param name The name of the section.
             * @return The configuration entry.
             */
            virtual IConfigurationSection *AddSection(const char *parent, const char *name) = 0;

            /**
             * @brief Remove an entry from the configuration.
             * @param parent The parent name of the entry.
             * @param name The name of the entry to be removed.
             * @return True if the entry is successfully removed, false otherwise.
             */
            virtual bool RemoveEntry(const char *parent, const char *name) = 0;

            /**
             * @brief Remove a section from the configuration.
             * @param parent The parent name of the section.
             * @param name The name of the section to be removed.
             * @return True if the section is successfully removed, false otherwise.
             */
            virtual bool RemoveSection(const char *parent, const char *name) = 0;

            /**
             * @brief Reads the configuration data into the provided buffer.
             * @param buffer The buffer to read the configuration data into.
             * @param len The length of the buffer.
             * @return True if the read operation is successful, false otherwise.
             */
            virtual bool Read(char *buffer, size_t len) = 0;

            /**
             * @brief Writes the configuration data and returns it as a character buffer.
             * @param len A pointer to store the length of the returned buffer.
             * @return  A character buffer containing the configuration data.
             */
            virtual char *Write(size_t *len) = 0;

            /**
             * @brief Free the memory allocated by the configuration.
             * @param ptr The memory pointer to free.
             */
            virtual void Free(void *ptr) const = 0;

            /**
             * @brief Retrieves the user data associated with the specified type.
             * @param type The type of the user data to retrieve.
             * @return A pointer to the user data, or nullptr if not found.
             */
            virtual void *GetUserData(size_t type = 0) const = 0;

            /**
             * @brief Sets the user data associated with the specified type.
             * @param data A pointer to the user data to set.
             * @param type The type of the user data to set.
             * @return A pointer to the previous user data associated with the type, or nullptr if not found.
             */
            virtual void *SetUserData(void *data, size_t type = 0) = 0;

        protected:
            virtual ~IConfiguration() = default;
        };

        /**
         * @brief Enumeration of configuration callback types.
         *
         * This enumeration defines the types of configuration callback events.
         */
        enum ConfigurationCallbackType {
            CFG_CB_ENTRY_ADD, /**< Configuration add event type. */
            CFG_CB_ENTRY_REMOVE, /**< Configuration remove event type. */
            CFG_CB_ENTRY_TYPE_CHANGE, /**< Configuration type change event type. */
            CFG_CB_ENTRY_VALUE_CHANGE, /**< Configuration value change event type. */
            CFG_CB_SECTION_ADD, /**< Configuration add event type. */
            CFG_CB_SECTION_REMOVE, /**< Configuration remove event type. */
            CFG_CB_COUNT,
        };

        /**
         * @brief Configuration callback function type.
         */
        typedef void (*ConfigurationCallback)(IConfigurationSection *parent, IConfigurationEntry *entry, IConfigurationSection *subsection, void *arg);

        /**
         * @class IConfigurationSection
         * @brief The interface for configuration sections.
         *
         * A configuration section represents a logical grouping of configuration entries and subsections.
         */
        class IConfigurationSection {
        public:
            /**
             * @brief Increase the reference count of the configuration section object.
             * @return The new reference count.
             */
            virtual int AddRef() const = 0;

            /**
             * @brief Decrease the reference count of the configuration section object.
             * @return The new reference count.
             */
            virtual int Release() const = 0;

            /**
             * @brief Get the name of the configuration section.
             * @return The name of the configuration section.
             */
            virtual const char *GetName() const = 0;

            /**
             * @brief Get the parent section of the current section.
             * @return Pointer to the parent section.
             */
            virtual IConfigurationSection *GetParent() const = 0;

            /**
             * @brief Clear all entries and subsections in the section.
             */
            virtual void Clear() = 0;

            /**
             * @brief Get the number of entries in the section.
             * @return The number of entries in the section.
             */
            virtual size_t GetNumberOfEntries() const = 0;

            /**
             * @brief Get the number of subsections in the section.
             * @return The number of subsections in the section.
             */
            virtual size_t GetNumberOfSections() const = 0;

            /**
             * @brief Get the total number of entries in the section and its subsections recursively.
             * @return The total number of entries in the section and its subsections.
             */
            virtual size_t GetNumberOfEntriesRecursive() const = 0;

            /**
             * @brief Get the total number of subsections in the section and its subsections recursively.
             * @return The total number of subsections in the section and its subsections.
             */
            virtual size_t GetNumberOfSectionsRecursive() const = 0;

            /**
             * @brief Retrieves a configuration entry by its index.
             * @param index The index of the entry.
             * @return The configuration entry if found, nullptr otherwise.
             */
            virtual IConfigurationEntry *GetEntry(size_t index) const = 0;

            /**
             * @brief Retrieves a configuration section by its index.
             * @param index The index of the section.
             * @return The configuration section if found, nullptr otherwise.
             */
            virtual IConfigurationSection *GetSection(size_t index) const = 0;

            /**
             * @brief Retrieves a configuration entry by its name.
             * @param name The name of the entry.
             * @return The configuration entry if found, nullptr otherwise.
             */
            virtual IConfigurationEntry *GetEntry(const char *name) const = 0;

            /**
             * @brief Retrieves a configuration section by its name.
             * @param name The name of the section.
             * @return The configuration section if found, nullptr otherwise.
             */
            virtual IConfigurationSection *GetSection(const char *name) const = 0;

            /**
             * @brief Add an entry to the configuration section.
             * @param name The name of the entry.
             * @return The configuration entry.
             */
            virtual IConfigurationEntry *AddEntry(const char *name) = 0;

            /**
             * @brief Add a boolean entry to the configuration section.
             * @param name The name of the entry.
             * @param value The default value for the entry.
             * @return The configuration entry.
             */
            virtual IConfigurationEntry *AddEntryBool(const char *name, bool value) = 0;

            /**
             * @brief Add an unsigned 32-bit integer entry to the configuration section.
             * @param name The name of the entry.
             * @param value The default value for the entry.
             * @return The configuration entry.
             */
            virtual IConfigurationEntry *AddEntryUint32(const char *name, uint32_t value) = 0;

            /**
             * @brief Add a signed 32-bit integer entry to the configuration section.
             * @param name The name of the entry.
             * @param value The default value for the entry.
             * @return The configuration entry.
             */
            virtual IConfigurationEntry *AddEntryInt32(const char *name, int32_t value) = 0;

            /**
             * @brief Add an unsigned 64-bit integer entry to the configuration section.
             * @param name The name of the entry.
             * @param value The default value for the entry.
             * @return The configuration entry.
             */
            virtual IConfigurationEntry *AddEntryUint64(const char *name, uint64_t value) = 0;

            /**
             * @brief Add a signed 64-bit integer entry to the configuration section.
             * @param name The name of the entry.
             * @param value The default value for the entry.
             * @return The configuration entry.
             */
            virtual IConfigurationEntry *AddEntryInt64(const char *name, int64_t value) = 0;

            /**
             * @brief Add a floating-point entry to the configuration section.
             * @param name The name of the entry.
             * @param value The default value for the entry.
             * @return The configuration entry.
             */
            virtual IConfigurationEntry *AddEntryFloat(const char *name, float value) = 0;

            /**
             * @brief Add a double precision floating-point entry to the configuration section.
             * @param name The name of the entry.
             * @param value The default value for the entry.
             * @return Pointer to the configuration entry.
             */
            virtual IConfigurationEntry *AddEntryDouble(const char *name, double value) = 0;

            /**
             * @brief Add a string entry to the configuration section.
             * @param name The name of the entry.
             * @param value The default value for the entry.
             * @return The configuration entry.
             */
            virtual IConfigurationEntry *AddEntryString(const char *name, const char *value) = 0;

            /**
              * @brief Add a new subsection in the configuration section.
              * @param name The name of the subsection.
              * @return The configuration section.
              */
            virtual IConfigurationSection *AddSection(const char *name) = 0;

            /**
             * @brief Removes an entry from the configuration section.
             * @param name The name of the entry to be removed.
             * @return True if the removal was successful, false otherwise.
             */
            virtual bool RemoveEntry(const char *name) = 0;

            /**
             * @brief Removes a subsection from the configuration section.
             * @param name The name of the subsection to be removed.
             * @return True if the removal was successful, false otherwise.
             */
            virtual bool RemoveSection(const char *name) = 0;

            /**
             * @brief Add a callback function to be triggered for the specified event type.
             * @param type The type of configuration callback event.
             * @param callback The callback function to be invoked.
             * @param arg An additional argument to be passed to the callback function.
             * @return True if the callback was added successfully, false otherwise.
             */
            virtual bool AddCallback(ConfigurationCallbackType type, ConfigurationCallback callback, void *arg) = 0;

            /**
             * @brief Clear all callbacks registered for the specified event type.
             * @param type The type of configuration callback event.
             */
            virtual void ClearCallbacks(ConfigurationCallbackType type) = 0;

            /**
             * @brief Invoke all callbacks registered for the specified event type.
             *
             * @param type The type of configuration callback event.
             * @param entry The entry of configuration section.
             * @param subsection The subsection of configuration section.
             */
            virtual void InvokeCallbacks(ConfigurationCallbackType type, IConfigurationEntry *entry, IConfigurationSection *subsection) = 0;

            /**
             * @brief Retrieves the user data associated with the specified type.
             * @param type The type of the user data to retrieve.
             * @return A pointer to the user data, or nullptr if not found.
             */
            virtual void *GetUserData(size_t type = 0) const = 0;

            /**
             * @brief Sets the user data associated with the specified type.
             * @param data A pointer to the user data to set.
             * @param type The type of the user data to set.
             * @return A pointer to the previous user data associated with the type, or nullptr if not found.
             */
            virtual void *SetUserData(void *data, size_t type = 0) = 0;

        protected:
            virtual ~IConfigurationSection() = default;
        };

        /**
         * @brief Enumeration of configuration entry types.
         *
         * This enumeration defines the available types for configuration entries.
         */
        enum EntryType {
            CFG_ENTRY_NONE, /**< No entry type. */
            CFG_ENTRY_BOOL, /**< Boolean entry type. */
            CFG_ENTRY_UINT, /**< Unsigned integer entry type. */
            CFG_ENTRY_INT, /**< Signed integer entry type. */
            CFG_ENTRY_REAL, /**< Real number entry type. */
            CFG_ENTRY_STR, /**< String entry type. */
        };

        /**
         * @brief Interface for a configuration entry.
         *
         * This interface provides methods to retrieve and set values for a configuration entry.
         */
        class IConfigurationEntry {
        public:
            /**
             * @brief Increase the reference count of the configuration section object.
             * @return The new reference count.
             */
            virtual int AddRef() const = 0;

            /**
             * @brief Decrease the reference count of the configuration section object.
             * @return The new reference count.
             */
            virtual int Release() const = 0;

            /**
             * @brief Get the name of the configuration entry.
             * @return The name of the configuration entry.
             */
            virtual const char *GetName() const = 0;

            /**
             * @brief Get the parent configuration section of the entry.
             * @return The parent configuration section of the entry.
             */
            virtual IConfigurationSection *GetParent() const = 0;

            /**
             * @brief Get the type of the configuration entry.
             * @return The type of the configuration entry.
             */
            virtual EntryType GetType() const = 0;

            /**
             * @brief Get the boolean value of the configuration entry.
             * @return The boolean value of the configuration entry.
             */
            virtual bool GetBool() = 0;

            /**
             * @brief Get the unsigned integer value of the configuration entry.
             * @return The unsigned integer value of the configuration entry.
             */
            virtual uint32_t GetUint32() = 0;

            /**
             * @brief Get the signed integer value of the configuration entry.
             * @return The signed integer value of the configuration entry.
             */
            virtual int32_t GetInt32() = 0;

            /**
             * @brief Get the unsigned 64-bit integer value of the configuration entry.
             * @return The unsigned 64-bit integer value of the configuration entry.
             */
            virtual uint64_t GetUint64() = 0;

            /**
             * @brief Get the signed 64-bit integer value of the configuration entry.
             * @return The signed 64-bit integer value of the configuration entry.
             */
            virtual int64_t GetInt64() = 0;

            /**
             * @brief Get the floating-point value of the configuration entry.
             * @return The floating-point value of the configuration entry.
             */
            virtual float GetFloat() = 0;

            /**
             * @brief Get the double precision floating-point value of the configuration entry.
             * @return The double precision floating-point value of the configuration entry.
             */
            virtual double GetDouble() = 0;

            /**
             * @brief Get the string value of the configuration entry.
             * @return The string value of the configuration entry.
             */
            virtual const char *GetString() const = 0;

            /**
             * @brief Get the string value of the configuration entry.
             * @return The string value of the configuration entry.
             */
            virtual size_t GetHash() const = 0;

            /**
             * @brief Set the value as a boolean.
             * @param value The boolean value to set.
             */
            virtual void SetBool(bool value) = 0;

            /**
             * @brief Set the value as an unsigned 32-bit integer.
             * @param value The unsigned 32-bit integer value to set.
             */
            virtual void SetUint32(uint32_t value) = 0;

            /**
             * @brief Set the value as a signed 32-bit integer.
             * @param value The signed 32-bit integer value to set.
             */
            virtual void SetInt32(int32_t value) = 0;

            /**
             * @brief Set the value as an unsigned 64-bit integer.
             * @param value The unsigned 64-bit integer value to set.
             */
            virtual void SetUint64(uint64_t value) = 0;

            /**
             * @brief Set the value as a signed 64-bit integer.
             * @param value The signed 64-bit integer value to set.
             */
            virtual void SetInt64(int64_t value) = 0;

            /**
             * @brief Set the value as a single-precision floating-point number.
             * @param value The single-precision floating-point value to set.
             */
            virtual void SetFloat(float value) = 0;

            /**
             * @brief Set the value as a double-precision floating-point number.
             * @param value The double-precision floating-point value to set.
             */
            virtual void SetDouble(double value) = 0;

            /**
             * @brief Set the value as a null-terminated string.
             * @param value The null-terminated string value to set.
             */
            virtual void SetString(const char *value) = 0;

            /**
             * @brief Set the default value as a boolean.
             * @param value The boolean value to set.
             */
            virtual void SetDefaultBool(bool value) = 0;

            /**
             * @brief Set the default value as an unsigned 32-bit integer.
             * @param value The unsigned 32-bit integer value to set.
             */
            virtual void SetDefaultUint32(uint32_t value) = 0;

            /**
             * @brief Set the default value as a signed 32-bit integer.
             * @param value The signed 32-bit integer value to set.
             */
            virtual void SetDefaultInt32(int32_t value) = 0;

            /**
             * @brief Set the default value as an unsigned 64-bit integer.
             * @param value The unsigned 64-bit integer value to set.
             */
            virtual void SetDefaultUint64(uint64_t value) = 0;

            /**
             * @brief Set the default value as a signed 64-bit integer.
             * @param value The signed 64-bit integer value to set.
             */
            virtual void SetDefaultInt64(int64_t value) = 0;

            /**
             * @brief Set the default value as a single-precision floating-point number.
             * @param value The single-precision floating-point value to set.
             */
            virtual void SetDefaultFloat(float value) = 0;

            /**
             * @brief Set the default value as a double-precision floating-point number.
             * @param value The double-precision floating-point value to set.
             */
            virtual void SetDefaultDouble(double value) = 0;

            /**
             * @brief Set the default value as a null-terminated string.
             * @param value The null-terminated string value to set.
             */
            virtual void SetDefaultString(const char *value) = 0;

            /**
             * @brief Copies the value of another configuration entry.
             * @param entry The configuration entry to copy the value from.
             */
            virtual void CopyValue(IConfigurationEntry *entry) = 0;

            /**
             * @brief Retrieves the user data associated with the specified type.
             * @param type The type of the user data to retrieve.
             * @return A pointer to the user data, or nullptr if not found.
             */
            virtual void *GetUserData(size_t type = 0) const = 0;

            /**
             * @brief Sets the user data associated with the specified type.
             * @param data A pointer to the user data to set.
             * @param type The type of the user data to set.
             * @return A pointer to the previous user data associated with the type, or nullptr if not found.
             */
            virtual void *SetUserData(void *data, size_t type = 0) = 0;

        protected:
            virtual ~IConfigurationEntry() = default;
        };
    }
}

#endif // BML_ICONFIGURATION_H
