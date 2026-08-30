#include "Behavior/Text2D.h"

#include "BML/Guids/Interface.h"

namespace BML::Behavior::Text2D {

Spec Make(const Options &options) {
    Spec spec(VT_INTERFACE_2DTEXT);
    spec.Target(CKPGUID_2DENTITY, options.Target)
        .Input(Slot::At(SlotKind::InputParameter, 0, CKPGUID_FONT),
               Value::From(CKPGUID_FONT, options.FontIndex))
        .Input(Slot::At(SlotKind::InputParameter, 1, CKPGUID_STRING),
               Value::String(options.Text))
        .Input(Slot::At(SlotKind::InputParameter, 2, CKPGUID_ALIGNMENT),
               Value::From(CKPGUID_ALIGNMENT, options.Alignment))
        .Input(Slot::At(SlotKind::InputParameter, 3, CKPGUID_RECT),
               Value::From(CKPGUID_RECT, options.Margin))
        .Input(Slot::At(SlotKind::InputParameter, 4, CKPGUID_2DVECTOR),
               Value::From(CKPGUID_2DVECTOR, options.Offset))
        .Input(Slot::At(SlotKind::InputParameter, 5, CKPGUID_2DVECTOR),
               Value::From(CKPGUID_2DVECTOR, options.ParagraphIndentation))
        .Input(Slot::At(SlotKind::InputParameter, 6, CKPGUID_MATERIAL),
               Value::Object(CKPGUID_MATERIAL, options.BackgroundMaterial))
        .Input(Slot::At(SlotKind::InputParameter, 7, CKPGUID_PERCENTAGE),
               Value::From(CKPGUID_PERCENTAGE, options.CaretSize))
        .Input(Slot::At(SlotKind::InputParameter, 8, CKPGUID_MATERIAL),
               Value::Object(CKPGUID_MATERIAL, options.CaretMaterial))
        .Setting(Slot::At(SlotKind::Setting, 0, CKPGUID_TEXTPROPERTIES),
                 Value::From(CKPGUID_TEXTPROPERTIES, options.Flags));
    return spec;
}

} // namespace BML::Behavior::Text2D
