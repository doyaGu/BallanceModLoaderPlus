#include "Behavior/ObjectLoad.h"

#include "BML/Guids/Narratives.h"

namespace BML::Behavior::ObjectLoad {

Spec Make(const Options &options) {
    Spec spec(VT_NARRATIVES_OBJECTLOAD);
    spec.Input(Slot::At(SlotKind::InputParameter, 0, CKPGUID_STRING),
               Value::String(options.File))
        .Input(Slot::At(SlotKind::InputParameter, 1, CKPGUID_STRING),
               Value::String(options.MasterName))
        .Input(Slot::At(SlotKind::InputParameter, 2, CKPGUID_CLASSID),
               Value::From(CKPGUID_CLASSID, options.FilterClass))
        .Input(Slot::At(SlotKind::InputParameter, 3, CKPGUID_BOOL),
               Value::From(CKPGUID_BOOL, options.AddToScene))
        .Input(Slot::At(SlotKind::InputParameter, 4, CKPGUID_BOOL),
               Value::From(CKPGUID_BOOL, options.ReuseMeshes))
        .Input(Slot::At(SlotKind::InputParameter, 5, CKPGUID_BOOL),
               Value::From(CKPGUID_BOOL, options.ReuseMaterials))
        .Setting(Slot::At(SlotKind::Setting, 0, CKPGUID_BOOL),
                 Value::From(CKPGUID_BOOL, options.Dynamic));
    return spec;
}

} // namespace BML::Behavior::ObjectLoad
