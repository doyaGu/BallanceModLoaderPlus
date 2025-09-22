 #include "Config.h"

#include <cstdio>
#include <sstream>
#include <algorithm>

#ifndef BML_TEST
#include "ModContext.h"
#endif
#include "StringUtils.h"

Config::Config(IMod *mod) : m_Mod(mod), m_ModID(mod ? mod->GetID() : "") {}

Config::~Config() {
    for (Category *cate : m_Categories) {
        delete cate;
    }
    m_Categories.clear();
    m_CategoryMap.clear();
}

bool Config::Load(const wchar_t *path) {
    if (!path || path[0] == L'\0')
        return false;

    std::wstring wPath(path);
    if (!m_CfgFile.ParseFromFile(wPath)) {
        return false;
    }

    // Convert CfgFile data to Config's internal structure
    for (const std::string& categoryName : m_CfgFile.GetCategoryNames()) {
        const CfgFile::Category* cfgCategory = m_CfgFile.GetCategory(categoryName);
        if (!cfgCategory) continue;

        Category* cate = GetCategory(categoryName.c_str());
        if (cate) {
            cate->m_Comment = cfgCategory->comment;
        }

        for (const CfgFile::Property& cfgProp : cfgCategory->properties) {
            auto* prop = new Property(this, categoryName, cfgProp.name);
            prop->SetComment(cfgProp.comment.c_str());

            switch (cfgProp.type) {
            case CfgPropertyType::STRING:
                prop->SetDefaultString(cfgProp.GetString().c_str());
                break;
            case CfgPropertyType::BOOLEAN:
                prop->SetDefaultBoolean(cfgProp.GetBoolean());
                break;
            case CfgPropertyType::INTEGER:
                prop->SetDefaultInteger(cfgProp.GetInteger());
                break;
            case CfgPropertyType::FLOAT:
                prop->SetDefaultFloat(cfgProp.GetFloat());
                break;
            case CfgPropertyType::KEY:
                prop->SetDefaultKey(static_cast<CKKEYBOARD>(cfgProp.GetInteger()));
                break;
            default:
                break;
            }

            if (cate) {
                cate->m_Properties.push_back(prop);
                cate->m_PropertyMap[cfgProp.name] = prop;
            } else {
                delete prop;
            }
        }
    }

    return true;
}

bool Config::Save(const wchar_t *path) {
    if (!path || path[0] == L'\0')
        return false;

    // Clean up properties without a config
    for (auto *category : m_Categories) {
        if (!category) continue;

        auto &props = category->m_Properties;
        for (auto it = props.begin(); it != props.end();) {
            if (!(*it) || !(*it)->m_Config) {
                if (*it) {
                    category->m_PropertyMap.erase((*it)->m_Key);
                    delete *it;
                }
                it = props.erase(it);
            } else {
                ++it;
            }
        }
    }

    // Clear and rebuild CfgFile from current Config state
    m_CfgFile.Clear();

    // Add mod header comment
    std::string headerComment = "Configuration File for Mod: " +
        (m_Mod ? std::string(m_Mod->GetName()) : "Unknown") +
        " - " +
        (m_Mod ? std::string(m_Mod->GetVersion()) : "Unknown");

    for (auto *category : m_Categories) {
        if (!category || category->GetPropertyCount() == 0)
            continue;

        CfgFile::Category* cfgCategory = m_CfgFile.AddCategory(category->m_Name);
        if (!cfgCategory) continue;

        cfgCategory->comment = category->m_Comment;

        for (auto *property : category->m_Properties) {
            if (!property) continue;

            CfgPropertyType cfgType;
            switch (property->GetType()) {
            case IProperty::STRING:
                cfgType = CfgPropertyType::STRING;
                break;
            case IProperty::BOOLEAN:
                cfgType = CfgPropertyType::BOOLEAN;
                break;
            case IProperty::FLOAT:
                cfgType = CfgPropertyType::FLOAT;
                break;
            case IProperty::KEY:
                cfgType = CfgPropertyType::KEY;
                break;
            case IProperty::INTEGER:
            default:
                cfgType = CfgPropertyType::INTEGER;
                break;
            }

            CfgFile::Property* cfgProp = cfgCategory->AddProperty(property->m_Key, cfgType);
            if (!cfgProp) continue;

            cfgProp->comment = property->GetComment();

            switch (property->GetType()) {
            case IProperty::STRING:
                cfgProp->SetString(property->GetString());
                break;
            case IProperty::BOOLEAN:
                cfgProp->SetBoolean(property->GetBoolean());
                break;
            case IProperty::FLOAT:
                cfgProp->SetFloat(property->GetFloat());
                break;
            case IProperty::KEY:
                cfgProp->type = CfgPropertyType::KEY;
                cfgProp->value = static_cast<int>(property->GetKey());
                break;
            case IProperty::INTEGER:
            default:
                cfgProp->SetInteger(property->GetInteger());
                break;
            }
        }
    }

    std::wstring wPath(path);
    return m_CfgFile.WriteToFile(wPath);
}

bool Config::HasCategory(const char *category) {
    if (!category)
        return false;

    return m_CategoryMap.find(category) != m_CategoryMap.end();
}

bool Config::HasKey(const char *category, const char *key) {
    if (!category || !key)
        return false;

    auto catIt = m_CategoryMap.find(category);
    if (catIt == m_CategoryMap.end())
        return false;

    return catIt->second->HasKey(key);
}

IProperty *Config::GetProperty(const char *category, const char *key) {
    if (!category || !key)
        return nullptr;

    Category *cate = GetCategory(category);
    bool exist = cate->HasKey(key);
    Property *prop = cate->GetProperty(key);
    prop->m_Config = this;

    if (!exist) {
        prop->m_Type = IProperty::NONE;
        prop->m_Value = 0;
        prop->m_Category = category;
        prop->m_Key = key;
    }

    return prop;
}

Category *Config::GetCategory(size_t i) {
    if (i >= m_Categories.size())
        return nullptr;
    return m_Categories[i];
}

Category *Config::GetCategory(const char *name) {
    if (!name)
        return nullptr;

    std::string n = name;
    auto it = m_CategoryMap.find(n);
    if (it != m_CategoryMap.end())
        return it->second;

    auto *cate = new Category(this, name);
    m_Categories.push_back(cate);
    m_CategoryMap[n] = cate;
    return cate;
}

const char *Config::GetCategoryComment(const char *category) {
    if (!category)
        return nullptr;

    auto *cate = GetCategory(category);
    return cate->GetComment();
}

void Config::SetCategoryComment(const char *category, const char *comment) {
    if (!category)
        return;

    auto *cate = GetCategory(category);
    cate->SetComment(comment);
}

Category::Category(Config *config, std::string name) : m_Config(config), m_Name(std::move(name)) {}

Category::~Category() {
    for (Property *prop : m_Properties) {
        delete prop;
    }
    m_Properties.clear();
    m_PropertyMap.clear();
}

Property *Category::GetProperty(size_t i) {
    if (i >= m_Properties.size())
        return nullptr;
    return m_Properties[i];
}

Property *Category::GetProperty(const char *key) {
    if (!key)
        return nullptr;

    std::string k = key;
    auto it = m_PropertyMap.find(k);
    if (it != m_PropertyMap.end() && it->second) {
        return it->second;
    }

    auto *prop = new Property(m_Config, m_Name, k);
    if (prop) {
        m_Properties.push_back(prop);
        m_PropertyMap[k] = prop;
    }
    return prop;
}

bool Category::HasKey(const char *key) const {
    if (!key)
        return false;
    return m_PropertyMap.find(key) != m_PropertyMap.end();
}

Property::Property(Config *config, std::string category, std::string key) {
    m_Type = NONE;
    m_Value = 0;
    m_Config = config;
    m_Category = std::move(category);
    m_Key = std::move(key);
}

const char *Property::GetString() {
    try {
        return m_Type == STRING ? std::get<std::string>(m_Value).c_str() : "";
    } catch (const std::bad_variant_access &) {
        m_Value = std::string(""); // Reset to empty string
        m_Type = STRING;
        return "";
    }
}

size_t Property::GetStringSize() {
    if (GetType() != STRING)
        return 0;
    return std::get<std::string>(m_Value).size();
}

bool Property::GetBoolean() {
    try {
        return m_Type == BOOLEAN ? std::get<bool>(m_Value) : false;
    } catch (const std::bad_variant_access &) {
        m_Value = false; // Reset to false
        m_Type = BOOLEAN;
        return false;
    }
}

int Property::GetInteger() {
    return m_Type == INTEGER ? std::get<int>(m_Value) : 0;
}

float Property::GetFloat() {
    return m_Type == FLOAT ? std::get<float>(m_Value) : 0.0f;
}

CKKEYBOARD Property::GetKey() {
    return m_Type == KEY ? static_cast<CKKEYBOARD>(std::get<int>(m_Value)) : static_cast<CKKEYBOARD>(0);
}

void Property::SetString(const char *value) {
    if (!value)
        value = "";
    std::string newValue = value;

    if (m_Type != STRING || std::get<std::string>(m_Value) != newValue) {
        m_Value = newValue;
        m_Type = STRING;
        m_Hash = utils::HashString(value);
        SetModified();
    }
}

void Property::SetBoolean(bool value) {
    if (m_Type != BOOLEAN || std::get<bool>(m_Value) != value) {
        m_Value = value;
        m_Type = BOOLEAN;
        SetModified();
    }
}

void Property::SetInteger(int value) {
    if (m_Type != INTEGER || std::get<int>(m_Value) != value) {
        m_Value = value;
        m_Type = INTEGER;
        SetModified();
    }
}

void Property::SetFloat(float value) {
    if (m_Type != FLOAT || std::get<float>(m_Value) != value) {
        m_Value = value;
        m_Type = FLOAT;
        SetModified();
    }
}

void Property::SetKey(CKKEYBOARD value) {
    if (m_Type != KEY || std::get<int>(m_Value) != static_cast<int>(value)) {
        m_Value = static_cast<int>(value);
        m_Type = KEY;
        SetModified();
    }
}

void Property::SetDefaultString(const char *value) {
    if (!value)
        value = "";

    if (m_Type != STRING) {
        m_Type = STRING;
        m_Hash = utils::HashString(value);
        m_Value = value;
    }
}

void Property::SetDefaultBoolean(bool value) {
    if (m_Type != BOOLEAN) {
        m_Type = BOOLEAN;
        m_Value = value;
    }
}

void Property::SetDefaultInteger(int value) {
    if (m_Type != INTEGER) {
        m_Type = INTEGER;
        m_Value = value;
    }
}

void Property::SetDefaultFloat(float value) {
    if (m_Type != FLOAT) {
        m_Type = FLOAT;
        m_Value = value;
    }
}

void Property::SetDefaultKey(CKKEYBOARD value) {
    if (m_Type != KEY) {
        m_Type = KEY;
        m_Value = static_cast<int>(value);
    }
}

size_t Property::GetHash() const {
    if (m_Type == STRING)
        return m_Hash;
    return std::get<int>(m_Value);
}

void Property::CopyValue(Property *o) {
    if (!o) return;

    switch (o->GetType()) {
    case INTEGER:
        SetInteger(o->GetInteger());
        break;
    case FLOAT:
        SetFloat(o->GetFloat());
        break;
    case BOOLEAN:
        SetBoolean(o->GetBoolean());
        break;
    case KEY:
        SetKey(o->GetKey());
        break;
    case STRING:
        SetString(o->GetString());
        break;
    case NONE:
        // Nothing to copy for NONE type
        break;
    default:
        // Handle unexpected type
        m_Type = NONE;
        m_Value = 0;
        break;
    }
}

bool *Property::GetBooleanPtr() {
    if (GetType() != BOOLEAN)
        return nullptr;
    return &std::get<bool>(m_Value);
}

int *Property::GetIntegerPtr() {
    if (GetType() != INTEGER)
        return nullptr;
    return &std::get<int>(m_Value);
}

float *Property::GetFloatPtr() {
    if (GetType() != FLOAT)
        return nullptr;
    return &std::get<float>(m_Value);
}

CKKEYBOARD *Property::GetKeyPtr() {
    if (GetType() != KEY)
        return nullptr;
    return reinterpret_cast<CKKEYBOARD *>(&std::get<int>(m_Value));
}

void Property::SetModified() {
    if (m_Config && m_Config->GetMod()) {
        m_Config->GetMod()->OnModifyConfig(m_Category.c_str(), m_Key.c_str(), this);
#ifndef BML_TEST
        BML_GetModContext()->SaveConfig(m_Config);
#endif
    }
}
