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
    class ConfigurationList;
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

        size_t GetNumberOfEntries() const override;
        size_t GetNumberOfLists() const override;
        size_t GetNumberOfSections() const override;

        IConfigurationEntry *GetEntry(size_t index) const override;
        IConfigurationList *GetList(size_t index) const override;
        IConfigurationSection *GetSection(size_t index) const override;

        IConfigurationEntry *GetEntry(const char *name) override;
        IConfigurationList *GetList(const char *name) override;
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
        IConfigurationList *AddList(const char *parent, const char *name) override;
        IConfigurationSection *AddSection(const char *parent, const char *name) override;

        bool RemoveEntry(const char *parent, const char *name) override;
        bool RemoveList(const char *parent, const char *name) override;
        bool RemoveSection(const char *parent, const char *name) override;

        void Clear() override;

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

        size_t GetNumberOfEntries() const override;
        size_t GetNumberOfLists() const override;
        size_t GetNumberOfSections() const override;

        IConfigurationEntry *GetEntry(size_t index) const override;
        IConfigurationList *GetList(size_t index) const override;
        IConfigurationSection *GetSection(size_t index) const override;

        IConfigurationEntry *GetEntry(const char *name) const override;
        IConfigurationList *GetList(const char *name) const override;
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
        IConfigurationList *AddList(const char *name) override;
        IConfigurationSection *AddSection(const char *name) override;

        bool RemoveEntry(const char *name) override;
        bool RemoveList(const char *name) override;
        bool RemoveSection(const char *name) override;

        void Clear() override;

        yyjson_mut_val *ToJsonKey(yyjson_mut_doc *doc);
        yyjson_mut_val *ToJsonObject(yyjson_mut_doc *doc);

        bool AddCallback(ConfigurationCallbackType type, ConfigurationCallback callback, void *arg) override;
        void ClearCallbacks(ConfigurationCallbackType type) override;
        void InvokeCallbacks(ConfigurationCallbackType type, const ConfigurationItem &item) const;

    private:
        struct Callback {
            ConfigurationCallback callback;
            void *userdata;

            Callback(ConfigurationCallback cb, void *data) : callback(cb), userdata(data) {}

            bool operator==(const Callback &rhs) const { return callback == rhs.callback && userdata == rhs.userdata; }
            bool operator!=(const Callback &rhs) const { return !(rhs == *this); }
        };

        mutable RefCount m_RefCount;
        mutable std::mutex m_RWLock;
        ConfigurationSection *m_Parent;
        std::string m_Name;
        std::vector<ConfigurationItem> m_Items;
        std::vector<ConfigurationEntry *> m_Entries;
        std::vector<ConfigurationList *> m_Lists;
        std::vector<ConfigurationSection *> m_Sections;
        std::unordered_map<std::string, ConfigurationEntry *> m_EntryMap;
        std::unordered_map<std::string, ConfigurationList *> m_ListMap;
        std::unordered_map<std::string, ConfigurationSection *> m_SectionMap;
        std::vector<Callback> m_Callbacks[CFG_CB_COUNT];
    };

    class ConfigurationList final : public IConfigurationList {
    public:
        ConfigurationList(ConfigurationSection *parent, const char *name);

        ConfigurationList(const ConfigurationList &rhs) = delete;
        ConfigurationList(ConfigurationList &&rhs) noexcept = delete;

        ~ConfigurationList() override;

        ConfigurationList &operator=(const ConfigurationList &rhs) = delete;
        ConfigurationList &operator=(ConfigurationList &&rhs) noexcept = delete;

        int AddRef() const override;
        int Release() const override;

        const char *GetName() const override { return m_Name.c_str(); }

        IConfigurationSection *GetParent() const override { return m_Parent; }
        void SetParent(ConfigurationSection *parent) { m_Parent = parent; }

        size_t GetNumberOfValues() const override;

        ConfigurationEntryType GetType(size_t index) const override;
        size_t GetSize(size_t index) const override;

        bool GetBool(size_t index) override;
        uint32_t GetUint32(size_t index) override;
        int32_t GetInt32(size_t index) override;
        uint64_t GetUint64(size_t index) override;
        int64_t GetInt64(size_t index) override;
        float GetFloat(size_t index) override;
        double GetDouble(size_t index) override;
        const char *GetString(size_t index) const override;

        void *GetValue(size_t index) { return &m_Values[index].GetValue(); }

        void SetBool(size_t index, bool value) override;
        void SetUint32(size_t index, uint32_t value) override;
        void SetInt32(size_t index, int32_t value) override;
        void SetUint64(size_t index, uint64_t value) override;
        void SetInt64(size_t index, int64_t value) override;
        void SetFloat(size_t index, float value) override;
        void SetDouble(size_t index, double value) override;
        void SetString(size_t index, const char *value) override;

        void InsertBool(size_t index, bool value) override;
        void InsertUint32(size_t index, uint32_t value) override;
        void InsertInt32(size_t index, int32_t value) override;
        void InsertUint64(size_t index, uint64_t value) override;
        void InsertInt64(size_t index, int64_t value) override;
        void InsertFloat(size_t index, float value) override;
        void InsertDouble(size_t index, double value) override;
        void InsertString(size_t index, const char *value) override;

        void AppendBool(bool value) override;
        void AppendUint32(uint32_t value) override;
        void AppendInt32(int32_t value) override;
        void AppendUint64(uint64_t value) override;
        void AppendInt64(int64_t value) override;
        void AppendFloat(float value) override;
        void AppendDouble(double value) override;
        void AppendString(const char *value) override;

        bool Remove(size_t index) override;

        void Clear() override;

        void Resize(size_t size) override;
        void Reserve(size_t size) override;

        yyjson_mut_val *ToJsonKey(yyjson_mut_doc *doc);
        yyjson_mut_val *ToJsonArray(yyjson_mut_doc *doc);

    private:
        mutable RefCount m_RefCount;
        mutable std::mutex m_RWLock;
        ConfigurationSection *m_Parent;
        std::string m_Name;
        std::vector<Variant> m_Values;
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

        ConfigurationEntryType GetType() const override;
        size_t GetSize() const override;

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
    };
}
#endif // BML_CONFIGURATION_H