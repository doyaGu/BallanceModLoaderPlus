#include "Config.h"

#include <cstdio>
#include <sstream>
#include <utility>

#include "StringUtils.h"

Config::Config(IMod *mod) : m_Mod(mod), m_ModID(mod->GetID()) {}

Config::~Config() {
    for (Category *cate: m_Categories)
        delete cate;
    m_Categories.clear();
}

bool Config::Load(const wchar_t *path) {
    if (!path || path[0] == '\0')
        return false;

    FILE *fp = _wfopen(path, L"rb");
    if (!fp)
        return false;

    fseek(fp, 0, SEEK_END);
    size_t sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    auto *buf = new char[sz + 1];

    if (fread(buf, sizeof(char), sz, fp) != sz) {
        delete[] buf;
        fclose(fp);
        return false;
    }
    fclose(fp);
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
        } else if (wToken == L"{")
            inCate = true;
        else if (wToken == L"}")
            inCate = false;
        else if (inCate) {
            std::wstring wPropName;
            in >> wPropName;
            std::string propName = utils::Utf16ToUtf8(wPropName);
            auto *prop = new Property(nullptr, category, propName);
            switch (wToken[0]) {
                case 'S': {
                    std::wstring wValue;
                    std::getline(in, wValue);
                    utils::TrimString(wValue);
                    std::string value = utils::Utf16ToUtf8(wValue);
                    prop->SetDefaultString(value.c_str());
                    break;
                }
                case 'B': {
                    bool value;
                    in >> value;
                    prop->SetDefaultBoolean(value);
                    break;
                }
                case 'K': {
                    int value;
                    in >> value;
                    prop->SetDefaultKey(CKKEYBOARD(value));
                    break;
                }
                case 'I': {
                    int value;
                    in >> value;
                    prop->SetDefaultInteger(value);
                    break;
                }
                case 'F': {
                    float value;
                    in >> value;
                    prop->SetDefaultFloat(value);
                    break;
                }
            }
            prop->SetComment(comment.c_str());
            GetCategory(category.c_str())->m_Properties.push_back(prop);
        } else {
            wCategory = wToken;
            category = utils::Utf16ToUtf8(wCategory);

            GetCategory(category.c_str())->m_Comment = comment;
            wComment.clear();
        }
    }

    return true;
}

bool Config::Save(const wchar_t *path) {
    if (!path || path[0] == '\0')
        return false;

    FILE *fp = _wfopen(path, L"wb");
    if (!fp)
        return false;

    std::ostringstream out;

    for (auto *category: m_Categories) {
        auto &props = category->m_Properties;
        for (auto iter = props.begin(); iter != props.end();) {
            if (!(*iter)->m_Config) {
                delete (*iter);
                iter = props.erase(iter);
            } else
                iter++;
        }
    }

    out << "# Configuration File for Mod: " << m_Mod->GetName()
        << " - " << m_Mod->GetVersion() << std::endl << std::endl;
    for (auto *category: m_Categories) {
        if (category->GetPropertyCount() == 0)
            continue;

        out << "# " << category->m_Comment << std::endl;
        out << category->m_Name << " {" << std::endl << std::endl;

        for (auto property: category->m_Properties) {
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
                    out << (int) property->GetKey();
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
    if (fwrite(buf.c_str(), sizeof(char), buf.size(), fp) != buf.size()) {
        fclose(fp);
        return false;
    }

    fclose(fp);
    return true;
}

bool Config::HasCategory(const char *category) {
    if (!category)
        return false;

    for (Category *cate: m_Categories)
        if (cate->m_Name == category)
            return true;
    return false;
}

bool Config::HasKey(const char *category, const char *key) {
    if (!category || !key)
        return false;

    auto *cate = GetCategory(category);
    return cate->HasKey(key);
}

IProperty *Config::GetProperty(const char *category, const char *key) {
    if (!category || !key)
        return nullptr;

    auto *cate = GetCategory(category);
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
    for (Category *cate: m_Categories)
        if (cate->m_Name == n)
            return cate;

    auto *cate = new Category(this, name);
    m_Categories.push_back(cate);
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

Category::Category(Config *config, std::string name)
        : m_Config(config), m_Name(std::move(name)) {}

Category::~Category() {
    for (Property *prop: m_Properties)
        delete prop;
    m_Properties.clear();
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
    for (Property *prop: m_Properties)
        if (prop->m_Key == k)
            return prop;

    auto *prop = new Property(m_Config, m_Name, k);
    m_Properties.push_back(prop);
    return prop;
}

bool Category::HasKey(const char *key) {
    if (!key)
        return false;

    for (Property *prop: m_Properties)
        if (prop->m_Key == key)
            return true;

    return false;
}

Property::Property(Config *config, std::string category, std::string key) {
    m_Type = NONE;
    m_Value = 0;
    m_Config = config;
    m_Category = std::move(category);
    m_Key = std::move(key);
}

const char *Property::GetString() {
    return m_Type == STRING ? m_Value.GetString() : "";
}

bool Property::GetBoolean() {
    return m_Type == BOOLEAN && m_Value.GetBool();
}

int Property::GetInteger() {
    return m_Type == INTEGER ? m_Value.GetInt32() : 0;
}

float Property::GetFloat() {
    return m_Type == FLOAT ? m_Value.GetFloat32() : 0.0f;
}

CKKEYBOARD Property::GetKey() {
    return m_Type == KEY ? (CKKEYBOARD) m_Value.GetInt32() : (CKKEYBOARD) 0;
}

void Property::SetString(const char *value) {
    if (m_Type != STRING || m_Value != value) {
        m_Value = value;
        m_Type = STRING;
        m_Hash = utils::HashString(value);
        SetModified();
    }
}

void Property::SetBoolean(bool value) {
    if (m_Type != BOOLEAN || m_Value != value) {
        m_Value = value;
        m_Type = BOOLEAN;
        SetModified();
    }
}

void Property::SetInteger(int value) {
    if (m_Type != INTEGER || m_Value != value) {
        m_Value = value;
        m_Type = INTEGER;
        SetModified();
    }
}

void Property::SetFloat(float value) {
    if (m_Type != FLOAT || m_Value != value) {
        m_Value = value;
        m_Type = FLOAT;
        SetModified();
    }
}

void Property::SetKey(CKKEYBOARD value) {
    if (m_Type != KEY || m_Value != static_cast<int32_t>(value)) {
        m_Value = static_cast<int32_t>(value);
        m_Type = KEY;
        SetModified();
    }
}

void Property::SetDefaultString(const char *value) {
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
        m_Value = value;
    }
}

size_t Property::GetHash() {
    if (m_Type == STRING)
        return m_Hash;
    else
        return m_Value.GetValue().u32;
}

void Property::CopyValue(Property *o) {
    m_Type = o->GetType();
    switch (m_Type) {
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
        default:
            break;
    }
}

size_t Property::GetStringSize() {
    if (GetType() != STRING)
        return 0;
    return m_Value.GetSize();
}

bool *Property::GetBooleanPtr() {
    if (GetType() != BOOLEAN)
        return nullptr;
    return &m_Value.GetValue().b;
}

int *Property::GetIntegerPtr() {
    if (GetType() != INTEGER)
        return nullptr;
    return &m_Value.GetValue().i32;
}

float *Property::GetFloatPtr() {
    if (GetType() != FLOAT)
        return nullptr;
    return &m_Value.GetValue().f32;
}

CKKEYBOARD *Property::GetKeyPtr() {
    if (GetType() != KEY)
        return nullptr;
    return reinterpret_cast<CKKEYBOARD *>(&m_Value.GetValue().i32);
}

void Property::SetModified() {
    if (m_Config)
        m_Config->m_Mod->OnModifyConfig(m_Category.c_str(), m_Key.c_str(), this);
}