// Included once by ScriptBehavior.cpp after its private wrapper types are defined.

static std::string RegistrationError;

template <class T>
void RegisterRefBehaviors(asIScriptEngine *engine, const char *name,
                          int &code) {
    if (code < 0)
        return;
    code = engine->RegisterObjectBehaviour(
        name, asBEHAVE_ADDREF, "void f()", asMETHOD(T, AddRef),
        asCALL_THISCALL);
    if (code >= 0) {
        code = engine->RegisterObjectBehaviour(
            name, asBEHAVE_RELEASE, "void f()", asMETHOD(T, Release),
            asCALL_THISCALL);
    }
}

bool RequireType(asIScriptEngine *engine, const char *type,
                 const char **errorMessage) {
    if (engine->GetTypeInfoByName(type))
        return true;
    RegistrationError = "BML Behavior bindings require CKAngelScript type ";
    RegistrationError += type;
    RegistrationError += ".";
    if (errorMessage)
        *errorMessage = RegistrationError.c_str();
    return false;
}

bool Registered(asIScriptEngine *engine, int code, const char *what,
                const char **errorMessage) {
    if (code >= 0)
        return true;
    RegistrationError = "Failed to register ";
    RegistrationError += what;
    RegistrationError += " (AngelScript code ";
    RegistrationError += std::to_string(code);
    RegistrationError += ").";
    engine->SetDefaultNamespace("BML");
    if (errorMessage)
        *errorMessage = RegistrationError.c_str();
    return false;
}

#define BML_BEHAVIOR_REGISTER(expr, what)                                        \
    do {                                                                          \
        const int registrationCode = (expr);                                      \
        if (!Registered(engine, registrationCode, what, errorMessage))            \
            return registrationCode;                                              \
    } while (false)

#define BML_BEHAVIOR_REF(type, name)                                              \
    BML_BEHAVIOR_REGISTER(engine->RegisterObjectType(                             \
        name, 0, asOBJ_REF), "BML::Behavior::" name);                            \
    do {                                                                          \
        int registrationCode = asSUCCESS;                                         \
        RegisterRefBehaviors<type>(engine, name, registrationCode);               \
        if (!Registered(engine, registrationCode,                                 \
                        "BML::Behavior::" name " reference behavior",            \
                        errorMessage))                                             \
            return registrationCode;                                              \
    } while (false)

#define BML_BEHAVIOR_METHOD(type, scriptType, declaration, method)                \
    BML_BEHAVIOR_REGISTER(engine->RegisterObjectMethod(                           \
        scriptType, declaration, BML_AS_GENERIC_METHOD(method),                   \
        asCALL_GENERIC), "BML::Behavior::" scriptType "::" declaration)

#define BML_BEHAVIOR_DIRECT_METHOD(type, scriptType, declaration, method)         \
    BML_BEHAVIOR_REGISTER(engine->RegisterObjectMethod(                           \
        scriptType, declaration, asMETHOD(type, method), asCALL_THISCALL),         \
        "BML::Behavior::" scriptType "::" declaration)

#define BML_BEHAVIOR_GENERIC_CONSTRUCTOR(scriptType, declaration, function, what) \
    BML_BEHAVIOR_REGISTER(engine->RegisterObjectBehaviour(                        \
        scriptType, asBEHAVE_CONSTRUCT, declaration,                              \
        BML_AS_GENERIC_OBJECT_FIRST_FUNCTION(function), asCALL_GENERIC), what)

int RegisterScriptBehavior(asIScriptEngine *engine, const char **errorMessage) {
    if (!engine)
        return asINVALID_ARG;
    const char *required[] = {
        "CKGUID", "CKObject", "CKBeObject", "CKBehavior", "Vx2DVector",
        "VxVector", "VxQuaternion", "VxRect", "VxColor", "VxBbox",
        "VxMatrix", "array"};
    for (const char *type : required) {
        if (!RequireType(engine, type, errorMessage)) {
            engine->SetDefaultNamespace("BML");
            return asINVALID_TYPE;
        }
    }

    BML_BEHAVIOR_REGISTER(engine->SetDefaultNamespace("BML::Behavior"),
                          "namespace BML::Behavior");

    BML_BEHAVIOR_REGISTER(engine->RegisterObjectType(
        "Selector", sizeof(ScriptBehaviorSelector),
        asOBJ_VALUE | asGetTypeTraits<ScriptBehaviorSelector>()),
        "BML::Behavior::Selector");
    BML_BEHAVIOR_REGISTER(engine->RegisterObjectBehaviour(
        "Selector", asBEHAVE_CONSTRUCT, "void f()",
        asFUNCTION(ConstructSelector), asCALL_CDECL_OBJLAST),
        "BML::Behavior::Selector default constructor");
    BML_BEHAVIOR_REGISTER(engine->RegisterObjectBehaviour(
        "Selector", asBEHAVE_CONSTRUCT, "void f(const Selector &in)",
        asFUNCTION(CopySelector), asCALL_CDECL_OBJLAST),
        "BML::Behavior::Selector copy constructor");
    BML_BEHAVIOR_REGISTER(engine->RegisterObjectBehaviour(
        "Selector", asBEHAVE_DESTRUCT, "void f()",
        asFUNCTION(DestructSelector), asCALL_CDECL_OBJLAST),
        "BML::Behavior::Selector destructor");
    BML_BEHAVIOR_REGISTER(engine->RegisterObjectMethod(
        "Selector", "Selector &opAssign(const Selector &in)",
        asFUNCTION(AssignSelector), asCALL_CDECL_OBJLAST),
        "BML::Behavior::Selector assignment");

    BML_BEHAVIOR_REGISTER(engine->RegisterObjectType(
        "FramePolicy", sizeof(ScriptBehaviorFramePolicy),
        asOBJ_VALUE | asGetTypeTraits<ScriptBehaviorFramePolicy>()),
        "BML::Behavior::FramePolicy");
    BML_BEHAVIOR_REGISTER(engine->RegisterObjectBehaviour(
        "FramePolicy", asBEHAVE_CONSTRUCT, "void f()",
        asFUNCTION(ConstructFramePolicy), asCALL_CDECL_OBJLAST),
        "BML::Behavior::FramePolicy default constructor");
    BML_BEHAVIOR_REGISTER(engine->RegisterObjectBehaviour(
        "FramePolicy", asBEHAVE_CONSTRUCT,
        "void f(const FramePolicy &in)", asFUNCTION(CopyFramePolicy),
        asCALL_CDECL_OBJLAST),
        "BML::Behavior::FramePolicy copy constructor");
    BML_BEHAVIOR_REGISTER(engine->RegisterObjectBehaviour(
        "FramePolicy", asBEHAVE_DESTRUCT, "void f()",
        asFUNCTION(DestructFramePolicy), asCALL_CDECL_OBJLAST),
        "BML::Behavior::FramePolicy destructor");
    BML_BEHAVIOR_REGISTER(engine->RegisterObjectMethod(
        "FramePolicy", "FramePolicy &opAssign(const FramePolicy &in)",
        asFUNCTION(AssignFramePolicy), asCALL_CDECL_OBJLAST),
        "BML::Behavior::FramePolicy assignment");
    BML_BEHAVIOR_METHOD(ScriptBehaviorFramePolicy, "FramePolicy",
                        "FramePolicy Pouts(bool include = true) const",
                        &ScriptBehaviorFramePolicy::Pouts);

    BML_BEHAVIOR_REGISTER(engine->RegisterEnum("ValueKind"),
                          "BML::Behavior::ValueKind");
#define BML_REGISTER_VALUE_KIND(name, value)                                     \
    BML_BEHAVIOR_REGISTER(engine->RegisterEnumValue(                             \
        "ValueKind", name, static_cast<int>(Authoring::ValueKind::value)),      \
        "BML::Behavior::ValueKind::" name)
    BML_REGISTER_VALUE_KIND("Bool", Bool);
    BML_REGISTER_VALUE_KIND("Int", Int32);
    BML_REGISTER_VALUE_KIND("Float", Float32);
    BML_REGISTER_VALUE_KIND("String", Utf8);
    BML_REGISTER_VALUE_KIND("Vec2", Vec2);
    BML_REGISTER_VALUE_KIND("Vec3", Vec3);
    BML_REGISTER_VALUE_KIND("Quaternion", Quaternion);
    BML_REGISTER_VALUE_KIND("Euler", Euler);
    BML_REGISTER_VALUE_KIND("Rect", Rect);
    BML_REGISTER_VALUE_KIND("Color", Color);
    BML_REGISTER_VALUE_KIND("Box", Box);
    BML_REGISTER_VALUE_KIND("Matrix", Mat4);
    BML_REGISTER_VALUE_KIND("Object", Object);
    BML_REGISTER_VALUE_KIND("ObjectList", ObjectList);
#undef BML_REGISTER_VALUE_KIND

    BML_BEHAVIOR_REGISTER(engine->RegisterEnum("SlotKind"),
                          "BML::Behavior::SlotKind");
#define BML_REGISTER_SLOT_KIND(name)                                             \
    BML_BEHAVIOR_REGISTER(engine->RegisterEnumValue(                             \
        "SlotKind", #name, static_cast<int>(Authoring::SlotKind::name)),       \
        "BML::Behavior::SlotKind::" #name)
    BML_REGISTER_SLOT_KIND(In);
    BML_REGISTER_SLOT_KIND(Out);
    BML_REGISTER_SLOT_KIND(Pin);
    BML_REGISTER_SLOT_KIND(Pout);
    BML_REGISTER_SLOT_KIND(Setting);
    BML_REGISTER_SLOT_KIND(Local);
    BML_REGISTER_SLOT_KIND(Target);
#undef BML_REGISTER_SLOT_KIND

    BML_BEHAVIOR_REGISTER(engine->RegisterEnum("LayoutOrigin"),
                          "BML::Behavior::LayoutOrigin");
    BML_BEHAVIOR_REGISTER(engine->RegisterEnumValue(
        "LayoutOrigin", "Declared",
        static_cast<int>(Authoring::LayoutOrigin::Declared)),
        "BML::Behavior::LayoutOrigin::Declared");
    BML_BEHAVIOR_REGISTER(engine->RegisterEnumValue(
        "LayoutOrigin", "Live", static_cast<int>(Authoring::LayoutOrigin::Live)),
        "BML::Behavior::LayoutOrigin::Live");

    BML_BEHAVIOR_REGISTER(engine->RegisterEnum("BehaviorKind"),
                          "BML::Behavior::BehaviorKind");
#define BML_REGISTER_BEHAVIOR_KIND(name)                                         \
    BML_BEHAVIOR_REGISTER(engine->RegisterEnumValue(                             \
        "BehaviorKind", #name,                                                 \
        static_cast<int>(Authoring::BehaviorKind::name)),                        \
        "BML::Behavior::BehaviorKind::" #name)
    BML_REGISTER_BEHAVIOR_KIND(Function);
    BML_REGISTER_BEHAVIOR_KIND(Callback);
    BML_REGISTER_BEHAVIOR_KIND(Graph);
#undef BML_REGISTER_BEHAVIOR_KIND

    BML_BEHAVIOR_REGISTER(engine->RegisterEnum("Relation"),
                          "BML::Behavior::Relation");
#define BML_REGISTER_RELATION(name)                                              \
    BML_BEHAVIOR_REGISTER(engine->RegisterEnumValue(                             \
        "Relation", #name, static_cast<int>(Authoring::Relation::name)),       \
        "BML::Behavior::Relation::" #name)
    BML_REGISTER_RELATION(Stored);
    BML_REGISTER_RELATION(Direct);
    BML_REGISTER_RELATION(Shared);
    BML_REGISTER_RELATION(Operation);
#undef BML_REGISTER_RELATION

#define BML_REGISTER_BEHAVIOR_ENUM(name)                                         \
    BML_BEHAVIOR_REGISTER(engine->RegisterEnum(#name),                          \
                          "BML::Behavior::" #name)
#define BML_REGISTER_BEHAVIOR_ENUM_VALUE(type, name, value)                      \
    BML_BEHAVIOR_REGISTER(engine->RegisterEnumValue(                            \
        #type, #name, static_cast<int>(Authoring::type::value)),                \
        "BML::Behavior::" #type "::" #name)

    BML_REGISTER_BEHAVIOR_ENUM(Error);
#define BML_REGISTER_ERROR(name)                                                 \
    BML_REGISTER_BEHAVIOR_ENUM_VALUE(Error, name, name)
    BML_REGISTER_ERROR(None);
    BML_REGISTER_ERROR(OwnerUnavailable);
    BML_REGISTER_ERROR(PrototypeNotFound);
    BML_REGISTER_ERROR(RequiredManagerMissing);
    BML_REGISTER_ERROR(CreationFailed);
    BML_REGISTER_ERROR(InitializationFailed);
    BML_REGISTER_ERROR(TargetInvalid);
    BML_REGISTER_ERROR(CallbackFailed);
    BML_REGISTER_ERROR(SlotNotFound);
    BML_REGISTER_ERROR(SlotAmbiguous);
    BML_REGISTER_ERROR(LayoutChanged);
    BML_REGISTER_ERROR(TypeMismatch);
    BML_REGISTER_ERROR(ValueInvalid);
    BML_REGISTER_ERROR(StateInvalid);
    BML_REGISTER_ERROR(NativeError);
    BML_REGISTER_ERROR(BreakUnsupported);
    BML_REGISTER_ERROR(PoutUnsupported);
    BML_REGISTER_ERROR(PoutUnavailable);
    BML_REGISTER_ERROR(FrameQueueFull);
    BML_REGISTER_ERROR(Cancelled);
    BML_REGISTER_ERROR(PrototypeChanged);
    BML_REGISTER_ERROR(PrototypeLoadFailed);
    BML_REGISTER_ERROR(LayoutUnavailable);
    BML_REGISTER_ERROR(ParameterTypeUnavailable);
    BML_REGISTER_ERROR(ParameterTypeUnsupported);
    BML_REGISTER_ERROR(DetachedUnsupported);
    BML_REGISTER_ERROR(ObserverUnavailable);
    BML_REGISTER_ERROR(GraphChanged);
    BML_REGISTER_ERROR(GraphLocalityInvalid);
    BML_REGISTER_ERROR(DelayInvalid);
    BML_REGISTER_ERROR(SameFrameCycle);
    BML_REGISTER_ERROR(SharedSourceCycle);
    BML_REGISTER_ERROR(PushCycle);
    BML_REGISTER_ERROR(InterfaceUnsupported);
    BML_REGISTER_ERROR(SourceConflict);
    BML_REGISTER_ERROR(SourceOrderCycle);
    BML_REGISTER_ERROR(OrderingTargetMismatch);
    BML_REGISTER_ERROR(OverlayOrderCycle);
    BML_REGISTER_ERROR(LinkNotFound);
    BML_REGISTER_ERROR(PathAmbiguous);
    BML_REGISTER_ERROR(PathCycle);
    BML_REGISTER_ERROR(QueryNotFound);
    BML_REGISTER_ERROR(QueryAmbiguous);
    BML_REGISTER_ERROR(WorldBoundValue);
    BML_REGISTER_ERROR(RevertConflict);
    BML_REGISTER_ERROR(TargetCardinality);
    BML_REGISTER_ERROR(SourceInvalid);
    BML_REGISTER_ERROR(OperationInvalid);
    BML_REGISTER_ERROR(Busy);
    BML_REGISTER_ERROR(Unavailable);
    BML_REGISTER_ERROR(WrongThread);
    BML_REGISTER_ERROR(RedirectConflict);
#undef BML_REGISTER_ERROR

    BML_REGISTER_BEHAVIOR_ENUM(RunState);
    BML_REGISTER_BEHAVIOR_ENUM_VALUE(RunState, Ready, Ready);
    BML_REGISTER_BEHAVIOR_ENUM_VALUE(RunState, Pending, Pending);
    BML_REGISTER_BEHAVIOR_ENUM_VALUE(RunState, Failed, Failed);
    BML_REGISTER_BEHAVIOR_ENUM(PulseResult);
    BML_REGISTER_BEHAVIOR_ENUM_VALUE(PulseResult, Ran, Ran);
    BML_REGISTER_BEHAVIOR_ENUM_VALUE(PulseResult, Queued, Queued);
    BML_REGISTER_BEHAVIOR_ENUM(Continuation);
    BML_REGISTER_BEHAVIOR_ENUM_VALUE(Continuation, None, None);
    BML_REGISTER_BEHAVIOR_ENUM_VALUE(Continuation, Native, Native);
    BML_REGISTER_BEHAVIOR_ENUM_VALUE(Continuation, QueuedInput, QueuedInput);
    BML_REGISTER_BEHAVIOR_ENUM(View);
    BML_REGISTER_BEHAVIOR_ENUM_VALUE(View, Logical, Logical);
    BML_REGISTER_BEHAVIOR_ENUM_VALUE(View, Live, Live);
    BML_REGISTER_BEHAVIOR_ENUM(ObservationState);
    BML_REGISTER_BEHAVIOR_ENUM_VALUE(ObservationState, Available, Available);
    BML_REGISTER_BEHAVIOR_ENUM_VALUE(ObservationState, Indeterminate,
                                     Indeterminate);
    BML_REGISTER_BEHAVIOR_ENUM_VALUE(ObservationState, Unsupported,
                                     Unsupported);
    BML_REGISTER_BEHAVIOR_ENUM(WatchState);
    BML_REGISTER_BEHAVIOR_ENUM_VALUE(WatchState, Active, Active);
    BML_REGISTER_BEHAVIOR_ENUM_VALUE(WatchState, Failed, Failed);
    BML_REGISTER_BEHAVIOR_ENUM(PatchState);
    BML_REGISTER_BEHAVIOR_ENUM_VALUE(PatchState, Pending, Pending);
    BML_REGISTER_BEHAVIOR_ENUM_VALUE(PatchState, Active, Active);
    BML_REGISTER_BEHAVIOR_ENUM_VALUE(PatchState, Disabled, Disabled);
    BML_REGISTER_BEHAVIOR_ENUM_VALUE(PatchState, Closing, Closing);
    BML_REGISTER_BEHAVIOR_ENUM_VALUE(PatchState, Conflicted, Conflicted);
    BML_REGISTER_BEHAVIOR_ENUM_VALUE(PatchState, Closed, Closed);
    BML_REGISTER_BEHAVIOR_ENUM_VALUE(PatchState, Failed, Failed);
    BML_REGISTER_BEHAVIOR_ENUM(PlanState);
    BML_REGISTER_BEHAVIOR_ENUM_VALUE(PlanState, Reconciling, Reconciling);
    BML_REGISTER_BEHAVIOR_ENUM_VALUE(PlanState, Active, Active);
    BML_REGISTER_BEHAVIOR_ENUM_VALUE(PlanState, Partial, Partial);
    BML_REGISTER_BEHAVIOR_ENUM_VALUE(PlanState, Unsatisfied, Unsatisfied);
    BML_REGISTER_BEHAVIOR_ENUM_VALUE(PlanState, Disabled, Disabled);
    BML_REGISTER_BEHAVIOR_ENUM_VALUE(PlanState, Conflicted, Conflicted);
    BML_REGISTER_BEHAVIOR_ENUM_VALUE(PlanState, Retiring, Retiring);
    BML_REGISTER_BEHAVIOR_ENUM(ScriptState);
    BML_REGISTER_BEHAVIOR_ENUM_VALUE(ScriptState, Ready, Ready);
    BML_REGISTER_BEHAVIOR_ENUM_VALUE(ScriptState, Closing, Closing);
    BML_REGISTER_BEHAVIOR_ENUM_VALUE(ScriptState, Failed, Failed);
    BML_REGISTER_BEHAVIOR_ENUM(CloseState);
    BML_REGISTER_BEHAVIOR_ENUM_VALUE(CloseState, Closing, Closing);
    BML_REGISTER_BEHAVIOR_ENUM_VALUE(CloseState, Closed, Closed);

#undef BML_REGISTER_BEHAVIOR_ENUM_VALUE
#undef BML_REGISTER_BEHAVIOR_ENUM

    BML_BEHAVIOR_REGISTER(engine->RegisterObjectType(
        "Value", sizeof(ScriptBehaviorValue),
        asOBJ_VALUE | asGetTypeTraits<ScriptBehaviorValue>()),
        "BML::Behavior::Value");
    BML_BEHAVIOR_REGISTER(engine->RegisterObjectBehaviour(
        "Value", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(ConstructValue),
        asCALL_CDECL_OBJLAST), "BML::Behavior::Value default constructor");
#define BML_REGISTER_VALUE_CTOR(declaration, function, description)              \
    BML_BEHAVIOR_REGISTER(engine->RegisterObjectBehaviour(                       \
        "Value", asBEHAVE_CONSTRUCT, declaration, asFUNCTION(function),        \
        asCALL_CDECL_OBJLAST), description)
    BML_REGISTER_VALUE_CTOR("void f(bool value)", ConstructValueBool,
                            "BML::Behavior::Value bool constructor");
    BML_REGISTER_VALUE_CTOR("void f(int value)", ConstructValueInt,
                            "BML::Behavior::Value int constructor");
    BML_REGISTER_VALUE_CTOR("void f(float value)", ConstructValueFloat,
                            "BML::Behavior::Value float constructor");
    // Script strings are CKAngelScript-owned objects. Marshal them through the
    // generic boundary instead of treating them as native std::string values.
    BML_BEHAVIOR_GENERIC_CONSTRUCTOR(
        "Value", "void f(const string &in value)", &ConstructValueString,
        "BML::Behavior::Value string constructor");
    BML_REGISTER_VALUE_CTOR("void f(const Vx2DVector &in value)", ConstructValueVec2,
                            "BML::Behavior::Value Vec2 constructor");
    BML_REGISTER_VALUE_CTOR("void f(const VxVector &in value)", ConstructValueVec3,
                            "BML::Behavior::Value Vec3 constructor");
    BML_REGISTER_VALUE_CTOR("void f(const VxQuaternion &in value)", ConstructValueQuaternion,
                            "BML::Behavior::Value Quaternion constructor");
    BML_REGISTER_VALUE_CTOR("void f(const VxRect &in value)", ConstructValueRect,
                            "BML::Behavior::Value Rect constructor");
    BML_REGISTER_VALUE_CTOR("void f(const VxColor &in value)", ConstructValueColor,
                            "BML::Behavior::Value Color constructor");
    BML_REGISTER_VALUE_CTOR("void f(const VxBbox &in value)", ConstructValueBox,
                            "BML::Behavior::Value Box constructor");
    BML_REGISTER_VALUE_CTOR("void f(const VxMatrix &in value)", ConstructValueMatrix,
                            "BML::Behavior::Value Matrix constructor");
    BML_REGISTER_VALUE_CTOR("void f(const CKGUID &in type, CKObject@ object)",
                            ConstructValueObject,
                            "BML::Behavior::Value object constructor");
    BML_REGISTER_VALUE_CTOR("void f(const Value &in other)", CopyValue,
                            "BML::Behavior::Value copy constructor");
#undef BML_REGISTER_VALUE_CTOR
    BML_BEHAVIOR_REGISTER(engine->RegisterObjectBehaviour(
        "Value", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(DestructValue),
        asCALL_CDECL_OBJLAST), "BML::Behavior::Value destructor");
    BML_BEHAVIOR_REGISTER(engine->RegisterObjectMethod(
        "Value", "Value &opAssign(const Value &in)", asFUNCTION(AssignValue),
        asCALL_CDECL_OBJLAST), "BML::Behavior::Value assignment");
    BML_BEHAVIOR_METHOD(ScriptBehaviorValue, "Value", "CKGUID get_Type() const",
                        &ScriptBehaviorValue::Type);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorValue, "Value",
                               "ValueKind get_Kind() const", Kind);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorValue, "Value",
                               "bool get_IsNull() const", IsNull);

    BML_BEHAVIOR_REGISTER(engine->RegisterObjectType(
        "SlotValue", sizeof(ScriptBehaviorSlotValue),
        asOBJ_VALUE | asGetTypeTraits<ScriptBehaviorSlotValue>()),
        "BML::Behavior::SlotValue");
    BML_BEHAVIOR_REGISTER(engine->RegisterObjectBehaviour(
        "SlotValue", asBEHAVE_CONSTRUCT, "void f()",
        asFUNCTION(ConstructSlotValue), asCALL_CDECL_OBJLAST),
        "BML::Behavior::SlotValue default constructor");
    BML_BEHAVIOR_REGISTER(engine->RegisterObjectBehaviour(
        "SlotValue", asBEHAVE_CONSTRUCT,
        "void f(const Selector &in slot, const Value &in value)",
        asFUNCTION(ConstructSlotValueAt), asCALL_CDECL_OBJLAST),
        "BML::Behavior::SlotValue selector constructor");
    BML_BEHAVIOR_GENERIC_CONSTRUCTOR(
        "SlotValue",
        "void f(const string &in slot, const Value &in value)",
        &ConstructSlotValueNamed,
        "BML::Behavior::SlotValue name constructor");
    BML_BEHAVIOR_REGISTER(engine->RegisterObjectBehaviour(
        "SlotValue", asBEHAVE_CONSTRUCT, "void f(const SlotValue &in other)",
        asFUNCTION(CopySlotValue), asCALL_CDECL_OBJLAST),
        "BML::Behavior::SlotValue copy constructor");
    BML_BEHAVIOR_REGISTER(engine->RegisterObjectBehaviour(
        "SlotValue", asBEHAVE_DESTRUCT, "void f()",
        asFUNCTION(DestructSlotValue), asCALL_CDECL_OBJLAST),
        "BML::Behavior::SlotValue destructor");
    BML_BEHAVIOR_REGISTER(engine->RegisterObjectMethod(
        "SlotValue", "SlotValue &opAssign(const SlotValue &in)",
        asFUNCTION(AssignSlotValue), asCALL_CDECL_OBJLAST),
        "BML::Behavior::SlotValue assignment");

    BML_BEHAVIOR_REGISTER(engine->RegisterObjectType(
        "NodePattern", sizeof(ScriptBehaviorNodePattern),
        asOBJ_VALUE | asGetTypeTraits<ScriptBehaviorNodePattern>()),
        "BML::Behavior::NodePattern");
    BML_BEHAVIOR_REGISTER(engine->RegisterObjectBehaviour(
        "NodePattern", asBEHAVE_CONSTRUCT, "void f()",
        asFUNCTION(ConstructNodePattern), asCALL_CDECL_OBJLAST),
        "BML::Behavior::NodePattern default constructor");
    BML_BEHAVIOR_REGISTER(engine->RegisterObjectBehaviour(
        "NodePattern", asBEHAVE_CONSTRUCT,
        "void f(const Selector &in selector)",
        asFUNCTION(ConstructNodePatternSelector), asCALL_CDECL_OBJLAST),
        "BML::Behavior::NodePattern selector constructor");
    BML_BEHAVIOR_GENERIC_CONSTRUCTOR(
        "NodePattern", "void f(const string &in name)",
        &ConstructNodePatternName,
        "BML::Behavior::NodePattern name constructor");
    BML_BEHAVIOR_REGISTER(engine->RegisterObjectBehaviour(
        "NodePattern", asBEHAVE_CONSTRUCT,
        "void f(const NodePattern &in other)", asFUNCTION(CopyNodePattern),
        asCALL_CDECL_OBJLAST),
        "BML::Behavior::NodePattern copy constructor");
    BML_BEHAVIOR_REGISTER(engine->RegisterObjectBehaviour(
        "NodePattern", asBEHAVE_DESTRUCT, "void f()",
        asFUNCTION(DestructNodePattern), asCALL_CDECL_OBJLAST),
        "BML::Behavior::NodePattern destructor");
    BML_BEHAVIOR_REGISTER(engine->RegisterObjectMethod(
        "NodePattern", "NodePattern &opAssign(const NodePattern &in)",
        asFUNCTION(AssignNodePattern), asCALL_CDECL_OBJLAST),
        "BML::Behavior::NodePattern assignment");
#define BML_REGISTER_PATTERN_METHOD(declaration, method)                         \
    BML_BEHAVIOR_METHOD(ScriptBehaviorNodePattern, "NodePattern", declaration, \
                        &ScriptBehaviorNodePattern::method)
    BML_REGISTER_PATTERN_METHOD(
        "NodePattern &Prototype(const CKGUID &in prototype)", Prototype);
    BML_REGISTER_PATTERN_METHOD(
        "NodePattern &Kind(BehaviorKind kind)", Kind);
    BML_REGISTER_PATTERN_METHOD("NodePattern &Ins(int count)", Ins);
    BML_REGISTER_PATTERN_METHOD("NodePattern &Outs(int count)", Outs);
    BML_REGISTER_PATTERN_METHOD("NodePattern &Pins(int count)", Pins);
    BML_REGISTER_PATTERN_METHOD("NodePattern &Pouts(int count)", Pouts);
    BML_REGISTER_PATTERN_METHOD("NodePattern &Settings(int count)", Settings);
    BML_REGISTER_PATTERN_METHOD("NodePattern &Locals(int count)", Locals);
    BML_REGISTER_PATTERN_METHOD(
        "NodePattern &Pin(const Selector &in slot, const Value &in value)", Pin);
    BML_REGISTER_PATTERN_METHOD(
        "NodePattern &Pout(const Selector &in slot, const Value &in value)", Pout);
    BML_REGISTER_PATTERN_METHOD(
        "NodePattern &Setting(const Selector &in slot, const Value &in value)",
        Setting);
    BML_REGISTER_PATTERN_METHOD(
        "NodePattern &Local(const Selector &in slot, const Value &in value)",
        Local);
    BML_REGISTER_PATTERN_METHOD(
        "NodePattern &Target(const Value &in value)", Target);
#undef BML_REGISTER_PATTERN_METHOD

    BML_BEHAVIOR_REGISTER(engine->RegisterEnum("HookResult"),
                          "BML::Behavior::HookResult");
    BML_BEHAVIOR_REGISTER(engine->RegisterEnumValue(
        "HookResult", "Error", static_cast<int>(Authoring::HookResult::Error)),
        "BML::Behavior::HookResult::Error");
    BML_BEHAVIOR_REGISTER(engine->RegisterEnumValue(
        "HookResult", "Ok", static_cast<int>(Authoring::HookResult::Ok)),
        "BML::Behavior::HookResult::Ok");
    BML_BEHAVIOR_REGISTER(engine->RegisterEnumValue(
        "HookResult", "AgainNextFrame",
        static_cast<int>(Authoring::HookResult::AgainNextFrame)),
        "BML::Behavior::HookResult::AgainNextFrame");
    BML_BEHAVIOR_REGISTER(engine->RegisterEnumValue(
        "HookResult", "Fault", static_cast<int>(Authoring::HookResult::Fault)),
        "BML::Behavior::HookResult::Fault");

    BML_BEHAVIOR_REGISTER(engine->RegisterEnum("ChangeKind"),
                          "BML::Behavior::ChangeKind");
    BML_BEHAVIOR_REGISTER(engine->RegisterEnumValue(
        "ChangeKind", "Graph", static_cast<int>(Authoring::ChangeKind::Graph)),
        "BML::Behavior::ChangeKind::Graph");
    BML_BEHAVIOR_REGISTER(engine->RegisterEnumValue(
        "ChangeKind", "Layout", static_cast<int>(Authoring::ChangeKind::Layout)),
        "BML::Behavior::ChangeKind::Layout");
    BML_BEHAVIOR_REGISTER(engine->RegisterEnumValue(
        "ChangeKind", "SampledValue",
        static_cast<int>(Authoring::ChangeKind::SampledValue)),
        "BML::Behavior::ChangeKind::SampledValue");

#define BML_REGISTER_BEHAVIOR_VALUE(type, name)                                  \
    BML_BEHAVIOR_REGISTER(engine->RegisterObjectType(                            \
        name, sizeof(type), asOBJ_VALUE | asGetTypeTraits<type>()),             \
        "BML::Behavior::" name);                                                \
    BML_BEHAVIOR_REGISTER(engine->RegisterObjectBehaviour(                       \
        name, asBEHAVE_CONSTRUCT, "void f()",                                  \
        asFUNCTION((ConstructBehaviorValue<type>)), asCALL_CDECL_OBJLAST),      \
        "BML::Behavior::" name " default constructor");                        \
    BML_BEHAVIOR_REGISTER(engine->RegisterObjectBehaviour(                       \
        name, asBEHAVE_CONSTRUCT, "void f(const " name " &in)",               \
        asFUNCTION((CopyBehaviorValue<type>)), asCALL_CDECL_OBJLAST),           \
        "BML::Behavior::" name " copy constructor");                           \
    BML_BEHAVIOR_REGISTER(engine->RegisterObjectBehaviour(                       \
        name, asBEHAVE_DESTRUCT, "void f()",                                   \
        asFUNCTION((DestructBehaviorValue<type>)), asCALL_CDECL_OBJLAST),       \
        "BML::Behavior::" name " destructor");                                 \
    BML_BEHAVIOR_REGISTER(engine->RegisterObjectMethod(                          \
        name, name " &opAssign(const " name " &in)",                          \
        asFUNCTION((AssignBehaviorValue<type>)), asCALL_CDECL_OBJLAST),         \
        "BML::Behavior::" name " assignment")
    BML_REGISTER_BEHAVIOR_VALUE(ScriptBehaviorHookEvent, "HookEvent");
    BML_REGISTER_BEHAVIOR_VALUE(ScriptBehaviorChange, "Change");
#undef BML_REGISTER_BEHAVIOR_VALUE

    BML_BEHAVIOR_REF(ScriptBehaviorBlock, "Block");
    BML_BEHAVIOR_REF(ScriptBehaviorCall, "Call");
    BML_BEHAVIOR_REF(ScriptBehaviorTask, "Task");
    BML_BEHAVIOR_REF(ScriptBehaviorInstance, "Instance");
    BML_BEHAVIOR_REF(ScriptBehaviorFrames, "Frames");
    BML_BEHAVIOR_REF(ScriptBehaviorFrame, "Frame");
    BML_BEHAVIOR_REF(ScriptBehaviorObjectRef, "ObjectRef");
    BML_BEHAVIOR_REF(ScriptBehaviorObjectList, "ObjectList");
    BML_BEHAVIOR_REF(ScriptBehaviorLayout, "Layout");
    BML_BEHAVIOR_REF(ScriptBehaviorSlot, "Slot");
    BML_BEHAVIOR_REF(ScriptBehaviorGraph, "Graph");
    BML_BEHAVIOR_REF(ScriptBehaviorNode, "Node");
    BML_BEHAVIOR_REF(ScriptBehaviorPort, "Port");
    BML_BEHAVIOR_REF(ScriptBehaviorLink, "Link");
    BML_BEHAVIOR_REF(ScriptBehaviorObservedValue, "ObservedValue");
    BML_BEHAVIOR_REF(ScriptBehaviorWatch, "Watch");
    BML_BEHAVIOR_REF(ScriptBehaviorEdit, "Edit");
    BML_BEHAVIOR_REF(ScriptBehaviorEditGraph, "EditGraph");
    BML_BEHAVIOR_REF(ScriptBehaviorEditNode, "EditNode");
    BML_BEHAVIOR_REF(ScriptBehaviorEditNodes, "EditNodes");
    BML_BEHAVIOR_REF(ScriptBehaviorEditPort, "EditPort");
    BML_BEHAVIOR_REF(ScriptBehaviorEditPorts, "EditPorts");
    BML_BEHAVIOR_REF(ScriptBehaviorEditLink, "EditLink");
    BML_BEHAVIOR_REF(ScriptBehaviorEditPath, "EditPath");
    BML_BEHAVIOR_REF(ScriptBehaviorEditOperation, "Operation");
    BML_BEHAVIOR_REF(ScriptBehaviorPatch, "Patch");
    BML_BEHAVIOR_REF(ScriptBehaviorPlan, "Plan");
    BML_BEHAVIOR_REF(ScriptBehaviorScript, "Script");

    BML_BEHAVIOR_REGISTER(engine->RegisterFuncdef(
        "HookResult HookCallback(const BML::ModContext &in, const HookEvent &in)"),
        "BML::Behavior::HookCallback");
    BML_BEHAVIOR_REGISTER(engine->RegisterFuncdef(
        "void WatchCallback(const BML::ModContext &in, const Change &in)"),
        "BML::Behavior::WatchCallback");

    BML_BEHAVIOR_REGISTER(engine->RegisterObjectBehaviour(
        "Edit", asBEHAVE_FACTORY, "Edit@ f()",
        BML_AS_GENERIC_FUNCTION(&CurrentEdit), asCALL_GENERIC),
        "BML::Behavior::Edit factory");

    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorBlock, "Block",
                               "bool get_IsValid() const", IsValid);
    BML_BEHAVIOR_METHOD(ScriptBehaviorBlock, "Block", "Block@ Clone() const",
                        &ScriptBehaviorBlock::Clone);
    BML_BEHAVIOR_METHOD(ScriptBehaviorBlock, "Block", "Block@ TargetOwner()",
                        &ScriptBehaviorBlock::TargetOwner);
    BML_BEHAVIOR_METHOD(ScriptBehaviorBlock, "Block",
                        "Block@ Target(const CKGUID &in type, CKObject@ object)",
                        &ScriptBehaviorBlock::Target);
    BML_BEHAVIOR_METHOD(ScriptBehaviorBlock, "Block",
                        "Block@ NullTarget(const CKGUID &in type)",
                        &ScriptBehaviorBlock::NullTarget);
    BML_BEHAVIOR_METHOD(
        ScriptBehaviorBlock, "Block",
        "Block@ Settings(const ? &in values)",
        &ScriptBehaviorBlock::Settings);
    BML_BEHAVIOR_METHOD(
        ScriptBehaviorBlock, "Block",
        "Block@ Pins(const ? &in values)",
        &ScriptBehaviorBlock::Pins);
    BML_BEHAVIOR_METHOD(
        ScriptBehaviorBlock, "Block",
        "Block@ Locals(const ? &in values)",
        &ScriptBehaviorBlock::Locals);
    BML_BEHAVIOR_METHOD(ScriptBehaviorBlock, "Block",
                        "Block@ Setting(const Selector &in slot, const Value &in value)",
                        &ScriptBehaviorBlock::SettingValue);
    BML_BEHAVIOR_METHOD(ScriptBehaviorBlock, "Block",
                        "Block@ Setting(const string &in slot, const Value &in value)",
                        &ScriptBehaviorBlock::SettingNamedValue);
    BML_BEHAVIOR_METHOD(ScriptBehaviorBlock, "Block",
                        "Block@ Pin(const Selector &in slot, const Value &in value)",
                        &ScriptBehaviorBlock::PinValue);
    BML_BEHAVIOR_METHOD(ScriptBehaviorBlock, "Block",
                        "Block@ Pin(const string &in slot, const Value &in value)",
                        &ScriptBehaviorBlock::PinNamedValue);
    BML_BEHAVIOR_METHOD(ScriptBehaviorBlock, "Block",
                        "Block@ Local(const Selector &in slot, const Value &in value)",
                        &ScriptBehaviorBlock::LocalValue);
    BML_BEHAVIOR_METHOD(ScriptBehaviorBlock, "Block",
                        "Block@ Local(const string &in slot, const Value &in value)",
                        &ScriptBehaviorBlock::LocalNamedValue);

#define BML_REGISTER_BLOCK_VALUES(Name)                                           \
    BML_BEHAVIOR_METHOD(ScriptBehaviorBlock, "Block",                           \
        "Block@ " #Name "(const string &in slot, bool value)",                  \
        &ScriptBehaviorBlock::Name##Bool);                                        \
    BML_BEHAVIOR_METHOD(ScriptBehaviorBlock, "Block",                           \
        "Block@ " #Name "(const Selector &in slot, bool value)",                \
        &ScriptBehaviorBlock::Name##SelectorBool);                                \
    BML_BEHAVIOR_METHOD(ScriptBehaviorBlock, "Block",                           \
        "Block@ " #Name "(const string &in slot, int value)",                   \
        &ScriptBehaviorBlock::Name##Int);                                         \
    BML_BEHAVIOR_METHOD(ScriptBehaviorBlock, "Block",                           \
        "Block@ " #Name "(const Selector &in slot, int value)",                 \
        &ScriptBehaviorBlock::Name##SelectorInt);                                 \
    BML_BEHAVIOR_METHOD(ScriptBehaviorBlock, "Block",                           \
        "Block@ " #Name "(const string &in slot, float value)",                 \
        &ScriptBehaviorBlock::Name##Float);                                       \
    BML_BEHAVIOR_METHOD(ScriptBehaviorBlock, "Block",                           \
        "Block@ " #Name "(const Selector &in slot, float value)",               \
        &ScriptBehaviorBlock::Name##SelectorFloat);                               \
    BML_BEHAVIOR_METHOD(ScriptBehaviorBlock, "Block",                           \
        "Block@ " #Name "(const string &in slot, const string &in value)",      \
        &ScriptBehaviorBlock::Name##String);                                      \
    BML_BEHAVIOR_METHOD(ScriptBehaviorBlock, "Block",                           \
        "Block@ " #Name "(const Selector &in slot, const string &in value)",    \
        &ScriptBehaviorBlock::Name##SelectorString);                              \
    BML_BEHAVIOR_METHOD(ScriptBehaviorBlock, "Block",                           \
        "Block@ " #Name "(const string &in slot, const Vx2DVector &in value)",  \
        &ScriptBehaviorBlock::Name##Vec2);                                        \
    BML_BEHAVIOR_METHOD(ScriptBehaviorBlock, "Block",                           \
        "Block@ " #Name "(const Selector &in slot, const Vx2DVector &in value)", \
        &ScriptBehaviorBlock::Name##SelectorVec2);                                \
    BML_BEHAVIOR_METHOD(ScriptBehaviorBlock, "Block",                           \
        "Block@ " #Name "(const string &in slot, const VxVector &in value)",    \
        &ScriptBehaviorBlock::Name##Vec3);                                        \
    BML_BEHAVIOR_METHOD(ScriptBehaviorBlock, "Block",                           \
        "Block@ " #Name "(const Selector &in slot, const VxVector &in value)",  \
        &ScriptBehaviorBlock::Name##SelectorVec3);                                \
    BML_BEHAVIOR_METHOD(ScriptBehaviorBlock, "Block",                           \
        "Block@ " #Name "(const string &in slot, const CKGUID &in type, CKObject@ value)", \
        &ScriptBehaviorBlock::Name##Object);                                      \
    BML_BEHAVIOR_METHOD(ScriptBehaviorBlock, "Block",                           \
        "Block@ " #Name "(const Selector &in slot, const CKGUID &in type, CKObject@ value)", \
        &ScriptBehaviorBlock::Name##SelectorObject)

    BML_REGISTER_BLOCK_VALUES(Setting);
    BML_REGISTER_BLOCK_VALUES(Pin);
    BML_REGISTER_BLOCK_VALUES(Local);
#undef BML_REGISTER_BLOCK_VALUES

    BML_BEHAVIOR_METHOD(ScriptBehaviorBlock, "Block",
                         "Block@ PinType(const string &in slot, const CKGUID &in type)",
                         &ScriptBehaviorBlock::PinType);
    BML_BEHAVIOR_METHOD(ScriptBehaviorBlock, "Block",
                        "Block@ PinType(const Selector &in slot, const CKGUID &in type)",
                        &ScriptBehaviorBlock::PinTypeAt);
    BML_BEHAVIOR_METHOD(ScriptBehaviorBlock, "Block",
                         "Block@ PoutType(const string &in slot, const CKGUID &in type)",
                         &ScriptBehaviorBlock::PoutType);
    BML_BEHAVIOR_METHOD(ScriptBehaviorBlock, "Block",
                        "Block@ PoutType(const Selector &in slot, const CKGUID &in type)",
                        &ScriptBehaviorBlock::PoutTypeAt);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorBlock, "Block", "bool Validate()",
                               Validate);
    BML_BEHAVIOR_METHOD(ScriptBehaviorBlock, "Block",
                        "Call@ Call(const Selector &in input)",
                        &ScriptBehaviorBlock::Call);
    BML_BEHAVIOR_METHOD(ScriptBehaviorBlock, "Block",
                        "Call@ Call(const Selector &in input, const FramePolicy &in frames)",
                        &ScriptBehaviorBlock::CallWith);
    BML_BEHAVIOR_METHOD(ScriptBehaviorBlock, "Block",
                        "Call@ Call(CKBeObject@ owner, const Selector &in input, const FramePolicy &in frames)",
                        &ScriptBehaviorBlock::CallOn);
    BML_BEHAVIOR_METHOD(ScriptBehaviorBlock, "Block",
                        "Task@ Start(const Selector &in input)",
                        &ScriptBehaviorBlock::Start);
    BML_BEHAVIOR_METHOD(ScriptBehaviorBlock, "Block",
                        "Task@ Start(const Selector &in input, const FramePolicy &in frames)",
                        &ScriptBehaviorBlock::StartWith);
    BML_BEHAVIOR_METHOD(ScriptBehaviorBlock, "Block",
                        "Task@ Start(CKBeObject@ owner, const Selector &in input, const FramePolicy &in frames)",
                        &ScriptBehaviorBlock::StartOn);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorBlock, "Block", "Instance@ Spawn()",
                               Spawn);
    BML_BEHAVIOR_METHOD(ScriptBehaviorBlock, "Block",
                        "Instance@ Spawn(const FramePolicy &in frames)",
                        &ScriptBehaviorBlock::SpawnWith);
    BML_BEHAVIOR_METHOD(ScriptBehaviorBlock, "Block",
                        "Instance@ Spawn(CKBeObject@ owner, const FramePolicy &in frames)",
                        &ScriptBehaviorBlock::SpawnOn);
    BML_BEHAVIOR_METHOD(ScriptBehaviorBlock, "Block",
                        "Instance@ SpawnIn(CKBehavior@ graph)",
                        &ScriptBehaviorBlock::SpawnIn);
    BML_BEHAVIOR_METHOD(ScriptBehaviorBlock, "Block",
                        "Instance@ SpawnIn(CKBehavior@ graph, const FramePolicy &in frames)",
                        &ScriptBehaviorBlock::SpawnInWith);

    const char *runTypes[] = {"Call", "Task", "Instance"};
    for (const char *type : runTypes) {
        BML_BEHAVIOR_REGISTER(engine->RegisterObjectMethod(
            type, "bool get_IsValid() const",
            asMETHOD(ScriptBehaviorRun, IsValid), asCALL_THISCALL),
            "BML::Behavior Run::IsValid");
        BML_BEHAVIOR_REGISTER(engine->RegisterObjectMethod(
            type, "RunState get_State() const",
            BML_AS_GENERIC_METHOD(&ScriptBehaviorRun::State), asCALL_GENERIC),
            "BML::Behavior Run::State");
        BML_BEHAVIOR_REGISTER(engine->RegisterObjectMethod(
            type, "string get_Error() const",
            BML_AS_GENERIC_METHOD(&ScriptBehaviorRun::Error), asCALL_GENERIC),
            "BML::Behavior Run::Error");
        BML_BEHAVIOR_REGISTER(engine->RegisterObjectMethod(
            type, "Frames@ TakeFrames()",
            BML_AS_GENERIC_METHOD(&ScriptBehaviorRun::TakeFrames), asCALL_GENERIC),
            "BML::Behavior Run::TakeFrames");
        BML_BEHAVIOR_REGISTER(engine->RegisterObjectMethod(
            type, "Layout@ Layout()",
            BML_AS_GENERIC_METHOD(&ScriptBehaviorRun::Layout), asCALL_GENERIC),
            "BML::Behavior Run::Layout");
        BML_BEHAVIOR_REGISTER(engine->RegisterObjectMethod(
            type, "Graph@ Inspect(bool live = false)",
            BML_AS_GENERIC_METHOD(&ScriptBehaviorRun::Inspect), asCALL_GENERIC),
            "BML::Behavior Run::Inspect");
        BML_BEHAVIOR_REGISTER(engine->RegisterObjectMethod(
            type, "bool Set(Slot@ slot, const Value &in value)",
            BML_AS_GENERIC_METHOD(&ScriptBehaviorRun::Set), asCALL_GENERIC),
            "BML::Behavior Run::Set");
        BML_BEHAVIOR_REGISTER(engine->RegisterObjectMethod(
            type, "bool Bind(Slot@ slot, Port@ source, "
                  "Relation relation = Direct)",
            BML_AS_GENERIC_METHOD(&ScriptBehaviorRun::Bind), asCALL_GENERIC),
            "BML::Behavior Run::Bind");
        BML_BEHAVIOR_REGISTER(engine->RegisterObjectMethod(
            type, "bool Close()",
            asMETHOD(ScriptBehaviorRun, Close), asCALL_THISCALL),
            "BML::Behavior Run::Close");
    }

#define BML_REGISTER_RUN_VALUES(scriptType, Name)                                 \
    BML_BEHAVIOR_REGISTER(engine->RegisterObjectMethod(                           \
        scriptType, "bool " #Name "(const string &in slot, bool value)",        \
        BML_AS_GENERIC_METHOD(&ScriptBehaviorRun::Name##Bool), asCALL_GENERIC),   \
        "BML::Behavior Run::" #Name " bool");                                  \
    BML_BEHAVIOR_REGISTER(engine->RegisterObjectMethod(                           \
        scriptType, "bool " #Name "(const Selector &in slot, bool value)",      \
        BML_AS_GENERIC_METHOD(&ScriptBehaviorRun::Name##SelectorBool),             \
        asCALL_GENERIC), "BML::Behavior Run::" #Name " bool selector");         \
    BML_BEHAVIOR_REGISTER(engine->RegisterObjectMethod(                           \
        scriptType, "bool " #Name "(const string &in slot, int value)",         \
        BML_AS_GENERIC_METHOD(&ScriptBehaviorRun::Name##Int), asCALL_GENERIC),    \
        "BML::Behavior Run::" #Name " int");                                   \
    BML_BEHAVIOR_REGISTER(engine->RegisterObjectMethod(                           \
        scriptType, "bool " #Name "(const Selector &in slot, int value)",       \
        BML_AS_GENERIC_METHOD(&ScriptBehaviorRun::Name##SelectorInt),              \
        asCALL_GENERIC), "BML::Behavior Run::" #Name " int selector");          \
    BML_BEHAVIOR_REGISTER(engine->RegisterObjectMethod(                           \
        scriptType, "bool " #Name "(const string &in slot, float value)",       \
        BML_AS_GENERIC_METHOD(&ScriptBehaviorRun::Name##Float), asCALL_GENERIC),  \
        "BML::Behavior Run::" #Name " float");                                 \
    BML_BEHAVIOR_REGISTER(engine->RegisterObjectMethod(                           \
        scriptType, "bool " #Name "(const Selector &in slot, float value)",     \
        BML_AS_GENERIC_METHOD(&ScriptBehaviorRun::Name##SelectorFloat),            \
        asCALL_GENERIC), "BML::Behavior Run::" #Name " float selector");        \
    BML_BEHAVIOR_REGISTER(engine->RegisterObjectMethod(                           \
        scriptType, "bool " #Name "(const string &in slot, const string &in value)", \
        BML_AS_GENERIC_METHOD(&ScriptBehaviorRun::Name##String), asCALL_GENERIC), \
        "BML::Behavior Run::" #Name " string");                                \
    BML_BEHAVIOR_REGISTER(engine->RegisterObjectMethod(                           \
        scriptType, "bool " #Name "(const Selector &in slot, const string &in value)", \
        BML_AS_GENERIC_METHOD(&ScriptBehaviorRun::Name##SelectorString),           \
        asCALL_GENERIC), "BML::Behavior Run::" #Name " string selector");       \
    BML_BEHAVIOR_REGISTER(engine->RegisterObjectMethod(                           \
        scriptType, "bool " #Name "(const string &in slot, const Vx2DVector &in value)", \
        BML_AS_GENERIC_METHOD(&ScriptBehaviorRun::Name##Vec2), asCALL_GENERIC),   \
        "BML::Behavior Run::" #Name " Vec2");                                  \
    BML_BEHAVIOR_REGISTER(engine->RegisterObjectMethod(                           \
        scriptType, "bool " #Name "(const Selector &in slot, const Vx2DVector &in value)", \
        BML_AS_GENERIC_METHOD(&ScriptBehaviorRun::Name##SelectorVec2),             \
        asCALL_GENERIC), "BML::Behavior Run::" #Name " Vec2 selector");         \
    BML_BEHAVIOR_REGISTER(engine->RegisterObjectMethod(                           \
        scriptType, "bool " #Name "(const string &in slot, const VxVector &in value)", \
        BML_AS_GENERIC_METHOD(&ScriptBehaviorRun::Name##Vec3), asCALL_GENERIC),   \
        "BML::Behavior Run::" #Name " Vec3");                                  \
    BML_BEHAVIOR_REGISTER(engine->RegisterObjectMethod(                           \
        scriptType, "bool " #Name "(const Selector &in slot, const VxVector &in value)", \
        BML_AS_GENERIC_METHOD(&ScriptBehaviorRun::Name##SelectorVec3),             \
        asCALL_GENERIC), "BML::Behavior Run::" #Name " Vec3 selector")

    for (const char *type : runTypes) {
        BML_REGISTER_RUN_VALUES(type, Pin);
        BML_REGISTER_RUN_VALUES(type, Local);
        BML_REGISTER_RUN_VALUES(type, Setting);
        BML_BEHAVIOR_REGISTER(engine->RegisterObjectMethod(
            type, "bool Pin(const Selector &in slot, const Value &in value)",
            BML_AS_GENERIC_METHOD(&ScriptBehaviorRun::PinValue), asCALL_GENERIC),
            "BML::Behavior Run::Pin Value");
        BML_BEHAVIOR_REGISTER(engine->RegisterObjectMethod(
            type, "bool Local(const Selector &in slot, const Value &in value)",
            BML_AS_GENERIC_METHOD(&ScriptBehaviorRun::LocalValue), asCALL_GENERIC),
            "BML::Behavior Run::Local Value");
        BML_BEHAVIOR_REGISTER(engine->RegisterObjectMethod(
            type, "bool Setting(const Selector &in slot, const Value &in value)",
            BML_AS_GENERIC_METHOD(&ScriptBehaviorRun::SettingValue), asCALL_GENERIC),
            "BML::Behavior Run::Setting Value");
        BML_BEHAVIOR_REGISTER(engine->RegisterObjectMethod(
            type, "bool Settings(const ? &in values)",
            BML_AS_GENERIC_METHOD(&ScriptBehaviorRun::Settings), asCALL_GENERIC),
            "BML::Behavior Run::Settings");
        BML_BEHAVIOR_REGISTER(engine->RegisterObjectMethod(
            type,
            "bool Pin(const string &in slot, const CKGUID &in type, CKObject@ value)",
            BML_AS_GENERIC_METHOD(&ScriptBehaviorRun::PinObject), asCALL_GENERIC),
            "BML::Behavior Run::Pin object");
        BML_BEHAVIOR_REGISTER(engine->RegisterObjectMethod(
            type,
            "bool Pin(const Selector &in slot, const CKGUID &in type, CKObject@ value)",
            BML_AS_GENERIC_METHOD(&ScriptBehaviorRun::PinObjectAt), asCALL_GENERIC),
            "BML::Behavior Run::Pin object selector");
    }
#undef BML_REGISTER_RUN_VALUES

    BML_BEHAVIOR_METHOD(ScriptBehaviorCall, "Call", "Task@ Continue()",
                        &ScriptBehaviorCall::Continue);
    BML_BEHAVIOR_METHOD(ScriptBehaviorTask, "Task",
                        "PulseResult Pulse(const Selector &in input)",
                        &ScriptBehaviorTask::Pulse);
    BML_BEHAVIOR_METHOD(ScriptBehaviorInstance, "Instance",
                        "PulseResult Pulse(const Selector &in input)",
                        &ScriptBehaviorInstance::Pulse);

    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorLayout, "Layout",
                               "bool get_IsValid() const", IsValid);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorLayout, "Layout",
                               "LayoutOrigin get_Origin() const", Origin);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorLayout, "Layout",
                               "uint64 get_Generation() const", Generation);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorLayout, "Layout",
                               "BehaviorKind get_Kind() const", Kind);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorLayout, "Layout",
                               "int get_CompatibleClass() const", CompatibleClass);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorLayout, "Layout",
                               "uint get_PrototypeFlags() const", PrototypeFlags);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorLayout, "Layout",
                               "uint get_BehaviorFlags() const", BehaviorFlags);
    BML_BEHAVIOR_METHOD(ScriptBehaviorLayout, "Layout",
                        "CKGUID get_Prototype() const",
                        &ScriptBehaviorLayout::Prototype);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorLayout, "Layout",
                               "uint64 get_PrototypeGeneration() const",
                               PrototypeGeneration);
    BML_BEHAVIOR_METHOD(ScriptBehaviorLayout, "Layout",
                        "CKGUID get_TargetType() const",
                        &ScriptBehaviorLayout::TargetType);
    BML_BEHAVIOR_METHOD(ScriptBehaviorLayout, "Layout",
                        "string get_Name() const", &ScriptBehaviorLayout::Name);
    BML_BEHAVIOR_METHOD(ScriptBehaviorLayout, "Layout",
                        "string get_Category() const",
                        &ScriptBehaviorLayout::Category);
    BML_BEHAVIOR_METHOD(ScriptBehaviorLayout, "Layout",
                        "string get_Provider() const",
                        &ScriptBehaviorLayout::Provider);
    BML_BEHAVIOR_METHOD(ScriptBehaviorLayout, "Layout",
                        "string get_Author() const", &ScriptBehaviorLayout::Author);
    BML_BEHAVIOR_METHOD(ScriptBehaviorLayout, "Layout",
                        "string get_Description() const",
                        &ScriptBehaviorLayout::Description);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorLayout, "Layout",
                               "int get_SlotCount() const", SlotCount);
    BML_BEHAVIOR_METHOD(ScriptBehaviorLayout, "Layout",
                        "Slot@ opIndex(int index) const",
                        &ScriptBehaviorLayout::At);
    BML_BEHAVIOR_METHOD(ScriptBehaviorLayout, "Layout",
                        "Slot@ Find(SlotKind kind, const Selector &in selector) const",
                        &ScriptBehaviorLayout::Find);
    BML_BEHAVIOR_METHOD(ScriptBehaviorLayout, "Layout",
                        "Slot@ Find(SlotKind kind, const string &in name) const",
                        &ScriptBehaviorLayout::FindNamed);

    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorSlot, "Slot",
                               "bool get_IsValid() const", IsValid);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorSlot, "Slot",
                               "SlotKind get_Kind() const", Kind);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorSlot, "Slot",
                               "uint64 get_Generation() const", Generation);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorSlot, "Slot",
                               "bool get_Dynamic() const", Dynamic);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorSlot, "Slot",
                               "int get_Index() const", Index);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorSlot, "Slot",
                               "int get_Occurrence() const", Occurrence);
    BML_BEHAVIOR_METHOD(ScriptBehaviorSlot, "Slot", "CKGUID get_Type() const",
                        &ScriptBehaviorSlot::Type);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorSlot, "Slot",
                               "ValueKind get_ValueKind() const", ValueKind);
    BML_BEHAVIOR_METHOD(ScriptBehaviorSlot, "Slot", "string get_Name() const",
                        &ScriptBehaviorSlot::Name);
    BML_BEHAVIOR_METHOD(ScriptBehaviorSlot, "Slot",
                        "string get_TypeName() const", &ScriptBehaviorSlot::TypeName);

    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorFrames, "Frames",
                               "int get_Count() const", Count);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorFrames, "Frames",
                               "bool get_Empty() const", Empty);
    BML_BEHAVIOR_METHOD(ScriptBehaviorFrames, "Frames", "Frame@ opIndex(int index) const",
                        &ScriptBehaviorFrames::At);

    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorFrame, "Frame",
                               "uint64 get_Sequence() const", Sequence);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorFrame, "Frame",
                               "uint64 get_GameFrame() const", GameFrame);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorFrame, "Frame",
                               "int get_NativeResult() const", NativeResult);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorFrame, "Frame",
                               "Continuation get_Continuation() const", Continuation);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorFrame, "Frame",
                               "Error get_Error() const", Error);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorFrame, "Frame",
                               "int get_OutCount() const", OutCount);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorFrame, "Frame",
                               "int get_PoutCount() const", PoutCount);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorFrame, "Frame",
                               "int get_StatusCount() const", StatusCount);
    BML_BEHAVIOR_METHOD(ScriptBehaviorFrame, "Frame",
                        "bool HasOut(const Selector &in selector) const",
                        &ScriptBehaviorFrame::HasOut);
    BML_BEHAVIOR_METHOD(ScriptBehaviorFrame, "Frame",
                        "string OutName(int index) const",
                        &ScriptBehaviorFrame::OutName);
    BML_BEHAVIOR_METHOD(ScriptBehaviorFrame, "Frame",
                        "string PoutName(int index) const",
                        &ScriptBehaviorFrame::PoutName);
    BML_BEHAVIOR_METHOD(ScriptBehaviorFrame, "Frame",
                        "string Status(int index) const",
                        &ScriptBehaviorFrame::Status);
    BML_BEHAVIOR_METHOD(ScriptBehaviorFrame, "Frame",
                        "bool Read(const Selector &in selector, bool &out value) const",
                        &ScriptBehaviorFrame::ReadBool);
    BML_BEHAVIOR_METHOD(ScriptBehaviorFrame, "Frame",
                        "bool Read(const Selector &in selector, int &out value) const",
                        &ScriptBehaviorFrame::ReadInt);
    BML_BEHAVIOR_METHOD(ScriptBehaviorFrame, "Frame",
                        "bool Read(const Selector &in selector, float &out value) const",
                        &ScriptBehaviorFrame::ReadFloat);
    BML_BEHAVIOR_METHOD(ScriptBehaviorFrame, "Frame",
                        "bool Read(const Selector &in selector, string &out value) const",
                        &ScriptBehaviorFrame::ReadString);
    BML_BEHAVIOR_METHOD(ScriptBehaviorFrame, "Frame",
                        "bool Read(const Selector &in selector, Vx2DVector &out value) const",
                        &ScriptBehaviorFrame::ReadVec2);
    BML_BEHAVIOR_METHOD(ScriptBehaviorFrame, "Frame",
                        "bool Read(const Selector &in selector, VxVector &out value) const",
                        &ScriptBehaviorFrame::ReadVec3);
    BML_BEHAVIOR_METHOD(ScriptBehaviorFrame, "Frame",
                        "bool Read(const Selector &in selector, VxQuaternion &out value) const",
                        &ScriptBehaviorFrame::ReadQuaternion);
    BML_BEHAVIOR_METHOD(ScriptBehaviorFrame, "Frame",
                        "bool ReadEuler(const Selector &in selector, VxVector &out value) const",
                        &ScriptBehaviorFrame::ReadEuler);
    BML_BEHAVIOR_METHOD(ScriptBehaviorFrame, "Frame",
                        "bool Read(const Selector &in selector, VxRect &out value) const",
                        &ScriptBehaviorFrame::ReadRect);
    BML_BEHAVIOR_METHOD(ScriptBehaviorFrame, "Frame",
                        "bool Read(const Selector &in selector, VxColor &out value) const",
                        &ScriptBehaviorFrame::ReadColor);
    BML_BEHAVIOR_METHOD(ScriptBehaviorFrame, "Frame",
                        "bool Read(const Selector &in selector, VxBbox &out value) const",
                        &ScriptBehaviorFrame::ReadBox);
    BML_BEHAVIOR_METHOD(ScriptBehaviorFrame, "Frame",
                        "bool Read(const Selector &in selector, VxMatrix &out value) const",
                        &ScriptBehaviorFrame::ReadMatrix);
    BML_BEHAVIOR_METHOD(ScriptBehaviorFrame, "Frame",
                        "ObjectRef@ ReadObject(const Selector &in selector) const",
                        &ScriptBehaviorFrame::ReadObject);
    BML_BEHAVIOR_METHOD(ScriptBehaviorFrame, "Frame",
                        "ObjectList@ ReadObjects(const Selector &in selector) const",
                        &ScriptBehaviorFrame::ReadObjects);

    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorObjectRef, "ObjectRef",
                               "bool get_IsNull() const", IsNull);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorObjectRef, "ObjectRef",
                               "bool get_IsValid() const", IsValid);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorObjectRef, "ObjectRef",
                               "bool get_IsStale() const", IsStale);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorObjectRef, "ObjectRef",
                               "uint get_Domain() const", Domain);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorObjectRef, "ObjectRef",
                               "uint get_Slot() const", Slot);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorObjectRef, "ObjectRef",
                               "uint get_Generation() const", Generation);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorObjectRef, "ObjectRef",
                               "CKObject@ Borrow() const", Borrow);

    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorObjectList, "ObjectList",
                               "int get_Count() const", Count);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorObjectList, "ObjectList",
                               "bool get_Empty() const", Empty);
    BML_BEHAVIOR_METHOD(ScriptBehaviorObjectList, "ObjectList",
                        "ObjectRef@ opIndex(int index) const",
                        &ScriptBehaviorObjectList::At);

    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorPort, "Port",
                               "bool get_IsValid() const", IsValid);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorPort, "Port",
                               "SlotKind get_Kind() const", Kind);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorPort, "Port",
                               "int get_Index() const", Index);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorPort, "Port",
                               "int get_Occurrence() const", Occurrence);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorPort, "Port",
                               "uint64 get_NodeId() const", NodeId);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorPort, "Port",
                               "uint64 get_LayoutGeneration() const",
                               LayoutGeneration);
    BML_BEHAVIOR_METHOD(ScriptBehaviorPort, "Port",
                        "CKGUID get_Type() const", &ScriptBehaviorPort::Type);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorPort, "Port",
                               "bool get_Dynamic() const", Dynamic);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorPort, "Port",
                               "bool get_Active() const", Active);
    BML_BEHAVIOR_METHOD(ScriptBehaviorPort, "Port",
                        "string get_Name() const", &ScriptBehaviorPort::Name);

    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorNode, "Node",
                               "bool get_IsValid() const", IsValid);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorNode, "Node",
                               "uint64 get_Id() const", Id);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorNode, "Node",
                               "int get_Index() const", Index);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorNode, "Node",
                               "int get_Occurrence() const", Occurrence);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorNode, "Node",
                               "uint64 get_LayoutGeneration() const",
                               LayoutGeneration);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorNode, "Node",
                               "BehaviorKind get_Kind() const", Kind);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorNode, "Node",
                               "bool get_IsGraph() const", IsGraph);
    BML_BEHAVIOR_METHOD(ScriptBehaviorNode, "Node",
                        "CKGUID get_Prototype() const",
                        &ScriptBehaviorNode::Prototype);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorNode, "Node",
                               "int get_Priority() const", Priority);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorNode, "Node",
                               "bool get_Active() const", Active);
    BML_BEHAVIOR_METHOD(ScriptBehaviorNode, "Node",
                        "string get_Name() const", &ScriptBehaviorNode::Name);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorNode, "Node",
                               "int get_PortCount() const", PortCount);
    BML_BEHAVIOR_METHOD(ScriptBehaviorNode, "Node", "Port@ PortAt(int index) const",
                        &ScriptBehaviorNode::PortAt);
#define BML_REGISTER_NODE_PORT(Name)                                              \
    BML_BEHAVIOR_METHOD(ScriptBehaviorNode, "Node",                            \
        "Port@ " #Name "(const Selector &in slot) const",                     \
        &ScriptBehaviorNode::Name);                                               \
    BML_BEHAVIOR_METHOD(ScriptBehaviorNode, "Node",                            \
        "Port@ " #Name "(const string &in name) const",                       \
        &ScriptBehaviorNode::Name##ByName)
    BML_REGISTER_NODE_PORT(In);
    BML_REGISTER_NODE_PORT(Out);
    BML_REGISTER_NODE_PORT(Pin);
    BML_REGISTER_NODE_PORT(Pout);
    BML_REGISTER_NODE_PORT(Setting);
    BML_REGISTER_NODE_PORT(Local);
#undef BML_REGISTER_NODE_PORT
    BML_BEHAVIOR_METHOD(ScriptBehaviorNode, "Node", "Port@ Target() const",
                        &ScriptBehaviorNode::Target);

    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorLink, "Link",
                               "bool get_IsValid() const", IsValid);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorLink, "Link",
                               "uint64 get_Id() const", Id);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorLink, "Link",
                               "int get_InitialDelay() const", InitialDelay);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorLink, "Link",
                               "int get_RemainingDelay() const", RemainingDelay);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorLink, "Link",
                               "int get_Pending() const", Pending);
    BML_BEHAVIOR_METHOD(ScriptBehaviorLink, "Link", "Port@ Source() const",
                        &ScriptBehaviorLink::Source);
    BML_BEHAVIOR_METHOD(ScriptBehaviorLink, "Link", "Port@ Target() const",
                        &ScriptBehaviorLink::Target);

    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorObservedValue, "ObservedValue",
                               "bool get_IsAvailable() const", IsAvailable);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorObservedValue, "ObservedValue",
                               "ObservationState get_State() const", State);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorObservedValue, "ObservedValue",
                               "Relation get_Relation() const", Relation);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorObservedValue, "ObservedValue",
                               "ValueKind get_Kind() const", Kind);
    BML_BEHAVIOR_METHOD(ScriptBehaviorObservedValue, "ObservedValue",
                        "CKGUID get_Type() const", &ScriptBehaviorObservedValue::Type);
    BML_BEHAVIOR_METHOD(ScriptBehaviorObservedValue, "ObservedValue",
                        "bool Read(bool &out value) const",
                        &ScriptBehaviorObservedValue::ReadBool);
    BML_BEHAVIOR_METHOD(ScriptBehaviorObservedValue, "ObservedValue",
                        "bool Read(int &out value) const",
                        &ScriptBehaviorObservedValue::ReadInt);
    BML_BEHAVIOR_METHOD(ScriptBehaviorObservedValue, "ObservedValue",
                        "bool Read(float &out value) const",
                        &ScriptBehaviorObservedValue::ReadFloat);
    BML_BEHAVIOR_METHOD(ScriptBehaviorObservedValue, "ObservedValue",
                        "bool Read(string &out value) const",
                        &ScriptBehaviorObservedValue::ReadString);
    BML_BEHAVIOR_METHOD(ScriptBehaviorObservedValue, "ObservedValue",
                        "bool Read(Vx2DVector &out value) const",
                        &ScriptBehaviorObservedValue::ReadVec2);
    BML_BEHAVIOR_METHOD(ScriptBehaviorObservedValue, "ObservedValue",
                        "bool Read(VxVector &out value) const",
                        &ScriptBehaviorObservedValue::ReadVec3);
    BML_BEHAVIOR_METHOD(ScriptBehaviorObservedValue, "ObservedValue",
                        "bool Read(VxQuaternion &out value) const",
                        &ScriptBehaviorObservedValue::ReadQuaternion);
    BML_BEHAVIOR_METHOD(ScriptBehaviorObservedValue, "ObservedValue",
                        "bool ReadEuler(VxVector &out value) const",
                        &ScriptBehaviorObservedValue::ReadEuler);
    BML_BEHAVIOR_METHOD(ScriptBehaviorObservedValue, "ObservedValue",
                        "bool Read(VxRect &out value) const",
                        &ScriptBehaviorObservedValue::ReadRect);
    BML_BEHAVIOR_METHOD(ScriptBehaviorObservedValue, "ObservedValue",
                        "bool Read(VxColor &out value) const",
                        &ScriptBehaviorObservedValue::ReadColor);
    BML_BEHAVIOR_METHOD(ScriptBehaviorObservedValue, "ObservedValue",
                        "bool Read(VxBbox &out value) const",
                        &ScriptBehaviorObservedValue::ReadBox);
    BML_BEHAVIOR_METHOD(ScriptBehaviorObservedValue, "ObservedValue",
                        "bool Read(VxMatrix &out value) const",
                        &ScriptBehaviorObservedValue::ReadMatrix);
    BML_BEHAVIOR_METHOD(ScriptBehaviorObservedValue, "ObservedValue",
                        "ObjectRef@ ReadObject() const",
                        &ScriptBehaviorObservedValue::ReadObject);

    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorHookEvent, "HookEvent",
                               "float get_DeltaTime() const", DeltaTime);
    BML_BEHAVIOR_METHOD(ScriptBehaviorHookEvent, "HookEvent",
                        "CKBehavior@ BorrowBlock() const",
                        &ScriptBehaviorHookEvent::BorrowBlock);
    BML_BEHAVIOR_METHOD(ScriptBehaviorHookEvent, "HookEvent",
                        "CKBehavior@ BorrowScript() const",
                        &ScriptBehaviorHookEvent::BorrowScript);
    BML_BEHAVIOR_METHOD(ScriptBehaviorHookEvent, "HookEvent",
                        "CKBeObject@ BorrowOwner() const",
                        &ScriptBehaviorHookEvent::BorrowOwner);

    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorChange, "Change",
                               "ChangeKind get_Kind() const", Kind);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorChange, "Change",
                               "uint64 get_Sequence() const", Sequence);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorChange, "Change",
                               "uint64 get_GameFrame() const", GameFrame);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorChange, "Change",
                               "uint64 get_Before() const", Before);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorChange, "Change",
                               "uint64 get_After() const", After);
    BML_BEHAVIOR_METHOD(ScriptBehaviorChange, "Change",
                        "ObservedValue@ Previous() const",
                        &ScriptBehaviorChange::Previous);
    BML_BEHAVIOR_METHOD(ScriptBehaviorChange, "Change",
                        "ObservedValue@ Current() const",
                        &ScriptBehaviorChange::Current);

    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorWatch, "Watch",
                               "bool get_IsValid() const", IsValid);
    BML_BEHAVIOR_METHOD(ScriptBehaviorWatch, "Watch", "WatchState get_State() const",
                        &ScriptBehaviorWatch::State);
    BML_BEHAVIOR_METHOD(ScriptBehaviorWatch, "Watch",
                        "string get_Error() const", &ScriptBehaviorWatch::Error);
    BML_BEHAVIOR_METHOD(ScriptBehaviorWatch, "Watch", "CloseState Close()",
                        &ScriptBehaviorWatch::Close);

    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorGraph, "Graph",
                               "bool get_IsValid() const", IsValid);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorGraph, "Graph",
                               "View get_View() const", View);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorGraph, "Graph",
                               "uint64 get_Generation() const", Generation);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorGraph, "Graph",
                               "uint64 get_Fingerprint() const", Fingerprint);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorGraph, "Graph",
                               "int get_NodeCount() const", NodeCount);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorGraph, "Graph",
                               "int get_LinkCount() const", LinkCount);
    BML_BEHAVIOR_METHOD(ScriptBehaviorGraph, "Graph", "Node@ Root() const",
                        &ScriptBehaviorGraph::Root);
    BML_BEHAVIOR_METHOD(ScriptBehaviorGraph, "Graph", "Node@ NodeAt(int index) const",
                        &ScriptBehaviorGraph::NodeAt);
    BML_BEHAVIOR_METHOD(ScriptBehaviorGraph, "Graph", "Link@ LinkAt(int index) const",
                        &ScriptBehaviorGraph::LinkAt);
    BML_BEHAVIOR_METHOD(ScriptBehaviorGraph, "Graph",
                        "Node@ Find(const Selector &in selector, const CKGUID &in prototype) const",
                        &ScriptBehaviorGraph::Find);
    BML_BEHAVIOR_METHOD(ScriptBehaviorGraph, "Graph",
                        "int IncomingCount(Node@ node) const",
                        &ScriptBehaviorGraph::IncomingNodeCount);
    BML_BEHAVIOR_METHOD(ScriptBehaviorGraph, "Graph",
                        "int IncomingCount(Port@ port) const",
                        &ScriptBehaviorGraph::IncomingPortCount);
    BML_BEHAVIOR_METHOD(ScriptBehaviorGraph, "Graph",
                        "int OutgoingCount(Node@ node) const",
                        &ScriptBehaviorGraph::OutgoingNodeCount);
    BML_BEHAVIOR_METHOD(ScriptBehaviorGraph, "Graph",
                        "int OutgoingCount(Port@ port) const",
                        &ScriptBehaviorGraph::OutgoingPortCount);
    BML_BEHAVIOR_METHOD(ScriptBehaviorGraph, "Graph",
                        "Link@ Incoming(Node@ node, int index) const",
                        &ScriptBehaviorGraph::IncomingNode);
    BML_BEHAVIOR_METHOD(ScriptBehaviorGraph, "Graph",
                        "Link@ Incoming(Port@ port, int index) const",
                        &ScriptBehaviorGraph::IncomingPort);
    BML_BEHAVIOR_METHOD(ScriptBehaviorGraph, "Graph",
                        "Link@ Outgoing(Node@ node, int index) const",
                        &ScriptBehaviorGraph::OutgoingNode);
    BML_BEHAVIOR_METHOD(ScriptBehaviorGraph, "Graph",
                        "Link@ Outgoing(Port@ port, int index) const",
                        &ScriptBehaviorGraph::OutgoingPort);
    BML_BEHAVIOR_METHOD(ScriptBehaviorGraph, "Graph",
                        "Link@ Entering(Node@ node) const",
                        &ScriptBehaviorGraph::EnteringNode);
    BML_BEHAVIOR_METHOD(ScriptBehaviorGraph, "Graph",
                        "Link@ Entering(Port@ port) const",
                        &ScriptBehaviorGraph::EnteringPort);
    BML_BEHAVIOR_METHOD(ScriptBehaviorGraph, "Graph",
                        "Link@ Leaving(Node@ node) const",
                        &ScriptBehaviorGraph::LeavingNode);
    BML_BEHAVIOR_METHOD(ScriptBehaviorGraph, "Graph",
                        "Link@ Leaving(Port@ port) const",
                        &ScriptBehaviorGraph::LeavingPort);
    BML_BEHAVIOR_METHOD(ScriptBehaviorGraph, "Graph",
                        "Node@ Previous(Node@ node) const",
                        &ScriptBehaviorGraph::PreviousNode);
    BML_BEHAVIOR_METHOD(ScriptBehaviorGraph, "Graph",
                        "Node@ Previous(Port@ port) const",
                        &ScriptBehaviorGraph::PreviousPort);
    BML_BEHAVIOR_METHOD(ScriptBehaviorGraph, "Graph",
                        "Node@ Next(Node@ node) const",
                        &ScriptBehaviorGraph::NextNode);
    BML_BEHAVIOR_METHOD(ScriptBehaviorGraph, "Graph",
                        "Node@ Next(Port@ port) const",
                        &ScriptBehaviorGraph::NextPort);
    BML_BEHAVIOR_METHOD(ScriptBehaviorGraph, "Graph",
                        "ObservedValue@ Read(Port@ port) const",
                        &ScriptBehaviorGraph::Read);
    BML_BEHAVIOR_METHOD(ScriptBehaviorGraph, "Graph",
                        "Graph@ Inspect(Node@ node) const",
                        &ScriptBehaviorGraph::Inspect);
    BML_BEHAVIOR_METHOD(ScriptBehaviorGraph, "Graph", "Graph@ Logical() const",
                        &ScriptBehaviorGraph::Logical);
    BML_BEHAVIOR_METHOD(ScriptBehaviorGraph, "Graph", "Graph@ Live() const",
                        &ScriptBehaviorGraph::Live);
    BML_BEHAVIOR_METHOD(ScriptBehaviorGraph, "Graph",
                        "Patch@ Apply(const string &in name, Edit@ edit) const",
                        &ScriptBehaviorGraph::Apply);
    BML_BEHAVIOR_METHOD(ScriptBehaviorGraph, "Graph",
                        "Watch@ Watch(WatchCallback@+ callback) const",
                        &ScriptBehaviorGraph::WatchGraph);
    BML_BEHAVIOR_METHOD(ScriptBehaviorGraph, "Graph",
                        "Watch@ Watch(Node@ node, WatchCallback@+ callback) const",
                        &ScriptBehaviorGraph::WatchLayout);
    BML_BEHAVIOR_METHOD(ScriptBehaviorGraph, "Graph",
                        "Watch@ Watch(Port@ value, WatchCallback@+ callback) const",
                        &ScriptBehaviorGraph::WatchValue);

    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorEdit, "Edit",
                               "bool get_IsValid() const", IsValid);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEdit, "Edit", "EditGraph@ Root() const",
                        &ScriptBehaviorEdit::Root);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEdit, "Edit",
                        "Plan@ Plan(const string &in name, const string &in script, bool each = false)",
                        &ScriptBehaviorEdit::Plan);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEdit, "Edit",
                        "Script@ CreateScript(CKBeObject@ owner, const string &in name, int priority = 0)",
                        &ScriptBehaviorEdit::CreateScript);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorEditPort, "EditPort",
                               "bool get_IsValid() const", IsValid);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorEditPorts, "EditPorts",
                               "bool get_IsValid() const", IsValid);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorEditLink, "EditLink",
                               "bool get_IsValid() const", IsValid);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorEditPath, "EditPath",
                               "bool get_IsValid() const", IsValid);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorEditOperation, "Operation",
                               "bool get_IsValid() const", IsValid);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditOperation, "Operation",
                        "EditPort@ Input(int index) const",
                        &ScriptBehaviorEditOperation::Input);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditOperation, "Operation",
                        "EditPort@ Result() const",
                        &ScriptBehaviorEditOperation::Result);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorEditNode, "EditNode",
                               "bool get_IsValid() const", IsValid);
    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorEditNodes, "EditNodes",
                               "bool get_IsValid() const", IsValid);
#define BML_REGISTER_EDIT_NODES_PORT(Name)                                       \
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditNodes, "EditNodes",                 \
        "EditPorts@ " #Name "(const Selector &in slot) const",               \
        &ScriptBehaviorEditNodes::Name)
    BML_REGISTER_EDIT_NODES_PORT(In);
    BML_REGISTER_EDIT_NODES_PORT(Out);
    BML_REGISTER_EDIT_NODES_PORT(Pin);
    BML_REGISTER_EDIT_NODES_PORT(Pout);
    BML_REGISTER_EDIT_NODES_PORT(Local);
#undef BML_REGISTER_EDIT_NODES_PORT
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditNodes, "EditNodes",
                        "EditPorts@ Target() const",
                        &ScriptBehaviorEditNodes::Target);
#define BML_REGISTER_EDIT_NODE_PORT(Name)                                         \
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditNode, "EditNode",                    \
        "EditPort@ " #Name "(const Selector &in slot) const",                  \
        &ScriptBehaviorEditNode::Name)
    BML_REGISTER_EDIT_NODE_PORT(In);
    BML_REGISTER_EDIT_NODE_PORT(Out);
    BML_REGISTER_EDIT_NODE_PORT(Pin);
    BML_REGISTER_EDIT_NODE_PORT(Pout);
    BML_REGISTER_EDIT_NODE_PORT(Local);
#undef BML_REGISTER_EDIT_NODE_PORT
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditNode, "EditNode",
                        "EditPort@ Target() const", &ScriptBehaviorEditNode::Target);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditNode, "EditNode",
                        "EditGraph@ Graph() const", &ScriptBehaviorEditNode::Graph);

    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                               "bool get_IsValid() const", IsValid);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "EditNode@ Root() const", &ScriptBehaviorEditGraph::Root);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "EditNode@ Require(const Selector &in selector, const CKGUID &in prototype) const",
                        &ScriptBehaviorEditGraph::Require);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "EditNode@ Require(const NodePattern &in pattern) const",
                        &ScriptBehaviorEditGraph::RequirePattern);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "EditNodes@ Each(const NodePattern &in pattern) const",
                        &ScriptBehaviorEditGraph::Each);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "EditNode@ Require(Node@ node) const",
                        &ScriptBehaviorEditGraph::RequireNode);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "EditLink@ Require(Link@ link) const",
                        &ScriptBehaviorEditGraph::RequireLink);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "EditNode@ Use(Node@ node) const",
                        &ScriptBehaviorEditGraph::Use);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "EditLink@ Use(Link@ link) const",
                        &ScriptBehaviorEditGraph::UseLink);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "EditLink@ Between(EditPort@ source, EditPort@ sink, int delay = 0) const",
                        &ScriptBehaviorEditGraph::Between);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "EditNode@ Next(EditPort@ source) const",
                        &ScriptBehaviorEditGraph::NextPort);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "EditNode@ Next(EditNode@ source) const",
                        &ScriptBehaviorEditGraph::NextNode);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "EditNode@ Previous(EditPort@ sink) const",
                        &ScriptBehaviorEditGraph::PreviousPort);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "EditNode@ Previous(EditNode@ sink) const",
                        &ScriptBehaviorEditGraph::PreviousNode);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "EditLink@ Leaving(EditPort@ source) const",
                        &ScriptBehaviorEditGraph::LeavingPort);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "EditLink@ Leaving(EditNode@ source) const",
                        &ScriptBehaviorEditGraph::LeavingNode);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "EditLink@ Entering(EditPort@ sink) const",
                        &ScriptBehaviorEditGraph::EnteringPort);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "EditLink@ Entering(EditNode@ sink) const",
                        &ScriptBehaviorEditGraph::EnteringNode);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "EditLink@ To(EditPort@ source, EditNode@ target) const",
                        &ScriptBehaviorEditGraph::To);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "EditPath@ Follow(EditPort@ source) const",
                        &ScriptBehaviorEditGraph::Follow);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "EditNode@ Add(Block@ block) const",
                        &ScriptBehaviorEditGraph::Add);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "EditNode@ AddGraph(const string &in name, int priority = 0) const",
                        &ScriptBehaviorEditGraph::AddGraph);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "EditNode@ Replace(EditNode@ target, Block@ block) const",
                        &ScriptBehaviorEditGraph::Replace);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "bool Remove(EditNode@ node) const",
                        &ScriptBehaviorEditGraph::Remove);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "bool Flow(EditPort@ source, EditPort@ sink, int delay = 0) const",
                        &ScriptBehaviorEditGraph::Flow);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "bool FlowCycle(EditPort@ source, EditPort@ sink, int delay = 0) const",
                        &ScriptBehaviorEditGraph::FlowCycle);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "bool Flow(EditPorts@ sources, EditPort@ sink, int delay = 0) const",
                        &ScriptBehaviorEditGraph::FlowFrom);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "bool Flow(EditPort@ source, EditPorts@ sinks, int delay = 0) const",
                        &ScriptBehaviorEditGraph::FlowTo);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "bool FlowCycle(EditPorts@ sources, EditPort@ sink, int delay = 0) const",
                        &ScriptBehaviorEditGraph::FlowCycleFrom);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "bool FlowCycle(EditPort@ source, EditPorts@ sinks, int delay = 0) const",
                        &ScriptBehaviorEditGraph::FlowCycleTo);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "bool Bind(EditPort@ sink, EditPort@ source) const",
                        &ScriptBehaviorEditGraph::BindPort);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "bool Share(EditPort@ sink, EditPort@ source) const",
                        &ScriptBehaviorEditGraph::Share);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "bool Bind(EditPorts@ sinks, EditPort@ source) const",
                        &ScriptBehaviorEditGraph::BindPorts);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "bool Share(EditPorts@ sinks, EditPort@ source) const",
                        &ScriptBehaviorEditGraph::SharePorts);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "bool Push(EditPort@ source, EditPort@ sink) const",
                        &ScriptBehaviorEditGraph::Push);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "bool Push(EditPorts@ sources, EditPort@ sink) const",
                        &ScriptBehaviorEditGraph::PushFrom);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "bool Push(EditPort@ source, EditPorts@ sinks) const",
                        &ScriptBehaviorEditGraph::PushTo);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "Operation@ AddOperation(const CKGUID &in operation, const CKGUID &in result, const CKGUID &in input1 = CKGUID(0, 0), const CKGUID &in input2 = CKGUID(0, 0)) const",
                        &ScriptBehaviorEditGraph::AddOperation);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "bool Bind(EditPort@ sink, bool value) const",
                        &ScriptBehaviorEditGraph::BindBool);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "bool Bind(EditPort@ sink, int value) const",
                        &ScriptBehaviorEditGraph::BindInt);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "bool Bind(EditPort@ sink, float value) const",
                        &ScriptBehaviorEditGraph::BindFloat);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "bool Bind(EditPort@ sink, const string &in value) const",
                        &ScriptBehaviorEditGraph::BindString);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "bool Bind(EditPort@ sink, const Vx2DVector &in value) const",
                        &ScriptBehaviorEditGraph::BindVec2);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "bool Bind(EditPort@ sink, const VxVector &in value) const",
                        &ScriptBehaviorEditGraph::BindVec3);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "bool Bind(EditPort@ sink, const Value &in value) const",
                        &ScriptBehaviorEditGraph::BindAny);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "bool Bind(EditPorts@ sinks, const Value &in value) const",
                        &ScriptBehaviorEditGraph::BindMany);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "EditPort@ AppendIn(const string &in name) const",
                        &ScriptBehaviorEditGraph::AppendIn);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "EditPort@ AppendOut(const string &in name) const",
                        &ScriptBehaviorEditGraph::AppendOut);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "EditPort@ AppendPin(const string &in name, const CKGUID &in type) const",
                        &ScriptBehaviorEditGraph::AppendPin);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "EditPort@ AppendPout(const string &in name, const CKGUID &in type) const",
                        &ScriptBehaviorEditGraph::AppendPout);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "EditPort@ AppendLocal(const string &in name, const CKGUID &in type) const",
                        &ScriptBehaviorEditGraph::AppendLocal);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "EditPort@ AppendIn(EditNode@ owner, const string &in name) const",
                        &ScriptBehaviorEditGraph::AppendInOn);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "EditPort@ AppendOut(EditNode@ owner, const string &in name) const",
                        &ScriptBehaviorEditGraph::AppendOutOn);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "EditPort@ AppendPin(EditNode@ owner, const string &in name, const CKGUID &in type) const",
                        &ScriptBehaviorEditGraph::AppendPinOn);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "EditPort@ AppendPout(EditNode@ owner, const string &in name, const CKGUID &in type) const",
                        &ScriptBehaviorEditGraph::AppendPoutOn);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "EditPort@ AppendLocal(EditNode@ owner, const string &in name, const CKGUID &in type) const",
                        &ScriptBehaviorEditGraph::AppendLocalOn);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "bool Splice(EditLink@ link, EditNode@ through) const",
                        &ScriptBehaviorEditGraph::SpliceNode);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "bool Splice(EditLink@ link, EditPort@ sink, EditPort@ source) const",
                        &ScriptBehaviorEditGraph::SplicePorts);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "bool Redirect(EditLink@ link, EditPort@ sink) const",
                        &ScriptBehaviorEditGraph::RedirectPort);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "bool Redirect(EditLink@ link, EditLink@ destination) const",
                        &ScriptBehaviorEditGraph::RedirectLink);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "bool Reconnect(EditLink@ link, EditPort@ source, EditPort@ sink) const",
                        &ScriptBehaviorEditGraph::Reconnect);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "bool ReconnectCycle(EditLink@ link, EditPort@ source, EditPort@ sink) const",
                        &ScriptBehaviorEditGraph::ReconnectCycle);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "bool Tap(EditPort@ source, HookCallback@+ callback) const",
                        &ScriptBehaviorEditGraph::Tap);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "bool Tap(EditPorts@ sources, HookCallback@+ callback) const",
                        &ScriptBehaviorEditGraph::TapMany);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "bool Before(EditLink@ link, HookCallback@+ callback) const",
                        &ScriptBehaviorEditGraph::Before);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "bool After(EditPath@ path, HookCallback@+ callback) const",
                        &ScriptBehaviorEditGraph::AfterPath);
    BML_BEHAVIOR_METHOD(ScriptBehaviorEditGraph, "EditGraph",
                        "bool After(EditPort@ source, HookCallback@+ callback) const",
                        &ScriptBehaviorEditGraph::AfterPort);

#define BML_REGISTER_OWNED_STATE(type, nativeType, stateType)                     \
    BML_BEHAVIOR_DIRECT_METHOD(nativeType, type, "bool get_IsValid() const",     \
                               IsValid);                                          \
    BML_BEHAVIOR_METHOD(nativeType, type, #stateType " get_State() const",      \
                        &nativeType::State);                                      \
    BML_BEHAVIOR_METHOD(nativeType, type, "string get_Error() const",           \
                        &nativeType::Error);                                      \
    BML_BEHAVIOR_METHOD(nativeType, type, "bool Enable()",                      \
                        &nativeType::Enable);                                     \
    BML_BEHAVIOR_METHOD(nativeType, type, "bool Disable()",                     \
                        &nativeType::Disable);                                    \
    BML_BEHAVIOR_METHOD(nativeType, type, "CloseState Close()", &nativeType::Close)
    BML_REGISTER_OWNED_STATE("Patch", ScriptBehaviorPatch, PatchState);
    BML_REGISTER_OWNED_STATE("Plan", ScriptBehaviorPlan, PlanState);
#undef BML_REGISTER_OWNED_STATE
    BML_BEHAVIOR_METHOD(ScriptBehaviorPatch, "Patch",
                        "bool Replace(Graph@ graph, Edit@ edit)",
                        &ScriptBehaviorPatch::Replace);
    BML_BEHAVIOR_METHOD(ScriptBehaviorPatch, "Patch",
                        "ObjectRef@ Resolve(EditNode@ node) const",
                        &ScriptBehaviorPatch::Resolve);
    BML_BEHAVIOR_METHOD(ScriptBehaviorPlan, "Plan",
                        "bool Replace(const string &in script, Edit@ edit, bool each = false)",
                        &ScriptBehaviorPlan::Replace);

    BML_BEHAVIOR_DIRECT_METHOD(ScriptBehaviorScript, "Script",
                               "bool get_IsValid() const", IsValid);
    BML_BEHAVIOR_METHOD(ScriptBehaviorScript, "Script", "ScriptState get_State() const",
                        &ScriptBehaviorScript::State);
    BML_BEHAVIOR_METHOD(ScriptBehaviorScript, "Script", "string get_Error() const",
                        &ScriptBehaviorScript::Error);
    BML_BEHAVIOR_METHOD(ScriptBehaviorScript, "Script", "bool Activate()",
                        &ScriptBehaviorScript::Activate);
    BML_BEHAVIOR_METHOD(ScriptBehaviorScript, "Script", "bool Restart()",
                        &ScriptBehaviorScript::Restart);
    BML_BEHAVIOR_METHOD(ScriptBehaviorScript, "Script", "bool Deactivate()",
                        &ScriptBehaviorScript::Deactivate);
    BML_BEHAVIOR_METHOD(ScriptBehaviorScript, "Script",
                        "Graph@ Inspect(bool live = false) const",
                        &ScriptBehaviorScript::Inspect);
    BML_BEHAVIOR_METHOD(ScriptBehaviorScript, "Script",
                        "Patch@ Apply(const string &in name, Edit@ edit) const",
                        &ScriptBehaviorScript::Apply);
    BML_BEHAVIOR_METHOD(ScriptBehaviorScript, "Script", "CloseState Close()",
                        &ScriptBehaviorScript::Close);

    BML_BEHAVIOR_REGISTER(engine->RegisterGlobalFunction(
        "Selector Only()", BML_AS_GENERIC_FUNCTION(&Only), asCALL_GENERIC),
        "BML::Behavior::Only");
    BML_BEHAVIOR_REGISTER(engine->RegisterGlobalFunction(
        "Selector At(int index)", BML_AS_GENERIC_FUNCTION(&At), asCALL_GENERIC),
        "BML::Behavior::At");
    BML_BEHAVIOR_REGISTER(engine->RegisterGlobalFunction(
        "Selector Named(const string &in name, int occurrence)",
        BML_AS_GENERIC_FUNCTION(&Named), asCALL_GENERIC),
        "BML::Behavior::Named");
    BML_BEHAVIOR_REGISTER(engine->RegisterGlobalFunction(
        "Selector Unique(const string &in name)",
        BML_AS_GENERIC_FUNCTION(&Unique), asCALL_GENERIC),
        "BML::Behavior::Unique");
    BML_BEHAVIOR_REGISTER(engine->RegisterGlobalFunction(
        "FramePolicy Signals(uint limit = 64)",
        BML_AS_GENERIC_FUNCTION(&Signals), asCALL_GENERIC),
        "BML::Behavior::Signals");
    BML_BEHAVIOR_REGISTER(engine->RegisterGlobalFunction(
        "FramePolicy EachFrame(uint limit)",
        BML_AS_GENERIC_FUNCTION(&EachFrame), asCALL_GENERIC),
        "BML::Behavior::EachFrame");
    BML_BEHAVIOR_REGISTER(engine->RegisterGlobalFunction(
        "FramePolicy Latest()", BML_AS_GENERIC_FUNCTION(&Latest), asCALL_GENERIC),
        "BML::Behavior::Latest");
    BML_BEHAVIOR_REGISTER(engine->RegisterGlobalFunction(
        "FramePolicy Ignore()", BML_AS_GENERIC_FUNCTION(&Ignore), asCALL_GENERIC),
        "BML::Behavior::Ignore");
    BML_BEHAVIOR_REGISTER(engine->RegisterGlobalFunction(
        "Value Euler(const VxVector &in value)",
        BML_AS_GENERIC_FUNCTION(&MakeEuler), asCALL_GENERIC),
        "BML::Behavior::Euler");
    BML_BEHAVIOR_REGISTER(engine->RegisterGlobalFunction(
        "Block@ Use(const CKGUID &in prototype)",
        BML_AS_GENERIC_FUNCTION(&CurrentUse), asCALL_GENERIC),
        "BML::Behavior::Use");
    BML_BEHAVIOR_REGISTER(engine->RegisterGlobalFunction(
        "Block@ Find(const string &in name, const string &in category = \"\", const string &in provider = \"\")",
        BML_AS_GENERIC_FUNCTION(&CurrentFind), asCALL_GENERIC),
        "BML::Behavior::Find");
    BML_BEHAVIOR_REGISTER(engine->RegisterGlobalFunction(
        "Layout@ Describe(const CKGUID &in prototype)",
        BML_AS_GENERIC_FUNCTION(&CurrentDescription), asCALL_GENERIC),
        "BML::Behavior::Describe");
    BML_BEHAVIOR_REGISTER(engine->RegisterGlobalFunction(
        "Graph@ Inspect(CKBehavior@ graph, bool live = false)",
        BML_AS_GENERIC_FUNCTION(&CurrentInspect), asCALL_GENERIC),
        "BML::Behavior::Inspect");
    BML_BEHAVIOR_REGISTER(engine->RegisterGlobalFunction(
        "Script@ CreateScript(CKBeObject@ owner, const string &in name, Edit@ body, int priority = 0)",
        BML_AS_GENERIC_FUNCTION(&CurrentCreateScript), asCALL_GENERIC),
        "BML::Behavior::CreateScript");

    BML_BEHAVIOR_REGISTER(engine->SetDefaultNamespace("BML"),
                          "restore namespace BML");
    return asSUCCESS;
}

#undef BML_BEHAVIOR_GENERIC_CONSTRUCTOR
#undef BML_BEHAVIOR_DIRECT_METHOD
#undef BML_BEHAVIOR_METHOD
#undef BML_BEHAVIOR_REF
#undef BML_BEHAVIOR_REGISTER
