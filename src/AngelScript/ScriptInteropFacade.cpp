#include "ScriptInteropFacade.h"

#include <cstddef>
#include <limits>
#include <new>
#include <string>
#include <utility>

#include <angelscript.h>

#include "BML/Generated/bml_gameplay_api.h"
#include "BML/Generated/bml_events_api.h"
#include "BML/Generated/bml_runtime_api.h"
#include "BML/Generated/bml_scene_api.h"
#include "BML/EventKinds.h"
#include "BML/InteropApi.h"

#include "BuiltinInteropApis.h"
#include "InteropRegistry.h"
#include "InteropSessionService.h"
#include "ModContext.h"
#include "ScriptMod.h"
#include "ScriptModRuntime.h"
#include "ScriptFunctionSupport.h"
#include "ScriptStringInterop.h"

namespace {

namespace RuntimeApi = BML::Interop::Generated::Bml::Runtime;
namespace SceneApi = BML::Interop::Generated::Bml::Scene;
namespace GameplayApi = BML::Interop::Generated::Bml::Gameplay;
namespace EventsApi = BML::Interop::Generated::Bml::Events;

struct RuntimeState {
    bool InGame = false;
    bool InLevel = false;
    bool Paused = false;
    bool Playing = false;
    bool CheatEnabled = false;
};

struct ClockState {
    float TimeMs = 0.0f;
    float AbsoluteMs = 0.0f;
    float DeltaMs = 0.0f;
    int Frame = 0;
};

struct ScoreState {
    float SR = 0.0f;
    int HS = 0;
};

struct ObjectInfo {
    BML_ObjectRef Object{};
    int Id = 0;
    std::string Name;
    int ClassId = 0;
    bool Visible = false;
    bool Dynamic = false;
};

struct EntityTransform {
    BML_Vec3 Position{};
    BML_Vec3 Scale{};
    BML_ObjectRef Parent{};
    int ChildCount = 0;
};

struct LevelState {
    int Id = 0;
    BML_ObjectRef ActiveBall{};
    BML_Mat4 ResetMatrix{};
    int Points = 0;
};

struct EnergyState {
    int Points = 0;
    int Lives = 0;
    int StartPoints = 0;
    int StartLives = 0;
    float TimeFactor = 0.0f;
    int LifeBonus = 0;
};

struct CatalogEntry {
    std::string File;
    std::string StartBall;
    std::string Sky;
    int Bonus = 0;
    int Music = 0;
};

struct Checkpoint {
    BML_Mat4 Matrix{};
    BML_ObjectRef Object{};
};

struct Resetpoint {
    BML_ObjectRef Object{};
};

std::string g_InteropRegistrationError;

template <typename T>
void ConstructValue(T *self) {
    new (self) T();
}

template <typename T>
void CopyConstructValue(const T &other, T *self) {
    new (self) T(other);
}

template <typename T>
void DestructValue(T *self) {
    self->~T();
}

template <typename T>
T &AssignValue(const T &other, T *self) {
    *self = other;
    return *self;
}

#define BML_AS_DEFINE_INTEROP_VALUE(Type, Suffix)                                 \
    static void Construct##Suffix(Type *self) { ConstructValue(self); }            \
    static void CopyConstruct##Suffix(const Type &other, Type *self) {              \
        CopyConstructValue(other, self);                                            \
    }                                                                                \
    static void Destruct##Suffix(Type *self) { DestructValue(self); }               \
    static Type &Assign##Suffix(const Type &other, Type *self) {                    \
        return AssignValue(other, self);                                            \
    }

BML_AS_DEFINE_INTEROP_VALUE(RuntimeState, RuntimeState)
BML_AS_DEFINE_INTEROP_VALUE(ClockState, ClockState)
BML_AS_DEFINE_INTEROP_VALUE(ScoreState, ScoreState)
BML_AS_DEFINE_INTEROP_VALUE(ObjectInfo, ObjectInfo)
BML_AS_DEFINE_INTEROP_VALUE(EntityTransform, EntityTransform)
BML_AS_DEFINE_INTEROP_VALUE(LevelState, LevelState)
BML_AS_DEFINE_INTEROP_VALUE(EnergyState, EnergyState)
BML_AS_DEFINE_INTEROP_VALUE(CatalogEntry, CatalogEntry)
BML_AS_DEFINE_INTEROP_VALUE(Checkpoint, Checkpoint)
BML_AS_DEFINE_INTEROP_VALUE(Resetpoint, Resetpoint)

#undef BML_AS_DEFINE_INTEROP_VALUE

struct ValueTypeRegistration {
    const char *Name;
    int Size;
    asDWORD Flags;
    asSFuncPtr Construct;
    asSFuncPtr CopyConstruct;
    asSFuncPtr Destruct;
    asSFuncPtr Assign;
};

template <typename T>
ValueTypeRegistration ValueType(const char *name,
                                asSFuncPtr construct,
                                asSFuncPtr copyConstruct,
                                asSFuncPtr destruct,
                                asSFuncPtr assign) {
    return {name, sizeof(T), asOBJ_VALUE | asGetTypeTraits<T>(), construct, copyConstruct, destruct, assign};
}

bool Register(asIScriptEngine *engine, int status, const char *declaration, const char **errorMessage) {
    if (status >= 0)
        return true;
    g_InteropRegistrationError = "Failed to register Interop script declaration: ";
    g_InteropRegistrationError += declaration;
    g_InteropRegistrationError += " returned ";
    g_InteropRegistrationError += std::to_string(status);
    if (errorMessage)
        *errorMessage = g_InteropRegistrationError.c_str();
    if (engine)
        engine->SetDefaultNamespace("");
    return false;
}

bool RegisterValue(asIScriptEngine *engine, const ValueTypeRegistration &registration, const char **errorMessage) {
    const std::string copyConstructor = std::string("void f(const ") + registration.Name + " &in)";
    const std::string assign = std::string(registration.Name) + " &opAssign(const " + registration.Name + " &in)";
    return Register(engine,
                    engine->RegisterObjectType(registration.Name, registration.Size, registration.Flags),
                    registration.Name,
                    errorMessage) &&
           Register(engine,
                    engine->RegisterObjectBehaviour(registration.Name,
                                                    asBEHAVE_CONSTRUCT,
                                                    "void f()",
                                                    registration.Construct,
                                                    asCALL_CDECL_OBJLAST),
                    "value default constructor",
                    errorMessage) &&
           Register(engine,
                    engine->RegisterObjectBehaviour(registration.Name,
                                                    asBEHAVE_CONSTRUCT,
                                                    copyConstructor.c_str(),
                                                    registration.CopyConstruct,
                                                    asCALL_CDECL_OBJLAST),
                    "value copy constructor",
                    errorMessage) &&
           Register(engine,
                    engine->RegisterObjectBehaviour(registration.Name,
                                                    asBEHAVE_DESTRUCT,
                                                    "void f()",
                                                    registration.Destruct,
                                                    asCALL_CDECL_OBJLAST),
                    "value destructor",
                    errorMessage) &&
           Register(engine,
                    engine->RegisterObjectMethod(registration.Name,
                                                 assign.c_str(),
                                                 registration.Assign,
                                                 asCALL_CDECL_OBJLAST),
                    "value assignment",
                    errorMessage);
}

struct ActiveCall {
    ModContext *Context = nullptr;
    BML_InteropCallContext CallContext{};
};

int GetActiveCall(ActiveCall &out) {
    out = {};
    if (BML::RejectScriptRestrictedHostCall("BML::Interop"))
        return BML_ERROR_FROZEN;
    BML::ScriptMod *mod = BML::ScriptModRuntime::GetCurrentScriptMod();
    if (!mod || !mod->GetModContext())
        return BML_ERROR_INTEROP_UNSUPPORTED;
    out.Context = mod->GetModContext();
    out.CallContext = out.Context->GetInteropSessions().CreateContextForOwner(mod->GetID());
    return out.Context->GetInteropSessions().ValidateContext(&out.CallContext, true);
}

int GetActiveCallForApi(ActiveCall &out, const BML_InteropApiDescriptor &api) {
    const int status = GetActiveCall(out);
    return status == BML_OK
               ? out.Context->GetInteropRegistry().RequireApi(&out.CallContext,
                                                                      api.ApiId,
                                                                      api.Major,
                                                                      api.Hash)
               : status;
}

int GetOwnerCall(ModContext *context, const std::string &owner, BML_InteropCallContext &out) {
    out = {};
    if (!context || owner.empty())
        return BML_ERROR_INTEROP_UNSUPPORTED;
    out = context->GetInteropSessions().CreateContextForOwner(owner);
    return context->GetInteropSessions().ValidateContext(&out, true);
}

class RecordLease {
public:
    RecordLease() = default;
    RecordLease(ModContext *context, BML_InteropCallContext callContext, BML_RecordRef record)
        : m_Context(context), m_CallContext(callContext), m_Record(record) {}
    ~RecordLease() { Reset(); }

    RecordLease(const RecordLease &) = delete;
    RecordLease &operator=(const RecordLease &) = delete;

    void Reset() {
        if (m_Context && m_Record.Value)
            (void)m_Context->GetInteropRegistry().ReleaseRecord(&m_CallContext, m_Record);
        m_Record = {};
    }

private:
    ModContext *m_Context = nullptr;
    BML_InteropCallContext m_CallContext{};
    BML_RecordRef m_Record{};
};

class BuilderLease {
public:
    BuilderLease() = default;
    BuilderLease(ModContext *context, BML_InteropRecordBuilder *builder) : m_Context(context), m_Builder(builder) {}
    ~BuilderLease() { Reset(); }

    BuilderLease(const BuilderLease &) = delete;
    BuilderLease &operator=(const BuilderLease &) = delete;

    void Reset() {
        if (m_Context && m_Builder)
            (void)m_Context->GetInteropRegistry().DestroyRecordBuilder(m_Builder);
        m_Builder = nullptr;
    }

private:
    ModContext *m_Context = nullptr;
    BML_InteropRecordBuilder *m_Builder = nullptr;
};

int ReadResource(const BML_InteropApiDescriptor &api,
                 const char *endpoint,
                 ActiveCall &call,
                 BML_RecordRef &outRecord) {
    outRecord = {};
    const int status = GetActiveCallForApi(call, api);
    return status == BML_OK
               ? call.Context->GetInteropRegistry().ReadResource(&call.CallContext,
                                                                   api.ApiId,
                                                                   endpoint,
                                                                   &outRecord)
               : status;
}

int ReadComponent(CKObject *object,
                  const BML_InteropApiDescriptor &api,
                  const char *endpoint,
                  ActiveCall &call,
                  BML_RecordRef &outRecord) {
    outRecord = {};
    const int status = GetActiveCallForApi(call, api);
    if (status != BML_OK)
        return status;
    const BML_ObjectRef reference = MakeBuiltinObjectRef(*call.Context, object);
    if (object && reference.Domain == 0)
        return BML_ERROR_INTEROP_OBJECT_INVALID;
    return call.Context->GetInteropRegistry().ReadComponent(&call.CallContext,
                                                              api.ApiId,
                                                              endpoint,
                                                              reference,
                                                              &outRecord);
}

int ReadBool(const ActiveCall &call, BML_RecordRef record, uint32_t field, bool &out) {
    int value = 0;
    const int status = call.Context->GetInteropRegistry().RecordGetBool(&call.CallContext, record, field, &value);
    if (status == BML_OK)
        out = value != 0;
    return status;
}

int ReadInt(const ActiveCall &call, BML_RecordRef record, uint32_t field, int &out) {
    return call.Context->GetInteropRegistry().RecordGetInt(&call.CallContext, record, field, &out);
}

int ReadFloat(const ActiveCall &call, BML_RecordRef record, uint32_t field, float &out) {
    return call.Context->GetInteropRegistry().RecordGetFloat(&call.CallContext, record, field, &out);
}

int ReadObject(const ActiveCall &call, BML_RecordRef record, uint32_t field, BML_ObjectRef &out) {
    return call.Context->GetInteropRegistry().RecordGetObject(&call.CallContext, record, field, &out);
}

int ReadVec3(const ActiveCall &call, BML_RecordRef record, uint32_t field, BML_Vec3 &out) {
    return call.Context->GetInteropRegistry().RecordGetVec3(&call.CallContext, record, field, &out);
}

int ReadMat4(const ActiveCall &call, BML_RecordRef record, uint32_t field, BML_Mat4 &out) {
    return call.Context->GetInteropRegistry().RecordGetMat4(&call.CallContext, record, field, &out);
}

int ReadString(const ActiveCall &call, BML_RecordRef record, uint32_t field, std::string &out) {
    size_t required = 0;
    int status = call.Context->GetInteropRegistry().RecordGetString(&call.CallContext,
                                                                       record,
                                                                       field,
                                                                       nullptr,
                                                                       0,
                                                                       &required);
    if (status != BML_OK)
        return status;
    std::string storage(required ? required : 1, '\0');
    status = call.Context->GetInteropRegistry().RecordGetString(&call.CallContext,
                                                                   record,
                                                                   field,
                                                                   storage.data(),
                                                                   storage.size(),
                                                                   &required);
    if (status == BML_OK)
        out.assign(storage.data(), required > 0 ? required - 1 : 0);
    return status;
}

int ReadRuntime(RuntimeState &out) {
    ActiveCall call;
    BML_RecordRef record{};
    int status = ReadResource(RuntimeApi::Descriptor, "state", call, record);
    RecordLease lease(call.Context, call.CallContext, record);
    RuntimeState value{};
    if (status == BML_OK) status = ReadBool(call, record, RuntimeApi::RuntimeStateField::InGame, value.InGame);
    if (status == BML_OK) status = ReadBool(call, record, RuntimeApi::RuntimeStateField::InLevel, value.InLevel);
    if (status == BML_OK) status = ReadBool(call, record, RuntimeApi::RuntimeStateField::Paused, value.Paused);
    if (status == BML_OK) status = ReadBool(call, record, RuntimeApi::RuntimeStateField::Playing, value.Playing);
    if (status == BML_OK) status = ReadBool(call, record, RuntimeApi::RuntimeStateField::CheatEnabled, value.CheatEnabled);
    if (status == BML_OK) out = value;
    return status;
}

int ReadClock(ClockState &out) {
    ActiveCall call;
    BML_RecordRef record{};
    int status = ReadResource(RuntimeApi::Descriptor, "clock", call, record);
    RecordLease lease(call.Context, call.CallContext, record);
    ClockState value{};
    if (status == BML_OK) status = ReadFloat(call, record, RuntimeApi::ClockStateField::TimeMs, value.TimeMs);
    if (status == BML_OK) status = ReadFloat(call, record, RuntimeApi::ClockStateField::AbsoluteMs, value.AbsoluteMs);
    if (status == BML_OK) status = ReadFloat(call, record, RuntimeApi::ClockStateField::DeltaMs, value.DeltaMs);
    if (status == BML_OK) status = ReadInt(call, record, RuntimeApi::ClockStateField::Frame, value.Frame);
    if (status == BML_OK) out = value;
    return status;
}

int ReadScore(ScoreState &out) {
    ActiveCall call;
    BML_RecordRef record{};
    int status = ReadResource(RuntimeApi::Descriptor, "score", call, record);
    RecordLease lease(call.Context, call.CallContext, record);
    ScoreState value{};
    if (status == BML_OK) status = ReadFloat(call, record, RuntimeApi::ScoreStateField::Sr, value.SR);
    if (status == BML_OK) status = ReadInt(call, record, RuntimeApi::ScoreStateField::Hs, value.HS);
    if (status == BML_OK) out = value;
    return status;
}

int ReadObjectInfo(CKObject *object, ObjectInfo &out) {
    ActiveCall call;
    BML_RecordRef record{};
    int status = ReadComponent(object, SceneApi::Descriptor, "object", call, record);
    RecordLease lease(call.Context, call.CallContext, record);
    ObjectInfo value{};
    if (status == BML_OK) value.Object = MakeBuiltinObjectRef(*call.Context, object);
    if (status == BML_OK) status = ReadInt(call, record, SceneApi::ObjectInfoField::Id, value.Id);
    if (status == BML_OK) status = ReadString(call, record, SceneApi::ObjectInfoField::Name, value.Name);
    if (status == BML_OK) status = ReadInt(call, record, SceneApi::ObjectInfoField::ClassId, value.ClassId);
    if (status == BML_OK) status = ReadBool(call, record, SceneApi::ObjectInfoField::Visible, value.Visible);
    if (status == BML_OK) status = ReadBool(call, record, SceneApi::ObjectInfoField::Dynamic, value.Dynamic);
    if (status == BML_OK) out = std::move(value);
    return status;
}

int ReadEntity(CKObject *object, EntityTransform &out) {
    ActiveCall call;
    BML_RecordRef record{};
    int status = ReadComponent(object, SceneApi::Descriptor, "entity", call, record);
    RecordLease lease(call.Context, call.CallContext, record);
    EntityTransform value{};
    if (status == BML_OK) status = ReadVec3(call, record, SceneApi::EntityTransformField::Position, value.Position);
    if (status == BML_OK) status = ReadVec3(call, record, SceneApi::EntityTransformField::Scale, value.Scale);
    if (status == BML_OK) status = ReadObject(call, record, SceneApi::EntityTransformField::Parent, value.Parent);
    if (status == BML_OK) status = ReadInt(call, record, SceneApi::EntityTransformField::ChildCount, value.ChildCount);
    if (status == BML_OK) out = value;
    return status;
}

int FindObject(const std::string &name, int classId, bool filterClass, CKObject *&out) {
    out = nullptr;
    ActiveCall call;
    int status = GetActiveCallForApi(call, SceneApi::Descriptor);
    if (status != BML_OK)
        return status;

    const uint32_t schema = filterClass ? SceneApi::FindNameClassRequest.Id : SceneApi::FindNameRequest.Id;
    const char *endpoint = filterClass ? "find_name_class" : "find_name";
    BML_InteropRecordBuilder *builder = nullptr;
    status = call.Context->GetInteropRegistry().CreateInputRecord(&call.CallContext, SceneApi::ApiId, schema, &builder);
    BuilderLease builderLease(call.Context, builder);
    if (status == BML_OK)
        status = call.Context->GetInteropRegistry().BuilderSetValue(builder,
                                                                       SceneApi::FindNameRequestField::Name,
                                                                       BML_INTEROP_FIELD_STRING,
                                                                       name.data(),
                                                                       name.size());
    if (status == BML_OK && filterClass)
        status = call.Context->GetInteropRegistry().BuilderSetValue(builder,
                                                                       SceneApi::FindNameClassRequestField::ClassId,
                                                                       BML_INTEROP_FIELD_INT,
                                                                       &classId,
                                                                       1);

    BML_RecordRef record{};
    if (status == BML_OK)
        status = call.Context->GetInteropRegistry().Invoke(&call.CallContext,
                                                              SceneApi::ApiId,
                                                              endpoint,
                                                              BML_INTEROP_ENDPOINT_QUERY,
                                                              builder,
                                                              &record);
    RecordLease recordLease(call.Context, call.CallContext, record);
    BML_ObjectRef reference{};
    if (status == BML_OK)
        status = ReadObject(call, record, SceneApi::FindResultField::Object, reference);
    if (status == BML_OK && reference.Domain != 0) {
        out = ResolveBuiltinObjectRef(*call.Context, reference);
        if (!out)
            status = BML_ERROR_INTEROP_OBJECT_INVALID;
    }
    return status;
}

int FindByName(const std::string &name, CKObject *&out) {
    return FindObject(name, 0, false, out);
}

int FindByNameAndClass(const std::string &name, int classId, CKObject *&out) {
    return FindObject(name, classId, true, out);
}

int ReadLevel(LevelState &out) {
    ActiveCall call;
    BML_RecordRef record{};
    int status = ReadResource(GameplayApi::Descriptor, "level", call, record);
    RecordLease lease(call.Context, call.CallContext, record);
    LevelState value{};
    if (status == BML_OK) status = ReadInt(call, record, GameplayApi::LevelStateField::Id, value.Id);
    if (status == BML_OK) status = ReadObject(call, record, GameplayApi::LevelStateField::ActiveBall, value.ActiveBall);
    if (status == BML_OK) status = ReadMat4(call, record, GameplayApi::LevelStateField::ResetMatrix, value.ResetMatrix);
    if (status == BML_OK) status = ReadInt(call, record, GameplayApi::LevelStateField::Points, value.Points);
    if (status == BML_OK) out = value;
    return status;
}

int ReadEnergy(EnergyState &out) {
    ActiveCall call;
    BML_RecordRef record{};
    int status = ReadResource(GameplayApi::Descriptor, "energy", call, record);
    RecordLease lease(call.Context, call.CallContext, record);
    EnergyState value{};
    if (status == BML_OK) status = ReadInt(call, record, GameplayApi::EnergyStateField::Points, value.Points);
    if (status == BML_OK) status = ReadInt(call, record, GameplayApi::EnergyStateField::Lives, value.Lives);
    if (status == BML_OK) status = ReadInt(call, record, GameplayApi::EnergyStateField::StartPoints, value.StartPoints);
    if (status == BML_OK) status = ReadInt(call, record, GameplayApi::EnergyStateField::StartLives, value.StartLives);
    if (status == BML_OK) status = ReadFloat(call, record, GameplayApi::EnergyStateField::TimeFactor, value.TimeFactor);
    if (status == BML_OK) status = ReadInt(call, record, GameplayApi::EnergyStateField::LifeBonus, value.LifeBonus);
    if (status == BML_OK) out = value;
    return status;
}

CKObject *BorrowObject(const ObjectInfo *value) {
    ActiveCall call;
    return value && GetActiveCall(call) == BML_OK
               ? ResolveBuiltinObjectRef(*call.Context, value->Object)
               : nullptr;
}

CKObject *BorrowParent(const EntityTransform *value) {
    ActiveCall call;
    return value && GetActiveCall(call) == BML_OK
               ? ResolveBuiltinObjectRef(*call.Context, value->Parent)
               : nullptr;
}

CKObject *BorrowActiveBall(const LevelState *value) {
    ActiveCall call;
    return value && GetActiveCall(call) == BML_OK
               ? ResolveBuiltinObjectRef(*call.Context, value->ActiveBall)
               : nullptr;
}

CKObject *BorrowCheckpointObject(const Checkpoint *value) {
    ActiveCall call;
    return value && GetActiveCall(call) == BML_OK
               ? ResolveBuiltinObjectRef(*call.Context, value->Object)
               : nullptr;
}

CKObject *BorrowResetpointObject(const Resetpoint *value) {
    ActiveCall call;
    return value && GetActiveCall(call) == BML_OK
               ? ResolveBuiltinObjectRef(*call.Context, value->Object)
               : nullptr;
}

class CollectionCursor {
public:
    void AddRef() { ++m_RefCount; }
    void Release() {
        if (--m_RefCount == 0)
            delete this;
    }
    bool IsOpen() const { return m_Cursor.Value != 0; }
    int Close() {
        if (m_Cursor.Value == 0)
            return BML_OK;
        BML_InteropCallContext call{};
        const int contextStatus = GetOwnerCall(m_Context, m_Owner, call);
        const int status = contextStatus == BML_OK
                               ? m_Context->GetInteropRegistry().CloseCollection(&call, m_Cursor)
                               : contextStatus;
        m_Cursor = {};
        return status;
    }

protected:
    CollectionCursor(ModContext *context, std::string owner, BML_CursorRef cursor)
        : m_Context(context), m_Owner(std::move(owner)), m_Cursor(cursor) {}
    virtual ~CollectionCursor() { (void)Close(); }

    ModContext *Context() const { return m_Context; }

    int ReadOne(BML_InteropCallContext &call,
                BML_RecordRef &outRecord,
                bool &outHasValue,
                bool &outComplete) {
        outRecord = {};
        outHasValue = false;
        outComplete = false;
        const int contextStatus = GetOwnerCall(m_Context, m_Owner, call);
        if (contextStatus != BML_OK)
            return contextStatus;
        size_t count = 0;
        int complete = 0;
        int status = m_Context->GetInteropRegistry().ReadCollectionPage(&call,
                                                                           m_Cursor,
                                                                           &outRecord,
                                                                           1,
                                                                           &count,
                                                                           &complete);
        if (status == BML_OK) {
            outHasValue = count != 0;
            outComplete = complete != 0;
        }
        return status;
    }

private:
    int m_RefCount = 1;
    ModContext *m_Context = nullptr;
    std::string m_Owner;
    BML_CursorRef m_Cursor{};
};

class CatalogCursor final : public CollectionCursor {
public:
    CatalogCursor(ModContext *context, std::string owner, BML_CursorRef cursor)
        : CollectionCursor(context, std::move(owner), cursor) {}
    int Next(CatalogEntry &out, bool &hasValue, bool &complete) {
        BML_InteropCallContext call{};
        BML_RecordRef record{};
        int status = ReadOne(call, record, hasValue, complete);
        RecordLease lease(Context(), call, record);
        CatalogEntry value{};
        if (status == BML_OK && hasValue) {
            if (status == BML_OK) status = ReadString({Context(), call}, record, GameplayApi::CatalogEntryField::File, value.File);
            if (status == BML_OK) status = ReadString({Context(), call}, record, GameplayApi::CatalogEntryField::StartBall, value.StartBall);
            if (status == BML_OK) status = ReadString({Context(), call}, record, GameplayApi::CatalogEntryField::Sky, value.Sky);
            if (status == BML_OK) status = ReadInt({Context(), call}, record, GameplayApi::CatalogEntryField::Bonus, value.Bonus);
            if (status == BML_OK) status = ReadInt({Context(), call}, record, GameplayApi::CatalogEntryField::Music, value.Music);
        }
        if (status == BML_OK && hasValue) out = std::move(value);
        return status;
    }
};

class CheckpointCursor final : public CollectionCursor {
public:
    CheckpointCursor(ModContext *context, std::string owner, BML_CursorRef cursor)
        : CollectionCursor(context, std::move(owner), cursor) {}
    int Next(Checkpoint &out, bool &hasValue, bool &complete) {
        BML_InteropCallContext call{};
        BML_RecordRef record{};
        int status = ReadOne(call, record, hasValue, complete);
        RecordLease lease(Context(), call, record);
        Checkpoint value{};
        if (status == BML_OK && hasValue) {
            if (status == BML_OK) status = ReadMat4({Context(), call}, record, GameplayApi::CheckpointField::Matrix, value.Matrix);
            if (status == BML_OK) status = ReadObject({Context(), call}, record, GameplayApi::CheckpointField::Object, value.Object);
        }
        if (status == BML_OK && hasValue) out = value;
        return status;
    }
};

class ResetpointCursor final : public CollectionCursor {
public:
    ResetpointCursor(ModContext *context, std::string owner, BML_CursorRef cursor)
        : CollectionCursor(context, std::move(owner), cursor) {}
    int Next(Resetpoint &out, bool &hasValue, bool &complete) {
        BML_InteropCallContext call{};
        BML_RecordRef record{};
        int status = ReadOne(call, record, hasValue, complete);
        RecordLease lease(Context(), call, record);
        Resetpoint value{};
        if (status == BML_OK && hasValue)
            status = ReadObject({Context(), call}, record, GameplayApi::ResetpointField::Object, value.Object);
        if (status == BML_OK && hasValue) out = value;
        return status;
    }
};

template <typename T>
int OpenCollection(const char *endpoint, T *&out) {
    out = nullptr;
    ActiveCall call;
    const int status = GetActiveCallForApi(call, GameplayApi::Descriptor);
    if (status != BML_OK)
        return status;
    BML_CursorRef cursor{};
    const int openStatus = call.Context->GetInteropRegistry().OpenCollection(&call.CallContext,
                                                                                GameplayApi::ApiId,
                                                                                endpoint,
                                                                                &cursor);
    if (openStatus != BML_OK)
        return openStatus;
    T *result = new (std::nothrow) T(call.Context, BML::ScriptModRuntime::GetCurrentScriptMod()->GetID(), cursor);
    if (!result) {
        (void)call.Context->GetInteropRegistry().CloseCollection(&call.CallContext, cursor);
        return BML_ERROR_OUT_OF_MEMORY;
    }
    out = result;
    return BML_OK;
}

int OpenCatalog(CatalogCursor *&out) { return OpenCollection("catalog", out); }
int OpenCheckpoints(CheckpointCursor *&out) { return OpenCollection("checkpoints", out); }
int OpenResetpoints(ResetpointCursor *&out) { return OpenCollection("resetpoints", out); }

/* A script Event owns the immutable record received from bml.events.  It does
 * not keep a CK pointer: object values are re-resolved only by Borrow* calls
 * and become null after the underlying object or session goes stale. */
class ScriptEvent {
public:
    ScriptEvent(ModContext *context, std::string owner, BML_RecordRef record)
        : m_Context(context), m_Owner(std::move(owner)), m_Record(record) {}
    ~ScriptEvent() { ReleaseNative(); }

    void AddRef() { ++m_RefCount; }
    void Release() {
        if (--m_RefCount == 0)
            delete this;
    }

    bool IsValid() const { return Status() == BML_OK; }
    int Status() const {
        BML_InteropCallContext call{};
        const int contextStatus = GetOwnerCall(m_Context, m_Owner, call);
        if (contextStatus != BML_OK)
            return contextStatus;
        uint32_t schema = 0;
        return m_Context->GetInteropRegistry().RecordSchema(&call, m_Record, &schema);
    }

    int GetKind() const { return Int(EventsApi::EventField::Kind); }
    uint64_t GetSequence() const {
        BML_InteropCallContext call{};
        uint64_t value = 0;
        return GetOwnerCall(m_Context, m_Owner, call) == BML_OK &&
                       m_Context->GetInteropRegistry().RecordSequence(&call, m_Record, &value) == BML_OK
                   ? value
                   : 0;
    }
    uint64_t GetTimestamp() const {
        BML_InteropCallContext call{};
        uint64_t value = 0;
        return GetOwnerCall(m_Context, m_Owner, call) == BML_OK &&
                       m_Context->GetInteropRegistry().RecordTimestamp(&call, m_Record, &value) == BML_OK
                   ? value
                   : 0;
    }

    bool IsLoad() const {
        const int kind = GetKind();
        return kind == BML_EVENT_LOAD_OBJECT || kind == BML_EVENT_LOAD_SCRIPT;
    }
    bool IsPhysics() const {
        const int kind = GetKind();
        return kind == BML_EVENT_PHYSICALIZE || kind == BML_EVENT_UNPHYSICALIZE;
    }
    bool IsCommand() const {
        const int kind = GetKind();
        return kind == BML_EVENT_COMMAND_PRE || kind == BML_EVENT_COMMAND_POST;
    }
    bool IsConfig() const { return GetKind() == BML_EVENT_CONFIG_MODIFIED; }
    bool IsCheat() const { return GetKind() == BML_EVENT_CHEAT_CHANGED; }

    std::string GetFilename() const { return String(EventsApi::EventField::Filename); }
    bool GetIsMap() const { return Bool(EventsApi::EventField::IsMap); }
    std::string GetMasterName() const { return String(EventsApi::EventField::MasterName); }
    int GetFilterClass() const { return Int(EventsApi::EventField::FilterClass); }
    bool GetAddToScene() const { return Bool(EventsApi::EventField::AddToScene); }
    bool GetReuseMeshes() const { return Bool(EventsApi::EventField::ReuseMeshes); }
    bool GetReuseMaterials() const { return Bool(EventsApi::EventField::ReuseMaterials); }
    bool GetDynamic() const { return Bool(EventsApi::EventField::Dynamic); }
    CKObject *BorrowMasterObject() const { return Object(EventsApi::EventField::MasterObject); }
    CKObject *BorrowScript() const { return Object(EventsApi::EventField::Script); }
    int GetObjectCount() const { return ArrayCount(EventsApi::EventField::ObjectIds, BML_INTEROP_FIELD_OBJECT_ARRAY); }
    CKObject *BorrowObject(int index) const { return ArrayObject(EventsApi::EventField::ObjectIds, index); }

    CKObject *BorrowTarget() const { return Object(EventsApi::EventField::Target); }
    bool GetFixed() const { return Bool(EventsApi::EventField::Fixed); }
    float GetFriction() const { return Float(EventsApi::EventField::Friction); }
    float GetElasticity() const { return Float(EventsApi::EventField::Elasticity); }
    float GetMass() const { return Float(EventsApi::EventField::Mass); }
    std::string GetCollisionGroup() const { return String(EventsApi::EventField::CollisionGroup); }
    bool GetStartFrozen() const { return Bool(EventsApi::EventField::StartFrozen); }
    bool GetEnableCollision() const { return Bool(EventsApi::EventField::EnableCollision); }
    bool GetAutoCalculateMassCenter() const { return Bool(EventsApi::EventField::AutoCalculateMassCenter); }
    float GetLinearDamp() const { return Float(EventsApi::EventField::LinearDamp); }
    float GetRotDamp() const { return Float(EventsApi::EventField::RotDamp); }
    std::string GetCollisionSurface() const { return String(EventsApi::EventField::CollisionSurface); }
    BML_Vec3 GetMassCenter() const { return Vec3(EventsApi::EventField::MassCenter); }
    int GetConvexMeshCount() const { return ArrayCount(EventsApi::EventField::ConvexMeshes, BML_INTEROP_FIELD_OBJECT_ARRAY); }
    CKObject *BorrowConvexMesh(int index) const { return ArrayObject(EventsApi::EventField::ConvexMeshes, index); }
    int GetBallCount() const { return ArrayCount(EventsApi::EventField::BallCenters, BML_INTEROP_FIELD_VEC3_ARRAY); }
    BML_Vec3 GetBallCenter(int index) const { return ArrayVec3(EventsApi::EventField::BallCenters, index); }
    float GetBallRadius(int index) const { return ArrayFloat(EventsApi::EventField::BallRadii, index); }
    int GetConcaveMeshCount() const { return ArrayCount(EventsApi::EventField::ConcaveMeshes, BML_INTEROP_FIELD_OBJECT_ARRAY); }
    CKObject *BorrowConcaveMesh(int index) const { return ArrayObject(EventsApi::EventField::ConcaveMeshes, index); }

    std::string GetCommand() const { return String(EventsApi::EventField::Command); }
    int GetCommandArgumentCount() const { return StringArrayCount(EventsApi::EventField::CommandArgs); }
    std::string GetCommandArgument(int index) const { return StringArrayItem(EventsApi::EventField::CommandArgs, index); }

    std::string GetConfigCategory() const { return String(EventsApi::EventField::ConfigCategory); }
    std::string GetConfigKey() const { return String(EventsApi::EventField::ConfigKey); }
    int GetConfigType() const { return Int(EventsApi::EventField::ConfigType); }
    std::string GetConfigValue() const { return String(EventsApi::EventField::ConfigValue); }
    bool GetCheatEnabled() const { return Bool(EventsApi::EventField::CheatEnabled); }

private:
    int Call(BML_InteropCallContext &out) const {
        return GetOwnerCall(m_Context, m_Owner, out);
    }
    int Int(uint32_t field) const {
        BML_InteropCallContext call{};
        int value = 0;
        return Call(call) == BML_OK &&
                       m_Context->GetInteropRegistry().RecordGetInt(&call, m_Record, field, &value) == BML_OK
                   ? value
                   : 0;
    }
    bool Bool(uint32_t field) const {
        BML_InteropCallContext call{};
        int value = 0;
        return Call(call) == BML_OK &&
               m_Context->GetInteropRegistry().RecordGetBool(&call, m_Record, field, &value) == BML_OK && value != 0;
    }
    float Float(uint32_t field) const {
        BML_InteropCallContext call{};
        float value = 0.0f;
        return Call(call) == BML_OK &&
                       m_Context->GetInteropRegistry().RecordGetFloat(&call, m_Record, field, &value) == BML_OK
                   ? value
                   : 0.0f;
    }
    std::string String(uint32_t field) const {
        BML_InteropCallContext call{};
        if (Call(call) != BML_OK)
            return {};
        size_t required = 0;
        if (m_Context->GetInteropRegistry().RecordGetString(&call, m_Record, field, nullptr, 0, &required) != BML_OK)
            return {};
        std::string result(required ? required : 1, '\0');
        if (m_Context->GetInteropRegistry().RecordGetString(&call, m_Record, field,
                                                              result.data(), result.size(), &required) != BML_OK)
            return {};
        result.resize(result.empty() ? 0 : result.size() - 1);
        return result;
    }
    BML_Vec3 Vec3(uint32_t field) const {
        BML_InteropCallContext call{};
        BML_Vec3 value{};
        return Call(call) == BML_OK &&
                       m_Context->GetInteropRegistry().RecordGetVec3(&call, m_Record, field, &value) == BML_OK
                   ? value
                   : BML_Vec3{};
    }
    CKObject *Object(uint32_t field) const {
        BML_InteropCallContext call{};
        BML_ObjectRef value{};
        return Call(call) == BML_OK &&
                       m_Context->GetInteropRegistry().RecordGetObject(&call, m_Record, field, &value) == BML_OK
                   ? ResolveBuiltinObjectRef(*m_Context, value)
                   : nullptr;
    }
    int ArrayCount(uint32_t field, BML_INTEROP_FIELD_TYPE type) const {
        BML_InteropCallContext call{};
        if (Call(call) != BML_OK)
            return 0;
        const void *data = nullptr;
        size_t count = 0;
        size_t elementSize = 0;
        if (m_Context->GetInteropRegistry().RecordBorrowValue(&call, m_Record, field, type,
                                                                 &data, &count, &elementSize) != BML_OK ||
            count > static_cast<size_t>((std::numeric_limits<int>::max)())) {
            return 0;
        }
        return static_cast<int>(count);
    }
    CKObject *ArrayObject(uint32_t field, int index) const {
        if (index < 0)
            return nullptr;
        BML_InteropCallContext call{};
        const void *data = nullptr;
        size_t count = 0;
        size_t elementSize = 0;
        if (Call(call) != BML_OK ||
            m_Context->GetInteropRegistry().RecordBorrowValue(&call, m_Record, field,
                                                                 BML_INTEROP_FIELD_OBJECT_ARRAY,
                                                                 &data, &count, &elementSize) != BML_OK ||
            elementSize != sizeof(BML_ObjectRef) || static_cast<size_t>(index) >= count || !data) {
            return nullptr;
        }
        return ResolveBuiltinObjectRef(*m_Context, static_cast<const BML_ObjectRef *>(data)[index]);
    }
    BML_Vec3 ArrayVec3(uint32_t field, int index) const {
        if (index < 0)
            return {};
        BML_InteropCallContext call{};
        const void *data = nullptr;
        size_t count = 0;
        size_t elementSize = 0;
        if (Call(call) != BML_OK ||
            m_Context->GetInteropRegistry().RecordBorrowValue(&call, m_Record, field,
                                                                 BML_INTEROP_FIELD_VEC3_ARRAY,
                                                                 &data, &count, &elementSize) != BML_OK ||
            elementSize != sizeof(BML_Vec3) || static_cast<size_t>(index) >= count || !data) {
            return {};
        }
        return static_cast<const BML_Vec3 *>(data)[index];
    }
    float ArrayFloat(uint32_t field, int index) const {
        if (index < 0)
            return 0.0f;
        BML_InteropCallContext call{};
        const void *data = nullptr;
        size_t count = 0;
        size_t elementSize = 0;
        if (Call(call) != BML_OK ||
            m_Context->GetInteropRegistry().RecordBorrowValue(&call, m_Record, field,
                                                                 BML_INTEROP_FIELD_FLOAT_ARRAY,
                                                                 &data, &count, &elementSize) != BML_OK ||
            elementSize != sizeof(float) || static_cast<size_t>(index) >= count || !data) {
            return 0.0f;
        }
        return static_cast<const float *>(data)[index];
    }
    int StringArrayCount(uint32_t field) const {
        BML_InteropCallContext call{};
        size_t count = 0;
        return Call(call) == BML_OK &&
                       m_Context->GetInteropRegistry().RecordGetStringArrayCount(&call, m_Record, field, &count) == BML_OK &&
                       count <= static_cast<size_t>((std::numeric_limits<int>::max)())
                   ? static_cast<int>(count)
                   : 0;
    }
    std::string StringArrayItem(uint32_t field, int index) const {
        if (index < 0)
            return {};
        BML_InteropCallContext call{};
        if (Call(call) != BML_OK)
            return {};
        size_t required = 0;
        if (m_Context->GetInteropRegistry().RecordGetStringArrayItem(&call, m_Record, field,
                                                                        static_cast<size_t>(index),
                                                                        nullptr, 0, &required) != BML_OK) {
            return {};
        }
        std::string result(required ? required : 1, '\0');
        if (m_Context->GetInteropRegistry().RecordGetStringArrayItem(&call, m_Record, field,
                                                                        static_cast<size_t>(index),
                                                                        result.data(), result.size(), &required) != BML_OK) {
            return {};
        }
        result.resize(result.empty() ? 0 : result.size() - 1);
        return result;
    }
    void ReleaseNative() {
        if (m_Record.Value == 0)
            return;
        BML_InteropCallContext call{};
        if (GetOwnerCall(m_Context, m_Owner, call) == BML_OK)
            (void)m_Context->GetInteropRegistry().ReleaseRecord(&call, m_Record);
        m_Record = {};
    }

    int m_RefCount = 1;
    ModContext *m_Context = nullptr;
    std::string m_Owner;
    BML_RecordRef m_Record{};
};

class EventStream {
public:
    EventStream(ModContext *context, std::string owner, BML_StreamRef stream)
        : m_Context(context), m_Owner(std::move(owner)), m_Stream(stream) {}
    ~EventStream() { CloseNative(); }

    void AddRef() { ++m_RefCount; }
    void Release() {
        if (--m_RefCount == 0)
            delete this;
    }
    bool IsOpen() const { return m_Stream.Value != 0; }
    int Close() {
        if (m_Stream.Value == 0)
            return BML_OK;
        BML_InteropCallContext call{};
        const int status = GetOwnerCall(m_Context, m_Owner, call) == BML_OK
                               ? m_Context->GetInteropRegistry().CloseStream(&call, m_Stream)
                               : BML_ERROR_INTEROP_HANDLE_STALE;
        m_Stream = {};
        return status;
    }
    int GetDroppedCount(int &out) const {
        out = 0;
        BML_InteropCallContext call{};
        const int status = GetOwnerCall(m_Context, m_Owner, call);
        return status == BML_OK
                   ? m_Context->GetInteropRegistry().DroppedStreamCount(&call, m_Stream, &out)
                   : status;
    }
    int Poll(ScriptEvent *&out) {
        out = nullptr;
        BML_InteropCallContext call{};
        const int contextStatus = GetOwnerCall(m_Context, m_Owner, call);
        if (contextStatus != BML_OK)
            return contextStatus;
        BML_RecordRef record{};
        const int status = m_Context->GetInteropRegistry().PollStream(&call, m_Stream, &record);
        if (status != BML_OK || record.Value == 0)
            return status;
        ScriptEvent *event = new (std::nothrow) ScriptEvent(m_Context, m_Owner, record);
        if (!event) {
            (void)m_Context->GetInteropRegistry().ReleaseRecord(&call, record);
            return BML_ERROR_OUT_OF_MEMORY;
        }
        out = event;
        return BML_OK;
    }

private:
    void CloseNative() { (void)Close(); }

    int m_RefCount = 1;
    ModContext *m_Context = nullptr;
    std::string m_Owner;
    BML_StreamRef m_Stream{};
};

int OpenEvents(EventStream *&out, int capacity) {
    out = nullptr;
    ActiveCall call;
    const int contextStatus = GetActiveCallForApi(call, EventsApi::Descriptor);
    if (contextStatus != BML_OK)
        return contextStatus;
    BML_StreamRef stream{};
    const int status = call.Context->GetInteropRegistry().OpenStream(&call.CallContext,
                                                                        EventsApi::ApiId,
                                                                        "all",
                                                                        capacity,
                                                                        &stream);
    if (status != BML_OK)
        return status;
    EventStream *result = new (std::nothrow) EventStream(call.Context,
                                                           BML::ScriptModRuntime::GetCurrentScriptMod()->GetID(),
                                                           stream);
    if (!result) {
        (void)call.Context->GetInteropRegistry().CloseStream(&call.CallContext, stream);
        return BML_ERROR_OUT_OF_MEMORY;
    }
    out = result;
    return BML_OK;
}

bool RegisterEvents(asIScriptEngine *engine, const char **errorMessage) {
    if (!Register(engine, engine->SetDefaultNamespace("BML::Events"), "namespace BML::Events", errorMessage) ||
        !Register(engine, engine->RegisterObjectType("Event", 0, asOBJ_REF), "Event", errorMessage) ||
        !Register(engine, engine->RegisterObjectType("Stream", 0, asOBJ_REF), "Stream", errorMessage)) {
        return false;
    }
    return Register(engine, engine->RegisterObjectBehaviour("Event", asBEHAVE_ADDREF, "void f()", asMETHOD(ScriptEvent, AddRef), asCALL_THISCALL), "Event::AddRef", errorMessage) &&
           Register(engine, engine->RegisterObjectBehaviour("Event", asBEHAVE_RELEASE, "void f()", asMETHOD(ScriptEvent, Release), asCALL_THISCALL), "Event::Release", errorMessage) &&
           Register(engine, engine->RegisterObjectBehaviour("Stream", asBEHAVE_ADDREF, "void f()", asMETHOD(EventStream, AddRef), asCALL_THISCALL), "Stream::AddRef", errorMessage) &&
           Register(engine, engine->RegisterObjectBehaviour("Stream", asBEHAVE_RELEASE, "void f()", asMETHOD(EventStream, Release), asCALL_THISCALL), "Stream::Release", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "bool get_IsValid() const", asMETHOD(ScriptEvent, IsValid), asCALL_THISCALL), "Event::IsValid", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "int get_Status() const", asMETHOD(ScriptEvent, Status), asCALL_THISCALL), "Event::Status", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "int get_Kind() const", asMETHOD(ScriptEvent, GetKind), asCALL_THISCALL), "Event::Kind", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "uint64 get_Sequence() const", asMETHOD(ScriptEvent, GetSequence), asCALL_THISCALL), "Event::Sequence", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "uint64 get_Timestamp() const", asMETHOD(ScriptEvent, GetTimestamp), asCALL_THISCALL), "Event::Timestamp", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "bool get_IsLoad() const", asMETHOD(ScriptEvent, IsLoad), asCALL_THISCALL), "Event::IsLoad", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "bool get_IsPhysics() const", asMETHOD(ScriptEvent, IsPhysics), asCALL_THISCALL), "Event::IsPhysics", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "bool get_IsCommand() const", asMETHOD(ScriptEvent, IsCommand), asCALL_THISCALL), "Event::IsCommand", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "bool get_IsConfig() const", asMETHOD(ScriptEvent, IsConfig), asCALL_THISCALL), "Event::IsConfig", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "bool get_IsCheat() const", asMETHOD(ScriptEvent, IsCheat), asCALL_THISCALL), "Event::IsCheat", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "string get_Filename() const", BML_AS_GENERIC_METHOD(&ScriptEvent::GetFilename), asCALL_GENERIC), "Event::Filename", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "bool get_IsMap() const", asMETHOD(ScriptEvent, GetIsMap), asCALL_THISCALL), "Event::IsMap", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "string get_MasterName() const", BML_AS_GENERIC_METHOD(&ScriptEvent::GetMasterName), asCALL_GENERIC), "Event::MasterName", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "int get_FilterClass() const", asMETHOD(ScriptEvent, GetFilterClass), asCALL_THISCALL), "Event::FilterClass", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "bool get_AddToScene() const", asMETHOD(ScriptEvent, GetAddToScene), asCALL_THISCALL), "Event::AddToScene", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "bool get_ReuseMeshes() const", asMETHOD(ScriptEvent, GetReuseMeshes), asCALL_THISCALL), "Event::ReuseMeshes", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "bool get_ReuseMaterials() const", asMETHOD(ScriptEvent, GetReuseMaterials), asCALL_THISCALL), "Event::ReuseMaterials", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "bool get_Dynamic() const", asMETHOD(ScriptEvent, GetDynamic), asCALL_THISCALL), "Event::Dynamic", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "CKObject@ BorrowMasterObject() const", BML_AS_GENERIC_METHOD(&ScriptEvent::BorrowMasterObject), asCALL_GENERIC), "Event::BorrowMasterObject", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "CKObject@ BorrowScript() const", BML_AS_GENERIC_METHOD(&ScriptEvent::BorrowScript), asCALL_GENERIC), "Event::BorrowScript", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "int get_ObjectCount() const", asMETHOD(ScriptEvent, GetObjectCount), asCALL_THISCALL), "Event::ObjectCount", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "CKObject@ BorrowObject(int index) const", BML_AS_GENERIC_METHOD(&ScriptEvent::BorrowObject), asCALL_GENERIC), "Event::BorrowObject", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "CKObject@ BorrowTarget() const", BML_AS_GENERIC_METHOD(&ScriptEvent::BorrowTarget), asCALL_GENERIC), "Event::BorrowTarget", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "bool get_Fixed() const", asMETHOD(ScriptEvent, GetFixed), asCALL_THISCALL), "Event::Fixed", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "float get_Friction() const", asMETHOD(ScriptEvent, GetFriction), asCALL_THISCALL), "Event::Friction", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "float get_Elasticity() const", asMETHOD(ScriptEvent, GetElasticity), asCALL_THISCALL), "Event::Elasticity", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "float get_Mass() const", asMETHOD(ScriptEvent, GetMass), asCALL_THISCALL), "Event::Mass", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "string get_CollisionGroup() const", BML_AS_GENERIC_METHOD(&ScriptEvent::GetCollisionGroup), asCALL_GENERIC), "Event::CollisionGroup", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "bool get_StartFrozen() const", asMETHOD(ScriptEvent, GetStartFrozen), asCALL_THISCALL), "Event::StartFrozen", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "bool get_EnableCollision() const", asMETHOD(ScriptEvent, GetEnableCollision), asCALL_THISCALL), "Event::EnableCollision", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "bool get_AutoCalculateMassCenter() const", asMETHOD(ScriptEvent, GetAutoCalculateMassCenter), asCALL_THISCALL), "Event::AutoCalculateMassCenter", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "float get_LinearDamp() const", asMETHOD(ScriptEvent, GetLinearDamp), asCALL_THISCALL), "Event::LinearDamp", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "float get_RotDamp() const", asMETHOD(ScriptEvent, GetRotDamp), asCALL_THISCALL), "Event::RotDamp", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "string get_CollisionSurface() const", BML_AS_GENERIC_METHOD(&ScriptEvent::GetCollisionSurface), asCALL_GENERIC), "Event::CollisionSurface", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "::BML::Vec3 get_MassCenter() const", asMETHOD(ScriptEvent, GetMassCenter), asCALL_THISCALL), "Event::MassCenter", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "int get_ConvexMeshCount() const", asMETHOD(ScriptEvent, GetConvexMeshCount), asCALL_THISCALL), "Event::ConvexMeshCount", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "CKObject@ BorrowConvexMesh(int index) const", BML_AS_GENERIC_METHOD(&ScriptEvent::BorrowConvexMesh), asCALL_GENERIC), "Event::BorrowConvexMesh", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "int get_BallCount() const", asMETHOD(ScriptEvent, GetBallCount), asCALL_THISCALL), "Event::BallCount", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "::BML::Vec3 GetBallCenter(int index) const", asMETHOD(ScriptEvent, GetBallCenter), asCALL_THISCALL), "Event::GetBallCenter", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "float GetBallRadius(int index) const", asMETHOD(ScriptEvent, GetBallRadius), asCALL_THISCALL), "Event::GetBallRadius", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "int get_ConcaveMeshCount() const", asMETHOD(ScriptEvent, GetConcaveMeshCount), asCALL_THISCALL), "Event::ConcaveMeshCount", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "CKObject@ BorrowConcaveMesh(int index) const", BML_AS_GENERIC_METHOD(&ScriptEvent::BorrowConcaveMesh), asCALL_GENERIC), "Event::BorrowConcaveMesh", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "string get_Command() const", BML_AS_GENERIC_METHOD(&ScriptEvent::GetCommand), asCALL_GENERIC), "Event::Command", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "int get_CommandArgumentCount() const", asMETHOD(ScriptEvent, GetCommandArgumentCount), asCALL_THISCALL), "Event::CommandArgumentCount", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "string GetCommandArgument(int index) const", BML_AS_GENERIC_METHOD(&ScriptEvent::GetCommandArgument), asCALL_GENERIC), "Event::GetCommandArgument", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "string get_ConfigCategory() const", BML_AS_GENERIC_METHOD(&ScriptEvent::GetConfigCategory), asCALL_GENERIC), "Event::ConfigCategory", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "string get_ConfigKey() const", BML_AS_GENERIC_METHOD(&ScriptEvent::GetConfigKey), asCALL_GENERIC), "Event::ConfigKey", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "int get_ConfigType() const", asMETHOD(ScriptEvent, GetConfigType), asCALL_THISCALL), "Event::ConfigType", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "string get_ConfigValue() const", BML_AS_GENERIC_METHOD(&ScriptEvent::GetConfigValue), asCALL_GENERIC), "Event::ConfigValue", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "bool get_CheatEnabled() const", asMETHOD(ScriptEvent, GetCheatEnabled), asCALL_THISCALL), "Event::CheatEnabled", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Stream", "bool get_IsOpen() const", asMETHOD(EventStream, IsOpen), asCALL_THISCALL), "Stream::IsOpen", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Stream", "int Close()", asMETHOD(EventStream, Close), asCALL_THISCALL), "Stream::Close", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Stream", "int GetDroppedCount(int &out count) const", BML_AS_GENERIC_METHOD(&EventStream::GetDroppedCount), asCALL_GENERIC), "Stream::GetDroppedCount", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Stream", "int Poll(Event@ &out event)", BML_AS_GENERIC_METHOD(&EventStream::Poll), asCALL_GENERIC), "Stream::Poll", errorMessage) &&
           Register(engine, engine->RegisterGlobalFunction("int Open(Stream@ &out stream, int capacity = 256)", BML_AS_GENERIC_FUNCTION(&OpenEvents), asCALL_GENERIC), "Events::Open", errorMessage) &&
           Register(engine, engine->SetDefaultNamespace(""), "namespace reset", errorMessage);
}

bool RegisterRuntime(asIScriptEngine *engine, const char **errorMessage) {
    if (!Register(engine, engine->SetDefaultNamespace("BML::Runtime"), "namespace BML::Runtime", errorMessage))
        return false;
    const ValueTypeRegistration values[] = {
        ValueType<RuntimeState>("State", asFUNCTION(ConstructRuntimeState), asFUNCTION(CopyConstructRuntimeState), asFUNCTION(DestructRuntimeState), asFUNCTION(AssignRuntimeState)),
        ValueType<ClockState>("Clock", asFUNCTION(ConstructClockState), asFUNCTION(CopyConstructClockState), asFUNCTION(DestructClockState), asFUNCTION(AssignClockState)),
        ValueType<ScoreState>("Score", asFUNCTION(ConstructScoreState), asFUNCTION(CopyConstructScoreState), asFUNCTION(DestructScoreState), asFUNCTION(AssignScoreState)),
    };
    for (const ValueTypeRegistration &value : values) {
        if (!RegisterValue(engine, value, errorMessage))
            return false;
    }
#define BML_AS_PROPERTY(Type, Declaration, Field) \
    if (!Register(engine, engine->RegisterObjectProperty(Type, Declaration, Field), Declaration, errorMessage)) return false
    BML_AS_PROPERTY("State", "bool InGame", asOFFSET(RuntimeState, InGame));
    BML_AS_PROPERTY("State", "bool InLevel", asOFFSET(RuntimeState, InLevel));
    BML_AS_PROPERTY("State", "bool Paused", asOFFSET(RuntimeState, Paused));
    BML_AS_PROPERTY("State", "bool Playing", asOFFSET(RuntimeState, Playing));
    BML_AS_PROPERTY("State", "bool CheatEnabled", asOFFSET(RuntimeState, CheatEnabled));
    BML_AS_PROPERTY("Clock", "float TimeMs", asOFFSET(ClockState, TimeMs));
    BML_AS_PROPERTY("Clock", "float AbsoluteMs", asOFFSET(ClockState, AbsoluteMs));
    BML_AS_PROPERTY("Clock", "float DeltaMs", asOFFSET(ClockState, DeltaMs));
    BML_AS_PROPERTY("Clock", "int Frame", asOFFSET(ClockState, Frame));
    BML_AS_PROPERTY("Score", "float SR", asOFFSET(ScoreState, SR));
    BML_AS_PROPERTY("Score", "int HS", asOFFSET(ScoreState, HS));
#undef BML_AS_PROPERTY
    return Register(engine, engine->RegisterGlobalFunction("int ReadState(State &out state)", BML_AS_GENERIC_FUNCTION(&ReadRuntime), asCALL_GENERIC), "Runtime::ReadState", errorMessage) &&
           Register(engine, engine->RegisterGlobalFunction("int ReadClock(Clock &out state)", BML_AS_GENERIC_FUNCTION(&ReadClock), asCALL_GENERIC), "Runtime::ReadClock", errorMessage) &&
           Register(engine, engine->RegisterGlobalFunction("int ReadScore(Score &out state)", BML_AS_GENERIC_FUNCTION(&ReadScore), asCALL_GENERIC), "Runtime::ReadScore", errorMessage) &&
           Register(engine, engine->SetDefaultNamespace(""), "namespace reset", errorMessage);
}

bool RegisterScene(asIScriptEngine *engine, const char **errorMessage) {
    if (!Register(engine, engine->SetDefaultNamespace("BML::Scene"), "namespace BML::Scene", errorMessage))
        return false;
    const ValueTypeRegistration values[] = {
        ValueType<ObjectInfo>("ObjectInfo", asFUNCTION(ConstructObjectInfo), asFUNCTION(CopyConstructObjectInfo), asFUNCTION(DestructObjectInfo), asFUNCTION(AssignObjectInfo)),
        ValueType<EntityTransform>("EntityTransform", asFUNCTION(ConstructEntityTransform), asFUNCTION(CopyConstructEntityTransform), asFUNCTION(DestructEntityTransform), asFUNCTION(AssignEntityTransform)),
    };
    for (const ValueTypeRegistration &value : values) {
        if (!RegisterValue(engine, value, errorMessage))
            return false;
    }
#define BML_AS_PROPERTY(Type, Declaration, Field) \
    if (!Register(engine, engine->RegisterObjectProperty(Type, Declaration, Field), Declaration, errorMessage)) return false
    BML_AS_PROPERTY("ObjectInfo", "int Id", asOFFSET(ObjectInfo, Id));
    BML_AS_PROPERTY("ObjectInfo", "int ClassId", asOFFSET(ObjectInfo, ClassId));
    BML_AS_PROPERTY("ObjectInfo", "bool Visible", asOFFSET(ObjectInfo, Visible));
    BML_AS_PROPERTY("ObjectInfo", "bool Dynamic", asOFFSET(ObjectInfo, Dynamic));
    BML_AS_PROPERTY("EntityTransform", "::BML::Vec3 Position", asOFFSET(EntityTransform, Position));
    BML_AS_PROPERTY("EntityTransform", "::BML::Vec3 Scale", asOFFSET(EntityTransform, Scale));
    BML_AS_PROPERTY("EntityTransform", "int ChildCount", asOFFSET(EntityTransform, ChildCount));
#undef BML_AS_PROPERTY
    return Register(engine, engine->RegisterObjectMethod("ObjectInfo", "string get_Name() const", BML_AS_STRING_FIELD_GETTER(ObjectInfo, Name), asCALL_GENERIC), "ObjectInfo::Name", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("ObjectInfo", "CKObject@ BorrowObject() const", BML_AS_GENERIC_OBJECT_FIRST_FUNCTION(&BorrowObject), asCALL_GENERIC), "ObjectInfo::BorrowObject", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("EntityTransform", "CKObject@ BorrowParent() const", BML_AS_GENERIC_OBJECT_FIRST_FUNCTION(&BorrowParent), asCALL_GENERIC), "EntityTransform::BorrowParent", errorMessage) &&
           Register(engine, engine->RegisterGlobalFunction("int Find(const string &in name, CKObject@ &out object)", BML_AS_GENERIC_FUNCTION(&FindByName), asCALL_GENERIC), "Scene::Find", errorMessage) &&
           Register(engine, engine->RegisterGlobalFunction("int Find(const string &in name, int classId, CKObject@ &out object)", BML_AS_GENERIC_FUNCTION(&FindByNameAndClass), asCALL_GENERIC), "Scene::FindByClass", errorMessage) &&
           Register(engine, engine->RegisterGlobalFunction("int ReadObject(CKObject@ object, ObjectInfo &out info)", BML_AS_GENERIC_FUNCTION(&ReadObjectInfo), asCALL_GENERIC), "Scene::ReadObject", errorMessage) &&
           Register(engine, engine->RegisterGlobalFunction("int ReadEntity(CKObject@ object, EntityTransform &out transform)", BML_AS_GENERIC_FUNCTION(&ReadEntity), asCALL_GENERIC), "Scene::ReadEntity", errorMessage) &&
           Register(engine, engine->SetDefaultNamespace(""), "namespace reset", errorMessage);
}

bool RegisterGameplay(asIScriptEngine *engine, const char **errorMessage) {
    if (!Register(engine, engine->SetDefaultNamespace("BML::Gameplay"), "namespace BML::Gameplay", errorMessage))
        return false;
    const ValueTypeRegistration values[] = {
        ValueType<LevelState>("LevelState", asFUNCTION(ConstructLevelState), asFUNCTION(CopyConstructLevelState), asFUNCTION(DestructLevelState), asFUNCTION(AssignLevelState)),
        ValueType<EnergyState>("EnergyState", asFUNCTION(ConstructEnergyState), asFUNCTION(CopyConstructEnergyState), asFUNCTION(DestructEnergyState), asFUNCTION(AssignEnergyState)),
        ValueType<CatalogEntry>("CatalogEntry", asFUNCTION(ConstructCatalogEntry), asFUNCTION(CopyConstructCatalogEntry), asFUNCTION(DestructCatalogEntry), asFUNCTION(AssignCatalogEntry)),
        ValueType<Checkpoint>("Checkpoint", asFUNCTION(ConstructCheckpoint), asFUNCTION(CopyConstructCheckpoint), asFUNCTION(DestructCheckpoint), asFUNCTION(AssignCheckpoint)),
        ValueType<Resetpoint>("Resetpoint", asFUNCTION(ConstructResetpoint), asFUNCTION(CopyConstructResetpoint), asFUNCTION(DestructResetpoint), asFUNCTION(AssignResetpoint)),
    };
    for (const ValueTypeRegistration &value : values) {
        if (!RegisterValue(engine, value, errorMessage))
            return false;
    }
    if (!Register(engine, engine->RegisterObjectType("CatalogCursor", 0, asOBJ_REF), "CatalogCursor", errorMessage) ||
        !Register(engine, engine->RegisterObjectType("CheckpointCursor", 0, asOBJ_REF), "CheckpointCursor", errorMessage) ||
        !Register(engine, engine->RegisterObjectType("ResetpointCursor", 0, asOBJ_REF), "ResetpointCursor", errorMessage)) {
        return false;
    }
#define BML_AS_PROPERTY(Type, Declaration, Field) \
    if (!Register(engine, engine->RegisterObjectProperty(Type, Declaration, Field), Declaration, errorMessage)) return false
    BML_AS_PROPERTY("LevelState", "int Id", asOFFSET(LevelState, Id));
    BML_AS_PROPERTY("LevelState", "::BML::Mat4 ResetMatrix", asOFFSET(LevelState, ResetMatrix));
    BML_AS_PROPERTY("LevelState", "int Points", asOFFSET(LevelState, Points));
    BML_AS_PROPERTY("EnergyState", "int Points", asOFFSET(EnergyState, Points));
    BML_AS_PROPERTY("EnergyState", "int Lives", asOFFSET(EnergyState, Lives));
    BML_AS_PROPERTY("EnergyState", "int StartPoints", asOFFSET(EnergyState, StartPoints));
    BML_AS_PROPERTY("EnergyState", "int StartLives", asOFFSET(EnergyState, StartLives));
    BML_AS_PROPERTY("EnergyState", "float TimeFactor", asOFFSET(EnergyState, TimeFactor));
    BML_AS_PROPERTY("EnergyState", "int LifeBonus", asOFFSET(EnergyState, LifeBonus));
    BML_AS_PROPERTY("CatalogEntry", "int Bonus", asOFFSET(CatalogEntry, Bonus));
    BML_AS_PROPERTY("CatalogEntry", "int Music", asOFFSET(CatalogEntry, Music));
    BML_AS_PROPERTY("Checkpoint", "::BML::Mat4 Matrix", asOFFSET(Checkpoint, Matrix));
#undef BML_AS_PROPERTY
    return Register(engine, engine->RegisterObjectMethod("LevelState", "CKObject@ BorrowActiveBall() const", BML_AS_GENERIC_OBJECT_FIRST_FUNCTION(&BorrowActiveBall), asCALL_GENERIC), "LevelState::BorrowActiveBall", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("CatalogEntry", "string get_File() const", BML_AS_STRING_FIELD_GETTER(CatalogEntry, File), asCALL_GENERIC), "CatalogEntry::File", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("CatalogEntry", "string get_StartBall() const", BML_AS_STRING_FIELD_GETTER(CatalogEntry, StartBall), asCALL_GENERIC), "CatalogEntry::StartBall", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("CatalogEntry", "string get_Sky() const", BML_AS_STRING_FIELD_GETTER(CatalogEntry, Sky), asCALL_GENERIC), "CatalogEntry::Sky", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Checkpoint", "CKObject@ BorrowObject() const", BML_AS_GENERIC_OBJECT_FIRST_FUNCTION(&BorrowCheckpointObject), asCALL_GENERIC), "Checkpoint::BorrowObject", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Resetpoint", "CKObject@ BorrowObject() const", BML_AS_GENERIC_OBJECT_FIRST_FUNCTION(&BorrowResetpointObject), asCALL_GENERIC), "Resetpoint::BorrowObject", errorMessage) &&
           Register(engine, engine->RegisterObjectBehaviour("CatalogCursor", asBEHAVE_ADDREF, "void f()", asMETHOD(CollectionCursor, AddRef), asCALL_THISCALL), "CatalogCursor::AddRef", errorMessage) &&
           Register(engine, engine->RegisterObjectBehaviour("CatalogCursor", asBEHAVE_RELEASE, "void f()", asMETHOD(CollectionCursor, Release), asCALL_THISCALL), "CatalogCursor::Release", errorMessage) &&
           Register(engine, engine->RegisterObjectBehaviour("CheckpointCursor", asBEHAVE_ADDREF, "void f()", asMETHOD(CollectionCursor, AddRef), asCALL_THISCALL), "CheckpointCursor::AddRef", errorMessage) &&
           Register(engine, engine->RegisterObjectBehaviour("CheckpointCursor", asBEHAVE_RELEASE, "void f()", asMETHOD(CollectionCursor, Release), asCALL_THISCALL), "CheckpointCursor::Release", errorMessage) &&
           Register(engine, engine->RegisterObjectBehaviour("ResetpointCursor", asBEHAVE_ADDREF, "void f()", asMETHOD(CollectionCursor, AddRef), asCALL_THISCALL), "ResetpointCursor::AddRef", errorMessage) &&
           Register(engine, engine->RegisterObjectBehaviour("ResetpointCursor", asBEHAVE_RELEASE, "void f()", asMETHOD(CollectionCursor, Release), asCALL_THISCALL), "ResetpointCursor::Release", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("CatalogCursor", "bool get_IsOpen() const", asMETHOD(CollectionCursor, IsOpen), asCALL_THISCALL), "CatalogCursor::IsOpen", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("CatalogCursor", "int Close()", asMETHOD(CollectionCursor, Close), asCALL_THISCALL), "CatalogCursor::Close", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("CatalogCursor", "int Next(CatalogEntry &out entry, bool &out hasValue, bool &out complete)", BML_AS_GENERIC_METHOD(&CatalogCursor::Next), asCALL_GENERIC), "CatalogCursor::Next", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("CheckpointCursor", "bool get_IsOpen() const", asMETHOD(CollectionCursor, IsOpen), asCALL_THISCALL), "CheckpointCursor::IsOpen", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("CheckpointCursor", "int Close()", asMETHOD(CollectionCursor, Close), asCALL_THISCALL), "CheckpointCursor::Close", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("CheckpointCursor", "int Next(Checkpoint &out entry, bool &out hasValue, bool &out complete)", BML_AS_GENERIC_METHOD(&CheckpointCursor::Next), asCALL_GENERIC), "CheckpointCursor::Next", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("ResetpointCursor", "bool get_IsOpen() const", asMETHOD(CollectionCursor, IsOpen), asCALL_THISCALL), "ResetpointCursor::IsOpen", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("ResetpointCursor", "int Close()", asMETHOD(CollectionCursor, Close), asCALL_THISCALL), "ResetpointCursor::Close", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("ResetpointCursor", "int Next(Resetpoint &out entry, bool &out hasValue, bool &out complete)", BML_AS_GENERIC_METHOD(&ResetpointCursor::Next), asCALL_GENERIC), "ResetpointCursor::Next", errorMessage) &&
           Register(engine, engine->RegisterGlobalFunction("int ReadLevel(LevelState &out state)", BML_AS_GENERIC_FUNCTION(&ReadLevel), asCALL_GENERIC), "Gameplay::ReadLevel", errorMessage) &&
           Register(engine, engine->RegisterGlobalFunction("int ReadEnergy(EnergyState &out state)", BML_AS_GENERIC_FUNCTION(&ReadEnergy), asCALL_GENERIC), "Gameplay::ReadEnergy", errorMessage) &&
           Register(engine, engine->RegisterGlobalFunction("int OpenCatalog(CatalogCursor@ &out cursor)", BML_AS_GENERIC_FUNCTION(&OpenCatalog), asCALL_GENERIC), "Gameplay::OpenCatalog", errorMessage) &&
           Register(engine, engine->RegisterGlobalFunction("int OpenCheckpoints(CheckpointCursor@ &out cursor)", BML_AS_GENERIC_FUNCTION(&OpenCheckpoints), asCALL_GENERIC), "Gameplay::OpenCheckpoints", errorMessage) &&
           Register(engine, engine->RegisterGlobalFunction("int OpenResetpoints(ResetpointCursor@ &out cursor)", BML_AS_GENERIC_FUNCTION(&OpenResetpoints), asCALL_GENERIC), "Gameplay::OpenResetpoints", errorMessage) &&
           Register(engine, engine->SetDefaultNamespace(""), "namespace reset", errorMessage);
}

} // namespace

int RegisterScriptInteropFacade(asIScriptEngine *engine, const char **errorMessage) {
    if (!engine) {
        g_InteropRegistrationError = "Interop script facade received a null engine.";
        if (errorMessage)
            *errorMessage = g_InteropRegistrationError.c_str();
        return asERROR;
    }
    if (!RegisterRuntime(engine, errorMessage) ||
        !RegisterScene(engine, errorMessage) ||
        !RegisterGameplay(engine, errorMessage) ||
        !RegisterEvents(engine, errorMessage)) {
        engine->SetDefaultNamespace("");
        return asERROR;
    }
    return asSUCCESS;
}
