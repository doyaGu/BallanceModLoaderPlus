#include "ScriptBuiltinFacade.h"

#include <cstddef>
#include <limits>
#include <new>
#include <string>
#include <utility>
#include <vector>

#include <angelscript.h>

#include "BML/Events.h"
#include "BML/Gameplay.h"
#include "BML/EventKinds.h"

#include "BuiltinCapabilities.h"
#include "EventStreams.h"
#include "ModContext.h"
#include "ScriptMod.h"
#include "ScriptModRuntime.h"
#include "ScriptFunctionSupport.h"
#include "ScriptStringInterop.h"

namespace {

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

std::string g_FacadeRegistrationError;

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
    g_FacadeRegistrationError = "Failed to register built-in facade declaration: ";
    g_FacadeRegistrationError += declaration;
    g_FacadeRegistrationError += " returned ";
    g_FacadeRegistrationError += std::to_string(status);
    if (errorMessage)
        *errorMessage = g_FacadeRegistrationError.c_str();
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

int GetActiveFacadeContext(ModContext *&outContext, const char *apiName) {
    outContext = nullptr;
    if (BML::RejectScriptRestrictedHostCall(apiName))
        return BML_ERROR_FROZEN;
    BML::ScriptMod *mod = BML::ScriptModRuntime::GetCurrentScriptMod();
    if (!mod || !mod->GetModContext())
        return BML_ERROR_UNAVAILABLE;
    outContext = mod->GetModContext();
    return BML_OK;
}

ModContext *RequireRuntimeContext() {
    ModContext *context = nullptr;
    const int status = GetActiveFacadeContext(context, "BML::Runtime");
    if (status == BML_OK)
        return context;
    if (status != BML_ERROR_FROZEN) {
        BML::ScriptStringInterop::RaiseActiveException(
            "BML::Runtime requires an active script mod callback.");
    }
    return nullptr;
}

RuntimeState GetRuntimeState() {
    ModContext *context = RequireRuntimeContext();
    if (!context)
        return {};
    const BML::RuntimeStateSnapshot state = context->ReadRuntimeState();
    return {state.InGame, state.InLevel, state.Paused, state.Playing,
            state.CheatEnabled};
}

ClockState GetRuntimeClock() {
    ModContext *context = RequireRuntimeContext();
    CKTimeManager *time = context ? context->GetTimeManager() : nullptr;
    if (!time) {
        if (context) {
            BML::ScriptStringInterop::RaiseActiveException(
                "BML::Runtime clock is unavailable.");
        }
        return {};
    }
    const CKDWORD tick = time->GetMainTickCount();
    const CKDWORD maxFrame = static_cast<CKDWORD>((std::numeric_limits<int>::max)());
    return {time->GetTime(), time->GetAbsoluteTime(), time->GetLastDeltaTime(),
            tick > maxFrame ? (std::numeric_limits<int>::max)()
                            : static_cast<int>(tick)};
}

ScoreState GetRuntimeScore() {
    ModContext *context = RequireRuntimeContext();
    return context ? ScoreState{context->GetSRScore(), context->GetHSScore()}
                   : ScoreState{};
}

int ReadLevel(LevelState &out) {
    ModContext *context = nullptr;
    int status = GetActiveFacadeContext(context, "BML::Gameplay");
    BML_GameplayLevelState value = {};
    if (status == BML_OK) status = ReadBuiltinGameplayLevel(*context, value);
    if (status == BML_OK)
        out = {value.Id, value.ActiveBall, value.ResetMatrix, value.Points};
    return status;
}

int ReadEnergy(EnergyState &out) {
    ModContext *context = nullptr;
    int status = GetActiveFacadeContext(context, "BML::Gameplay");
    BML_GameplayEnergyState value = {};
    if (status == BML_OK) status = ReadBuiltinGameplayEnergy(*context, value);
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

CKObject *BorrowActiveBall(const LevelState *value) {
    return value ? ResolveScriptObject(value->ActiveBall) : nullptr;
}

CKObject *BorrowCheckpointObject(const Checkpoint *value) {
    return value ? ResolveScriptObject(value->Object) : nullptr;
}

CKObject *BorrowResetpointObject(const Resetpoint *value) {
    return value ? ResolveScriptObject(value->Object) : nullptr;
}

class ScriptArrayOutput {
public:
    ScriptArrayOutput() = default;
    ScriptArrayOutput(const ScriptArrayOutput &) = delete;
    ScriptArrayOutput &operator=(const ScriptArrayOutput &) = delete;

    ~ScriptArrayOutput() {
        if (m_Array && m_Api && m_Api->ArrayRelease)
            (void)m_Api->ArrayRelease(m_Array);
    }

    int Create(const char *declaration, std::size_t count) {
        if (count > static_cast<std::size_t>((std::numeric_limits<CKDWORD>::max)()))
            return BML_ERROR_OUT_OF_MEMORY;

        BML::ScriptMod *mod = BML::ScriptModRuntime::GetCurrentScriptMod();
        const BML::ScriptModRuntime *runtime =
            BML::ScriptModRuntime::GetCurrentScriptModRuntime();
        if (!runtime && mod)
            runtime = &mod->GetRuntimeForFacade();
        if (!runtime || !runtime->GetAngelScript())
            return BML_ERROR_SCRIPT_EXECUTION;

        m_Api = &runtime->GetApi();
        if (!m_Api->CreateArray || !m_Api->ArrayRelease ||
            !m_Api->ArrayGetElementAddress) {
            return BML_ERROR_NOT_IMPLEMENTED;
        }

        const CKAS_STATUS status = m_Api->CreateArray(
            runtime->GetAngelScript(), declaration, static_cast<CKDWORD>(count),
            &m_Array);
        if (status == CKAS_EXECUTIONFAILED)
            return BML_ERROR_OUT_OF_MEMORY;
        return status == CKAS_OK && m_Array ? BML_OK
                                            : BML_ERROR_SCRIPT_EXECUTION;
    }

    template <typename Value>
    int MoveElement(CKDWORD index, Value &value) {
        void *element = nullptr;
        if (!m_Api || !m_Array ||
            m_Api->ArrayGetElementAddress(m_Array, index, &element) != CKAS_OK ||
            !element) {
            return BML_ERROR_SCRIPT_EXECUTION;
        }
        *static_cast<Value *>(element) = std::move(value);
        return BML_OK;
    }

    void *Detach() {
        void *array = m_Array;
        m_Array = nullptr;
        return array;
    }

private:
    const CKAngelScriptAdapter::Api *m_Api = nullptr;
    void *m_Array = nullptr;
};

template <typename Value, typename Reader>
int CreateGameplayArray(void *&out, const char *declaration,
                        std::size_t count, Reader readElement) {
    out = nullptr;
    ScriptArrayOutput array;
    int status = array.Create(declaration, count);
    if (status != BML_OK)
        return status;
    try {
        for (std::size_t i = 0; i < count; ++i) {
            Value value{};
            status = readElement(i, value);
            if (status != BML_OK)
                return status;
            status = array.MoveElement(static_cast<CKDWORD>(i), value);
            if (status != BML_OK)
                return status;
        }
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_SCRIPT_EXECUTION;
    }
    out = array.Detach();
    return BML_OK;
}

int ReadCatalog(void *&out) {
    out = nullptr;
    ModContext *context = nullptr;
    int status = GetActiveFacadeContext(context, "BML::Gameplay");
    std::size_t count = 0;
    if (status == BML_OK)
        status = ReadBuiltinGameplayCatalogCount(*context, count);
    if (status != BML_OK)
        return status;
    return CreateGameplayArray<CatalogEntry>(
        out, "array<BML::Gameplay::CatalogEntry>", count,
        [context](std::size_t i, CatalogEntry &value) {
            BML_GameplayCatalogEntry row = {};
            const int rowStatus = ReadBuiltinGameplayCatalogEntry(*context, i, row);
            if (rowStatus != BML_OK)
                return rowStatus;
            value = {std::string(row.File), std::string(row.StartBall),
                     std::string(row.Sky), row.Bonus, row.Music};
            return BML_OK;
        });
}

int ReadCheckpoints(void *&out) {
    out = nullptr;
    ModContext *context = nullptr;
    int status = GetActiveFacadeContext(context, "BML::Gameplay");
    std::size_t count = 0;
    if (status == BML_OK)
        status = ReadBuiltinGameplayCheckpointCount(*context, count);
    if (status != BML_OK)
        return status;
    return CreateGameplayArray<Checkpoint>(
        out, "array<BML::Gameplay::Checkpoint>", count,
        [context](std::size_t i, Checkpoint &value) {
            BML_GameplayCheckpoint row = {};
            const int rowStatus = ReadBuiltinGameplayCheckpoint(*context, i, row);
            if (rowStatus != BML_OK)
                return rowStatus;
            value = {row.Matrix, row.Object};
            return BML_OK;
        });
}

int ReadResetpoints(void *&out) {
    out = nullptr;
    ModContext *context = nullptr;
    int status = GetActiveFacadeContext(context, "BML::Gameplay");
    std::size_t count = 0;
    if (status == BML_OK)
        status = ReadBuiltinGameplayResetpointCount(*context, count);
    if (status != BML_OK)
        return status;
    return CreateGameplayArray<Resetpoint>(
        out, "array<BML::Gameplay::Resetpoint>", count,
        [context](std::size_t i, Resetpoint &value) {
            BML_GameplayResetpoint row = {};
            const int rowStatus = ReadBuiltinGameplayResetpoint(*context, i, row);
            if (rowStatus != BML_OK)
                return rowStatus;
            value = {row.Object};
            return BML_OK;
        });
}
/* Script events own decoded domain values, never CK pointers. Object references
 * are resolved only by Borrow* calls, so destroyed Virtools objects remain safe. */
class ScriptEvent {
public:
    ScriptEvent(ModContext *context, BML::Events::Event value)
        : m_Context(context), m_Value(std::move(value)) {}

    void AddRef() { ++m_RefCount; }
    void Release() {
        if (--m_RefCount == 0)
            delete this;
    }

    bool IsValid() const { return m_Context != nullptr; }
    int Status() const { return IsValid() ? BML_OK : BML_ERROR_IMC_HANDLE_STALE; }
    int GetKind() const { return m_Value.Kind; }
    std::uint64_t GetSequence() const { return m_Value.Sequence; }
    std::uint64_t GetTimestamp() const { return m_Value.Timestamp; }

    bool IsLoad() const { return m_Value.LoadData.has_value(); }
    bool IsPhysics() const { return m_Value.PhysicsData.has_value(); }
    bool IsCommand() const { return m_Value.CommandData.has_value(); }
    bool IsConfig() const { return m_Value.ConfigData.has_value(); }
    bool IsCheat() const { return m_Value.CheatData.has_value(); }

    std::string GetFilename() const { return Load() ? Load()->Filename : std::string(); }
    bool GetIsMap() const { return Load() ? Load()->IsMap : false; }
    std::string GetMasterName() const { return Load() ? Load()->MasterName : std::string(); }
    int GetFilterClass() const { return Load() ? Load()->FilterClass : 0; }
    bool GetAddToScene() const { return Load() ? Load()->AddToScene : false; }
    bool GetReuseMeshes() const { return Load() ? Load()->ReuseMeshes : false; }
    bool GetReuseMaterials() const { return Load() ? Load()->ReuseMaterials : false; }
    bool GetDynamic() const { return Load() ? Load()->Dynamic : false; }
    CKObject *BorrowMasterObject() const {
        return Load() ? Resolve(Load()->MasterObject) : nullptr;
    }
    CKObject *BorrowScript() const {
        return Load() ? Resolve(Load()->Script) : nullptr;
    }
    int GetObjectCount() const { return Load() ? Count(Load()->Objects.size()) : 0; }
    CKObject *BorrowObject(int index) const {
        return Load() ? ResolveAt(Load()->Objects, index) : nullptr;
    }

    CKObject *BorrowTarget() const {
        return Physics() ? Resolve(Physics()->Target) : nullptr;
    }
    bool GetFixed() const { return Physics() ? Physics()->Fixed : false; }
    float GetFriction() const { return Physics() ? Physics()->Friction : 0.0f; }
    float GetElasticity() const { return Physics() ? Physics()->Elasticity : 0.0f; }
    float GetMass() const { return Physics() ? Physics()->Mass : 0.0f; }
    std::string GetCollisionGroup() const {
        return Physics() ? Physics()->CollisionGroup : std::string();
    }
    bool GetStartFrozen() const { return Physics() ? Physics()->StartFrozen : false; }
    bool GetEnableCollision() const { return Physics() ? Physics()->EnableCollision : false; }
    bool GetAutoCalculateMassCenter() const {
        return Physics() ? Physics()->AutoCalculateMassCenter : false;
    }
    float GetLinearDamp() const { return Physics() ? Physics()->LinearDamp : 0.0f; }
    float GetRotDamp() const { return Physics() ? Physics()->RotDamp : 0.0f; }
    std::string GetCollisionSurface() const {
        return Physics() ? Physics()->CollisionSurface : std::string();
    }
    BML_Vec3 GetMassCenter() const {
        return Physics() ? Physics()->MassCenter : BML_Vec3{};
    }
    int GetConvexMeshCount() const {
        return Physics() ? Count(Physics()->ConvexMeshes.size()) : 0;
    }
    CKObject *BorrowConvexMesh(int index) const {
        return Physics() ? ResolveAt(Physics()->ConvexMeshes, index) : nullptr;
    }
    int GetBallCount() const {
        return Physics() ? Count(Physics()->BallCenters.size()) : 0;
    }
    BML_Vec3 GetBallCenter(int index) const {
        return Physics() ? At(Physics()->BallCenters, index) : BML_Vec3{};
    }
    float GetBallRadius(int index) const {
        return Physics() ? At(Physics()->BallRadii, index) : 0.0f;
    }
    int GetConcaveMeshCount() const {
        return Physics() ? Count(Physics()->ConcaveMeshes.size()) : 0;
    }
    CKObject *BorrowConcaveMesh(int index) const {
        return Physics() ? ResolveAt(Physics()->ConcaveMeshes, index) : nullptr;
    }

    std::string GetCommand() const {
        return Command() ? Command()->Name : std::string();
    }
    int GetCommandArgumentCount() const {
        return Command() ? Count(Command()->Arguments.size()) : 0;
    }
    std::string GetCommandArgument(int index) const {
        return Command() ? At(Command()->Arguments, index) : std::string();
    }
    std::string GetConfigCategory() const {
        return Config() ? Config()->Category : std::string();
    }
    std::string GetConfigKey() const {
        return Config() ? Config()->Key : std::string();
    }
    int GetConfigType() const { return Config() ? Config()->Type : 0; }
    std::string GetConfigValue() const {
        return Config() ? Config()->Value : std::string();
    }
    bool GetCheatEnabled() const {
        return Cheat() ? Cheat()->Enabled : false;
    }

private:
    const BML::Events::Load *Load() const {
        return m_Value.LoadData ? &*m_Value.LoadData : nullptr;
    }
    const BML::Events::Physics *Physics() const {
        return m_Value.PhysicsData ? &*m_Value.PhysicsData : nullptr;
    }
    const BML::Events::Command *Command() const {
        return m_Value.CommandData ? &*m_Value.CommandData : nullptr;
    }
    const BML::Events::Config *Config() const {
        return m_Value.ConfigData ? &*m_Value.ConfigData : nullptr;
    }
    const BML::Events::Cheat *Cheat() const {
        return m_Value.CheatData ? &*m_Value.CheatData : nullptr;
    }

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
    BML::Events::Event m_Value;
};

/* The script side of a stream is the loader's own queue with a reference count
 * on top: the queue in EventStreams.h holds the events, and this holds the
 * ModContext a polled event needs to turn an ObjectRef back into a CKObject. */
class EventStream {
public:
    explicit EventStream(ModContext *context) : m_Context(context) {}
    ~EventStream() { (void)Close(); }

    void AddRef() { ++m_RefCount; }
    void Release() {
        if (--m_RefCount == 0)
            delete this;
    }

    int Open(int capacity) { return BML::OpenEventStream(capacity, m_Handle); }

    bool IsOpen() const { return m_Handle != nullptr; }

    int Close() {
        if (!m_Handle)
            return BML_ERROR_INVALID_HANDLE;
        const int status = BML::CloseEventStream(m_Handle);
        m_Handle = nullptr;
        return status;
    }

    int GetDroppedCount(int &out) const {
        out = 0;
        if (!m_Handle)
            return BML_ERROR_INVALID_HANDLE;
        return BML::ReadEventStreamDroppedCount(m_Handle, out);
    }

    int Poll(ScriptEvent *&out) {
        out = nullptr;
        if (!m_Handle)
            return BML_ERROR_INVALID_HANDLE;

        // Taking the queued event as the whole C++ value copies its strings and
        // its lists, so the allocation is caught here rather than in a script.
        BML::Events::Event value;
        int status = BML_OK;
        try {
            status = BML::PollEventStreamValue(m_Handle, value);
        } catch (const std::bad_alloc &) {
            return BML_ERROR_OUT_OF_MEMORY;
        } catch (...) {
            return BML_ERROR_FAIL;
        }
        if (status != BML_OK)
            return status;

        ScriptEvent *event = new (std::nothrow) ScriptEvent(m_Context, std::move(value));
        if (!event)
            return BML_ERROR_OUT_OF_MEMORY;
        out = event;
        return BML_OK;
    }

private:
    int m_RefCount = 1;
    ModContext *m_Context = nullptr;
    BML_EventStream m_Handle = nullptr;
};

int OpenEvents(EventStream *&out, int capacity) {
    out = nullptr;
    ModContext *context = nullptr;
    int status = GetActiveFacadeContext(context, "BML::Events");
    if (status != BML_OK)
        return status;
    EventStream *result = new (std::nothrow) EventStream(context);
    if (!result)
        return BML_ERROR_OUT_OF_MEMORY;
    status = result->Open(capacity);
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
    return Register(engine, engine->RegisterGlobalFunction("State GetState()", BML_AS_GENERIC_FUNCTION(&GetRuntimeState), asCALL_GENERIC), "Runtime::GetState", errorMessage) &&
           Register(engine, engine->RegisterGlobalFunction("Clock GetClock()", BML_AS_GENERIC_FUNCTION(&GetRuntimeClock), asCALL_GENERIC), "Runtime::GetClock", errorMessage) &&
           Register(engine, engine->RegisterGlobalFunction("Score GetScore()", BML_AS_GENERIC_FUNCTION(&GetRuntimeScore), asCALL_GENERIC), "Runtime::GetScore", errorMessage) &&
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
           Register(engine, engine->RegisterGlobalFunction("int ReadLevel(LevelState &out state)", BML_AS_GENERIC_FUNCTION(&ReadLevel), asCALL_GENERIC), "Gameplay::ReadLevel", errorMessage) &&
           Register(engine, engine->RegisterGlobalFunction("int ReadEnergy(EnergyState &out state)", BML_AS_GENERIC_FUNCTION(&ReadEnergy), asCALL_GENERIC), "Gameplay::ReadEnergy", errorMessage) &&
           Register(engine, engine->RegisterGlobalFunction("int ReadCatalog(array<CatalogEntry>@ &out entries)", BML_AS_GENERIC_FUNCTION(&ReadCatalog), asCALL_GENERIC), "Gameplay::ReadCatalog", errorMessage) &&
           Register(engine, engine->RegisterGlobalFunction("int ReadCheckpoints(array<Checkpoint>@ &out entries)", BML_AS_GENERIC_FUNCTION(&ReadCheckpoints), asCALL_GENERIC), "Gameplay::ReadCheckpoints", errorMessage) &&
           Register(engine, engine->RegisterGlobalFunction("int ReadResetpoints(array<Resetpoint>@ &out entries)", BML_AS_GENERIC_FUNCTION(&ReadResetpoints), asCALL_GENERIC), "Gameplay::ReadResetpoints", errorMessage) &&
           Register(engine, engine->SetDefaultNamespace(""), "namespace reset", errorMessage);
}

} // namespace

int RegisterScriptBuiltinFacade(asIScriptEngine *engine, const char **errorMessage) {
    if (!engine) {
        g_FacadeRegistrationError = "The built-in script facade received a null engine.";
        if (errorMessage)
            *errorMessage = g_FacadeRegistrationError.c_str();
        return asERROR;
    }
    if (!RegisterRuntime(engine, errorMessage) ||
        !RegisterGameplay(engine, errorMessage) ||
        !RegisterEvents(engine, errorMessage)) {
        engine->SetDefaultNamespace("");
        return asERROR;
    }
    return asSUCCESS;
}
