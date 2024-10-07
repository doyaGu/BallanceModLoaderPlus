#include "Configuration.h"

#include <cassert>
#include <utility>

#include "StringUtils.h"

using namespace BML;

std::mutex Configuration::s_MapMutex;
std::unordered_map<std::string, Configuration *> Configuration::s_Configurations;

ConfigurationEntryType GetValueType(const Variant &value) {
    switch (value.GetType()) {
        case VAR_TYPE_BOOL:
            return CFG_ENTRY_BOOL;
        case VAR_TYPE_NUM:
            switch (value.GetSubtype()) {
                case VAR_SUBTYPE_UINT64:
                case VAR_SUBTYPE_UINT32:
                    return CFG_ENTRY_UINT;
                case VAR_SUBTYPE_INT64:
                case VAR_SUBTYPE_INT32:
                    return CFG_ENTRY_INT;
                case VAR_SUBTYPE_FLOAT64:
                case VAR_SUBTYPE_FLOAT32:
                    return CFG_ENTRY_REAL;
                default:
                    break;
            }
        case VAR_TYPE_STR:
            return CFG_ENTRY_STR;
        default:
            break;
    }
    return CFG_ENTRY_NONE;
}

Configuration *Configuration::GetInstance(const std::string &name) {
    auto it = s_Configurations.find(name);
    if (it == s_Configurations.end()) {
        return new Configuration(name);
    }
    return it->second;
}

Configuration::Configuration(std::string name)
    : m_Name(std::move(name)), m_Root(new ConfigurationSection(nullptr, "root")) {
    std::lock_guard<std::mutex> lock{s_MapMutex};
    s_Configurations[m_Name] = this;
}

Configuration::~Configuration() {
    Clear();
    m_Root->Release();

    std::lock_guard<std::mutex> lock{s_MapMutex};
    s_Configurations.erase(m_Name);
}

int Configuration::AddRef() const {
    return m_RefCount.AddRef();
}

int Configuration::Release() const {
    int r = m_RefCount.Release();
    if (r == 0) {
        std::atomic_thread_fence(std::memory_order_acquire);
        delete const_cast<Configuration *>(this);
    }
    return r;
}

size_t Configuration::GetNumberOfEntries() const {
    return m_Root->GetNumberOfEntries();
}

size_t Configuration::GetNumberOfLists() const {
    return m_Root->GetNumberOfLists();
}

size_t Configuration::GetNumberOfSections() const {
    return m_Root->GetNumberOfSections();
}

IConfigurationEntry *Configuration::GetEntry(size_t index) const {
    return m_Root->GetEntry(index);
}

IConfigurationList * Configuration::GetList(size_t index) const {
    return m_Root->GetList(index);
}

IConfigurationSection *Configuration::GetSection(size_t index) const {
    return m_Root->GetSection(index);
}

IConfigurationEntry *Configuration::GetEntry(const char *name) {
    return m_Root->GetEntry(name);
}

IConfigurationList * Configuration::GetList(const char *name) {
    return m_Root->GetList(name);
}

IConfigurationSection *Configuration::GetSection(const char *name) {
    return GetSection(m_Root, name);
}

IConfigurationEntry *Configuration::AddEntry(const char *parent, const char *name) {
    if (parent == nullptr)
        return m_Root->AddEntry(name);
    IConfigurationSection *section = GetSection(parent);
    if (section || (section = CreateSection(m_Root, parent)))
        return section->AddEntry(name);
    return nullptr;
}

IConfigurationEntry *Configuration::AddEntryBool(const char *parent, const char *name, bool value) {
    if (parent == nullptr)
        return m_Root->AddEntryBool(name, value);
    IConfigurationSection *section = GetSection(parent);
    if (section || (section = CreateSection(m_Root, parent)))
        return section->AddEntryBool(name, value);
    return nullptr;
}

IConfigurationEntry *Configuration::AddEntryUint32(const char *parent, const char *name, uint32_t value) {
    if (parent == nullptr)
        return m_Root->AddEntryUint32(name, value);
    IConfigurationSection *section = GetSection(parent);
    if (section || (section = CreateSection(m_Root, parent)))
        return section->AddEntryUint32(name, value);
    return nullptr;
}

IConfigurationEntry *Configuration::AddEntryInt32(const char *parent, const char *name, int32_t value) {
    if (parent == nullptr)
        return m_Root->AddEntryInt32(name, value);
    IConfigurationSection *section = GetSection(parent);
    if (section || (section = CreateSection(m_Root, parent)))
        return section->AddEntryInt32(name, value);
    return nullptr;
}

IConfigurationEntry *Configuration::AddEntryUint64(const char *parent, const char *name, uint64_t value) {
    if (parent == nullptr)
        return m_Root->AddEntryUint64(name, value);
    IConfigurationSection *section = GetSection(parent);
    if (section || (section = CreateSection(m_Root, parent)))
        return section->AddEntryUint64(name, value);
    return nullptr;
}

IConfigurationEntry *Configuration::AddEntryInt64(const char *parent, const char *name, int64_t value) {
    if (parent == nullptr)
        return m_Root->AddEntryInt64(name, value);
    IConfigurationSection *section = GetSection(parent);
    if (section || (section = CreateSection(m_Root, parent)))
        return section->AddEntryInt64(name, value);
    return nullptr;
}

IConfigurationEntry *Configuration::AddEntryFloat(const char *parent, const char *name, float value) {
    if (parent == nullptr)
        return m_Root->AddEntryFloat(name, value);
    IConfigurationSection *section = GetSection(parent);
    if (section || (section = CreateSection(m_Root, parent)))
        return section->AddEntryFloat(name, value);
    return nullptr;
}

IConfigurationEntry *Configuration::AddEntryDouble(const char *parent, const char *name, double value) {
    if (parent == nullptr)
        return m_Root->AddEntryDouble(name, value);
    IConfigurationSection *section = GetSection(parent);
    if (section || (section = CreateSection(m_Root, parent)))
        return section->AddEntryDouble(name, value);
    return nullptr;
}

IConfigurationEntry *Configuration::AddEntryString(const char *parent, const char *name, const char *value) {
    if (parent == nullptr)
        return m_Root->AddEntryString(name, value);
    IConfigurationSection *section = GetSection(parent);
    if (section || (section = CreateSection(m_Root, parent)))
        return section->AddEntryString(name, value);
    return nullptr;
}

IConfigurationList * Configuration::AddList(const char *parent, const char *name) {
    if (!parent)
        return m_Root->AddList(name);
    auto *section = (ConfigurationSection *) m_Root->GetSection(parent);
    if (section)
        return section->AddList(name);
    return nullptr;
}

IConfigurationSection *Configuration::AddSection(const char *parent, const char *name) {
    if (!parent)
        return m_Root->AddSection(name);
    auto *section = (ConfigurationSection *) m_Root->GetSection(parent);
    if (section)
        return section->AddSection(name);
    return nullptr;
}

bool Configuration::RemoveEntry(const char *parent, const char *name) {
    if (!parent)
        return m_Root->RemoveEntry(name);
    IConfigurationSection *section = m_Root->GetSection(parent);
    if (section)
        return section->RemoveEntry(name);
    return false;
}

bool Configuration::RemoveList(const char *parent, const char *name) {
    if (!parent)
        return m_Root->RemoveList(name);
    IConfigurationSection *section = m_Root->GetSection(parent);
    if (section)
        return section->RemoveList(name);
    return false;
}

bool Configuration::RemoveSection(const char *parent, const char *name) {
    if (!parent)
        return m_Root->RemoveSection(name);
    IConfigurationSection *section = m_Root->GetSection(parent);
    if (section)
        return section->RemoveSection(name);
    return false;
}

void Configuration::Clear() {
    m_Root->Clear();
}

bool Configuration::Read(char *buffer, size_t len) {
    yyjson_read_flag flg = YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_INF_AND_NAN;
    yyjson_read_err err;
    yyjson_doc *doc = yyjson_read_opts(buffer, len, flg, nullptr, &err);
    if (!doc)
        return false;

    std::lock_guard<std::mutex> guard(m_RWLock);

    Clear();

    yyjson_val *obj = yyjson_doc_get_root(doc);
    ConvertObjectToSection(obj, m_Root);
    yyjson_doc_free(doc);

    return true;
}

char *Configuration::Write(size_t *len) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(nullptr);
    if (!doc) {
        *len = 0;
        return nullptr;
    }

    yyjson_mut_val *root = nullptr;
    if (m_Root->GetNumberOfEntries() != 0 || m_Root->GetNumberOfSections() != 0) {
        root = m_Root->ToJsonObject(doc);
    }

    if (!root || yyjson_mut_obj_size(root) == 0) {
        *len = 0;
        return nullptr;
    }
    yyjson_mut_doc_set_root(doc, root);

    yyjson_write_flag flg = YYJSON_WRITE_PRETTY | YYJSON_WRITE_ESCAPE_UNICODE;
    yyjson_write_err err;
    char *json = yyjson_mut_write_opts(doc, flg, nullptr, len, &err);
    return json;
}

void Configuration::Free(void *ptr) const {
    free(ptr);
}

void *Configuration::GetUserData(size_t type) const {
    return m_UserData.GetData(type);
}

void *Configuration::SetUserData(void *data, size_t type) {
    return m_UserData.SetData(data, type);
}

void Configuration::ConvertObjectToSection(yyjson_val *obj, ConfigurationSection *section) {
    if (!yyjson_is_obj(obj))
        return;

    yyjson_val *key, *val;
    yyjson_obj_iter iter;
    yyjson_obj_iter_init(obj, &iter);
    while ((key = yyjson_obj_iter_next(&iter))) {
        val = yyjson_obj_iter_get_val(key);
        switch (yyjson_get_tag(val)) {
            case YYJSON_TYPE_OBJ | YYJSON_SUBTYPE_NONE:
                ConvertObjectToSection(val, (ConfigurationSection *) section->AddSection(yyjson_get_str(key)));
                break;
            case YYJSON_TYPE_BOOL | YYJSON_SUBTYPE_TRUE:
            case YYJSON_TYPE_BOOL | YYJSON_SUBTYPE_FALSE:
                section->AddEntryBool(yyjson_get_str(key), yyjson_get_bool(val));
                break;
            case YYJSON_TYPE_NUM | YYJSON_SUBTYPE_UINT:
                section->AddEntryUint64(yyjson_get_str(key), yyjson_get_uint(val));
                break;
            case YYJSON_TYPE_NUM | YYJSON_SUBTYPE_SINT:
                section->AddEntryInt64(yyjson_get_str(key), yyjson_get_sint(val));
                break;
            case YYJSON_TYPE_NUM | YYJSON_SUBTYPE_REAL:
                section->AddEntryDouble(yyjson_get_str(key), yyjson_get_real(val));
                break;
            case YYJSON_TYPE_STR | YYJSON_SUBTYPE_NONE:
                section->AddEntryString(yyjson_get_str(key), yyjson_get_str(val));
                break;
            case YYJSON_TYPE_NULL | YYJSON_SUBTYPE_NONE:
                section->AddEntry(yyjson_get_str(key));
                break;
            case YYJSON_TYPE_ARR | YYJSON_SUBTYPE_NONE:
                ConvertArrayToSection(val, (ConfigurationSection *) section->AddSection(yyjson_get_str(key)));
                break;
            default:
                break;
        }
    }
}

void Configuration::ConvertArrayToSection(yyjson_val *arr, ConfigurationSection *section) {
    if (!yyjson_is_arr(arr))
        return;

    size_t idx = 0;
    yyjson_val *val;
    yyjson_arr_iter iter = yyjson_arr_iter_with(arr);
    while ((val = yyjson_arr_iter_next(&iter))) {
        char buf[32];
        snprintf(buf, 32, "%u", idx);
        switch (yyjson_get_tag(val)) {
            case YYJSON_TYPE_OBJ | YYJSON_SUBTYPE_NONE:
                ConvertObjectToSection(val, (ConfigurationSection *) section->AddSection(buf));
                break;
            case YYJSON_TYPE_BOOL | YYJSON_SUBTYPE_TRUE:
            case YYJSON_TYPE_BOOL | YYJSON_SUBTYPE_FALSE:
                section->AddEntryBool(buf, yyjson_get_bool(val));
                break;
            case YYJSON_TYPE_NUM | YYJSON_SUBTYPE_UINT:
                section->AddEntryUint64(buf, yyjson_get_uint(val));
                break;
            case YYJSON_TYPE_NUM | YYJSON_SUBTYPE_SINT:
                section->AddEntryInt64(buf, yyjson_get_sint(val));
                break;
            case YYJSON_TYPE_NUM | YYJSON_SUBTYPE_REAL:
                section->AddEntryDouble(buf, yyjson_get_real(val));
                break;
            case YYJSON_TYPE_STR | YYJSON_SUBTYPE_NONE:
                section->AddEntryString(buf, yyjson_get_str(val));
                break;
            case YYJSON_TYPE_NULL | YYJSON_SUBTYPE_NONE:
                section->AddEntry(buf);
                break;
            case YYJSON_TYPE_ARR | YYJSON_SUBTYPE_NONE:
                ConvertArrayToSection(val, (ConfigurationSection *) section->AddSection(buf));
                break;
            default:
                break;
        }
        ++idx;
    }
}

ConfigurationSection *Configuration::CreateSection(ConfigurationSection *root, const char *name) {
    return (ConfigurationSection *) root->AddSection(name);
}

ConfigurationSection *Configuration::GetSection(ConfigurationSection *root, const char *name) {
    return (ConfigurationSection *) root->GetSection(name);
}

ConfigurationSection::ConfigurationSection(ConfigurationSection *parent, const char *name)
    : m_Parent(parent), m_Name(name) {
    assert(name != nullptr);
}

ConfigurationSection::~ConfigurationSection() {
    Clear();
    if (m_Parent) {
        m_Parent->RemoveSection(m_Name.c_str());
    }
}

int ConfigurationSection::AddRef() const {
    return m_RefCount.AddRef();
}

int ConfigurationSection::Release() const {
    int r = m_RefCount.Release();
    if (r == 0) {
        std::atomic_thread_fence(std::memory_order_acquire);
        delete const_cast<ConfigurationSection *>(this);
    }
    return r;
}

size_t ConfigurationSection::GetNumberOfEntries() const {
    return m_Entries.size();
}

size_t ConfigurationSection::GetNumberOfLists() const {
    return m_Lists.size();
}

size_t ConfigurationSection::GetNumberOfSections() const {
    return m_Sections.size();
}

IConfigurationEntry *ConfigurationSection::GetEntry(size_t index) const {
    if (index >= m_Entries.size())
        return nullptr;

    return m_Entries[index];
}

IConfigurationList * ConfigurationSection::GetList(size_t index) const {
    if (index >= m_Lists.size())
        return nullptr;

    return m_Lists[index];
}

IConfigurationSection *ConfigurationSection::GetSection(size_t index) const {
    if (index >= m_Sections.size())
        return nullptr;

    return m_Sections[index];
}

IConfigurationEntry *ConfigurationSection::GetEntry(const char *name) const {
    if (!name)
        return nullptr;

    auto it = m_EntryMap.find(name);
    if (it == m_EntryMap.end())
        return nullptr;
    return it->second;
}

IConfigurationList * ConfigurationSection::GetList(const char *name) const {
    if (!name)
        return nullptr;

    auto it = m_ListMap.find(name);
    if (it == m_ListMap.end())
        return nullptr;
    return it->second;
}

IConfigurationSection *ConfigurationSection::GetSection(const char *name) const {
    if (!name)
        return nullptr;

    auto it = m_SectionMap.find(name);
    if (it == m_SectionMap.end())
        return nullptr;
    return it->second;
}

IConfigurationEntry *ConfigurationSection::AddEntry(const char *name) {
    if (!name)
        return nullptr;

    auto *entry = (ConfigurationEntry *) GetEntry(name);
    if (entry)
        return entry;

    std::lock_guard<std::mutex> guard(m_RWLock);

    entry = new ConfigurationEntry(this, name);
    m_Items.emplace_back(entry);
    m_Entries.emplace_back(entry);
    m_EntryMap[entry->GetName()] = entry;

    InvokeCallbacks(CFG_CB_ENTRY_ADD, ConfigurationItem(entry));
    return entry;
}

IConfigurationEntry *ConfigurationSection::AddEntryBool(const char *name, bool value) {
    if (!name)
        return nullptr;

    auto *entry = (ConfigurationEntry *) GetEntry(name);
    if (entry) {
        entry->SetDefaultBool(value);
        return entry;
    }

    std::lock_guard<std::mutex> guard(m_RWLock);

    entry = new ConfigurationEntry(this, name, value);
    m_Items.emplace_back(entry);
    m_Entries.emplace_back(entry);
    m_EntryMap[entry->GetName()] = entry;

    InvokeCallbacks(CFG_CB_ENTRY_ADD, ConfigurationItem(entry));
    return entry;
}

IConfigurationEntry *ConfigurationSection::AddEntryUint32(const char *name, uint32_t value) {
    if (!name)
        return nullptr;

    auto *entry = (ConfigurationEntry *) GetEntry(name);
    if (entry) {
        entry->SetDefaultUint32(value);
        return entry;
    }

    std::lock_guard<std::mutex> guard(m_RWLock);

    entry = new ConfigurationEntry(this, name, value);
    m_Items.emplace_back(entry);
    m_Entries.emplace_back(entry);
    m_EntryMap[entry->GetName()] = entry;

    InvokeCallbacks(CFG_CB_ENTRY_ADD, ConfigurationItem(entry));
    return entry;
}

IConfigurationEntry *ConfigurationSection::AddEntryInt32(const char *name, int32_t value) {
    if (!name)
        return nullptr;

    auto *entry = (ConfigurationEntry *) GetEntry(name);
    if (entry) {
        entry->SetDefaultInt32(value);
        return entry;
    }

    std::lock_guard<std::mutex> guard(m_RWLock);

    entry = new ConfigurationEntry(this, name, value);
    m_Items.emplace_back(entry);
    m_Entries.emplace_back(entry);
    m_EntryMap[entry->GetName()] = entry;

    InvokeCallbacks(CFG_CB_ENTRY_ADD, ConfigurationItem(entry));
    return entry;
}

IConfigurationEntry *ConfigurationSection::AddEntryUint64(const char *name, uint64_t value) {
    if (!name)
        return nullptr;

    auto *entry = (ConfigurationEntry *) GetEntry(name);
    if (entry) {
        entry->SetDefaultUint64(value);
        return entry;
    }

    std::lock_guard<std::mutex> guard(m_RWLock);

    entry = new ConfigurationEntry(this, name, value);
    m_Items.emplace_back(entry);
    m_Entries.emplace_back(entry);
    m_EntryMap[entry->GetName()] = entry;

    InvokeCallbacks(CFG_CB_ENTRY_ADD, ConfigurationItem(entry));
    return entry;
}

IConfigurationEntry *ConfigurationSection::AddEntryInt64(const char *name, int64_t value) {
    if (!name)
        return nullptr;

    auto *entry = (ConfigurationEntry *) GetEntry(name);
    if (entry) {
        entry->SetDefaultInt64(value);
        return entry;
    }

    std::lock_guard<std::mutex> guard(m_RWLock);

    entry = new ConfigurationEntry(this, name, value);
    m_Items.emplace_back(entry);
    m_Entries.emplace_back(entry);
    m_EntryMap[entry->GetName()] = entry;

    InvokeCallbacks(CFG_CB_ENTRY_ADD, ConfigurationItem(entry));
    return entry;
}

IConfigurationEntry *ConfigurationSection::AddEntryFloat(const char *name, float value) {
    if (!name)
        return nullptr;

    auto *entry = (ConfigurationEntry *) GetEntry(name);
    if (entry) {
        entry->SetDefaultFloat(value);
        return entry;
    }

    std::lock_guard<std::mutex> guard(m_RWLock);

    entry = new ConfigurationEntry(this, name, value);
    m_Items.emplace_back(entry);
    m_Entries.emplace_back(entry);
    m_EntryMap[entry->GetName()] = entry;

    InvokeCallbacks(CFG_CB_ENTRY_ADD, ConfigurationItem(entry));
    return entry;
}

IConfigurationEntry *ConfigurationSection::AddEntryDouble(const char *name, double value) {
    if (!name)
        return nullptr;

    auto *entry = (ConfigurationEntry *) GetEntry(name);
    if (entry) {
        entry->SetDefaultDouble(value);
        return entry;
    }

    std::lock_guard<std::mutex> guard(m_RWLock);

    entry = new ConfigurationEntry(this, name, value);
    m_Items.emplace_back(entry);
    m_Entries.emplace_back(entry);
    m_EntryMap[entry->GetName()] = entry;

    InvokeCallbacks(CFG_CB_ENTRY_ADD, ConfigurationItem(entry));
    return entry;
}

IConfigurationEntry *ConfigurationSection::AddEntryString(const char *name, const char *value) {
    if (!name)
        return nullptr;

    if (!value)
        value = "";

    auto *entry = (ConfigurationEntry *) GetEntry(name);
    if (entry) {
        entry->SetDefaultString(value);
        return entry;
    }

    std::lock_guard<std::mutex> guard(m_RWLock);

    entry = new ConfigurationEntry(this, name, value);
    m_Items.emplace_back(entry);
    m_Entries.emplace_back(entry);
    m_EntryMap[entry->GetName()] = entry;

    InvokeCallbacks(CFG_CB_ENTRY_ADD, ConfigurationItem(entry));
    return entry;
}

IConfigurationList * ConfigurationSection::AddList(const char *name) {
    if (!name)
        return nullptr;

    auto *list = (ConfigurationList *) GetList(name);
    if (list)
        return list;

    std::lock_guard<std::mutex> guard(m_RWLock);

    list = new ConfigurationList(this, name);
    m_Items.emplace_back(list);
    m_Lists.emplace_back(list);
    m_ListMap[list->GetName()] = list;

    InvokeCallbacks(CFG_CB_LIST_ADD, ConfigurationItem(list));
    return list;
}

IConfigurationSection *ConfigurationSection::AddSection(const char *name) {
    if (!name)
        return nullptr;

    auto *section = (ConfigurationSection *) GetSection(name);
    if (section)
        return section;

    std::lock_guard<std::mutex> guard(m_RWLock);

    section = new ConfigurationSection(this, name);
    m_Items.emplace_back(section);
    m_Sections.emplace_back(section);
    m_SectionMap[section->GetName()] = section;

    InvokeCallbacks(CFG_CB_SECTION_ADD, ConfigurationItem(section));
    return section;
}

bool ConfigurationSection::RemoveEntry(const char *name) {
    if (!name)
        return false;

    auto it = m_EntryMap.find(name);
    if (it == m_EntryMap.end())
        return false;

    std::lock_guard<std::mutex> guard(m_RWLock);

    auto *entry = it->second;
    auto del = std::remove_if(m_Items.begin(), m_Items.end(),
                              [entry](const ConfigurationItem &e) { return e.data.entry == entry; });
    m_Items.erase(del, m_Items.end());
    m_Entries.erase(std::remove(m_Entries.begin(), m_Entries.end(), entry), m_Entries.end());
    m_EntryMap.erase(it);

    InvokeCallbacks(CFG_CB_ENTRY_REMOVE, ConfigurationItem(entry));

    if (entry->Release() != 0) {
        entry->SetParent(nullptr);
    }
    return true;
}

bool ConfigurationSection::RemoveList(const char *name) {
    if (!name)
        return false;

    auto it = m_ListMap.find(name);
    if (it == m_ListMap.end())
        return false;

    std::lock_guard<std::mutex> guard(m_RWLock);

    auto *list = it->second;
    auto del = std::remove_if(m_Items.begin(), m_Items.end(),
                              [list](const ConfigurationItem &e) { return e.data.list == list; });
    m_Items.erase(del, m_Items.end());
    m_Lists.erase(std::remove(m_Lists.begin(), m_Lists.end(), list), m_Lists.end());
    m_ListMap.erase(it);

    if (list->Release() != 0) {
        list->SetParent(nullptr);
    }

    InvokeCallbacks(CFG_CB_LIST_REMOVE, ConfigurationItem(list));
    return true;
}

bool ConfigurationSection::RemoveSection(const char *name) {
    if (!name)
        return false;

    auto it = m_SectionMap.find(name);
    if (it == m_SectionMap.end())
        return false;

    std::lock_guard<std::mutex> guard(m_RWLock);

    auto *section = it->second;
    auto del = std::remove_if(m_Items.begin(), m_Items.end(),
                              [section](const ConfigurationItem &e) { return e.data.section == section; });
    m_Items.erase(del, m_Items.end());
    m_Sections.erase(std::remove(m_Sections.begin(), m_Sections.end(), section), m_Sections.end());
    m_SectionMap.erase(it);

    if (section->Release() != 0) {
        section->SetParent(nullptr);
    }

    InvokeCallbacks(CFG_CB_SECTION_REMOVE, ConfigurationItem(section));
    return true;
}

void ConfigurationSection::Clear() {
    std::lock_guard<std::mutex> guard(m_RWLock);

    for (auto &pair: m_EntryMap) {
        auto *entry = pair.second;
        if (entry->Release() != 0) {
            entry->SetParent(nullptr);
        }
    }
    m_EntryMap.clear();
    m_Entries.clear();

    for (auto &pair: m_SectionMap) {
        auto *section = pair.second;
        if (section->Release() != 0) {
            section->SetParent(nullptr);
        }
    }
    m_SectionMap.clear();
    m_Sections.clear();

    m_Items.clear();
}

yyjson_mut_val *ConfigurationSection::ToJsonKey(yyjson_mut_doc *doc) {
    if (!doc)
        return nullptr;
    return yyjson_mut_str(doc, m_Name.c_str());
}

yyjson_mut_val *ConfigurationSection::ToJsonObject(yyjson_mut_doc *doc) {
    if (!doc)
        return nullptr;

    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    if (!obj)
        return nullptr;

    for (auto &e: m_Items) {
        switch (e.type) {
            case CFG_TYPE_ENTRY: {
                auto *entry = (ConfigurationEntry *) e.data.entry;
                if (entry) {
                    yyjson_mut_val *key = entry->ToJsonKey(doc);
                    yyjson_mut_val *val = entry->ToJsonValue(doc);
                    if (val)
                        yyjson_mut_obj_add(obj, key, val);
                }
            }
            break;
            case CFG_TYPE_LIST: {
                auto *list = (ConfigurationList *) e.data.list;
                if (list) {
                    yyjson_mut_val *key = list->ToJsonKey(doc);
                    yyjson_mut_val *val = list->ToJsonArray(doc);
                    if (val)
                        yyjson_mut_obj_add(obj, key, val);
                }
            }
            case CFG_TYPE_SECTION: {
                auto *section = (ConfigurationSection *) e.data.section;
                if (section) {
                    yyjson_mut_val *key = section->ToJsonKey(doc);
                    yyjson_mut_val *val = section->ToJsonObject(doc);
                    if (val)
                        yyjson_mut_obj_add(obj, key, val);
                }
            }
            break;
            default:
                break;
        }
    }

    return obj;
}

bool ConfigurationSection::AddCallback(ConfigurationCallbackType type, ConfigurationCallback callback, void *arg) {
    if (type < 0 || type >= CFG_CB_COUNT)
        return false;

    if (!callback)
        return false;

    std::lock_guard<std::mutex> guard(m_RWLock);

    auto &callbacks = m_Callbacks[type];
    Callback cb(callback, arg);
    auto it = std::find(callbacks.begin(), callbacks.end(), cb);
    if (it != callbacks.end())
        return false;

    callbacks.emplace_back(cb);
    return true;
}

void ConfigurationSection::ClearCallbacks(ConfigurationCallbackType type) {
    std::lock_guard<std::mutex> guard(m_RWLock);

    if (type >= 0 && type < CFG_CB_COUNT)
        m_Callbacks[type].clear();
}

void ConfigurationSection::InvokeCallbacks(ConfigurationCallbackType type, const ConfigurationItem &item) const {
    assert(type >= 0 && type < CFG_CB_COUNT);
    const ConfigurationCallbackArgument arg = {type, item};
    for (const auto &cb: m_Callbacks[type]) {
        cb.callback(arg, cb.userdata);
    }
}

ConfigurationList::ConfigurationList(ConfigurationSection *parent, const char *name) : m_Parent(parent), m_Name(name) {
    assert(name != nullptr);
}

ConfigurationList::~ConfigurationList() {
    if (m_Parent) {
        m_Parent->RemoveEntry(m_Name.c_str());
    }
}

int ConfigurationList::AddRef() const {
    return m_RefCount.AddRef();
}

int ConfigurationList::Release() const {
    int r = m_RefCount.Release();
    if (r == 0) {
        std::atomic_thread_fence(std::memory_order_acquire);
        delete const_cast<ConfigurationList *>(this);
    }
    return r;
}

size_t ConfigurationList::GetNumberOfValues() const {
    return m_Values.size();
}

ConfigurationEntryType ConfigurationList::GetType(size_t index) const {
    if (index >= m_Values.size())
        return CFG_ENTRY_NONE;
    return GetValueType(m_Values[index]);
}

size_t ConfigurationList::GetSize(size_t index) const {
    if (index >= m_Values.size())
        return 0;
    return m_Values[index].GetSize();
}

bool ConfigurationList::GetBool(size_t index) {
    if (index >= m_Values.size())
        return false;
    return m_Values[index].GetBool();
}

uint32_t ConfigurationList::GetUint32(size_t index) {
    if (index >= m_Values.size())
        return 0;
    return m_Values[index].GetUint32();
}

int32_t ConfigurationList::GetInt32(size_t index) {
    if (index >= m_Values.size())
        return 0;
    return m_Values[index].GetInt32();
}

uint64_t ConfigurationList::GetUint64(size_t index) {
    if (index >= m_Values.size())
        return 0;
    return m_Values[index].GetUint64();
}

int64_t ConfigurationList::GetInt64(size_t index) {
    if (index >= m_Values.size())
        return 0;
    return m_Values[index].GetInt64();
}

float ConfigurationList::GetFloat(size_t index) {
    if (index >= m_Values.size())
        return 0;
    return m_Values[index].GetFloat32();
}

double ConfigurationList::GetDouble(size_t index) {
    if (index >= m_Values.size())
        return 0;
    return m_Values[index].GetFloat64();
}

const char *ConfigurationList::GetString(size_t index) const {
    if (index >= m_Values.size())
        return nullptr;
    return m_Values[index].GetString();
}

void ConfigurationList::SetBool(size_t index, bool value) {
    if (index < m_Values.size())
        m_Values[index] = value;
}

void ConfigurationList::SetUint32(size_t index, uint32_t value) {
    if (index < m_Values.size())
        m_Values[index] = value;
}

void ConfigurationList::SetInt32(size_t index, int32_t value) {
    if (index < m_Values.size())
        m_Values[index] = value;
}

void ConfigurationList::SetUint64(size_t index, uint64_t value) {
    if (index < m_Values.size())
        m_Values[index] = value;
}

void ConfigurationList::SetInt64(size_t index, int64_t value) {
    if (index < m_Values.size())
        m_Values[index] = value;
}

void ConfigurationList::SetFloat(size_t index, float value) {
    if (index < m_Values.size())
        m_Values[index] = value;
}

void ConfigurationList::SetDouble(size_t index, double value) {
    if (index < m_Values.size())
        m_Values[index] = value;
}

void ConfigurationList::SetString(size_t index, const char *value) {
    if (index < m_Values.size())
        m_Values[index] = value;
}

void ConfigurationList::InsertBool(size_t index, bool value) {
    if (index < m_Values.size())
        m_Values.insert(m_Values.begin() + static_cast<int>(index), value);
}

void ConfigurationList::InsertUint32(size_t index, uint32_t value) {
    if (index < m_Values.size())
        m_Values.insert(m_Values.begin() + static_cast<int>(index), value);
}

void ConfigurationList::InsertInt32(size_t index, int32_t value) {
    if (index < m_Values.size())
        m_Values.insert(m_Values.begin() + static_cast<int>(index), value);
}

void ConfigurationList::InsertUint64(size_t index, uint64_t value) {
    if (index < m_Values.size())
        m_Values.insert(m_Values.begin() + static_cast<int>(index), value);
}

void ConfigurationList::InsertInt64(size_t index, int64_t value) {
    if (index < m_Values.size())
        m_Values.insert(m_Values.begin() + static_cast<int>(index), value);
}

void ConfigurationList::InsertFloat(size_t index, float value) {
    if (index < m_Values.size())
        m_Values.insert(m_Values.begin() + static_cast<int>(index), value);
}

void ConfigurationList::InsertDouble(size_t index, double value) {
    if (index < m_Values.size())
        m_Values.insert(m_Values.begin() + static_cast<int>(index), value);
}

void ConfigurationList::InsertString(size_t index, const char *value) {
    if (index < m_Values.size())
        m_Values.insert(m_Values.begin() + static_cast<int>(index), value);
}

void ConfigurationList::AppendBool(bool value) {
    m_Values.emplace_back(value);
}

void ConfigurationList::AppendUint32(uint32_t value) {
    m_Values.emplace_back(value);
}

void ConfigurationList::AppendInt32(int32_t value) {
    m_Values.emplace_back(value);
}

void ConfigurationList::AppendUint64(uint64_t value) {
    m_Values.emplace_back(value);
}

void ConfigurationList::AppendInt64(int64_t value) {
    m_Values.emplace_back(value);
}

void ConfigurationList::AppendFloat(float value) {
    m_Values.emplace_back(value);
}

void ConfigurationList::AppendDouble(double value) {
    m_Values.emplace_back(value);
}

void ConfigurationList::AppendString(const char *value) {
    m_Values.emplace_back(value);
}

bool ConfigurationList::Remove(size_t index) {
    if (index == -1) {
        m_Values.pop_back();
        return true;
    }

    if (index < m_Values.size()) {
        m_Values.erase(m_Values.begin() + static_cast<int>(index));
        return true;
    }
    return false;
}

void ConfigurationList::Clear() {
    m_Values.clear();
}

void ConfigurationList::Resize(size_t size) {
    m_Values.resize(size);
}

void ConfigurationList::Reserve(size_t size) {
    m_Values.reserve(size);
}

yyjson_mut_val *ConfigurationList::ToJsonKey(yyjson_mut_doc *doc) {
    if (!doc)
        return nullptr;
    return yyjson_mut_str(doc, m_Name.c_str());
}

yyjson_mut_val *ConfigurationList::ToJsonArray(yyjson_mut_doc *doc) {
    if (!doc)
        return nullptr;

    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    for (const auto &value : m_Values) {
        switch (GetValueType(value)) {
            case CFG_ENTRY_NONE:
                yyjson_mut_arr_add_null(doc, arr);
            case CFG_ENTRY_BOOL:
                yyjson_mut_arr_add_bool(doc, arr, value.GetBool());
            case CFG_ENTRY_UINT:
                yyjson_mut_arr_add_uint(doc, arr, value.GetUint64());
            case CFG_ENTRY_INT:
                yyjson_mut_arr_add_int(doc, arr, value.GetInt64());
            case CFG_ENTRY_REAL:
                yyjson_mut_arr_add_real(doc, arr, value.GetFloat64());
            case CFG_ENTRY_STR:
                yyjson_mut_arr_add_str(doc, arr, value.GetString());
            default:
                return nullptr;
        }
    }

    return arr;
}

ConfigurationEntry::ConfigurationEntry(ConfigurationSection *parent, const char *name)
    : m_Parent(parent), m_Name(name) {
    assert(name != nullptr);
}

ConfigurationEntry::ConfigurationEntry(ConfigurationSection *parent, const char *name, bool value)
    : m_Parent(parent), m_Name(name), m_Value(value) {
    assert(name != nullptr);
}

ConfigurationEntry::ConfigurationEntry(ConfigurationSection *parent, const char *name, uint32_t value)
    : m_Parent(parent), m_Name(name), m_Value(static_cast<uint64_t>(value)) {
    assert(name != nullptr);
}

ConfigurationEntry::ConfigurationEntry(ConfigurationSection *parent, const char *name, int32_t value)
    : m_Parent(parent), m_Name(name), m_Value(static_cast<int64_t>(value)) {
    assert(name != nullptr);
}

ConfigurationEntry::ConfigurationEntry(ConfigurationSection *parent, const char *name, uint64_t value)
    : m_Parent(parent), m_Name(name), m_Value(value) {
    assert(name != nullptr);
}

ConfigurationEntry::ConfigurationEntry(ConfigurationSection *parent, const char *name, int64_t value)
    : m_Parent(parent), m_Name(name), m_Value(value) {
    assert(name != nullptr);
}

ConfigurationEntry::ConfigurationEntry(ConfigurationSection *parent, const char *name, float value)
    : m_Parent(parent), m_Name(name), m_Value(static_cast<double>(value)) {
    assert(name != nullptr);
}

ConfigurationEntry::ConfigurationEntry(ConfigurationSection *parent, const char *name, double value)
    : m_Parent(parent), m_Name(name), m_Value(value) {
    assert(name != nullptr);
}

ConfigurationEntry::ConfigurationEntry(ConfigurationSection *parent, const char *name, const char *value)
    : m_Parent(parent), m_Name(name), m_Value(value) {
    assert(name != nullptr);
}

ConfigurationEntry::~ConfigurationEntry() {
    if (m_Parent) {
        m_Parent->RemoveEntry(m_Name.c_str());
    }
}

int ConfigurationEntry::AddRef() const {
    return m_RefCount.AddRef();
}

int ConfigurationEntry::Release() const {
    int r = m_RefCount.Release();
    if (r == 0) {
        std::atomic_thread_fence(std::memory_order_acquire);
        delete const_cast<ConfigurationEntry *>(this);
    }
    return r;
}

ConfigurationEntryType ConfigurationEntry::GetType() const {
    return GetValueType(m_Value);
}

size_t ConfigurationEntry::GetSize() const {
    return m_Value.GetSize();
}

void ConfigurationEntry::SetBool(bool value) {
    std::lock_guard<std::mutex> guard(m_RWLock);

    bool v = false;
    bool t = false;

    if (GetType() != CFG_ENTRY_BOOL) {
        t = true;
    }

    if (m_Value != value) {
        v = true;
    }

    if (t || v) {
        m_Value = value;
    }

    InvokeCallbacks(t, v);
}

void ConfigurationEntry::SetUint32(uint32_t value) {
    std::lock_guard<std::mutex> guard(m_RWLock);

    bool v = false;
    bool t = false;

    if (GetType() != CFG_ENTRY_UINT) {
        t = true;
    }

    if (m_Value != value) {
        v = true;
    }

    if (t || v) {
        m_Value = value;
    }

    InvokeCallbacks(t, v);
}

void ConfigurationEntry::SetInt32(int32_t value) {
    std::lock_guard<std::mutex> guard(m_RWLock);

    bool v = false;
    bool t = false;

    if (GetType() != CFG_ENTRY_INT) {
        t = true;
    }

    if (m_Value != value) {
        v = true;
    }

    if (t || v) {
        m_Value = value;
    }

    InvokeCallbacks(t, v);
}

void ConfigurationEntry::SetUint64(uint64_t value) {
    std::lock_guard<std::mutex> guard(m_RWLock);

    bool v = false;
    bool t = false;

    if (GetType() != CFG_ENTRY_UINT) {
        t = true;
    }

    if (m_Value != value) {
        v = true;
    }

    if (t || v) {
        m_Value = value;
    }

    InvokeCallbacks(t, v);
}

void ConfigurationEntry::SetInt64(int64_t value) {
    std::lock_guard<std::mutex> guard(m_RWLock);

    bool v = false;
    bool t = false;

    if (GetType() != CFG_ENTRY_INT) {
        t = true;
    }

    if (m_Value != value) {
        v = true;
    }

    if (t || v) {
        m_Value = value;
    }

    InvokeCallbacks(t, v);
}

void ConfigurationEntry::SetFloat(float value) {
    std::lock_guard<std::mutex> guard(m_RWLock);

    bool v = false;
    bool t = false;

    if (GetType() != CFG_ENTRY_REAL) {
        t = true;
    }

    if (m_Value != value) {
        v = true;
    }

    if (t || v) {
        m_Value = value;
    }

    InvokeCallbacks(t, v);
}

void ConfigurationEntry::SetDouble(double value) {
    std::lock_guard<std::mutex> guard(m_RWLock);

    bool v = false;
    bool t = false;

    if (GetType() != CFG_ENTRY_REAL) {
        t = true;
    }

    if (m_Value != value) {
        v = true;
    }

    if (t || v) {
        m_Value = value;
    }

    InvokeCallbacks(t, v);
}

void ConfigurationEntry::SetString(const char *value) {
    if (!value)
        return;

    std::lock_guard<std::mutex> guard(m_RWLock);

    bool v = false;
    bool t = false;

    if (GetType() != CFG_ENTRY_REAL) {
        t = true;
    }

    if (m_Value != value) {
        v = true;
    }

    if (t || v) {
        m_Value = value;
        m_Hash = utils::HashString(value);
    }

    InvokeCallbacks(t, v);
}

void ConfigurationEntry::SetDefaultBool(bool value) {
    std::lock_guard<std::mutex> guard(m_RWLock);
    if (GetType() != CFG_ENTRY_BOOL) {
        m_Value = value;
    }
}

void ConfigurationEntry::SetDefaultUint32(uint32_t value) {
    std::lock_guard<std::mutex> guard(m_RWLock);
    if (GetType() != CFG_ENTRY_UINT) {
        m_Value = value;
    }
}

void ConfigurationEntry::SetDefaultInt32(int32_t value) {
    std::lock_guard<std::mutex> guard(m_RWLock);
    if (GetType() != CFG_ENTRY_INT) {
        m_Value = value;
    }
}

void ConfigurationEntry::SetDefaultUint64(uint64_t value) {
    std::lock_guard<std::mutex> guard(m_RWLock);
    if (GetType() != CFG_ENTRY_UINT) {
        m_Value = value;
    }
}

void ConfigurationEntry::SetDefaultInt64(int64_t value) {
    std::lock_guard<std::mutex> guard(m_RWLock);
    if (GetType() != CFG_ENTRY_INT) {
        m_Value = value;
    }
}

void ConfigurationEntry::SetDefaultFloat(float value) {
    std::lock_guard<std::mutex> guard(m_RWLock);
    if (GetType() != CFG_ENTRY_REAL) {
        m_Value = value;
    }
}

void ConfigurationEntry::SetDefaultDouble(double value) {
    std::lock_guard<std::mutex> guard(m_RWLock);
    if (GetType() != CFG_ENTRY_REAL) {
        m_Value = value;
    }
}

void ConfigurationEntry::SetDefaultString(const char *value) {
    std::lock_guard<std::mutex> guard(m_RWLock);
    if (value && GetType() != CFG_ENTRY_STR) {
        m_Value = value;
    }
}

void ConfigurationEntry::CopyValue(IConfigurationEntry *entry) {
    std::lock_guard<std::mutex> guard(m_RWLock);
    switch (entry->GetType()) {
        case CFG_ENTRY_BOOL:
            SetBool(entry->GetBool());
            break;
        case CFG_ENTRY_UINT:
            if (GetSize() == sizeof(uint32_t))
                SetUint32(entry->GetUint32());
            else
                SetUint64(entry->GetUint64());
            break;
        case CFG_ENTRY_INT:
            if (GetSize() == sizeof(int32_t))
                SetInt32(entry->GetInt32());
            else
                SetInt64(entry->GetInt64());
            break;
        case CFG_ENTRY_REAL:
            if (GetSize() == sizeof(float))
                SetFloat(entry->GetFloat());
            else
                SetDouble(entry->GetDouble());
            break;
        case CFG_ENTRY_STR:
            SetString(entry->GetString());
            break;
        default:
            break;
    }
}

yyjson_mut_val *ConfigurationEntry::ToJsonKey(yyjson_mut_doc *doc) {
    if (!doc)
        return nullptr;
    return yyjson_mut_str(doc, m_Name.c_str());
}

yyjson_mut_val *ConfigurationEntry::ToJsonValue(yyjson_mut_doc *doc) {
    if (!doc)
        return nullptr;

    switch (GetType()) {
        case CFG_ENTRY_NONE:
            return yyjson_mut_null(doc);
        case CFG_ENTRY_BOOL:
            return yyjson_mut_bool(doc, GetBool());
        case CFG_ENTRY_UINT:
            return yyjson_mut_uint(doc, GetUint64());
        case CFG_ENTRY_INT:
            return yyjson_mut_int(doc, GetInt64());
        case CFG_ENTRY_REAL:
            return yyjson_mut_real(doc, GetDouble());
        case CFG_ENTRY_STR:
            return yyjson_mut_str(doc, GetString());
        default:
            return nullptr;
    }
}

void ConfigurationEntry::InvokeCallbacks(bool typeChanged, bool valueChanged) {
    if (!m_Parent)
        return;

    if (typeChanged) {
        m_Parent->InvokeCallbacks(CFG_CB_ENTRY_TYPE_CHANGE, ConfigurationItem(this));
    }

    if (valueChanged) {
        m_Parent->InvokeCallbacks(CFG_CB_ENTRY_VALUE_CHANGE, ConfigurationItem(this));
    }
}
