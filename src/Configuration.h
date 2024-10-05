#ifndef BML_CONFIGURATION_H
#define BML_CONFIGURATION_H

#include <mutex>
#include <string>
#include <vector>
#include <unordered_map>

#include "BML/IConfiguration.h"
#include "BML/DataBox.h"
#include "BML/RefCount.h"
#include "Variant.h"

#include <yyjson.h>

namespace BML {
    class ConfigurationSection;
    class ConfigurationEntry;

    class Configuration final : public IConfiguration {
    public:
        static Configuration *GetInstance(const std::string &name);

        explicit Configuration(std::string name);

        Configuration(const Configuration &rhs) = delete;
        Configuration(Configuration &&rhs) noexcept = delete;

        ~Configuration() override;

        Configuration &operator=(const Configuration &rhs) = delete;
        Configuration &operator=(Configuration &&rhs) noexcept = delete;

        int AddRef() const override;
        int Release() const override;

        const char *GetName() const override { return m_Name.c_str(); }

        void Clear() override;

        size_t GetNumberOfEntries() const override;
        size_t GetNumberOfSections() const override;

        size_t GetNumberOfEntriesRecursive() const override;
        size_t GetNumberOfSectionsRecursive() const override;

        IConfigurationEntry *GetEntry(size_t index) const override;
        IConfigurationSection *GetSection(size_t index) const override;

        IConfigurationEntry *GetEntry(const char *name) override;
        IConfigurationSection *GetSection(const char *name) override;

        IConfigurationEntry *AddEntry(const char *parent, const char *name) override;
        IConfigurationEntry *AddEntryBool(const char *parent, const char *name, bool value) override;
        IConfigurationEntry *AddEntryUint32(const char *parent, const char *name, uint32_t value) override;
        IConfigurationEntry *AddEntryInt32(const char *parent, const char *name, int32_t value) override;
        IConfigurationEntry *AddEntryUint64(const char *parent, const char *name, uint64_t value) override;
        IConfigurationEntry *AddEntryInt64(const char *parent, const char *name, int64_t value) override;
        IConfigurationEntry *AddEntryFloat(const char *parent, const char *name, float value) override;
        IConfigurationEntry *AddEntryDouble(const char *parent, const char *name, double value) override;
        IConfigurationEntry *AddEntryString(const char *parent, const char *name, const char *value) override;
        IConfigurationSection *AddSection(const char *parent, const char *name) override;

        bool RemoveEntry(const char *parent, const char *name) override;
        bool RemoveSection(const char *parent, const char *name) override;

        bool Read(char *buffer, size_t len) override;
        char *Write(size_t *len) override;

        void Free(void *ptr) const override;

        void *GetUserData(size_t type) const override;
        void *SetUserData(void *data, size_t type) override;

    private:
        void ConvertObjectToSection(yyjson_val *obj, ConfigurationSection *section);
        void ConvertArrayToSection(yyjson_val *arr, ConfigurationSection *section);

        static ConfigurationSection *CreateSection(ConfigurationSection *root, const char *name);
        static ConfigurationSection *GetSection(ConfigurationSection *root, const char *name);

        mutable RefCount m_RefCount;
        mutable std::mutex m_RWLock;
        std::string m_Name;
        ConfigurationSection *m_Root;
        DataBox m_UserData;

        static std::mutex s_MapMutex;
        static std::unordered_map<std::string, Configuration *> s_Configurations;
    };

    class ConfigurationSection final : public IConfigurationSection {
    public:
        ConfigurationSection(ConfigurationSection *parent, const char *name);

        ConfigurationSection(const ConfigurationSection &rhs) = delete;
        ConfigurationSection(ConfigurationSection &&rhs) noexcept = delete;

        ~ConfigurationSection() override;

        ConfigurationSection &operator=(const ConfigurationSection &rhs) = delete;
        ConfigurationSection &operator=(ConfigurationSection &&rhs) noexcept = delete;

        int AddRef() const override;
        int Release() const override;

        const char *GetName() const override { return m_Name.c_str(); }

        IConfigurationSection *GetParent() const override { return m_Parent; }
        void SetParent(ConfigurationSection *parent) { m_Parent = parent; }

        void Clear() override;

        size_t GetNumberOfEntries() const override;
        size_t GetNumberOfSections() const override;

        size_t GetNumberOfEntriesRecursive() const override;
        size_t GetNumberOfSectionsRecursive() const override;

        IConfigurationEntry *GetEntry(size_t index) const override;
        IConfigurationSection *GetSection(size_t index) const override;

        IConfigurationEntry *GetEntry(const char *name) const override;
        IConfigurationSection *GetSection(const char *name) const override;

        IConfigurationEntry *AddEntry(const char *name) override;
        IConfigurationEntry *AddEntryBool(const char *name, bool value) override;
        IConfigurationEntry *AddEntryUint32(const char *name, uint32_t value) override;
        IConfigurationEntry *AddEntryInt32(const char *name, int32_t value) override;
        IConfigurationEntry *AddEntryUint64(const char *name, uint64_t value) override;
        IConfigurationEntry *AddEntryInt64(const char *name, int64_t value) override;
        IConfigurationEntry *AddEntryFloat(const char *name, float value) override;
        IConfigurationEntry *AddEntryDouble(const char *name, double value) override;
        IConfigurationEntry *AddEntryString(const char *name, const char *value) override;
        IConfigurationSection *AddSection(const char *name) override;

        bool RemoveEntry(const char *name) override;
        bool RemoveSection(const char *name) override;

        yyjson_mut_val *ToJsonKey(yyjson_mut_doc *doc);
        yyjson_mut_val *ToJsonObject(yyjson_mut_doc *doc);

        bool AddCallback(ConfigurationCallbackType type, ConfigurationCallback callback, void *arg) override;
        void ClearCallbacks(ConfigurationCallbackType type) override;
        void InvokeCallbacks(ConfigurationCallbackType type, IConfigurationEntry *entry, IConfigurationSection *subsection) override;

        void *GetUserData(size_t type) const override;
        void *SetUserData(void *data, size_t type) override;

    private:
        union Item {
            ConfigurationSection *section;
            ConfigurationEntry *entry;

            explicit Item(ConfigurationSection *s) : section(s) {}
            explicit Item(ConfigurationEntry *e) : entry(e) {}
        };

        struct Callback {
            ConfigurationCallback callback;
            void *arg;

            Callback(ConfigurationCallback cb, void *data) : callback(cb), arg(data) {}

            bool operator==(const Callback &rhs) const { return callback == rhs.callback && arg == rhs.arg; }
            bool operator!=(const Callback &rhs) const { return !(rhs == *this); }
        };

        mutable RefCount m_RefCount;
        mutable std::mutex m_RWLock;
        ConfigurationSection *m_Parent;
        std::string m_Name;
        std::vector<std::tuple<uint8_t, Item>> m_Elements;
        std::vector<ConfigurationSection *> m_Sections;
        std::vector<ConfigurationEntry *> m_Entries;
        std::unordered_map<std::string, ConfigurationSection *> m_SectionMap;
        std::unordered_map<std::string, ConfigurationEntry *> m_EntryMap;
        std::vector<Callback> m_Callbacks[CFG_CB_COUNT];
        DataBox m_UserData;
    };

    class ConfigurationEntry final : public IConfigurationEntry {
    public:
        ConfigurationEntry(ConfigurationSection *parent, const char *name);
        ConfigurationEntry(ConfigurationSection *parent, const char *name, bool value);
        ConfigurationEntry(ConfigurationSection *parent, const char *name, uint32_t value);
        ConfigurationEntry(ConfigurationSection *parent, const char *name, int32_t value);
        ConfigurationEntry(ConfigurationSection *parent, const char *name, uint64_t value);
        ConfigurationEntry(ConfigurationSection *parent, const char *name, int64_t value);
        ConfigurationEntry(ConfigurationSection *parent, const char *name, float value);
        ConfigurationEntry(ConfigurationSection *parent, const char *name, double value);
        ConfigurationEntry(ConfigurationSection *parent, const char *name, const char *value);

        ConfigurationEntry(const ConfigurationEntry &rhs) = delete;
        ConfigurationEntry(ConfigurationEntry &&rhs) noexcept = delete;

        ~ConfigurationEntry() override;

        ConfigurationEntry &operator=(const ConfigurationEntry &rhs) = delete;
        ConfigurationEntry &operator=(ConfigurationEntry &&rhs) noexcept = delete;

        int AddRef() const override;
        int Release() const override;

        const char *GetName() const override { return m_Name.c_str(); }

        IConfigurationSection *GetParent() const override { return m_Parent; }
        void SetParent(ConfigurationSection *parent) { m_Parent = parent; }

        EntryType GetType() const override;

        bool GetBool() override { return m_Value.GetBool(); }
        uint32_t GetUint32() override { return static_cast<uint32_t>(m_Value.GetUint64()); }
        int32_t GetInt32() override { return static_cast<int32_t>(m_Value.GetInt64()); }
        uint64_t GetUint64() override { return m_Value.GetUint64(); }
        int64_t GetInt64() override { return m_Value.GetInt64(); }
        float GetFloat() override { return static_cast<float>(m_Value.GetFloat64()); }
        double GetDouble() override { return m_Value.GetFloat64(); }
        const char *GetString() const override { return m_Value.GetString(); }
        size_t GetHash() const override { return m_Value.IsString() ? m_Hash : m_Value.GetUint32(); }

        void *GetValue() { return &m_Value.GetValue(); }

        void SetBool(bool value) override;
        void SetUint32(uint32_t value) override;
        void SetInt32(int32_t value) override;
        void SetUint64(uint64_t value) override;
        void SetInt64(int64_t value) override;
        void SetFloat(float value) override;
        void SetDouble(double value) override;
        void SetString(const char *value) override;

        void SetDefaultBool(bool value) override;
        void SetDefaultUint32(uint32_t value) override;
        void SetDefaultInt32(int32_t value) override;
        void SetDefaultUint64(uint64_t value) override;
        void SetDefaultInt64(int64_t value) override;
        void SetDefaultFloat(float value) override;
        void SetDefaultDouble(double value) override;
        void SetDefaultString(const char *value) override;

        void CopyValue(IConfigurationEntry *entry) override;

        void *GetUserData(size_t type) const override;
        void *SetUserData(void *data, size_t type) override;

        yyjson_mut_val *ToJsonKey(yyjson_mut_doc *doc);
        yyjson_mut_val *ToJsonValue(yyjson_mut_doc *doc);

    private:
        void InvokeCallbacks(bool typeChanged, bool valueChanged);

        mutable RefCount m_RefCount;
        mutable std::mutex m_RWLock;
        ConfigurationSection *m_Parent;
        std::string m_Name;
        Variant m_Value;
        size_t m_Hash = 0;
        DataBox m_UserData;
    };
}
#endif // BML_CONFIGURATION_H