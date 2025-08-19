#include "Config.h"

#include <cstdio>
#include <sstream>
#include <algorithm>

#include "ModContext.h"
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

    FILE *fp = _wfopen(path, L"rb");
    if (!fp)
        return false;

    fseek(fp, 0, SEEK_END);
    size_t sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (sz == 0) {
        fclose(fp);
        return false;
    }

    auto *buf = new char[sz + 1];
    size_t read = fread(buf, sizeof(char), sz, fp);
    fclose(fp);

    if (read != sz) {
        delete[] buf;
        return false;
    }

    buf[sz] = '\0';

    std::wstring wBuf = utils::Utf8ToUtf16(buf);
    delete[] buf;

    std::wistringstream in(wBuf);
    std::wstring wToken, wComment, wCategory;
    std::string comment, category;
    bool inCate = false;

    while (in >> wToken) {
        if (wToken == L"#") {
            std::getline(in, wComment);
            utils::TrimString(wComment);
            comment = utils::Utf16ToUtf8(wComment);
        } else if (wToken == L"{") {
            inCate = true;
        } else if (wToken == L"}") {
            inCate = false;
        } else if (inCate) {
            std::wstring wPropName;
            if (!(in >> wPropName)) break;

            std::string propName = utils::Utf16ToUtf8(wPropName);
            auto *prop = new Property(this, category, propName);

            bool parseSuccess = false;

            switch (wToken[0]) {
            case L'S': {
                std::wstring wValue;
                std::getline(in, wValue);
                utils::TrimString(wValue);
                std::string value = utils::Utf16ToUtf8(wValue);
                prop->SetDefaultString(value.c_str());
                parseSuccess = true;
                break;
            }
            case L'B': {
                bool value;
                if (in >> value) {
                    prop->SetDefaultBoolean(value);
                    parseSuccess = true;
                }
                break;
            }
            case L'K': {
                int value;
                if (in >> value) {
                    prop->SetDefaultKey(static_cast<CKKEYBOARD>(value));
                    parseSuccess = true;
                }
                break;
            }
            case L'I': {
                int value;
                if (in >> value) {
                    prop->SetDefaultInteger(value);
                    parseSuccess = true;
                }
                break;
            }
            case L'F': {
                float value;
                if (in >> value) {
                    prop->SetDefaultFloat(value);
                    parseSuccess = true;
                }
                break;
            }
            default:
                break;
            }

            if (!parseSuccess) {
                delete prop;
                continue;
            }

            prop->SetComment(comment.c_str());

            Category *cate = GetCategory(category.c_str());
            if (cate) {
                cate->m_Properties.push_back(prop);
                cate->m_PropertyMap[propName] = prop;
            } else {
                delete prop;
            }
        } else {
            wCategory = wToken;
            category = utils::Utf16ToUtf8(wCategory);

            Category *cate = GetCategory(category.c_str());
            if (cate) {
                cate->m_Comment = comment;
            }
            comment.clear();
        }
    }

    return true;
}

bool Config::Save(const wchar_t *path) {
    if (!path || path[0] == L'\0')
        return false;

    FILE *fp = _wfopen(path, L"wb");
    if (!fp)
        return false;

    std::ostringstream out;

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

    out << "# Configuration File for Mod: " << (m_Mod ? m_Mod->GetName() : "Unknown")
        << " - " << (m_Mod ? m_Mod->GetVersion() : "Unknown") << std::endl << std::endl;

    for (auto *category : m_Categories) {
        if (!category || category->GetPropertyCount() == 0)
            continue;

        out << "# " << category->m_Comment << std::endl;
        out << category->m_Name << " {" << std::endl << std::endl;

        for (auto *property : category->m_Properties) {
            if (!property) continue;

            out << "\t# " << property->GetComment() << std::endl;
            out << "\t";
            switch (property->GetType()) {
            case IProperty::STRING:
                out << "S ";
                break;
            case IProperty::BOOLEAN:
                out << "B ";
                break;
            case IProperty::FLOAT:
                out << "F ";
                break;
            case IProperty::KEY:
                out << "K ";
                break;
            case IProperty::INTEGER:
            default:
                out << "I ";
                break;
            }

            out << property->m_Key << " ";
            switch (property->GetType()) {
            case IProperty::STRING:
                out << property->GetString();
                break;
            case IProperty::BOOLEAN:
                out << property->GetBoolean();
                break;
            case IProperty::FLOAT:
                out << property->GetFloat();
                break;
            case IProperty::KEY:
                out << static_cast<int>(property->GetKey());
                break;
            case IProperty::INTEGER:
            default:
                out << property->GetInteger();
                break;
            }

            out << std::endl << std::endl;
        }

        out << "}" << std::endl << std::endl;
    }

    std::string buf = out.str();
    bool success = (fwrite(buf.c_str(), sizeof(char), buf.size(), fp) == buf.size());
    fclose(fp);
    return success;
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
        BML_GetModContext()->SaveConfig(m_Config);
    }
}
