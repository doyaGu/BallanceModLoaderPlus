#include "ScriptImcFacade.h"

#include <cstddef>
#include <deque>
#include <limits>
#include <new>
#include <string>
#include <utility>
#include <vector>

#include <angelscript.h>

#include "BML/Generated/bml_events_imc.hpp"
#include "BML/Generated/bml_gameplay_imc.hpp"
#include "BML/Generated/bml_scene_imc.hpp"
#include "BML/EventKinds.h"

#include "BuiltinImcApis.h"
#include "ModContext.h"
#include "ScriptMod.h"
#include "ScriptImcClients.h"
#include "ScriptModRuntime.h"
#include "ScriptFunctionSupport.h"
#include "ScriptStringInterop.h"

namespace {

namespace SceneImc = BML::Imc::Generated::Bml::Scene;
namespace GameplayImc = BML::Imc::Generated::Bml::Gameplay;
namespace EventsImc = BML::Imc::Generated::Bml::Events;

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

std::string g_ImcFacadeRegistrationError;

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

#define BML_AS_DEFINE_IMC_VALUE(Type, Suffix)                                     \
    static void Construct##Suffix(Type *self) { ConstructValue(self); }            \
    static void CopyConstruct##Suffix(const Type &other, Type *self) {              \
        CopyConstructValue(other, self);                                            \
    }                                                                                \
    static void Destruct##Suffix(Type *self) { DestructValue(self); }               \
    static Type &Assign##Suffix(const Type &other, Type *self) {                    \
        return AssignValue(other, self);                                            \
    }

BML_AS_DEFINE_IMC_VALUE(RuntimeState, RuntimeState)
BML_AS_DEFINE_IMC_VALUE(ClockState, ClockState)
BML_AS_DEFINE_IMC_VALUE(ScoreState, ScoreState)
BML_AS_DEFINE_IMC_VALUE(ObjectInfo, ObjectInfo)
BML_AS_DEFINE_IMC_VALUE(EntityTransform, EntityTransform)
BML_AS_DEFINE_IMC_VALUE(LevelState, LevelState)
BML_AS_DEFINE_IMC_VALUE(EnergyState, EnergyState)
BML_AS_DEFINE_IMC_VALUE(CatalogEntry, CatalogEntry)
BML_AS_DEFINE_IMC_VALUE(Checkpoint, Checkpoint)
BML_AS_DEFINE_IMC_VALUE(Resetpoint, Resetpoint)

#undef BML_AS_DEFINE_IMC_VALUE

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
    g_ImcFacadeRegistrationError = "Failed to register IMC facade declaration: ";
    g_ImcFacadeRegistrationError += declaration;
    g_ImcFacadeRegistrationError += " returned ";
    g_ImcFacadeRegistrationError += std::to_string(status);
    if (errorMessage)
        *errorMessage = g_ImcFacadeRegistrationError.c_str();
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

int GetActiveFacadeContext(ModContext *&outContext) {
    outContext = nullptr;
    if (BML::RejectScriptRestrictedHostCall("BML capability facade"))
        return BML_ERROR_FROZEN;
    BML::ScriptMod *mod = BML::ScriptModRuntime::GetCurrentScriptMod();
    if (!mod || !mod->GetModContext())
        return BML_ERROR_IMC_UNSUPPORTED;
    outContext = mod->GetModContext();
    return BML_OK;
}

int GetActiveImcClients(BML::ScriptImcClients *&outClients,
                        ModContext *&outContext) {
    outClients = nullptr;
    int status = GetActiveFacadeContext(outContext);
    if (status != BML_OK)
        return status;
    BML::ScriptMod *mod = BML::ScriptModRuntime::GetCurrentScriptMod();
    BML::ScriptModRuntime *runtime = BML::ScriptModRuntime::GetCurrentScriptModRuntime();
    if (!runtime)
        runtime = const_cast<BML::ScriptModRuntime *>(&mod->GetRuntimeForImc());
    outClients = runtime->GetImcClients();
    if (!outClients)
        return BML_ERROR_OUT_OF_MEMORY;
    return BML_OK;
}

int ReadRuntime(RuntimeState &out) {
    ModContext *context = nullptr;
    const int status = GetActiveFacadeContext(context);
    if (status == BML_OK) {
        const BML::RuntimeStateSnapshot state = context->ReadRuntimeState();
        out = {state.InGame, state.InLevel, state.Paused, state.Playing,
               state.CheatEnabled};
    }
    return status;
}

int ReadClock(ClockState &out) {
    ModContext *context = nullptr;
    int status = GetActiveFacadeContext(context);
    CKTimeManager *time = status == BML_OK ? context->GetTimeManager() : nullptr;
    if (status == BML_OK && !time)
        status = BML_ERROR_IMC_UNSUPPORTED;
    if (status == BML_OK) {
        const CKDWORD tick = time->GetMainTickCount();
        const CKDWORD maxFrame = static_cast<CKDWORD>((std::numeric_limits<int>::max)());
        out = {time->GetTime(), time->GetAbsoluteTime(), time->GetLastDeltaTime(),
               tick > maxFrame ? (std::numeric_limits<int>::max)()
                               : static_cast<int>(tick)};
    }
    return status;
}

int ReadScore(ScoreState &out) {
    ModContext *context = nullptr;
    const int status = GetActiveFacadeContext(context);
    if (status == BML_OK)
        out = {context->GetSRScore(), context->GetHSScore()};
    return status;
}

int ReadObjectInfo(CKObject *object, ObjectInfo &out) {
    BML::ScriptImcClients *clients = nullptr;
    ModContext *context = nullptr;
    int status = GetActiveImcClients(clients, context);
    SceneImc::Client *client = nullptr;
    if (status == BML_OK) status = clients->Scene(client);
    const BML_ObjectRef reference = context ? MakeBuiltinObjectRef(*context, object) : BML_ObjectRef{};
    if (status == BML_OK && object && reference.Domain == 0)
        status = BML_ERROR_IMC_OBJECT_INVALID;
    SceneImc::ObjectRequestValue input{};
    input.Object = reference;
    SceneImc::ObjectInfoValue value{};
    if (status == BML_OK) status = client->CallObject(input, value);
    if (status == BML_OK) {
        out.Object = reference;
        out.Id = value.Id;
        out.Name = std::move(value.Name);
        out.ClassId = value.ClassId;
        out.Visible = value.Visible;
        out.Dynamic = value.Dynamic;
    }
    return status;
}

int ReadEntity(CKObject *object, EntityTransform &out) {
    BML::ScriptImcClients *clients = nullptr;
    ModContext *context = nullptr;
    int status = GetActiveImcClients(clients, context);
    SceneImc::Client *client = nullptr;
    if (status == BML_OK) status = clients->Scene(client);
    const BML_ObjectRef reference = context ? MakeBuiltinObjectRef(*context, object) : BML_ObjectRef{};
    if (status == BML_OK && object && reference.Domain == 0)
        status = BML_ERROR_IMC_OBJECT_INVALID;
    SceneImc::ObjectRequestValue input{};
    input.Object = reference;
    SceneImc::EntityTransformValue value{};
    if (status == BML_OK) status = client->CallEntity(input, value);
    if (status == BML_OK)
        out = {value.Position, value.Scale, value.Parent, value.ChildCount};
    return status;
}

int FindObject(const std::string &name, int classId, bool filterClass, CKObject *&out) {
    out = nullptr;
    BML::ScriptImcClients *clients = nullptr;
    ModContext *context = nullptr;
    int status = GetActiveImcClients(clients, context);
    SceneImc::Client *client = nullptr;
    if (status == BML_OK) status = clients->Scene(client);
    SceneImc::FindResultValue result{};
    if (status == BML_OK && filterClass) {
        SceneImc::FindNameClassRequestValue request{name, classId};
        status = client->CallFindNameClass(request, result);
    } else if (status == BML_OK) {
        SceneImc::FindNameRequestValue request{name};
        status = client->CallFindName(request, result);
    }
    if (status == BML_OK && result.Object.Domain != 0) {
        out = ResolveBuiltinObjectRef(*context, result.Object);
        if (!out)
            status = BML_ERROR_IMC_OBJECT_INVALID;
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
    BML::ScriptImcClients *clients = nullptr;
    ModContext *context = nullptr;
    int status = GetActiveImcClients(clients, context);
    GameplayImc::Client *client = nullptr;
    if (status == BML_OK) status = clients->Gameplay(client);
    GameplayImc::LevelStateValue value{};
    if (status == BML_OK) status = client->CallLevel(value);
    if (status == BML_OK)
        out = {value.Id, value.ActiveBall, value.ResetMatrix, value.Points};
    return status;
}

int ReadEnergy(EnergyState &out) {
    BML::ScriptImcClients *clients = nullptr;
    ModContext *context = nullptr;
    int status = GetActiveImcClients(clients, context);
    GameplayImc::Client *client = nullptr;
    if (status == BML_OK) status = clients->Gameplay(client);
    GameplayImc::EnergyStateValue value{};
    if (status == BML_OK) status = client->CallEnergy(value);
    if (status == BML_OK) {
        out = {value.Points, value.Lives, value.StartPoints, value.StartLives,
               value.TimeFactor, value.LifeBonus};
    }
    return status;
}

ModContext *GetActiveScriptContext() {
    if (BML::RejectScriptRestrictedHostCall("BML IMC facade"))
        return nullptr;
    BML::ScriptMod *mod = BML::ScriptModRuntime::GetCurrentScriptMod();
    return mod ? mod->GetModContext() : nullptr;
}

CKObject *ResolveScriptObject(const BML_ObjectRef &reference) {
    ModContext *context = GetActiveScriptContext();
    return context && reference.Domain != 0
               ? ResolveBuiltinObjectRef(*context, reference)
               : nullptr;
}

CKObject *BorrowObject(const ObjectInfo *value) {
    return value ? ResolveScriptObject(value->Object) : nullptr;
}

CKObject *BorrowParent(const EntityTransform *value) {
    return value ? ResolveScriptObject(value->Parent) : nullptr;
}

CKObject *BorrowActiveBall(const LevelState *value) {
    return value ? ResolveScriptObject(value->ActiveBall) : nullptr;
}

CKObject *BorrowCheckpointObject(const Checkpoint *value) {
    return value ? ResolveScriptObject(value->Object) : nullptr;
}

CKObject *BorrowResetpointObject(const Resetpoint *value) {
    return value ? ResolveScriptObject(value->Object) : nullptr;
}

class CollectionCursor {
public:
    void AddRef() { ++m_RefCount; }
    void Release() {
        if (--m_RefCount == 0)
            delete this;
    }
    bool IsOpen() const { return m_Open; }
    int Close() {
        m_Open = false;
        return BML_OK;
    }

protected:
    virtual ~CollectionCursor() = default;
    int BeginNext(std::size_t count, bool &hasValue, bool &complete) const {
        hasValue = false;
        complete = false;
        if (!m_Open)
            return BML_ERROR_INVALID_HANDLE;
        hasValue = m_Index < count;
        complete = !hasValue || m_Index + 1 >= count;
        return BML_OK;
    }
    void Advance() { ++m_Index; }
    std::size_t Index() const { return m_Index; }

private:
    int m_RefCount = 1;
    bool m_Open = true;
    std::size_t m_Index = 0;
};

class CatalogCursor final : public CollectionCursor {
public:
    explicit CatalogCursor(std::vector<CatalogEntry> values)
        : m_Values(std::move(values)) {}
    int Next(CatalogEntry &out, bool &hasValue, bool &complete) {
        const int status = BeginNext(m_Values.size(), hasValue, complete);
        if (status != BML_OK || !hasValue)
            return status;
        out = std::move(m_Values[Index()]);
        Advance();
        return BML_OK;
    }
private:
    std::vector<CatalogEntry> m_Values;
};

class CheckpointCursor final : public CollectionCursor {
public:
    explicit CheckpointCursor(std::vector<Checkpoint> values)
        : m_Values(std::move(values)) {}
    int Next(Checkpoint &out, bool &hasValue, bool &complete) {
        const int status = BeginNext(m_Values.size(), hasValue, complete);
        if (status != BML_OK || !hasValue)
            return status;
        out = m_Values[Index()];
        Advance();
        return BML_OK;
    }
private:
    std::vector<Checkpoint> m_Values;
};

class ResetpointCursor final : public CollectionCursor {
public:
    explicit ResetpointCursor(std::vector<Resetpoint> values)
        : m_Values(std::move(values)) {}
    int Next(Resetpoint &out, bool &hasValue, bool &complete) {
        const int status = BeginNext(m_Values.size(), hasValue, complete);
        if (status != BML_OK || !hasValue)
            return status;
        out = m_Values[Index()];
        Advance();
        return BML_OK;
    }
private:
    std::vector<Resetpoint> m_Values;
};

template <typename Cursor, typename Value, typename Reader>
int OpenCollection(Cursor *&out, Reader reader) {
    out = nullptr;
    BML::ScriptImcClients *clients = nullptr;
    ModContext *context = nullptr;
    int status = GetActiveImcClients(clients, context);
    GameplayImc::Client *client = nullptr;
    if (status == BML_OK) status = clients->Gameplay(client);
    std::vector<Value> values;
    if (status == BML_OK) status = reader(*client, values);
    if (status != BML_OK)
        return status;
    Cursor *result = new (std::nothrow) Cursor(std::move(values));
    if (!result)
        return BML_ERROR_OUT_OF_MEMORY;
    out = result;
    return BML_OK;
}

int OpenCatalog(CatalogCursor *&out) {
    return OpenCollection<CatalogCursor, CatalogEntry>(
        out, [](GameplayImc::Client &client, auto &values) {
            GameplayImc::CatalogResponseValue response{};
            int status = client.CallCatalog(response);
            const std::size_t count = response.Files.size();
            if (status == BML_OK && (response.StartBalls.size() != count ||
                                     response.Skies.size() != count ||
                                     response.Bonuses.size() != count ||
                                     response.Music.size() != count))
                status = BML_ERROR_MALFORMED_MESSAGE;
            if (status != BML_OK)
                return status;
            try {
                values.reserve(count);
                for (std::size_t i = 0; i < count; ++i) {
                    values.push_back({std::move(response.Files[i]),
                                      std::move(response.StartBalls[i]),
                                      std::move(response.Skies[i]),
                                      response.Bonuses[i], response.Music[i]});
                }
            } catch (const std::bad_alloc &) {
                return BML_ERROR_OUT_OF_MEMORY;
            }
            return BML_OK;
        });
}

int OpenCheckpoints(CheckpointCursor *&out) {
    return OpenCollection<CheckpointCursor, Checkpoint>(
        out, [](GameplayImc::Client &client, auto &values) {
            GameplayImc::CheckpointsResponseValue response{};
            int status = client.CallCheckpoints(response);
            if (status == BML_OK && response.Matrices.size() != response.Objects.size())
                status = BML_ERROR_MALFORMED_MESSAGE;
            if (status != BML_OK)
                return status;
            try {
                values.reserve(response.Matrices.size());
                for (std::size_t i = 0; i < response.Matrices.size(); ++i)
                    values.push_back({response.Matrices[i], response.Objects[i]});
            } catch (const std::bad_alloc &) {
                return BML_ERROR_OUT_OF_MEMORY;
            }
            return BML_OK;
        });
}

int OpenResetpoints(ResetpointCursor *&out) {
    return OpenCollection<ResetpointCursor, Resetpoint>(
        out, [](GameplayImc::Client &client, auto &values) {
            GameplayImc::ResetpointsResponseValue response{};
            const int status = client.CallResetpoints(response);
            if (status != BML_OK)
                return status;
            try {
                values.reserve(response.Objects.size());
                for (const BML_ObjectRef &object : response.Objects)
                    values.push_back({object});
            } catch (const std::bad_alloc &) {
                return BML_ERROR_OUT_OF_MEMORY;
            }
            return BML_OK;
        });
}
/* Script events own decoded IMC values, never CK pointers. Object references are
 * resolved only by Borrow* calls, so destroyed Virtools objects remain safe. */
class ScriptEvent {
public:
    ScriptEvent(ModContext *context, EventsImc::EventValue value,
                std::uint64_t sequence, std::uint64_t timestamp)
        : m_Context(context), m_Value(std::move(value)),
          m_Sequence(sequence), m_Timestamp(timestamp) {}

    void AddRef() { ++m_RefCount; }
    void Release() {
        if (--m_RefCount == 0)
            delete this;
    }

    bool IsValid() const { return m_Context != nullptr; }
    int Status() const { return IsValid() ? BML_OK : BML_ERROR_IMC_HANDLE_STALE; }
    int GetKind() const { return m_Value.Kind; }
    std::uint64_t GetSequence() const { return m_Sequence; }
    std::uint64_t GetTimestamp() const { return m_Timestamp; }

    bool IsLoad() const {
        return m_Value.Kind == BML_EVENT_LOAD_OBJECT || m_Value.Kind == BML_EVENT_LOAD_SCRIPT;
    }
    bool IsPhysics() const {
        return m_Value.Kind == BML_EVENT_PHYSICALIZE || m_Value.Kind == BML_EVENT_UNPHYSICALIZE;
    }
    bool IsCommand() const {
        return m_Value.Kind == BML_EVENT_COMMAND_PRE || m_Value.Kind == BML_EVENT_COMMAND_POST;
    }
    bool IsConfig() const { return m_Value.Kind == BML_EVENT_CONFIG_MODIFIED; }
    bool IsCheat() const { return m_Value.Kind == BML_EVENT_CHEAT_CHANGED; }

    std::string GetFilename() const { return m_Value.Filename; }
    bool GetIsMap() const { return m_Value.IsMap; }
    std::string GetMasterName() const { return m_Value.MasterName; }
    int GetFilterClass() const { return m_Value.FilterClass; }
    bool GetAddToScene() const { return m_Value.AddToScene; }
    bool GetReuseMeshes() const { return m_Value.ReuseMeshes; }
    bool GetReuseMaterials() const { return m_Value.ReuseMaterials; }
    bool GetDynamic() const { return m_Value.Dynamic; }
    CKObject *BorrowMasterObject() const { return Resolve(m_Value.MasterObject); }
    CKObject *BorrowScript() const { return Resolve(m_Value.Script); }
    int GetObjectCount() const { return Count(m_Value.ObjectIds.size()); }
    CKObject *BorrowObject(int index) const { return ResolveAt(m_Value.ObjectIds, index); }

    CKObject *BorrowTarget() const { return Resolve(m_Value.Target); }
    bool GetFixed() const { return m_Value.Fixed; }
    float GetFriction() const { return m_Value.Friction; }
    float GetElasticity() const { return m_Value.Elasticity; }
    float GetMass() const { return m_Value.Mass; }
    std::string GetCollisionGroup() const { return m_Value.CollisionGroup; }
    bool GetStartFrozen() const { return m_Value.StartFrozen; }
    bool GetEnableCollision() const { return m_Value.EnableCollision; }
    bool GetAutoCalculateMassCenter() const { return m_Value.AutoCalculateMassCenter; }
    float GetLinearDamp() const { return m_Value.LinearDamp; }
    float GetRotDamp() const { return m_Value.RotDamp; }
    std::string GetCollisionSurface() const { return m_Value.CollisionSurface; }
    BML_Vec3 GetMassCenter() const { return m_Value.MassCenter; }
    int GetConvexMeshCount() const { return Count(m_Value.ConvexMeshes.size()); }
    CKObject *BorrowConvexMesh(int index) const { return ResolveAt(m_Value.ConvexMeshes, index); }
    int GetBallCount() const { return Count(m_Value.BallCenters.size()); }
    BML_Vec3 GetBallCenter(int index) const { return At(m_Value.BallCenters, index); }
    float GetBallRadius(int index) const { return At(m_Value.BallRadii, index); }
    int GetConcaveMeshCount() const { return Count(m_Value.ConcaveMeshes.size()); }
    CKObject *BorrowConcaveMesh(int index) const { return ResolveAt(m_Value.ConcaveMeshes, index); }

    std::string GetCommand() const { return m_Value.Command; }
    int GetCommandArgumentCount() const { return Count(m_Value.CommandArgs.size()); }
    std::string GetCommandArgument(int index) const { return At(m_Value.CommandArgs, index); }
    std::string GetConfigCategory() const { return m_Value.ConfigCategory; }
    std::string GetConfigKey() const { return m_Value.ConfigKey; }
    int GetConfigType() const { return m_Value.ConfigType; }
    std::string GetConfigValue() const { return m_Value.ConfigValue; }
    bool GetCheatEnabled() const { return m_Value.CheatEnabled; }

private:
    static int Count(std::size_t count) {
        const auto limit = static_cast<std::size_t>((std::numeric_limits<int>::max)());
        return count > limit ? (std::numeric_limits<int>::max)()
                             : static_cast<int>(count);
    }

    template <typename T>
    static T At(const std::vector<T> &values, int index) {
        return index >= 0 && static_cast<std::size_t>(index) < values.size()
                   ? values[static_cast<std::size_t>(index)]
                   : T{};
    }

    CKObject *Resolve(const BML_ObjectRef &reference) const {
        return m_Context && reference.Domain != 0
                   ? ResolveBuiltinObjectRef(*m_Context, reference)
                   : nullptr;
    }

    CKObject *ResolveAt(const std::vector<BML_ObjectRef> &values, int index) const {
        return Resolve(At(values, index));
    }

    int m_RefCount = 1;
    ModContext *m_Context = nullptr;
    EventsImc::EventValue m_Value;
    std::uint64_t m_Sequence = 0;
    std::uint64_t m_Timestamp = 0;
};

class EventStream {
public:
    EventStream(ModContext *context, std::size_t capacity)
        : m_Context(context), m_Capacity(capacity) {}
    ~EventStream() { (void)Close(); }

    void AddRef() { ++m_RefCount; }
    void Release() {
        if (--m_RefCount == 0)
            delete this;
    }

    int Open(EventsImc::Client &client) {
        return client.SubscribeAll(m_Subscription, &OnEvent, this,
                                   static_cast<std::uint32_t>(m_Capacity),
                                   BML_IMC_BACKPRESSURE_DROP_OLDEST,
                                   BML_IMC_EXECUTION_GAME_THREAD);
    }

    bool IsOpen() const { return m_Subscription.IsOpen(); }

    int Close() {
        const int status = m_Subscription.Close();
        if (!m_Subscription.IsOpen()) {
            m_Queue.clear();
            m_LocalDrops = 0;
        }
        return status;
    }

    int GetDroppedCount(int &out) const {
        out = 0;
        if (!m_Subscription.IsOpen())
            return BML_ERROR_INVALID_HANDLE;
        std::uint64_t runtimeDrops = 0;
        const int status = m_Subscription.DroppedCount(runtimeDrops);
        if (status != BML_OK)
            return status;
        const std::uint64_t total = runtimeDrops + m_LocalDrops;
        out = total > static_cast<std::uint64_t>((std::numeric_limits<int>::max)())
                  ? (std::numeric_limits<int>::max)()
                  : static_cast<int>(total);
        return BML_OK;
    }

    int Poll(ScriptEvent *&out) {
        out = nullptr;
        if (!m_Subscription.IsOpen())
            return BML_ERROR_INVALID_HANDLE;
        if (m_Queue.empty())
            return BML_ERROR_NOT_FOUND;
        QueuedEvent queued = std::move(m_Queue.front());
        m_Queue.pop_front();
        ScriptEvent *event = new (std::nothrow) ScriptEvent(
            m_Context, std::move(queued.Value), queued.Sequence, queued.Timestamp);
        if (!event)
            return BML_ERROR_OUT_OF_MEMORY;
        out = event;
        return BML_OK;
    }

private:
    struct QueuedEvent {
        EventsImc::EventValue Value;
        std::uint64_t Sequence = 0;
        std::uint64_t Timestamp = 0;
    };

    static void OnEvent(int status, EventsImc::EventValue *value,
                        const BML_ImcMessage *message, void *userdata) noexcept {
        auto *self = static_cast<EventStream *>(userdata);
        if (!self)
            return;
        if (status != BML_OK || !value || !message) {
            ++self->m_LocalDrops;
            return;
        }
        try {
            if (self->m_Queue.size() >= self->m_Capacity) {
                self->m_Queue.pop_front();
                ++self->m_LocalDrops;
            }
            self->m_Queue.push_back(
                {std::move(*value), message->MessageId, message->TimestampNs});
        } catch (...) {
            ++self->m_LocalDrops;
        }
    }

    int m_RefCount = 1;
    ModContext *m_Context = nullptr;
    std::size_t m_Capacity = 0;
    std::uint64_t m_LocalDrops = 0;
    EventsImc::AllSubscription m_Subscription;
    std::deque<QueuedEvent> m_Queue;
};

int OpenEvents(EventStream *&out, int capacity) {
    out = nullptr;
    if (capacity < 0)
        return BML_ERROR_INVALID_PARAMETER;
    if (capacity == 0)
        capacity = 256;
    BML::ScriptImcClients *clients = nullptr;
    ModContext *context = nullptr;
    int status = GetActiveImcClients(clients, context);
    EventsImc::Client *client = nullptr;
    if (status == BML_OK) status = clients->Events(client);
    if (status != BML_OK)
        return status;
    EventStream *result = new (std::nothrow) EventStream(
        context, static_cast<std::size_t>(capacity));
    if (!result)
        return BML_ERROR_OUT_OF_MEMORY;
    status = result->Open(*client);
    if (status != BML_OK) {
        result->Release();
        return status;
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
           Register(engine, engine->RegisterObjectMethod("Event", "BML::Vec3 get_MassCenter() const", asMETHOD(ScriptEvent, GetMassCenter), asCALL_THISCALL), "Event::MassCenter", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "int get_ConvexMeshCount() const", asMETHOD(ScriptEvent, GetConvexMeshCount), asCALL_THISCALL), "Event::ConvexMeshCount", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "CKObject@ BorrowConvexMesh(int index) const", BML_AS_GENERIC_METHOD(&ScriptEvent::BorrowConvexMesh), asCALL_GENERIC), "Event::BorrowConvexMesh", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "int get_BallCount() const", asMETHOD(ScriptEvent, GetBallCount), asCALL_THISCALL), "Event::BallCount", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Event", "BML::Vec3 GetBallCenter(int index) const", asMETHOD(ScriptEvent, GetBallCenter), asCALL_THISCALL), "Event::GetBallCenter", errorMessage) &&
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
    BML_AS_PROPERTY("EntityTransform", "BML::Vec3 Position", asOFFSET(EntityTransform, Position));
    BML_AS_PROPERTY("EntityTransform", "BML::Vec3 Scale", asOFFSET(EntityTransform, Scale));
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
    BML_AS_PROPERTY("LevelState", "BML::Mat4 ResetMatrix", asOFFSET(LevelState, ResetMatrix));
    BML_AS_PROPERTY("LevelState", "int Points", asOFFSET(LevelState, Points));
    BML_AS_PROPERTY("EnergyState", "int Points", asOFFSET(EnergyState, Points));
    BML_AS_PROPERTY("EnergyState", "int Lives", asOFFSET(EnergyState, Lives));
    BML_AS_PROPERTY("EnergyState", "int StartPoints", asOFFSET(EnergyState, StartPoints));
    BML_AS_PROPERTY("EnergyState", "int StartLives", asOFFSET(EnergyState, StartLives));
    BML_AS_PROPERTY("EnergyState", "float TimeFactor", asOFFSET(EnergyState, TimeFactor));
    BML_AS_PROPERTY("EnergyState", "int LifeBonus", asOFFSET(EnergyState, LifeBonus));
    BML_AS_PROPERTY("CatalogEntry", "int Bonus", asOFFSET(CatalogEntry, Bonus));
    BML_AS_PROPERTY("CatalogEntry", "int Music", asOFFSET(CatalogEntry, Music));
    BML_AS_PROPERTY("Checkpoint", "BML::Mat4 Matrix", asOFFSET(Checkpoint, Matrix));
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

int RegisterScriptImcFacade(asIScriptEngine *engine, const char **errorMessage) {
    if (!engine) {
        g_ImcFacadeRegistrationError = "IMC script facade received a null engine.";
        if (errorMessage)
            *errorMessage = g_ImcFacadeRegistrationError.c_str();
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
