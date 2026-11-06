#ifndef BML_CONFIG_VALUE_H
#define BML_CONFIG_VALUE_H

#include <cmath>
#include <cstdint>
#include <string>
#include <variant>

#include "BML/Config.h"

enum class PropertyType : std::uint32_t {
    STRING = BML_CONFIG_TYPE_STRING,
    BOOLEAN = BML_CONFIG_TYPE_BOOLEAN,
    INTEGER = BML_CONFIG_TYPE_INTEGER,
    KEY = BML_CONFIG_TYPE_KEY,
    FLOAT = BML_CONFIG_TYPE_FLOAT,
    NONE = BML_CONFIG_TYPE_NONE
};

using ConfigValue = std::variant<bool, int, float, std::string>;

inline bool ConfigValueMatchesType(PropertyType type, const ConfigValue &value) noexcept {
    switch (type) {
    case PropertyType::STRING:
        return std::holds_alternative<std::string>(value);
    case PropertyType::BOOLEAN:
        return std::holds_alternative<bool>(value);
    case PropertyType::INTEGER:
    case PropertyType::KEY:
        return std::holds_alternative<int>(value);
    case PropertyType::FLOAT:
        return std::holds_alternative<float>(value);
    case PropertyType::NONE:
    default:
        return false;
    }
}

inline bool ConfigValuesEqual(PropertyType type, const ConfigValue &left,
                              const ConfigValue &right) noexcept {
    if (!ConfigValueMatchesType(type, left) || !ConfigValueMatchesType(type, right))
        return false;
    if (type != PropertyType::FLOAT)
        return left == right;

    const float leftValue = std::get<float>(left);
    const float rightValue = std::get<float>(right);
    return leftValue == rightValue || (std::isnan(leftValue) && std::isnan(rightValue));
}

#endif // BML_CONFIG_VALUE_H
