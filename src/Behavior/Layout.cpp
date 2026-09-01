#include "Behavior/Layout.h"

#include <utility>

namespace BML::Behavior {

Slot Slot::At(SlotKind kind, int index, CKGUID expectedType) {
    Slot selector;
    selector.Kind = kind;
    selector.Index = index;
    selector.ExpectedType = expectedType;
    return selector;
}

Slot Slot::Named(SlotKind kind, std::string name, CKGUID expectedType) {
    Slot selector;
    selector.Kind = kind;
    selector.Name = std::move(name);
    selector.ExpectedType = expectedType;
    selector.RequireUnique = true;
    return selector;
}

Slot Slot::OccurrenceOf(SlotKind kind, std::string name, int occurrence,
                        CKGUID expectedType) {
    Slot selector;
    selector.Kind = kind;
    selector.Name = std::move(name);
    selector.Occurrence = occurrence;
    selector.ExpectedType = expectedType;
    return selector;
}

Slot Slot::Only(SlotKind kind, CKGUID expectedType) {
    Slot selector;
    selector.Kind = kind;
    selector.RequireOnly = true;
    selector.ExpectedType = expectedType;
    return selector;
}

} // namespace BML::Behavior
