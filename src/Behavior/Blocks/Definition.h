#ifndef BML_BEHAVIOR_BLOCK_DEFINITION_H
#define BML_BEHAVIOR_BLOCK_DEFINITION_H

#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "Behavior/Block.h"
#include "Behavior/Value.h"

namespace BML::Behavior::Blocks::Detail {

// Lowers the same concrete Block definition used by the public headers into
// Runtime's private BlockSpec. It contains no knowledge of any particular Block.
class Definition final {
public:
    explicit Definition(CKGUID prototype) : m_Spec(prototype) {}

    void Target(CKGUID type, CKObject *object) {
        m_Spec.Target(type, object);
    }

    template <class T>
    void Pin(std::string_view name, CKGUID type, T &&value) {
        m_Spec.Input(Slot::Named(SlotKind::InputParameter,
                                 std::string(name), type),
                     Literal(type, std::forward<T>(value)));
    }
    template <class T>
    void Pin(int index, CKGUID type, T &&value) {
        m_Spec.Input(Slot::At(SlotKind::InputParameter, index, type),
                     Literal(type, std::forward<T>(value)));
    }
    void ObjectPin(std::string_view name, CKGUID type, CKObject *object) {
        m_Spec.Input(Slot::Named(SlotKind::InputParameter,
                                 std::string(name), type),
                     Parameter::Binding::Object(type, object));
    }
    void ObjectPin(int index, CKGUID type, CKObject *object) {
        m_Spec.Input(Slot::At(SlotKind::InputParameter, index, type),
                     Parameter::Binding::Object(type, object));
    }

    template <class T>
    void Setting(std::string_view name, CKGUID type, T &&value) {
        m_Spec.Setting(Slot::Named(SlotKind::Setting, std::string(name), type),
                       Literal(type, std::forward<T>(value)));
    }
    template <class T>
    void Setting(int index, CKGUID type, T &&value) {
        m_Spec.Setting(Slot::At(SlotKind::Setting, index, type),
                       Literal(type, std::forward<T>(value)));
    }

    void NextStage() { m_Spec.NextSettingStage(); }

    [[nodiscard]] BlockSpec Build() && { return std::move(m_Spec); }

private:
    template <class T>
    static Parameter::Binding Literal(CKGUID type, T &&value) {
        using Source = std::remove_cv_t<std::remove_reference_t<T>>;
        if constexpr (std::is_same_v<Source, std::string> ||
                      std::is_same_v<Source, std::string_view>) {
            return Value::Text(type, std::string(value));
        } else if constexpr (std::is_same_v<Source, const char *> ||
                             std::is_same_v<Source, char *>) {
            return Value::Text(type, value ? std::string(value) : std::string());
        } else if constexpr (std::is_same_v<Source, bool>) {
            const CKBOOL native = value ? TRUE : FALSE;
            return Value::From(type, native);
        } else {
            return Value::From(type, value);
        }
    }

    BlockSpec m_Spec;
};

} // namespace BML::Behavior::Blocks::Detail

#endif // BML_BEHAVIOR_BLOCK_DEFINITION_H
