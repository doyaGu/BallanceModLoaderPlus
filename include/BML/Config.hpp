#ifndef BML_CONFIG_HPP
#define BML_CONFIG_HPP
#include "BML/Config.h"
#include "BML/Mod.hpp"
#include <variant>

namespace BML::Config {
using Value = std::variant<std::monostate, std::string, bool, int32_t, float, uint32_t>;
struct PropertyDefinition {
    std::string Category, Key;
    Value DefaultValue;
    std::string Comment;
    uint32_t Editor = 0;
    std::vector<std::string> Choices;
};
struct PropertyInfo {
    BML_ConfigPropertyHandle Handle = 0;
    PropertyDefinition Definition;
    Value CurrentValue;
};
namespace Detail {
inline bool Encode(const Value &value, BML_ConfigValue &out) {
    out = {};
    out.StructSize = sizeof(out);
    switch (value.index()) {
    case 0:
        out.Type = BML_CONFIG_TYPE_NONE;
        break;
    case 1: {
        const auto &text = std::get<std::string>(value);
        if (!BML::Detail::IsText(text))
            return false;
        out.Type = BML_CONFIG_TYPE_STRING;
        out.Data.String = text.c_str();
        break;
    }
    case 2:
        out.Type = BML_CONFIG_TYPE_BOOLEAN;
        out.Data.Boolean = std::get<bool>(value) ? 1u : 0u;
        break;
    case 3:
        out.Type = BML_CONFIG_TYPE_INTEGER;
        out.Data.Integer = std::get<int32_t>(value);
        break;
    case 4:
        out.Type = BML_CONFIG_TYPE_FLOAT;
        out.Data.Float = std::get<float>(value);
        break;
    case 5:
        out.Type = BML_CONFIG_TYPE_KEY;
        out.Data.Key = std::get<uint32_t>(value);
        break;
    default:
        return false;
    }
    return true;
}
inline Value Decode(const BML_ConfigValue &value) {
    switch (value.Type) {
    case BML_CONFIG_TYPE_STRING:
        return std::string(value.Data.String ? value.Data.String : "");
    case BML_CONFIG_TYPE_BOOLEAN:
        return value.Data.Boolean != 0;
    case BML_CONFIG_TYPE_INTEGER:
        return value.Data.Integer;
    case BML_CONFIG_TYPE_FLOAT:
        return value.Data.Float;
    case BML_CONFIG_TYPE_KEY:
        return value.Data.Key;
    default:
        return {};
    }
}
inline const BML_ConfigInterface *Api() noexcept {
    return BML::Detail::Bind<BML_ConfigInterface>(
        BML_CONFIG_INTERFACE_ID, 1, BML_CONFIG_INTERFACE_1_0_SIZE,
        +[](const BML_ConfigInterface *table) {
            return BML_IFACE_HAS(table, BML_ConfigInterface, DeclareProperty) &&
                   BML_IFACE_HAS(table, BML_ConfigInterface, FindProperty) &&
                   BML_IFACE_HAS(table, BML_ConfigInterface, ReadProperty) &&
                   BML_IFACE_HAS(table, BML_ConfigInterface, SetValue) &&
                   BML_IFACE_HAS(table, BML_ConfigInterface, SetDefaultValue) &&
                   BML_IFACE_HAS(table, BML_ConfigInterface, SetComment) &&
                   BML_IFACE_HAS(table, BML_ConfigInterface, SetEditor) &&
                   BML_IFACE_HAS(table, BML_ConfigInterface, SetChoices) &&
                   BML_IFACE_HAS(table, BML_ConfigInterface, SetCategoryComment) &&
                   BML_IFACE_HAS(table, BML_ConfigInterface, HasCategory) &&
                   BML_IFACE_HAS(table, BML_ConfigInterface, HasKey);
        });
}
} // namespace Detail

class View;
class Property {
  public:
    Property() noexcept = default;
    [[nodiscard]] bool IsValid() const noexcept {
        return m_Api && m_Api->ReadProperty(
                            m_Context.Handle(), m_Handle,
                            BML::Detail::Cdecl<[](void *, const BML_ConfigPropertyInfo *) -> int {
                                return BML_OK;
                            }>::Call,
                            nullptr) == BML_OK;
    }
    [[nodiscard]] Result<PropertyInfo> Read() const try {
        PropertyInfo value;
        const int code =
            m_Api
                ? m_Api->ReadProperty(
                      m_Context.Handle(), m_Handle,
                      BML::Detail::Cdecl<[](void *data, const BML_ConfigPropertyInfo *info) -> int {
                          return BML::Detail::Guard([&] {
                              auto &out = *static_cast<PropertyInfo *>(data);
                              const auto &d = *info->Definition;
                              out.Handle = info->Handle;
                              out.Definition = {d.Category,
                                                d.Key,
                                                Detail::Decode(*d.DefaultValue),
                                                d.Comment ? d.Comment : "",
                                                d.Editor,
                                                {}};
                              for (size_t i = 0; i < d.ChoiceCount; ++i)
                                  out.Definition.Choices.emplace_back(d.Choices[i]);
                              out.CurrentValue = Detail::Decode(*info->Value);
                              return BML_OK;
                          });
                      }>::Call,
                      &value)
                : BML_ERROR_UNAVAILABLE;
        return code == BML_OK ? Result<PropertyInfo>::Success(std::move(value))
                              : Result<PropertyInfo>::Failure(code);
    } catch (const std::bad_alloc &) {
        return Result<PropertyInfo>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<PropertyInfo>::Failure(BML_ERROR_FAIL);
    }
    int Set(const Value &value) const {
        BML_ConfigValue record;
        if (!Detail::Encode(value, record))
            return BML_ERROR_INVALID_PARAMETER;
        return m_Api ? m_Api->SetValue(m_Context.Handle(), m_Handle, &record)
                     : BML_ERROR_UNAVAILABLE;
    }
    int SetDefault(const Value &value) const {
        BML_ConfigValue record;
        if (!Detail::Encode(value, record))
            return BML_ERROR_INVALID_PARAMETER;
        return m_Api ? m_Api->SetDefaultValue(m_Context.Handle(), m_Handle, &record)
                     : BML_ERROR_UNAVAILABLE;
    }
    int SetComment(std::string_view comment) const try {
        if (!BML::Detail::IsText(comment))
            return BML_ERROR_INVALID_PARAMETER;
        const std::string text(comment);
        return m_Api ? m_Api->SetComment(m_Context.Handle(), m_Handle, text.c_str())
                     : BML_ERROR_UNAVAILABLE;
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
    int SetEditor(uint32_t editor) const {
        return m_Api ? m_Api->SetEditor(m_Context.Handle(), m_Handle, editor)
                     : BML_ERROR_UNAVAILABLE;
    }
    int SetChoices(std::span<const std::string> choices) const try {
        std::vector<const char *> views;
        views.reserve(choices.size());
        for (const auto &text : choices) {
            if (!BML::Detail::IsText(text))
                return BML_ERROR_INVALID_PARAMETER;
            views.push_back(text.c_str());
        }
        return m_Api ? m_Api->SetChoices(m_Context.Handle(), m_Handle, views.data(), views.size())
                     : BML_ERROR_UNAVAILABLE;
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
    template <class T> [[nodiscard]] T Get(T fallback = {}) const {
        static_assert(std::is_same_v<T, std::string> || std::is_same_v<T, bool> ||
                      std::is_same_v<T, int32_t> || std::is_same_v<T, float> ||
                      std::is_same_v<T, uint32_t>);
        T value = std::move(fallback);
        const int code =
            m_Api
                ? m_Api->ReadProperty(
                      m_Context.Handle(), m_Handle,
                      BML::Detail::Cdecl<[](void *data, const BML_ConfigPropertyInfo *info) -> int {
                          return BML::Detail::Guard([&] {
                              const auto &v = *info->Value;
                              if constexpr (std::is_same_v<T, std::string>) {
                                  if (v.Type == BML_CONFIG_TYPE_STRING)
                                      *static_cast<T *>(data) = v.Data.String;
                              } else if constexpr (std::is_same_v<T, bool>) {
                                  if (v.Type == BML_CONFIG_TYPE_BOOLEAN)
                                      *static_cast<T *>(data) = v.Data.Boolean != 0;
                              } else if constexpr (std::is_same_v<T, int32_t>) {
                                  if (v.Type == BML_CONFIG_TYPE_INTEGER)
                                      *static_cast<T *>(data) = v.Data.Integer;
                              } else if constexpr (std::is_same_v<T, float>) {
                                  if (v.Type == BML_CONFIG_TYPE_FLOAT)
                                      *static_cast<T *>(data) = v.Data.Float;
                              } else {
                                  if (v.Type == BML_CONFIG_TYPE_KEY)
                                      *static_cast<T *>(data) = v.Data.Key;
                              }
                              return BML_OK;
                          });
                      }>::Call,
                      &value)
                : BML_ERROR_UNAVAILABLE;
        (void)code;
        return value;
    }

  private:
    friend class View;
    Property(Mod::Context ctx, BML_ConfigPropertyHandle handle) noexcept
        : m_Context(ctx), m_Handle(handle), m_Api(Detail::Api()) {}
    Mod::Context m_Context;
    BML_ConfigPropertyHandle m_Handle = 0;
    const BML_ConfigInterface *m_Api = nullptr;
};

class View {
  public:
    explicit View(const Mod::Context &context) noexcept
        : m_Context(context), m_Api(Detail::Api()) {}
    [[nodiscard]] bool IsValid() const noexcept { return m_Api && m_Context.IsValid(); }
    [[nodiscard]] Result<Property> DeclareProperty(const PropertyDefinition &definition) const try {
        if (!BML::Detail::IsText(definition.Category) || !BML::Detail::IsText(definition.Key) ||
            !BML::Detail::IsText(definition.Comment))
            return Result<Property>::Failure(BML_ERROR_INVALID_PARAMETER);
        BML_ConfigValue initial;
        if (!Detail::Encode(definition.DefaultValue, initial))
            return Result<Property>::Failure(BML_ERROR_INVALID_PARAMETER);
        std::vector<const char *> choices;
        choices.reserve(definition.Choices.size());
        for (const auto &choice : definition.Choices) {
            if (!BML::Detail::IsText(choice))
                return Result<Property>::Failure(BML_ERROR_INVALID_PARAMETER);
            choices.push_back(choice.c_str());
        }
        const BML_ConfigPropertyDefinition record{sizeof(BML_ConfigPropertyDefinition),
                                                  definition.Category.c_str(),
                                                  definition.Key.c_str(),
                                                  &initial,
                                                  definition.Comment.c_str(),
                                                  definition.Editor,
                                                  choices.data(),
                                                  choices.size()};
        BML_ConfigPropertyHandle handle = 0;
        const int code = m_Api ? m_Api->DeclareProperty(m_Context.Handle(), &record, &handle)
                               : BML_ERROR_UNAVAILABLE;
        return code == BML_OK ? Result<Property>::Success(Property(m_Context, handle))
                              : Result<Property>::Failure(code);
    } catch (const std::bad_alloc &) {
        return Result<Property>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<Property>::Failure(BML_ERROR_FAIL);
    }
    [[nodiscard]] Result<Property> FindProperty(std::string_view category,
                                                std::string_view key) const try {
        if (!BML::Detail::IsText(category) || !BML::Detail::IsText(key))
            return Result<Property>::Failure(BML_ERROR_INVALID_PARAMETER);
        const std::string c(category), k(key);
        BML_ConfigPropertyHandle handle = 0;
        const int code =
            m_Api ? m_Api->FindProperty(m_Context.Handle(), c.c_str(), k.c_str(), &handle)
                  : BML_ERROR_UNAVAILABLE;
        return code == BML_OK ? Result<Property>::Success(Property(m_Context, handle))
                              : Result<Property>::Failure(code);
    } catch (const std::bad_alloc &) {
        return Result<Property>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<Property>::Failure(BML_ERROR_FAIL);
    }
    [[nodiscard]] Result<bool> HasCategory(std::string_view category) const try {
        if (!BML::Detail::IsText(category))
            return Result<bool>::Failure(BML_ERROR_INVALID_PARAMETER);
        const std::string c(category);
        uint32_t exists = 0;
        const int code = m_Api ? m_Api->HasCategory(m_Context.Handle(), c.c_str(), &exists)
                               : BML_ERROR_UNAVAILABLE;
        return code == BML_OK ? Result<bool>::Success(exists != 0) : Result<bool>::Failure(code);
    } catch (const std::bad_alloc &) {
        return Result<bool>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<bool>::Failure(BML_ERROR_FAIL);
    }
    [[nodiscard]] Result<bool> HasKey(std::string_view category, std::string_view key) const try {
        if (!BML::Detail::IsText(category) || !BML::Detail::IsText(key))
            return Result<bool>::Failure(BML_ERROR_INVALID_PARAMETER);
        const std::string c(category), k(key);
        uint32_t exists = 0;
        const int code = m_Api ? m_Api->HasKey(m_Context.Handle(), c.c_str(), k.c_str(), &exists)
                               : BML_ERROR_UNAVAILABLE;
        return code == BML_OK ? Result<bool>::Success(exists != 0) : Result<bool>::Failure(code);
    } catch (const std::bad_alloc &) {
        return Result<bool>::Failure(BML_ERROR_OUT_OF_MEMORY);
    } catch (...) {
        return Result<bool>::Failure(BML_ERROR_FAIL);
    }
    int SetCategoryComment(std::string_view category, std::string_view comment) const try {
        if (!BML::Detail::IsText(category) || !BML::Detail::IsText(comment))
            return BML_ERROR_INVALID_PARAMETER;
        const std::string c(category), text(comment);
        return m_Api ? m_Api->SetCategoryComment(m_Context.Handle(), c.c_str(), text.c_str())
                     : BML_ERROR_UNAVAILABLE;
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }

  private:
    Mod::Context m_Context;
    const BML_ConfigInterface *m_Api;
};
} // namespace BML::Config
inline BML::Config::View BML::Mod::Context::Config() const noexcept {
    return BML::Config::View(*this);
}
#endif
