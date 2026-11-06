#include "Api/HandleSequence.h"
#include "Api/ConfigApi.h"
#include "Api/ModApi.h"
#include "StringUtils.h"
#include <atomic>
#include <cmath>
#include <unordered_set>

namespace BML::Api {
namespace {
bool Key(const char *text) {
    if (!text || !*text)
        return false;
    for (const unsigned char *p = reinterpret_cast<const unsigned char *>(text); *p; ++p)
        if (*p <= 32 || *p == '{' || *p == '}' || *p == '#')
            return false;
    return utils::IsValidUtf8(text);
}
bool Decode(const BML_ConfigValue *input, ConfigValue &out) {
    if (!input || input->StructSize < BML_CONFIG_VALUE_1_0_SIZE)
        return false;
    switch (input->Type) {
    case BML_CONFIG_TYPE_NONE:
        out = 0;
        return true;
    case BML_CONFIG_TYPE_STRING:
        if (!input->Data.String || !utils::IsValidUtf8(input->Data.String))
            return false;
        out = std::string(input->Data.String);
        return true;
    case BML_CONFIG_TYPE_BOOLEAN:
        if (input->Data.Boolean > 1)
            return false;
        out = input->Data.Boolean != 0;
        return true;
    case BML_CONFIG_TYPE_INTEGER:
        out = static_cast<int>(input->Data.Integer);
        return true;
    case BML_CONFIG_TYPE_FLOAT:
        if (!std::isfinite(input->Data.Float))
            return false;
        out = input->Data.Float;
        return true;
    case BML_CONFIG_TYPE_KEY:
        if (input->Data.Key >= 256)
            return false;
        out = static_cast<int>(input->Data.Key);
        return true;
    default:
        return false;
    }
}
BML_ConfigValue Encode(uint32_t type, const ConfigValue &value) {
    BML_ConfigValue out{};
    out.StructSize = sizeof(out);
    out.Type = type;
    switch (type) {
    case BML_CONFIG_TYPE_STRING:
        out.Data.String = std::get<std::string>(value).c_str();
        break;
    case BML_CONFIG_TYPE_BOOLEAN:
        out.Data.Boolean = std::get<bool>(value) ? 1u : 0u;
        break;
    case BML_CONFIG_TYPE_INTEGER:
        out.Data.Integer = std::get<int>(value);
        break;
    case BML_CONFIG_TYPE_KEY:
        out.Data.Key = static_cast<uint32_t>(std::get<int>(value));
        break;
    case BML_CONFIG_TYPE_FLOAT:
        out.Data.Float = std::get<float>(value);
        break;
    default:
        break;
    }
    return out;
}
Config *Ensure(ModContext &ctx, IMod &owner) {
    if (auto *config = ctx.GetConfig(&owner))
        return config;
    return ctx.AddConfig(std::make_unique<Config>(&owner));
}
bool CopyChoices(const char *const *input, size_t count, std::vector<std::string> &out) {
    if ((!input && count) || count > out.max_size())
        return false;
    out.reserve(count);
    std::unordered_set<std::string_view> seen;
    for (size_t i = 0; i < count; ++i) {
        if (!input[i] || !utils::IsValidUtf8(input[i]) || !seen.insert(input[i]).second)
            return false;
        out.emplace_back(input[i]);
    }
    return true;
}
} // namespace

ConfigApi::Record *ConfigApi::Resolve(ModContext &ctx, IMod &owner,
                                      BML_ConfigPropertyHandle handle) {
    const auto it = m_Records.find(handle);
    if (it == m_Records.end() || it->second->Owner != &owner)
        return nullptr;
    auto *config = ctx.GetConfig(&owner);
    if (!config)
        return nullptr;
    const auto category = config->m_CategoryMap.find(it->second->Category);
    if (category == config->m_CategoryMap.end())
        return nullptr;
    const auto property = category->second->m_PropertyMap.find(it->second->Key);
    return property != category->second->m_PropertyMap.end() &&
                   property->second == it->second->Target &&
                   property->second->m_Identity == it->second->TargetGeneration
               ? it->second.get()
               : nullptr;
}
int ConfigApi::Declare(ModContext &ctx, IMod &owner, const BML_ConfigPropertyDefinition *definition,
                       BML_ConfigPropertyHandle *out) {
    if (!out)
        return BML_ERROR_INVALID_PARAMETER;
    *out = 0;
    if (!definition || definition->StructSize < BML_CONFIG_PROPERTY_DEFINITION_1_0_SIZE ||
        !Key(definition->Category) || !Key(definition->Key) ||
        definition->Editor > BML_CONFIG_EDITOR_CHOICE)
        return BML_ERROR_INVALID_PARAMETER;
    if (!AcceptsModWork(ctx, owner))
        return BML_ERROR_FROZEN;
    auto record = std::make_shared<Record>();
    if (!Decode(definition->DefaultValue, record->Default))
        return BML_ERROR_INVALID_PARAMETER;
    record->Type = definition->DefaultValue->Type;
    for (const auto &entry : m_Records) {
        const auto &r = *entry.second;
        if (r.Owner == &owner && r.Category == definition->Category && r.Key == definition->Key) {
            if (!Resolve(ctx, owner, entry.first))
                return BML_ERROR_INVALID_HANDLE;
            if (r.Type != record->Type)
                return BML_ERROR_TYPE_MISMATCH;
            *out = entry.first;
            return BML_OK;
        }
    }
    std::string comment = definition->Comment ? definition->Comment : "";
    if (!utils::IsValidUtf8(comment))
        return BML_ERROR_INVALID_PARAMETER;
    std::vector<std::string> choices;
    if (!CopyChoices(definition->Choices, definition->ChoiceCount, choices))
        return BML_ERROR_INVALID_PARAMETER;
    record->Category = definition->Category;
    record->Key = definition->Key;
    record->Owner = &owner;
    record->Comment = comment;
    record->Editor = definition->Editor;
    record->ChoiceValues = choices;
    record->BindChoices();
    ConfigValue initial = record->Default; // All allocations precede publishing the declaration.
    auto *config = Ensure(ctx, owner);
    if (!config)
        return BML_ERROR_UNAVAILABLE;
    const bool hadCategory = config->m_CategoryMap.contains(record->Category);
    const auto schema = config->m_SchemaRevision;
    static std::atomic<uint64_t> next{1};
    const auto handle = BML::Api::NextHandle(next);
    if (!handle)
        return BML_ERROR_FAIL;
    auto [entry, inserted] = m_Records.emplace(handle, std::move(record));
    if (!inserted)
        return BML_ERROR_FAIL;
    auto &r = *entry->second;
    try {
        auto *property =
            static_cast<Property *>(config->GetProperty(r.Category.c_str(), r.Key.c_str()));
        r.Target = property;
        r.TargetGeneration = property->m_Identity;
        if (r.Type != BML_CONFIG_TYPE_NONE &&
            property->m_Type != static_cast<IProperty::PropertyType>(r.Type)) {
            property->m_Value.swap(initial);
            property->m_Type = static_cast<IProperty::PropertyType>(r.Type);
            if (r.Type == BML_CONFIG_TYPE_STRING)
                property->m_Hash =
                    utils::HashString(std::get<std::string>(property->m_Value).c_str());
        }
        property->m_Comment.swap(comment);
        property->m_Choices.swap(choices);
        property->m_Editor = static_cast<BML_ConfigPropertyEditor>(definition->Editor);
        config->TouchSchema();
        *out = handle;
        return BML_OK;
    } catch (...) {
        // GetProperty itself publishes only after allocation; an empty newly
        // created category is the sole possible intermediate state.
        if (!hadCategory) {
            const auto category = config->m_CategoryMap.find(r.Category);
            if (category != config->m_CategoryMap.end() && category->second->m_Properties.empty()) {
                auto *target = category->second;
                config->m_CategoryMap.erase(category);
                config->m_Categories.erase(
                    std::remove(config->m_Categories.begin(), config->m_Categories.end(), target),
                    config->m_Categories.end());
                delete target;
            }
        }
        config->m_SchemaRevision = schema;
        m_Records.erase(entry);
        throw;
    }
}
int ConfigApi::Find(ModContext &ctx, IMod &owner, const char *category, const char *key,
                    BML_ConfigPropertyHandle *out) {
    if (!category || !key || !out)
        return BML_ERROR_INVALID_PARAMETER;
    *out = 0;
    for (const auto &entry : m_Records)
        if (entry.second->Owner == &owner && entry.second->Category == category &&
            entry.second->Key == key && Resolve(ctx, owner, entry.first)) {
            *out = entry.first;
            return BML_OK;
        }
    return BML_ERROR_NOT_FOUND;
}
int ConfigApi::Read(ModContext &ctx, IMod &owner, BML_ConfigPropertyHandle handle,
                    BML_ConfigPropertyVisitor visitor, void *data) {
    if (!visitor)
        return BML_ERROR_INVALID_PARAMETER;
    auto *r = Resolve(ctx, owner, handle);
    if (!r)
        return BML_ERROR_INVALID_HANDLE;
    const auto hold = m_Records.at(handle);
    auto &property = *r->Target;
    const ConfigValue currentValue = property.m_Value;
    const auto initial = Encode(r->Type, r->Default),
               current = Encode(property.m_Type, currentValue);
    const BML_ConfigPropertyDefinition definition{sizeof(BML_ConfigPropertyDefinition),
                                                  r->Category.c_str(),
                                                  r->Key.c_str(),
                                                  &initial,
                                                  r->Comment.c_str(),
                                                  r->Editor,
                                                  r->Choices.data(),
                                                  r->Choices.size()};
    const BML_ConfigPropertyInfo info{sizeof(BML_ConfigPropertyInfo), handle, &definition,
                                      &current};
    return visitor(data, &info);
}
int ConfigApi::Set(ModContext &ctx, IMod &owner, BML_ConfigPropertyHandle handle,
                   const BML_ConfigValue *value, bool isDefault) {
    auto *r = Resolve(ctx, owner, handle);
    if (!r)
        return BML_ERROR_INVALID_HANDLE;
    ConfigValue decoded;
    if (!Decode(value, decoded))
        return BML_ERROR_INVALID_PARAMETER;
    if (r->Type != value->Type && !(isDefault && r->Type == BML_CONFIG_TYPE_NONE))
        return BML_ERROR_TYPE_MISMATCH;
    auto *config = ctx.GetConfig(&owner);
    auto &property = *r->Target;
    if (isDefault) {
        auto next = std::make_shared<Record>(*r);
        next->BindChoices();
        ConfigValue initial = decoded;
        if (property.m_Type != static_cast<IProperty::PropertyType>(value->Type)) {
            property.m_Value.swap(initial);
            property.m_Type = static_cast<IProperty::PropertyType>(value->Type);
            if (value->Type == BML_CONFIG_TYPE_STRING)
                property.m_Hash =
                    utils::HashString(std::get<std::string>(property.m_Value).c_str());
        }
        next->Default.swap(decoded);
        next->Type = value->Type;
        m_Records.at(handle) = std::move(next);
        config->TouchSchema();
        return BML_OK;
    }
    if (value->Type == BML_CONFIG_TYPE_NONE)
        return BML_ERROR_TYPE_MISMATCH;
    Config::Edit edit{r->Category, r->Key, property.m_Type, property.m_Value, std::move(decoded)};
    return config->ApplyEdits(&owner, config->GetSchemaRevision(), {std::move(edit)})
               ? BML_OK
               : BML_ERROR_FAIL;
}
int ConfigApi::Comment(ModContext &ctx, IMod &owner, BML_ConfigPropertyHandle h,
                       const char *comment) {
    if (!comment || !utils::IsValidUtf8(comment))
        return BML_ERROR_INVALID_PARAMETER;
    auto *r = Resolve(ctx, owner, h);
    if (!r)
        return BML_ERROR_INVALID_HANDLE;
    auto next = std::make_shared<Record>(*r);
    next->Comment = comment;
    next->BindChoices();
    r->Target->SetComment(comment);
    m_Records.at(h) = std::move(next);
    return BML_OK;
}
int ConfigApi::Editor(ModContext &ctx, IMod &owner, BML_ConfigPropertyHandle h, uint32_t editor) {
    if (editor > BML_CONFIG_EDITOR_CHOICE)
        return BML_ERROR_INVALID_PARAMETER;
    auto *r = Resolve(ctx, owner, h);
    if (!r)
        return BML_ERROR_INVALID_HANDLE;
    auto next = std::make_shared<Record>(*r);
    next->Editor = editor;
    next->BindChoices();
    if (!r->Target->SetEditorMetadata(static_cast<BML_ConfigPropertyEditor>(editor)))
        return BML_ERROR_INVALID_PARAMETER;
    m_Records.at(h) = std::move(next);
    return BML_OK;
}
int ConfigApi::Choices(ModContext &ctx, IMod &owner, BML_ConfigPropertyHandle h,
                       const char *const *choices, size_t count) {
    auto *r = Resolve(ctx, owner, h);
    if (!r)
        return BML_ERROR_INVALID_HANDLE;
    std::vector<std::string> values;
    if (!CopyChoices(choices, count, values))
        return BML_ERROR_INVALID_PARAMETER;
    auto next = std::make_shared<Record>(*r);
    next->ChoiceValues = values;
    next->BindChoices();
    r->Target->m_Choices.swap(values);
    m_Records.at(h) = std::move(next);
    ctx.GetConfig(&owner)->TouchSchema();
    return BML_OK;
}
BML_ConfigPropertyHandle ConfigApi::HandleFor(IMod &owner, Property *property) const noexcept {
    for (const auto &entry : m_Records)
        if (entry.second->Owner == &owner && entry.second->Target == property)
            return entry.first;
    return 0;
}
void ConfigApi::CleanupOwner(IMod &owner) noexcept {
    for (auto it = m_Records.begin(); it != m_Records.end();)
        if (it->second->Owner == &owner)
            it = m_Records.erase(it);
        else
            ++it;
}
namespace {
template <auto Method, class... Args> int Serve(BML_ModContextHandle h, Args... args) {
    return WithModContext(h, [&](ModContext &ctx, IMod &owner) {
        return (ctx.GetConfigApi().*Method)(ctx, owner, args...);
    });
}
int BML_CDECL Declare(BML_ModContextHandle h, const BML_ConfigPropertyDefinition *d,
                      BML_ConfigPropertyHandle *out) {
    return Serve<&ConfigApi::Declare>(h, d, out);
}
int BML_CDECL Find(BML_ModContextHandle h, const char *c, const char *k,
                   BML_ConfigPropertyHandle *out) {
    return Serve<&ConfigApi::Find>(h, c, k, out);
}
int BML_CDECL Read(BML_ModContextHandle h, BML_ConfigPropertyHandle p, BML_ConfigPropertyVisitor v,
                   void *d) {
    return Serve<&ConfigApi::Read>(h, p, v, d);
}
int BML_CDECL Set(BML_ModContextHandle h, BML_ConfigPropertyHandle p, const BML_ConfigValue *v) {
    return Serve<&ConfigApi::Set>(h, p, v, false);
}
int BML_CDECL Default(BML_ModContextHandle h, BML_ConfigPropertyHandle p,
                      const BML_ConfigValue *v) {
    return Serve<&ConfigApi::Set>(h, p, v, true);
}
int BML_CDECL Comment(BML_ModContextHandle h, BML_ConfigPropertyHandle p, const char *v) {
    return Serve<&ConfigApi::Comment>(h, p, v);
}
int BML_CDECL Editor(BML_ModContextHandle h, BML_ConfigPropertyHandle p, uint32_t v) {
    return Serve<&ConfigApi::Editor>(h, p, v);
}
int BML_CDECL Choices(BML_ModContextHandle h, BML_ConfigPropertyHandle p, const char *const *v,
                      size_t n) {
    return Serve<&ConfigApi::Choices>(h, p, v, n);
}
int BML_CDECL CategoryComment(BML_ModContextHandle h, const char *c, const char *v) {
    if (!Key(c) || !v)
        return BML_ERROR_INVALID_PARAMETER;
    return WithModContext(h, [&](ModContext &ctx, IMod &owner) {
        auto *config = Ensure(ctx, owner);
        if (!config)
            return BML_ERROR_UNAVAILABLE;
        config->SetCategoryComment(c, v);
        return BML_OK;
    });
}
int BML_CDECL HasCategory(BML_ModContextHandle h, const char *c, uint32_t *out) {
    if (!c || !out)
        return BML_ERROR_INVALID_PARAMETER;
    return WithModContext(h, [&](ModContext &ctx, IMod &owner) {
        auto *config = Ensure(ctx, owner);
        if (!config)
            return BML_ERROR_UNAVAILABLE;
        *out = config->HasCategory(c) ? 1u : 0u;
        return BML_OK;
    });
}
int BML_CDECL HasKey(BML_ModContextHandle h, const char *c, const char *k, uint32_t *out) {
    if (!c || !k || !out)
        return BML_ERROR_INVALID_PARAMETER;
    return WithModContext(h, [&](ModContext &ctx, IMod &owner) {
        auto *config = Ensure(ctx, owner);
        if (!config)
            return BML_ERROR_UNAVAILABLE;
        *out = config->HasKey(c, k) ? 1u : 0u;
        return BML_OK;
    });
}
const BML_ConfigInterface table{
    BML_IFACE_HEADER(BML_ConfigInterface, BML_CONFIG_INTERFACE_ID, 1, 0),
    Declare,
    Find,
    Read,
    Set,
    Default,
    Comment,
    Editor,
    Choices,
    CategoryComment,
    HasCategory,
    HasKey};
} // namespace
const BML_ConfigInterface &ConfigInterface() { return table; }
} // namespace BML::Api
