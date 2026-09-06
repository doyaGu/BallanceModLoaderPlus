#ifndef BML_BEHAVIOR_LAYOUT_H
#define BML_BEHAVIOR_LAYOUT_H

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "CKAll.h"
#include "Behavior/Parameter.h"

namespace BML::Behavior::Internal {

struct Status;

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
    bool RequireOnly = false;
    CKGUID ExpectedType;

    static Slot At(SlotKind kind, int index,
                   CKGUID expectedType = CKGUID()) {
        Slot selector;
        selector.Kind = kind;
        selector.Index = index;
        selector.ExpectedType = expectedType;
        return selector;
    }
    static Slot Named(SlotKind kind, std::string name,
                      CKGUID expectedType = CKGUID()) {
        Slot selector;
        selector.Kind = kind;
        selector.Name = std::move(name);
        selector.ExpectedType = expectedType;
        selector.RequireUnique = true;
        return selector;
    }
    static Slot OccurrenceOf(SlotKind kind, std::string name, int occurrence,
                             CKGUID expectedType = CKGUID()) {
        Slot selector;
        selector.Kind = kind;
        selector.Name = std::move(name);
        selector.Occurrence = occurrence;
        selector.ExpectedType = expectedType;
        return selector;
    }
    static Slot Only(SlotKind kind, CKGUID expectedType = CKGUID()) {
        Slot selector;
        selector.Kind = kind;
        selector.RequireOnly = true;
        selector.ExpectedType = expectedType;
        return selector;
    }
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

// A Layout describes which native representation CKBehavior currently uses.
// Execution does not branch on this distinction: CK2 exposes continuation for
// both representations through CKBehavior::IsActive().
enum class BehaviorKind {
    Function,
    Callback,
    Graph,
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
    std::vector<CKGUID> RequiredManagers;
    std::vector<ManagerRequirement> Managers;
    std::vector<SlotInfo> Slots;
};

// Identifies the live native interface of one CKBehavior. It includes the
// identity and declaration of every control and parameter slot, so replacing
// a slot with an equivalent-looking CK object still changes the identity.
// Stored parameter values and execution activity are deliberately excluded.
[[nodiscard]] std::uint64_t LayoutIdentity(CKBehavior *behavior) noexcept;

// Virtools requires a Behavior whose Execute function may create, remove, or
// retype interface elements to declare the corresponding internally-created
// flags. Static Blocks therefore do not need an interface scan around Execute.
[[nodiscard]] bool IsLayoutDynamic(CKBehavior *behavior) noexcept;

// A resolved slot is valid only for one configured live Layout.
struct SlotRef {
    std::uint64_t InstanceId = 0;
    std::uint64_t LayoutGeneration = 0;
    CKObject *Object = nullptr;
    SlotInfo Slot;
};

// The current native Layout of one CKBehavior. This object is deliberately
// short-lived: it borrows the CK objects only while Runtime is handling one
// operation. A full Layout is built only for inspection; Resolve walks only
// the requested slot family and keeps normal execution off that allocation
// path.
class LiveLayout final {
public:
    LiveLayout(CKContext *context, CKBehavior *behavior, CKGUID prototype,
               CKBehaviorPrototype *declaration) noexcept
        : m_Context(context), m_Behavior(behavior), m_Prototype(prototype),
          m_Declaration(declaration) {}

    [[nodiscard]] Layout Describe(
        std::uint64_t generation = 0,
        const Layout *declared = nullptr) const;
    [[nodiscard]] Status Resolve(const Slot &selector, SlotInfo &slot) const;
    [[nodiscard]] CKObject *Object(const SlotInfo &slot) const;
    [[nodiscard]] CKParameter *Parameter(const SlotInfo &slot) const;

private:
    CKContext *m_Context = nullptr;
    CKBehavior *m_Behavior = nullptr;
    CKGUID m_Prototype = CKGUID();
    CKBehaviorPrototype *m_Declaration = nullptr;
};

} // namespace BML::Behavior::Internal

#endif // BML_BEHAVIOR_LAYOUT_H
