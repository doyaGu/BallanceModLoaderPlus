#include "Behavior/Text2DView.h"

#include "BML/Guids/Interface.h"
#include <string>
#include <utility>

#include "Behavior/Layout.h"
#include "Behavior/Parameter.h"

namespace BML::Behavior::Text2DView {
namespace {

// The retail prototype's parameter order. Nothing outside this file needs it.
enum Pin {
    FontPin = 0,
    TextPin = 1,
    AlignmentPin = 2,
    MarginPin = 3,
    OffsetPin = 4,
    IndentationPin = 5,
    BackgroundPin = 6,
    CaretSizePin = 7,
    CaretMaterialPin = 8,
};

constexpr int kFlagsSetting = 0;
constexpr int kDrawInput = 0;

Slot PinSlot(int index, CKGUID type) {
    return Slot::At(SlotKind::InputParameter, index, type);
}

Slot FlagsSlot() {
    return Slot::At(SlotKind::Setting, kFlagsSetting, CKPGUID_TEXTPROPERTIES);
}

Status Failure(Error error, std::string message) {
    return {error, CK_OK, CKBR_OK, std::move(message)};
}

template <typename T>
T Read(CKParameter *parameter, T fallback) {
    if (!parameter || parameter->GetDataSize() != static_cast<int>(sizeof(T)))
        return fallback;
    T value = fallback;
    return parameter->GetValue(&value, FALSE) == CK_OK ? value : fallback;
}

} // namespace

CKBehavior *Add(Runtime &runtime, CKBehavior *graph, const Options &options) {
    const AttachResult added = runtime.AddToGraph(
        graph, Blocks::Text2D::Make(options));
    return added ? added.Block : nullptr;
}

CKParameter *View::Find(const Slot &slot) const {
    if (!*this)
        return nullptr;
    const LiveLayout layout(m_Context, m_Block, m_Block->GetPrototypeGuid(),
                            m_Block->GetPrototype());
    SlotInfo resolved;
    if (!layout.Resolve(slot, resolved))
        return nullptr;
    return layout.Parameter(resolved);
}

Status View::Write(const Slot &slot, const Parameter::Binding &value) {
    CKParameter *parameter = Find(slot);
    if (!parameter)
        return Failure(Error::SlotNotFound,
                       "The 2D Text Block does not hold that Slot.");
    return Parameter::Write(m_Context, parameter, value);
}

Status View::SetFont(int index) {
    return Write(PinSlot(FontPin, CKPGUID_FONT),
                 Value::From(CKPGUID_FONT, index));
}

Status View::SetText(const char *text) {
    return Write(PinSlot(TextPin, CKPGUID_STRING),
                 Value::String(text ? text : ""));
}

Status View::SetAlignment(int alignment) {
    return Write(PinSlot(AlignmentPin, CKPGUID_ALIGNMENT),
                 Value::From(CKPGUID_ALIGNMENT, alignment));
}

Status View::SetOffset(const Vx2DVector &offset) {
    return Write(PinSlot(OffsetPin, CKPGUID_2DVECTOR),
                 Value::From(CKPGUID_2DVECTOR, offset));
}

Status View::SetCaretMaterial(CKMaterial *material) {
    return Write(PinSlot(CaretMaterialPin, CKPGUID_MATERIAL),
                 Parameter::Binding::Object(CKPGUID_MATERIAL, material));
}

Status View::SetFlags(int flags) {
    return Write(FlagsSlot(), Value::From(CKPGUID_TEXTPROPERTIES, flags));
}

int View::Font() const {
    return Read<int>(Find(PinSlot(FontPin, CKPGUID_FONT)), 0);
}

const char *View::Text() const {
    CKParameter *parameter = Find(PinSlot(TextPin, CKPGUID_STRING));
    return parameter ? static_cast<const char *>(parameter->GetReadDataPtr())
                     : nullptr;
}

int View::Flags() const {
    return Read<int>(Find(FlagsSlot()), 0);
}

void View::Draw() {
    if (!*this)
        return;
    m_Block->ActivateInput(kDrawInput);
    m_Block->Execute(0);
}

} // namespace BML::Behavior::Text2DView
