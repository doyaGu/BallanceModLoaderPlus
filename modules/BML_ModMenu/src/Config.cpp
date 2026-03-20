#include "Config.h"

namespace {
void ConfigEnumCallback(BML_Context, const BML_ConfigKey *key, const BML_ConfigValue *value, void *userData) {
    if (!key || !value || !userData) {
        return;
    }

    auto *config = static_cast<Config *>(userData);
    auto *category = config->GetCategory(key->category ? key->category : "");
    if (!category) {
        return;
    }

    auto *property = category->GetProperty(key->name ? key->name : "");
    if (!property) {
        return;
    }

    switch (value->type) {
    case BML_CONFIG_BOOL:
        property->SetDefaultBoolean(value->data.bool_value != 0);
        break;
    case BML_CONFIG_INT:
        property->SetDefaultInteger(value->data.int_value);
        break;
    case BML_CONFIG_FLOAT:
        property->SetDefaultFloat(value->data.float_value);
        break;
    case BML_CONFIG_STRING:
        property->SetDefaultString(value->data.string_value ? value->data.string_value : "");
        break;
    default:
        break;
    }
}
} // namespace

Property::Property(BML_Mod mod,
                   const BML_CoreConfigInterface *config,
                   std::string category,
                   std::string key)
    : m_Mod(mod),
      m_Config(config),
      m_Category(std::move(category)),
      m_Key(std::move(key)) {}

const char *Property::GetString() {
    return m_Type == STRING ? std::get<std::string>(m_Value).c_str() : "";
}

bool Property::GetBoolean() {
    return m_Type == BOOLEAN ? std::get<bool>(m_Value) : false;
}

int Property::GetInteger() {
    return (m_Type == INTEGER || m_Type == KEY) ? std::get<int>(m_Value) : 0;
}

float Property::GetFloat() {
    return m_Type == FLOAT ? std::get<float>(m_Value) : 0.0f;
}

CKKEYBOARD Property::GetKey() {
    return m_Type == KEY ? static_cast<CKKEYBOARD>(std::get<int>(m_Value)) : static_cast<CKKEYBOARD>(0);
}

bool Property::CommitValue() {
    if (!m_Mod || !m_Config || !m_Config->Set) {
        return false;
    }

    BML_ConfigKey key = BML_CONFIG_KEY_INIT(m_Category.c_str(), m_Key.c_str());
    BML_ConfigValue value{};
    value.struct_size = sizeof(BML_ConfigValue);

    switch (m_Type) {
    case STRING:
        value.type = BML_CONFIG_STRING;
        value.data.string_value = std::get<std::string>(m_Value).c_str();
        break;
    case BOOLEAN:
        value.type = BML_CONFIG_BOOL;
        value.data.bool_value = std::get<bool>(m_Value) ? BML_TRUE : BML_FALSE;
        break;
    case FLOAT:
        value.type = BML_CONFIG_FLOAT;
        value.data.float_value = std::get<float>(m_Value);
        break;
    case KEY:
    case INTEGER:
        value.type = BML_CONFIG_INT;
        value.data.int_value = std::get<int>(m_Value);
        break;
    case NONE:
    default:
        return false;
    }

    return m_Config->Set(m_Mod, &key, &value) == BML_RESULT_OK;
}

void Property::SetString(const char *value) {
    m_Type = STRING;
    m_Value = std::string(value ? value : "");
    CommitValue();
}

void Property::SetBoolean(bool value) {
    m_Type = BOOLEAN;
    m_Value = value;
    CommitValue();
}

void Property::SetInteger(int value) {
    m_Type = INTEGER;
    m_Value = value;
    CommitValue();
}

void Property::SetFloat(float value) {
    m_Type = FLOAT;
    m_Value = value;
    CommitValue();
}

void Property::SetKey(CKKEYBOARD value) {
    m_Type = KEY;
    m_Value = static_cast<int>(value);
    CommitValue();
}

void Property::SetDefaultString(const char *value) {
    if (m_Type == NONE) {
        m_Type = STRING;
        m_Value = std::string(value ? value : "");
    }
}

void Property::SetDefaultBoolean(bool value) {
    if (m_Type == NONE) {
        m_Type = BOOLEAN;
        m_Value = value;
    }
}

void Property::SetDefaultInteger(int value) {
    if (m_Type == NONE) {
        m_Type = INTEGER;
        m_Value = value;
    }
}

void Property::SetDefaultFloat(float value) {
    if (m_Type == NONE) {
        m_Type = FLOAT;
        m_Value = value;
    }
}

void Property::SetDefaultKey(CKKEYBOARD value) {
    if (m_Type == NONE) {
        m_Type = KEY;
        m_Value = static_cast<int>(value);
    }
}

Category::Category(BML_Mod mod, const BML_CoreConfigInterface *config, std::string name)
    : m_Mod(mod),
      m_Config(config),
      m_Name(std::move(name)) {}

Category::~Category() {
    for (auto *property : m_Properties) {
        delete property;
    }
}

Property *Category::GetProperty(size_t index) {
    return index < m_Properties.size() ? m_Properties[index] : nullptr;
}

Property *Category::GetProperty(const char *key) {
    if (!key) {
        return nullptr;
    }

    auto it = m_PropertyMap.find(key);
    if (it != m_PropertyMap.end()) {
        return it->second;
    }

    auto *property = new Property(m_Mod, m_Config, m_Name, key);
    m_Properties.push_back(property);
    m_PropertyMap[key] = property;
    return property;
}

bool Category::HasKey(const char *key) const {
    if (!key) {
        return false;
    }
    return m_PropertyMap.find(key) != m_PropertyMap.end();
}

Config::Config(BML_Mod mod, const BML_CoreConfigInterface *config)
    : m_Mod(mod),
      m_Config(config) {
    Reload();
}

Config::~Config() {
    Clear();
}

void Config::Clear() {
    for (auto *category : m_Categories) {
        delete category;
    }
    m_Categories.clear();
    m_CategoryMap.clear();
}

Category *Config::GetCategory(size_t index) {
    return index < m_Categories.size() ? m_Categories[index] : nullptr;
}

Category *Config::GetCategory(const char *name) {
    if (!name) {
        return nullptr;
    }

    auto it = m_CategoryMap.find(name);
    if (it != m_CategoryMap.end()) {
        return it->second;
    }

    auto *category = new Category(m_Mod, m_Config, name);
    m_Categories.push_back(category);
    m_CategoryMap[name] = category;
    return category;
}

void Config::Reload() {
    Clear();
    if (!m_Mod || !m_Config || !m_Config->Enumerate) {
        return;
    }
    m_Config->Enumerate(m_Mod, ConfigEnumCallback, this);
}
