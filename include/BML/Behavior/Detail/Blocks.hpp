// Shared implementation for the named retail Building Block adapters.
#ifndef BML_BEHAVIOR_DETAIL_BLOCKS_HPP
#define BML_BEHAVIOR_DETAIL_BLOCKS_HPP

#include "BML/Behavior.hpp"

namespace BML::Behavior::Detail {

inline Result<Value> ObjectValue(const Session &session, CKGUID type,
                                 CKObject *object) {
    if (!object)
        return Result<Value>::Success(Value::Null(type));
    const Result<BML_ObjectRef> reference = session.Reference(object);
    if (!reference)
        return Result<Value>::Failure(reference.Code(), reference.GetStatus());
    return Result<Value>::Success(Value::Object(type, reference.Value()));
}

inline Result<Block> TargetObject(const Session &session, CKGUID type,
                                  CKObject *object, Block block) {
    if (!object) {
        block.NullTarget(type);
        return Result<Block>::Success(std::move(block));
    }
    const Result<BML_ObjectRef> reference = session.Reference(object);
    if (!reference)
        return Result<Block>::Failure(reference.Code(), reference.GetStatus());
    block.Target(type, reference.Value());
    return Result<Block>::Success(std::move(block));
}

// Collects the named fields of one retail Building Block adapter. Settings
// are committed in the same native stages expected by the prototype.
class Definition final {
public:
    Definition(const Session &session, CKGUID prototype)
        : m_Session(session), m_Block(session.Use(prototype)) {}

    void Target(CKGUID type, CKObject *object) {
        if (m_Code != BML_OK)
            return;
        Result<Block> targeted = TargetObject(
            m_Session, type, object, std::move(m_Block));
        if (!targeted) {
            m_Code = targeted.Code();
            m_Status = targeted.GetStatus();
            return;
        }
        m_Block = std::move(targeted.Value());
    }

    template <class T>
    void Pin(std::string_view name, CKGUID type, T &&value) {
        if (m_Code == BML_OK)
            m_Pins.emplace_back(Selector::Unique(name),
                                Value::As(type, std::forward<T>(value)));
    }
    template <class T>
    void Pin(std::int32_t index, CKGUID type, T &&value) {
        if (m_Code == BML_OK)
            m_Pins.emplace_back(Selector::At(index),
                                Value::As(type, std::forward<T>(value)));
    }
    void ObjectPin(std::string_view name, CKGUID type, CKObject *object) {
        ObjectPin(Selector::Unique(name), type, object);
    }
    void ObjectPin(std::int32_t index, CKGUID type, CKObject *object) {
        ObjectPin(Selector::At(index), type, object);
    }

    template <class T>
    void Setting(std::string_view name, CKGUID type, T &&value) {
        if (m_Code == BML_OK)
            m_Settings.emplace_back(Selector::Unique(name),
                                    Value::As(type, std::forward<T>(value)));
    }
    template <class T>
    void Setting(std::int32_t index, CKGUID type, T &&value) {
        if (m_Code == BML_OK)
            m_Settings.emplace_back(Selector::At(index),
                                    Value::As(type, std::forward<T>(value)));
    }

    void NextStage() {
        if (m_Code == BML_OK)
            CommitSettings();
    }

    [[nodiscard]] Result<Block> Build() && {
        if (m_Code != BML_OK)
            return Result<Block>::Failure(m_Code, std::move(m_Status));
        CommitSettings();
        ApplyPins();
        return Result<Block>::Success(std::move(m_Block));
    }

private:
    void CommitSettings() {
        if (m_Settings.empty())
            return;
        m_Block.Settings(std::move(m_Settings));
        m_Settings = {};
    }

    void ApplyPins() {
        m_Block.Pins(std::move(m_Pins));
        m_Pins = {};
    }

    void ObjectPin(Selector slot, CKGUID type, CKObject *object) {
        if (m_Code != BML_OK)
            return;
        Result<Value> value = ObjectValue(m_Session, type, object);
        if (!value) {
            m_Code = value.Code();
            m_Status = value.GetStatus();
            return;
        }
        m_Pins.emplace_back(std::move(slot), std::move(value.Value()));
    }

    const Session &m_Session;
    Block m_Block;
    std::vector<SlotValue> m_Settings;
    std::vector<SlotValue> m_Pins;
    int m_Code = BML_OK;
    Status m_Status;
};

} // namespace BML::Behavior::Detail

#endif // BML_BEHAVIOR_DETAIL_BLOCKS_HPP
