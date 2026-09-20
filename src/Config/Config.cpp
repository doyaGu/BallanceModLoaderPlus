#include "Config/Config.h"

#include <cstdio>
#include <cstring>
#include <iterator>
#include <memory>
#include <sstream>
#include <string_view>
#include <unordered_set>
#include <utility>

#include "StringUtils.h"

namespace {
    struct PreparedConfigEdit {
        Property *property = nullptr;
        Property::Value value = 0;
        std::size_t hash = 0;
    };
}

Config::Config(IMod *mod) : m_Mod(mod) {
    if (mod) {
        const char *id = mod->GetID();
        if (id)
            m_ModID = id;
    }
}

void Config::SnapshotModMetadata() {
    if (!m_Mod)
        return;
    if (const char *name = m_Mod->GetName())
        m_ModName = name;
    if (const char *version = m_Mod->GetVersion())
        m_ModVersion = version;
}

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
    long rawSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (rawSize <= 0) {
        fclose(fp);
        return false;
    }

    const std::size_t size = static_cast<std::size_t>(rawSize);
    std::vector<char> buffer(size + 1);
    const std::size_t read = fread(buffer.data(), sizeof(char), size, fp);
    fclose(fp);

    if (read != size)
        return false;

    buffer[size] = '\0';
    std::wstring wBuf = utils::Utf8ToUtf16(buffer.data());

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
            auto prop = std::make_unique<Property>(nullptr, category, propName);

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
                continue;
            }

            prop->SetComment(comment.c_str());
            comment.clear();

            Category *cate = GetCategory(category.c_str());
            if (cate) {
                Property *target = nullptr;
                const auto existing = cate->m_PropertyMap.find(propName);
                if (existing == cate->m_PropertyMap.end() || !existing->second) {
                    target = cate->GetProperty(propName.c_str());
                } else {
                    target = existing->second;
                    const bool schemaChanged = target->m_Type != prop->m_Type ||
                                               target->m_Comment != prop->m_Comment;
                    const bool valueChanged = !ConfigValuesEqual(
                        prop->m_Type, target->m_Value, prop->m_Value);
                    if (schemaChanged)
                        TouchSchema();
                    else if (valueChanged)
                        TouchValue();
                }

                target->m_Type = prop->m_Type;
                target->m_Value = std::move(prop->m_Value);
                target->m_Hash = prop->m_Hash;
                target->m_Comment = std::move(prop->m_Comment);
            }
        } else {
            wCategory = wToken;
            category = utils::Utf16ToUtf8(wCategory);

            Category *cate = GetCategory(category.c_str());
            if (cate)
                cate->SetComment(comment.c_str());
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
    bool removedProperty = false;
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
                removedProperty = true;
            } else {
                ++it;
            }
        }
    }
    if (removedProperty)
        TouchSchema();

    out << "# Configuration File for Mod: " << m_ModName
        << " - " << m_ModVersion << std::endl << std::endl;

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
    if (success)
        m_Dirty = false;
    return success;
}

std::vector<Config::PendingNotification> Config::TakePendingNotifications() {
    std::vector<PendingNotification> pending;
    pending.swap(m_PendingNotifications);
    return pending;
}

Config::ApplyResult Config::ApplyEdits(const IMod *expectedOwner,
                                       std::uint64_t expectedSchemaRevision,
                                       const std::vector<Edit> &edits) {
    if (expectedOwner != m_Mod)
        return {ApplyError::OwnerChanged};
    if (expectedSchemaRevision != m_SchemaRevision)
        return {ApplyError::SchemaChanged};

    std::vector<PreparedConfigEdit> prepared;
    prepared.reserve(edits.size());
    std::vector<PendingNotification> notifications;
    notifications.reserve(edits.size());
    std::unordered_set<Property *> targets;
    targets.reserve(edits.size());

    for (std::size_t i = 0; i < edits.size(); ++i) {
        const Edit &edit = edits[i];
        const auto category = m_CategoryMap.find(edit.Category);
        if (category == m_CategoryMap.end())
            return {ApplyError::PropertyMissing, i};

        const auto propertyEntry = category->second->m_PropertyMap.find(edit.Key);
        if (propertyEntry == category->second->m_PropertyMap.end() || !propertyEntry->second)
            return {ApplyError::PropertyMissing, i};

        Property *property = propertyEntry->second;
        if (!targets.emplace(property).second)
            return {ApplyError::DuplicateTarget, i};
        if (property->m_Type != edit.ExpectedType)
            return {ApplyError::TypeChanged, i};
        if (!ConfigValueMatchesType(edit.ExpectedType, edit.BaseValue) ||
            !ConfigValueMatchesType(edit.ExpectedType, edit.NewValue)) {
            return {ApplyError::InvalidValue, i};
        }
        if (!ConfigValuesEqual(edit.ExpectedType, property->m_Value, edit.BaseValue))
            return {ApplyError::BaseChanged, i};
        if (ConfigValuesEqual(edit.ExpectedType, property->m_Value, edit.NewValue))
            continue;

        PreparedConfigEdit preparedEdit;
        preparedEdit.property = property;
        preparedEdit.value = edit.NewValue;
        if (edit.ExpectedType == IProperty::STRING)
            preparedEdit.hash = utils::HashString(std::get<std::string>(preparedEdit.value).c_str());
        prepared.push_back(std::move(preparedEdit));

        bool alreadyQueued = false;
        for (const PendingNotification &pending : m_PendingNotifications) {
            if (pending.ChangedProperty == property) {
                alreadyQueued = true;
                break;
            }
        }
        if (!alreadyQueued)
            notifications.push_back({edit.Category, edit.Key, property});
    }

    m_PendingNotifications.reserve(m_PendingNotifications.size() + notifications.size());

    for (PreparedConfigEdit &edit : prepared) {
        edit.property->m_Value.swap(edit.value);
        if (edit.property->m_Type == IProperty::STRING)
            edit.property->m_Hash = edit.hash;
        TouchValue();
    }

    if (!prepared.empty()) {
        MarkDirty();
        m_PendingNotifications.insert(m_PendingNotifications.end(),
                                      std::make_move_iterator(notifications.begin()),
                                      std::make_move_iterator(notifications.end()));
    }
    return {ApplyError::None, ApplyResult::NoEdit, prepared.size()};
}

void Config::QueueNotification(Property *property, const std::string &category, const std::string &key) {
    if (!property)
        return;

    for (const PendingNotification &notification : m_PendingNotifications) {
        if (notification.ChangedProperty == property)
            return;
    }
    m_PendingNotifications.push_back({category, key, property});
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

Category *Config::GetCategory(std::size_t i) {
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

    auto category = std::make_unique<Category>(this, name);
    m_Categories.reserve(m_Categories.size() + 1);
    const auto inserted = m_CategoryMap.emplace(n, category.get());
    if (!inserted.second)
        return inserted.first->second;
    m_Categories.push_back(category.release());
    TouchSchema();
    return m_Categories.back();
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

void Category::SetComment(const char *comment) {
    const std::string value = comment ? comment : "";
    if (m_Comment == value)
        return;
    m_Comment = value;
    if (m_Config)
        m_Config->TouchSchema();
}

Property *Category::GetProperty(std::size_t i) {
    if (i >= m_Properties.size())
        return nullptr;
    return m_Properties[i];
}

Property *Category::GetProperty(const char *key) {
    if (!key)
        return nullptr;

    std::string k = key;
    auto it = m_PropertyMap.find(k);
    if (it != m_PropertyMap.end()) {
        if (it->second)
            return it->second;
        m_PropertyMap.erase(it);
    }

    auto property = std::make_unique<Property>(m_Config, m_Name, k);
    m_Properties.reserve(m_Properties.size() + 1);
    const auto inserted = m_PropertyMap.emplace(k, property.get());
    if (!inserted.second)
        return inserted.first->second;
    m_Properties.push_back(property.release());
    if (m_Config)
        m_Config->TouchSchema();
    return m_Properties.back();
}

bool Category::HasKey(const char *key) const {
    if (!key)
        return false;
    const auto property = m_PropertyMap.find(key);
    return property != m_PropertyMap.end() && property->second;
}

Property::Property(Config *config, std::string category, std::string key) {
    m_Type = NONE;
    m_Value = 0;
    m_Config = config;
    m_Category = std::move(category);
    m_Key = std::move(key);
}

void Property::SetComment(const char *comment) {
    const std::string value = comment ? comment : "";
    if (m_Comment == value)
        return;
    m_Comment = value;
    if (m_Config)
        m_Config->TouchSchema();
}

BML_ConfigPropertyEditor BML_GetConfigPropertyEditor(const IProperty *property) {
    const auto *concrete = dynamic_cast<const Property *>(property);
    return concrete ? concrete->GetEditorMetadata() : BML_CONFIG_EDITOR_DEFAULT;
}

int BML_SetConfigPropertyEditor(IProperty *property, BML_ConfigPropertyEditor editor) {
    auto *concrete = dynamic_cast<Property *>(property);
    return concrete && concrete->SetEditorMetadata(editor) ? 1 : 0;
}

size_t BML_GetConfigPropertyChoiceCount(const IProperty *property) {
    const auto *concrete = dynamic_cast<const Property *>(property);
    return concrete ? concrete->m_Choices.size() : 0;
}

const char *BML_GetConfigPropertyChoice(const IProperty *property, size_t index) {
    const auto *concrete = dynamic_cast<const Property *>(property);
    return concrete && index < concrete->m_Choices.size()
        ? concrete->m_Choices[index].c_str()
        : nullptr;
}

int BML_SetConfigPropertyChoices(IProperty *property,
                                 const char *const choices[], size_t count) {
    auto *concrete = dynamic_cast<Property *>(property);
    if (!concrete)
        return 0;
    try {
        return concrete->SetChoiceMetadata(choices, count) ? 1 : 0;
    } catch (...) {
        return 0;
    }
}

BML_ConfigPropertyEditor Property::GetEditorMetadata() const {
    return m_Editor;
}

bool Property::SetEditorMetadata(BML_ConfigPropertyEditor editor) {
    if (editor != BML_CONFIG_EDITOR_DEFAULT &&
        editor != BML_CONFIG_EDITOR_COLOR &&
        editor != BML_CONFIG_EDITOR_CHOICE) {
        return false;
    }
    if (m_Editor == editor)
        return true;
    m_Editor = editor;
    if (m_Config)
        m_Config->TouchSchema();
    return true;
}

bool Property::SetChoiceMetadata(const char *const choices[], std::size_t count) {
    if (!choices && count != 0)
        return false;

    std::vector<std::string> next;
    next.reserve(count);
    std::unordered_set<std::string_view> seen;
    seen.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        if (!choices[index])
            return false;
        const std::string_view choice(choices[index]);
        if (!seen.emplace(choice).second)
            return false;
        next.emplace_back(choice);
    }

    if (m_Choices == next)
        return true;
    m_Choices = std::move(next);
    if (m_Config)
        m_Config->TouchSchema();
    return true;
}

const char *Property::GetString() {
    if (m_Type != STRING)
        return "";
    const auto *value = std::get_if<std::string>(&m_Value);
    return value ? value->c_str() : "";
}

std::size_t Property::GetStringSize() {
    if (GetType() != STRING)
        return 0;
    const auto *value = std::get_if<std::string>(&m_Value);
    return value ? value->size() : 0;
}

bool Property::GetBoolean() {
    if (m_Type != BOOLEAN)
        return false;
    const auto *value = std::get_if<bool>(&m_Value);
    return value ? *value : false;
}

int Property::GetInteger() {
    if (m_Type != INTEGER)
        return 0;
    const auto *value = std::get_if<int>(&m_Value);
    return value ? *value : 0;
}

float Property::GetFloat() {
    if (m_Type != FLOAT)
        return 0.0f;
    const auto *value = std::get_if<float>(&m_Value);
    return value ? *value : 0.0f;
}

CKKEYBOARD Property::GetKey() {
    if (m_Type != KEY)
        return static_cast<CKKEYBOARD>(0);
    const auto *value = std::get_if<int>(&m_Value);
    return value ? static_cast<CKKEYBOARD>(*value) : static_cast<CKKEYBOARD>(0);
}

void Property::SetString(const char *value) {
    if (!value)
        value = "";
    std::string newValue = value;

    const PropertyType previousType = m_Type;
    const auto *current = std::get_if<std::string>(&m_Value);
    if (m_Type != STRING || !current || *current != newValue) {
        m_Value = newValue;
        m_Type = STRING;
        m_Hash = utils::HashString(value);
        SetModified(previousType);
    }
}

void Property::SetBoolean(bool value) {
    const PropertyType previousType = m_Type;
    const auto *current = std::get_if<bool>(&m_Value);
    if (m_Type != BOOLEAN || !current || *current != value) {
        m_Value = value;
        m_Type = BOOLEAN;
        SetModified(previousType);
    }
}

void Property::SetInteger(int value) {
    const PropertyType previousType = m_Type;
    const auto *current = std::get_if<int>(&m_Value);
    if (m_Type != INTEGER || !current || *current != value) {
        m_Value = value;
        m_Type = INTEGER;
        SetModified(previousType);
    }
}

void Property::SetFloat(float value) {
    const PropertyType previousType = m_Type;
    if (m_Type != FLOAT || !ConfigValuesEqual(FLOAT, m_Value, ConfigValue(value))) {
        m_Value = value;
        m_Type = FLOAT;
        SetModified(previousType);
    }
}

void Property::SetKey(CKKEYBOARD value) {
    const PropertyType previousType = m_Type;
    const int newValue = static_cast<int>(value);
    const auto *current = std::get_if<int>(&m_Value);
    if (m_Type != KEY || !current || *current != newValue) {
        m_Value = newValue;
        m_Type = KEY;
        SetModified(previousType);
    }
}

void Property::SetValue(const Value &value) {
    switch (m_Type) {
    case STRING: {
        const auto *typed = std::get_if<std::string>(&value);
        if (typed)
            SetString(typed->c_str());
        break;
    }
    case BOOLEAN: {
        const auto *typed = std::get_if<bool>(&value);
        if (typed)
            SetBoolean(*typed);
        break;
    }
    case INTEGER:
    case KEY: {
        const auto *typed = std::get_if<int>(&value);
        if (!typed)
            break;
        if (m_Type == INTEGER)
            SetInteger(*typed);
        else
            SetKey(static_cast<CKKEYBOARD>(*typed));
        break;
    }
    case FLOAT: {
        const auto *typed = std::get_if<float>(&value);
        if (typed)
            SetFloat(*typed);
        break;
    }
    case NONE:
    default:
        break;
    }
}

void Property::SetDefaultString(const char *value) {
    if (!value)
        value = "";

    if (m_Type != STRING) {
        m_Type = STRING;
        m_Hash = utils::HashString(value);
        m_Value = value;
        if (m_Config)
            m_Config->TouchSchema();
    }
}

void Property::SetDefaultBoolean(bool value) {
    if (m_Type != BOOLEAN) {
        m_Type = BOOLEAN;
        m_Value = value;
        if (m_Config)
            m_Config->TouchSchema();
    }
}

void Property::SetDefaultInteger(int value) {
    if (m_Type != INTEGER) {
        m_Type = INTEGER;
        m_Value = value;
        if (m_Config)
            m_Config->TouchSchema();
    }
}

void Property::SetDefaultFloat(float value) {
    if (m_Type != FLOAT) {
        m_Type = FLOAT;
        m_Value = value;
        if (m_Config)
            m_Config->TouchSchema();
    }
}

void Property::SetDefaultKey(CKKEYBOARD value) {
    if (m_Type != KEY) {
        m_Type = KEY;
        m_Value = static_cast<int>(value);
        if (m_Config)
            m_Config->TouchSchema();
    }
}

std::size_t Property::GetHash() const {
    switch (m_Type) {
    case STRING:
        return m_Hash;
    case INTEGER:
    case KEY:
        return static_cast<std::size_t>(std::get<int>(m_Value));
    case BOOLEAN:
        return std::get<bool>(m_Value) ? 1 : 0;
    case FLOAT: {
        float f = std::get<float>(m_Value);
        std::uint32_t bits;
        std::memcpy(&bits, &f, sizeof(bits));
        return static_cast<std::size_t>(bits);
    }
    default:
        return 0;
    }
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
    default:
        break;
    }
}

void Property::SetModified(PropertyType previousType) {
    if (!m_Config)
        return;

    if (previousType == m_Type)
        m_Config->TouchValue();
    else
        m_Config->TouchSchema();
    m_Config->MarkDirty();
    m_Config->QueueNotification(this, m_Category, m_Key);
}
