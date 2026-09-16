#ifndef BML_CONFIG_VALUE_H
#define BML_CONFIG_VALUE_H

#include <cmath>
#include <string>
#include <variant>

#include "BML/IConfig.h"

using ConfigValue = std::variant<bool, int, float, std::string>;

inline bool ConfigValueMatchesType(IProperty::PropertyType type,
                                   const ConfigValue &value) noexcept {
    switch (type) {
    case IProperty::STRING:
        return std::holds_alternative<std::string>(value);
    case IProperty::BOOLEAN:
        return std::holds_alternative<bool>(value);
    case IProperty::INTEGER:
    case IProperty::KEY:
        return std::holds_alternative<int>(value);
    case IProperty::FLOAT:
        return std::holds_alternative<float>(value);
    case IProperty::NONE:
    default:
        return false;
    }
}

inline bool ConfigValuesEqual(IProperty::PropertyType type, const ConfigValue &left,
                              const ConfigValue &right) noexcept {
    if (!ConfigValueMatchesType(type, left) || !ConfigValueMatchesType(type, right))
        return false;
    if (type != IProperty::FLOAT)
        return left == right;

    const float leftValue = std::get<float>(left);
    const float rightValue = std::get<float>(right);
    return leftValue == rightValue || (std::isnan(leftValue) && std::isnan(rightValue));
}

#endif // BML_CONFIG_VALUE_H
