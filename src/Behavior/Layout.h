#ifndef BML_BEHAVIOR_LAYOUT_H
#define BML_BEHAVIOR_LAYOUT_H

#include <cstdint>
#include <string>
#include <vector>

#include "CKAll.h"
#include "Behavior/Execution.h"
#include "Behavior/Parameter.h"

namespace BML::Behavior {

enum class SlotKind {
    Input,
    Output,
    InputParameter,
    OutputParameter,
    // Settings share CKBehavior's native local array. Their author-facing
    // index is filtered; ordinary locals retain their native index.
    Setting,
    Local,
    Target,
};

struct Slot {
    SlotKind Kind = SlotKind::InputParameter;
    int Index = -1;
    std::string Name;
    int Occurrence = 0;
    bool RequireUnique = false;
    CKGUID ExpectedType;

    static Slot At(SlotKind kind, int index,
                   CKGUID expectedType = CKGUID());
    static Slot Named(SlotKind kind, std::string name,
                      CKGUID expectedType = CKGUID());
    static Slot OccurrenceOf(SlotKind kind, std::string name, int occurrence,
                             CKGUID expectedType = CKGUID());
    [[nodiscard]] bool UsesName() const noexcept { return !Name.empty(); }
};

struct SlotInfo {
    SlotKind Kind = SlotKind::InputParameter;
    int Index = -1;
    int NativeIndex = -1;
    std::string Name;
    CKGUID Type;
    int DataSize = 0;
    int Occurrence = 0;
    std::string TypeName;
    Parameter::Form ValueForm = Parameter::Form::Unsupported;
    bool Dynamic = false;
};

enum class LayoutOrigin {
    Declared,
    Live,
};

struct ManagerRequirement {
    CKGUID Guid = CKGUID();
    bool Available = false;
};

struct Layout {
    LayoutOrigin Origin = LayoutOrigin::Live;
    CKGUID Prototype = CKGUID();
    std::uint64_t ProviderGeneration = 0;
    std::string PrototypeName;
    std::string Category;
    CKGUID Provider = CKGUID();
    std::string ProviderName;
    std::string Author;
    std::string Description;
    CKDWORD Version = 0;
    CK_CLASSID CompatibleClass = CKCID_BEOBJECT;
    CKGUID TargetType = CKGUID();
    BehaviorKind Kind = BehaviorKind::Function;
    CKDWORD PrototypeFlags = 0;
    CKDWORD BehaviorFlags = 0;
    std::uint64_t Generation = 0;
    bool MaterializedNow = false;
    std::vector<CKGUID> RequiredManagers;
    std::vector<ManagerRequirement> Managers;
    std::vector<SlotInfo> Slots;
};

// A resolved slot is valid only for one configured live Layout.
struct SlotRef {
    std::uint64_t InstanceId = 0;
    std::uint64_t LayoutGeneration = 0;
    CKObject *Object = nullptr;
    SlotInfo Slot;
};

} // namespace BML::Behavior

#endif // BML_BEHAVIOR_LAYOUT_H
