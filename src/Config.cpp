#include "Config.h"

#include <fstream>
#include <utility>

#include "ModLoader.h"

Config::Config(IMod *mod) : m_Mod(mod), m_ModID(mod->GetID()) {}

Config::~Config() {
    for (Category &cate: m_Data) {
        for (Property *prop: cate.props)
            delete prop;
    }
}

bool Config::Load(const char *path) {
    if (!path || path[0] == '\0')
        return false;
        
    std::ifstream fin(path);
    if (fin.fail())
        return false;

    std::string token, comment, category;
    bool inCate = false;
    while (fin >> token) {
        if (token == "#") {
            std::getline(fin, comment);
            Trim(comment);
        } else if (token == "{")
            inCate = true;
        else if (token == "}")
            inCate = false;
        else if (inCate) {
            std::string propName;
            fin >> propName;
            Property *prop = new Property(nullptr, category, propName);
            switch (token[0]) {
                case 'S': {
                    std::string value;
                    std::getline(fin, value);
                    Trim(value);
                    prop->SetDefaultString(value.c_str());
                    break;
                }
                case 'B': {
                    bool value;
                    fin >> value;
                    prop->SetDefaultBoolean(value);
                    break;
                }
                case 'K': {
                    int value;
                    fin >> value;
                    prop->SetDefaultKey(CKKEYBOARD(value));
                    break;
                }
                case 'I': {
                    int value;
                    fin >> value;
                    prop->SetDefaultInteger(value);
                    break;
                }
                case 'F': {
                    float value;
                    fin >> value;
                    prop->SetDefaultFloat(value);
                    break;
                }
            }
            prop->SetComment(comment.c_str());
            GetCategory(category.c_str()).props.push_back(prop);
        } else {
            category = token;
            GetCategory(category.c_str()).comment = comment;
            comment.clear();
        }
    }

    return true;
}

bool Config::Save(const char *path) {
    if (!path || path[0] == '\0')
        return false;

    std::ofstream fout(path);
    if (fout.fail())
        return false;

    for (auto &category: m_Data) {
        for (auto iter = category.props.begin(); iter != category.props.end();) {
            if (!(*iter)->m_Config) {
                delete (*iter);
                iter = category.props.erase(iter);
            } else
                iter++;
        }
    }

    for (auto iter = m_Data.begin(); iter != m_Data.end();) {
        if (iter->props.empty())
            iter = m_Data.erase(iter);
        else
            iter++;
    }

    fout << "# Configuration File for Mod: " << m_Mod->GetName() << " - " << m_Mod->GetVersion() << std::endl
         << std::endl;
    for (auto &category: m_Data) {
        fout << "# " << category.comment << std::endl;
        fout << category.name << " {" << std::endl
             << std::endl;

        for (auto property: category.props) {
            fout << "\t# " << property->GetComment() << std::endl;
            fout << "\t";
            switch (property->GetType()) {
                case IProperty::STRING:
                    fout << "S ";
                    break;
                case IProperty::BOOLEAN:
                    fout << "B ";
                    break;
                case IProperty::FLOAT:
                    fout << "F ";
                    break;
                case IProperty::KEY:
                    fout << "K ";
                    break;
                case IProperty::INTEGER:
                default:
                    fout << "I ";
                    break;
            }

            fout << property->m_Key << " ";
            switch (property->GetType()) {
                case IProperty::STRING:
                    fout << property->GetString();
                    break;
                case IProperty::BOOLEAN:
                    fout << property->GetBoolean();
                    break;
                case IProperty::FLOAT:
                    fout << property->GetFloat();
                    break;
                case IProperty::KEY:
                    fout << (int) property->GetKey();
                    break;
                case IProperty::INTEGER:
                default:
                    fout << property->GetInteger();
                    break;
            }

            fout << std::endl
                 << std::endl;
        }

        fout << "}" << std::endl
             << std::endl;
    }

    return true;
}

bool Config::HasCategory(const char *category) {
    for (Category &cate: m_Data)
        if (cate.name == category)
            return true;
    return false;
}

bool Config::HasKey(const char *category, const char *key) {
    if (HasCategory(category))
        for (Property *prop: GetCategory(category).props)
            if (prop->m_Key == key)
                return true;
    return false;
}

IProperty *Config::GetProperty(const char *category, const char *key) {
    bool exist = HasKey(category, key);
    Property *prop = GetCategory(category).GetProperty(key);
    prop->m_Config = this;
    if (!exist) {
        prop->m_Type = IProperty::NONE;
        prop->m_Value.m_Int = 0;
        prop->m_Category = category;
        prop->m_Key = key;
    }
    return prop;
}

Config::Category &Config::GetCategory(const char *name) {
    for (Category &cate: m_Data)
        if (cate.name == name)
            return cate;

    Config::Category cate;
    cate.name = name;
    cate.config = this;
    m_Data.push_back(cate);
    return m_Data.back();
}

Property *Config::Category::GetProperty(const char *name) {
    for (Property *prop: props)
        if (prop->m_Key == name)
            return prop;

    auto *prop = new Property(config, this->name, name);
    props.push_back(prop);
    return props.back();
}

const char *Config::GetCategoryComment(const char *category) {
    return GetCategory(category).comment.c_str();
}

void Config::SetCategoryComment(const char *category, const char *comment) {
    GetCategory(category).comment = comment;
}

Property::Property(Config *config, std::string category, std::string key) {
    m_Type = NONE;
    m_Value.m_Int = 0;
    m_Config = config;
    m_Category = std::move(category);
    m_Key = std::move(key);
}

const char *Property::GetString() {
    return m_Type == STRING ? m_String.c_str() : "";
}

bool Property::GetBoolean() {
    return m_Type == BOOLEAN && m_Value.m_Bool;
}

int Property::GetInteger() {
    return m_Type == INTEGER ? m_Value.m_Int : 0;
}

float Property::GetFloat() {
    return m_Type == FLOAT ? m_Value.m_Float : 0.0f;
}

CKKEYBOARD Property::GetKey() {
    return m_Type == KEY ? m_Value.m_Key : (CKKEYBOARD) 0;
}

void Property::SetString(const char *value) {
    if (m_Type != STRING || m_String != value) {
        m_String = value;
        m_Type = STRING;
        if (m_Config)
            ModLoader::GetInstance().OnModifyConfig(m_Config->m_Mod, m_Category.c_str(), m_Key.c_str(), this);
    }
}

void Property::SetBoolean(bool value) {
    if (m_Type != BOOLEAN || m_Value.m_Bool != value) {
        m_Value.m_Bool = value;
        m_Type = BOOLEAN;
        if (m_Config)
            ModLoader::GetInstance().OnModifyConfig(m_Config->m_Mod, m_Category.c_str(), m_Key.c_str(), this);
    }
}

void Property::SetInteger(int value) {
    if (m_Type != INTEGER || m_Value.m_Int != value) {
        m_Value.m_Int = value;
        m_Type = INTEGER;
        if (m_Config)
            ModLoader::GetInstance().OnModifyConfig(m_Config->m_Mod, m_Category.c_str(), m_Key.c_str(), this);
    }
}

void Property::SetFloat(float value) {
    if (m_Type != FLOAT || m_Value.m_Float != value) {
        m_Value.m_Float = value;
        m_Type = FLOAT;
        if (m_Config)
            ModLoader::GetInstance().OnModifyConfig(m_Config->m_Mod, m_Category.c_str(), m_Key.c_str(), this);
    }
}

void Property::SetKey(CKKEYBOARD value) {
    if (m_Type != KEY || m_Value.m_Key != value) {
        m_Value.m_Key = value;
        m_Type = KEY;
        if (m_Config)
            ModLoader::GetInstance().OnModifyConfig(m_Config->m_Mod, m_Category.c_str(), m_Key.c_str(), this);
    }
}

const char *Property::GetComment() {
    return m_Comment.c_str();
}

void Property::SetComment(const char *comment) {
    m_Comment = comment;
}

void Property::SetDefaultString(const char *value) {
    if (m_Type != STRING) {
        m_Type = STRING;
        m_String = value;
    }
}

void Property::SetDefaultBoolean(bool value) {
    if (m_Type != BOOLEAN) {
        m_Type = BOOLEAN;
        m_Value.m_Bool = value;
    }
}

void Property::SetDefaultInteger(int value) {
    if (m_Type != INTEGER) {
        m_Type = INTEGER;
        m_Value.m_Int = value;
    }
}

void Property::SetDefaultFloat(float value) {
    if (m_Type != FLOAT) {
        m_Type = FLOAT;
        m_Value.m_Float = value;
    }
}

void Property::SetDefaultKey(CKKEYBOARD value) {
    if (m_Type != KEY) {
        m_Type = KEY;
        m_Value.m_Key = value;
    }
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